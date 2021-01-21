// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MountingTree.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/Streams/PathUtils.h"

namespace Assets
{
	using HashValue = uint64;

	class MountingTree::Pimpl
	{
	public:
		FilenameRules _rules;

		class Mount
		{
		public:
			HashValue	_hash;
			unsigned	_depth;	// number of different path sections combined into the hash value
			std::shared_ptr<IFileSystem> _fileSystem;
			MountID		_id;
			SplitPath<utf8>	_mountPoint;
			std::basic_string<utf8> _mountPointBuffer;

			Mount& operator=(const Mount&) = delete;
			Mount(const Mount&) = delete;
			Mount& operator=(Mount&&);
			Mount(Mount&&);
			Mount();
			Mount(HashValue hash, unsigned depth, const std::shared_ptr<IFileSystem>& fileSystem, MountID id, SplitPath<utf8>&&	mountPoint, std::basic_string<utf8>&& mountPointBuffer);
		};

		std::vector<Mount>	_mounts;	// ordered from highest to lowest priority
		uint32				_changeId = 0;
		Threading::RecursiveMutex	_mountsLock;
		bool				_hasAtLeastOneMount;
		AbsolutePathMode    _absolutePathMode;

		Pimpl(const FilenameRules &rules) : _rules(rules), _hasAtLeastOneMount(false),
		                                    _absolutePathMode(AbsolutePathMode::RawOS) {}
	};

	template<typename CharType>
		static bool	IsSeparator(CharType chr)
	{
		return chr == CharType('/') || chr == CharType('\\');
	}

	template<typename CharType>
		static const CharType* FindFirstSeparator(StringSection<CharType> section)
	{
		const CharType seps[] = { (CharType)'\\', (CharType)'/' };
		return std::find_first_of(section.begin(), section.end(), seps, ArrayEnd(seps));
	}

	template<typename CharType>
		static const CharType* SkipSeparators(StringSection<CharType> section)
	{
		auto i = section.begin();
		for (;i!=section.end(); ++i)
			if (!IsSeparator(*i)) break;
		return i;
	}

	template<typename CharType>
		const CharType* XlFindChar(StringSection<CharType> section, CharType chr)
		{
				// This is a simple implementation that doesn't support searching for multibyte character
				// types in UTF8 or pairs in UTF16.
				// Due to the way UTF8 and UTF16 are defined, we will never get incorrect matches -- but
				// we can't use this to search for code points that are larger than the size of a single "CharType"
				//		-- that can only be done with a version of this function that searches for a string
				// Note; it's different from strchr in that we return "section.end()" if the chr isn't found
			return std::find(section.begin(), section.end(), chr);
		}

	template<typename CharType>
		auto MountingTree::EnumerableLookup::TryGetNext_Internal(CandidateObject& result) const -> Result
	{
		ScopedLock(_pimpl->_mountsLock);
		if (!_pimpl || _pimpl->_changeId != _changeId)
			return Result::Invalidated;

		auto requestString = MakeStringSection(
			(const CharType*)AsPointer(_request.cbegin()), 
			(const CharType*)AsPointer(_request.cend()));

		for (;;) {
			if (_nextMountToTest >= (uint32)_pimpl->_mounts.size())
				return Result::NoCandidates;

			const auto& mt = _pimpl->_mounts[_nextMountToTest];
			++_nextMountToTest;

			// simple case for mount depth 0
			if (mt._depth == 0) {
				IFileSystem::Marker marker;
				auto transResult = mt._fileSystem->TryTranslate(marker, requestString);
				if (transResult == IFileSystem::TranslateResult::Success) {
					result._fileSystem = mt._fileSystem;
					result._marker = std::move(marker);
					result._mountPoint = mt._mountPointBuffer;
					return Result::Success;
				}
				continue;
			}

			// If the mount point is too deep, we can't match it.
			// It's a limitation of the system, but helps us write a little optimisation.
			if (mt._depth > dimof(_cachedRemainders)) continue;

			if (_cachedRemainders[mt._depth-1] == ~0u) {
				// build the cached value for this depth
				for (auto d = 0u; d < mt._depth; ++d) {
					if (_cachedRemainders[d] != ~0u) continue;
					auto startingPt = (d == 0) ? 0 : _cachedRemainders[d - 1];
					auto s = MakeStringSection(PtrAdd(requestString.begin(), startingPt), requestString.end());
					auto sep = FindFirstSeparator(s);

					if (sep == s.end()) {
						// We didn't find a separator, or we started with an empty string. There are no
						// further matches possible. We can just clear out the rest of the array
						for (unsigned d2 = d; d2 < dimof(_cachedRemainders); ++d2) {
							_cachedHashValues[d2] = 0x0;
							_cachedRemainders[d2] = (uint32)(size_t(s.end()) - size_t(AsPointer(_request.begin())));
						}
						break;
					}

					auto newRemainder = SkipSeparators(MakeStringSection(sep, s.end()));
					_cachedRemainders[d] = (uint32)PtrDiff(newRemainder, requestString.begin());

					auto rootHash = (d == 0) ? 0 : _cachedHashValues[d - 1];
					_cachedHashValues[d] = HashCombine(
						rootHash, 
						HashFilename(MakeStringSection(s.begin(), sep), _pimpl->_rules));
				}
			}

			if (_cachedHashValues[mt._depth-1] == mt._hash) {
				// We got a match. We have to pass this onto the filesystem to try to translate 
				// it into a "Marker" which can later be used for file operations.
				// Note that if the filesystem is still mounting, we can get a "pending/mounting" state for
				// some files that will later become available.
				auto remainderSection = MakeStringSection(
					PtrAdd(requestString.begin(), _cachedRemainders[mt._depth-1]), 
					requestString.end());

				IFileSystem::Marker marker;
				auto transResult = mt._fileSystem->TryTranslate(marker, remainderSection);
				if (transResult == IFileSystem::TranslateResult::Success) {
					result._fileSystem = mt._fileSystem;
					result._marker = std::move(marker);
					result._mountPoint = mt._mountPointBuffer;
					return Result::Success;
				}
			}
		}
	}

	auto MountingTree::EnumerableLookup::TryGetNext(CandidateObject& result) const -> Result
	{
		if (IsGood()) {
			if (_encoding == Encoding::UTF8) {
				return TryGetNext_Internal<utf8>(result);
			} else if (_encoding == Encoding::UTF16) {
				return TryGetNext_Internal<utf16>(result);
			}
		}

		return Result::NoCandidates;
	}

	MountingTree::EnumerableLookup::EnumerableLookup(
		std::vector<uint8>&& request, Encoding encoding, MountingTree::Pimpl* pimpl)
	: _request(std::move(request))
	, _encoding(encoding)
	, _nextMountToTest(0)
	, _pimpl(pimpl)
	{
		// get the mounts lock to make sure we get the correct value from _changeId
		ScopedLock(pimpl->_mountsLock);
		_changeId = pimpl->_changeId;
	}

	MountingTree::EnumerableLookup::EnumerableLookup()
	: _encoding(Encoding::UTF8)
	, _nextMountToTest(0)
	, _changeId(0)
	, _pimpl(nullptr)
	{}

	template<typename CharType>
		static CharType IsRawFilesystem(StringSection<CharType> filename)
	{
		bool isRawFilesystem = IsSeparator(*filename.begin());
		auto* firstSep = FindFirstSeparator(filename);
		auto* driveMarker = std::find(filename.begin(), firstSep, (CharType)':');
		isRawFilesystem |= driveMarker != firstSep;
		return isRawFilesystem;
	}

	auto MountingTree::Lookup(StringSection<utf8> filename) -> EnumerableLookup
	{
		// If the filename begins with a "/" or a Windows-style drive (eg, c:/) then we can't
		// use the mounting system, and we must drop back to the raw OS filesystem.
		if (filename.IsEmpty()) return EnumerableLookup();

		// todo -- fall back to raw filesystem in this case
		if (_pimpl->_absolutePathMode == AbsolutePathMode::RawOS && IsRawFilesystem(filename))
			return EnumerableLookup();

		if (!_pimpl->_hasAtLeastOneMount) return EnumerableLookup();

		// We need to find all possible matching candidates for this filename. There are a number
		// of possible ways to this.
		//
		// Consider a filename like:
		//		one/two/three/filename.ext
		// and a filesystem mounted at:
		//		one/two
		//
		// We need to compare the "one" and "two" against the filesystem mounting point.
		//
		// There are couple of approaches...
		// We maintain a linear list of filesystem, ordered by priority. In this case, we store a
		// single hash value and a depth value for each filesystem.
		// We must calculate a comparison hash value from "filename" that matches the correct depth.
		// Then we just compare that with the filesystem hash value.
		//
		// Another possibility is to arrange the filesystems in a tree (like a directory tree). We walk
		// through the tree, comparing the path section against the values in the tree.
		// After finding all candidates, we have to sort by priority order.
		//
		// In most cases, we should have only a few filesystems (let's say, less than 10). Maybe for
		// final production games we might only have 3 or 4.
		// So, given this, it seems like maybe the linear list could be the ideal option? Anyway, it 
		// gives the fastest resolution when the highest priority filesystem is the one selected.

		std::vector<uint8> request(filename.begin(), filename.end());
		return EnumerableLookup 
			{
				std::move(request), EnumerableLookup::Encoding::UTF8, _pimpl.get()
			};
	}

	auto MountingTree::Lookup(StringSection<utf16> filename) -> EnumerableLookup
	{
		if (filename.IsEmpty()) return EnumerableLookup();
		if (_pimpl->_absolutePathMode == AbsolutePathMode::RawOS && IsRawFilesystem(filename))
			return EnumerableLookup();
		if (!_pimpl->_hasAtLeastOneMount) return EnumerableLookup();
		std::vector<uint8> request((const uint8*)filename.begin(), (const uint8*)filename.end());
		return EnumerableLookup
			{
				std::move(request), EnumerableLookup::Encoding::UTF16, _pimpl.get()
			};
	}

	auto MountingTree::Mount(StringSection<utf8> mountPointInput, std::shared_ptr<IFileSystem> system) -> MountID
	{
			// Note that we're going to be ignoring slashs at the beginning or end. These have no effect 
			// on how we interpret the mount point.
			// Let's do some normalization of the input to avoid any edge cases
		std::string mountPoint;
		{
			auto mountPointSplitPath = MakeSplitPath(mountPointInput);
			mountPointSplitPath.BeginsWithSeparator() = true;
			mountPointSplitPath.EndsWithSeparator() = true;
			mountPoint = mountPointSplitPath.Simplify().Rebuild(_pimpl->_rules);
		}

		auto split = MakeSplitPath(mountPoint);

		uint64 hash = 0;
		for (auto i:split.GetSections())
			hash = HashCombine(hash, HashFilename(i, _pimpl->_rules));
		
		ScopedLock(_pimpl->_mountsLock);
		MountID id = _pimpl->_changeId++;
		auto sectionCount = split.GetSectionCount();
		_pimpl->_mounts.emplace_back(Pimpl::Mount{hash, sectionCount, std::move(system), id, std::move(split), std::move(mountPoint)});
		_pimpl->_hasAtLeastOneMount = true;
		return id;
	}

	void MountingTree::Unmount(MountID mountId)
	{
		// just search for the mount with the same id, and remove it
		ScopedLock(_pimpl->_mountsLock);
		auto i = std::find_if(
			_pimpl->_mounts.begin(), _pimpl->_mounts.end(),
			[mountId](const Pimpl::Mount& m) { return m._id == mountId; });
		if (i != _pimpl->_mounts.end())
			_pimpl->_mounts.erase(i);

		_pimpl->_hasAtLeastOneMount = !_pimpl->_mounts.empty();
	}

	IFileSystem* MountingTree::GetMountedFileSystem(MountID mountId)
	{
		ScopedLock(_pimpl->_mountsLock);
		auto i = std::find_if(
			_pimpl->_mounts.begin(), _pimpl->_mounts.end(),
			[mountId](const Pimpl::Mount& m) { return m._id == mountId; });
		if (i != _pimpl->_mounts.end())
			return i->_fileSystem.get();
		return nullptr;
	}

	std::basic_string<utf8> MountingTree::GetMountPoint(MountID mountId)
	{
		ScopedLock(_pimpl->_mountsLock);
		auto i = std::find_if(
			_pimpl->_mounts.begin(), _pimpl->_mounts.end(),
			[mountId](const Pimpl::Mount& m) { return m._id == mountId; });
		if (i != _pimpl->_mounts.end())
			return i->_mountPointBuffer;
		return {};
	}

	void MountingTree::SetAbsolutePathMode(AbsolutePathMode newMode)
	{
		_pimpl->_absolutePathMode = newMode;
	}

	FileSystemWalker MountingTree::BeginWalk(StringSection<utf8> initialSubDirectory)
	{
		// Find each filesystem that matches the 
		auto splitInitial = MakeSplitPath(initialSubDirectory);
		std::vector<FileSystemWalker::StartingFS> result;
		ScopedLock(_pimpl->_mountsLock);
		for (const auto&mount:_pimpl->_mounts) {
			auto searchingFs = std::dynamic_pointer_cast<ISearchableFileSystem>(mount._fileSystem);
			if (!searchingFs) continue;

			bool match = true;
			auto minCount = std::min(mount._depth, splitInitial.GetSectionCount());
			for (unsigned c=0; c<minCount; ++c)
				if (HashFilenameAndPath(splitInitial.GetSections()[c]) != HashFilenameAndPath(mount._mountPoint.GetSections()[c])) {
					match = false;
					break;
				}
			if (!match) continue;

			utf8 remainingPath[MaxPath];
			if (splitInitial.GetSectionCount() > mount._depth) {
				SplitPath<utf8>{
					std::vector<SplitPath<utf8>::Section>{
						&splitInitial.GetSections()[minCount],
						splitInitial.GetSections().end()}}.Rebuild(remainingPath);
				result.emplace_back(FileSystemWalker::StartingFS{{}, remainingPath, std::move(searchingFs), mount._id});	// (note that we use the mount id as the filesystem ie, due to behaviour in MainFileSystem::GetFileSystem)
			} else {
				SplitPath<utf8>{
					std::vector<SplitPath<utf8>::Section>{
						&mount._mountPoint.GetSections()[minCount],
						mount._mountPoint.GetSections().end()}}.Rebuild(remainingPath);
				result.emplace_back(FileSystemWalker::StartingFS{remainingPath, {}, std::move(searchingFs), mount._id});
			}
		}

		return result;
	}

	MountingTree::MountingTree(FilenameRules& rules)
	{
		_pimpl = std::make_unique<Pimpl>(rules);
		_pimpl->_changeId = 0;
	}

	MountingTree::~MountingTree() {}

	MountingTree::MountingTree(const MountingTree& cloneFrom) { assert(0);  }
	MountingTree& MountingTree::operator=(const MountingTree& cloneFrom) { assert(0); return *this; }


	MountingTree::Pimpl::Mount::Mount() 
	{
		_hash = 0;
		_depth = 0;
		_id = ~0u;
	}

	auto MountingTree::Pimpl::Mount::operator=(Mount&& moveFrom) -> Mount&
	{
		_hash = moveFrom._hash;
		_depth = moveFrom._depth;
		_fileSystem = std::move(moveFrom._fileSystem);
		_id = moveFrom._id;
		void* originalBaseAddress = moveFrom._mountPointBuffer.data();
		_mountPointBuffer = std::move(moveFrom._mountPointBuffer);
		if (_mountPointBuffer.data() != originalBaseAddress) {
			_mountPoint = MakeSplitPath(_mountPointBuffer);
		} else {
			_mountPoint = std::move(moveFrom._mountPoint);
		}

		moveFrom._hash = 0;
		moveFrom._depth = 0;
		moveFrom._id = ~0u;

		return *this;
	}

	MountingTree::Pimpl::Mount::Mount(Mount&& moveFrom)
	{
		_hash = moveFrom._hash;
		_depth = moveFrom._depth;
		_fileSystem = std::move(moveFrom._fileSystem);
		_id = moveFrom._id;
		void* originalBaseAddress = moveFrom._mountPointBuffer.data();
		_mountPointBuffer = std::move(moveFrom._mountPointBuffer);
		if (_mountPointBuffer.data() != originalBaseAddress) {
			_mountPoint = MakeSplitPath(_mountPointBuffer);
		} else {
			_mountPoint = std::move(moveFrom._mountPoint);
		}

		moveFrom._hash = 0;
		moveFrom._depth = 0;
		moveFrom._id = ~0u;
	}

	MountingTree::Pimpl::Mount::Mount(HashValue hash, unsigned depth, const std::shared_ptr<IFileSystem>& fileSystem, MountID id, SplitPath<utf8>&&	mountPoint, std::basic_string<utf8>&& mountPointBuffer)
	{
		_hash = hash;
		_depth = depth;
		_fileSystem = fileSystem;
		_id = id;
		_mountPoint = std::move(mountPoint);
		_mountPointBuffer = std::move(mountPointBuffer);
	}


}

