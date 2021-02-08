// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "System_Apple.h"
#include "../RawFS.h"
#include "../TimeUtils.h"
#include "../FileSystemMonitor.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/Threading/Mutex.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Core/SelectConfiguration.h"
#include "../../Core/Types.h"

#include <cstdio>
#include <mach/mach_time.h>
#include <mach-o/dyld.h>
#include <pthread/pthread.h>
#include <libgen.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include <unordered_map>
#include <unordered_set>
#include <thread>

namespace OSServices
{
	uint64 GetPerformanceCounter()
	{
		return mach_absolute_time();
	}
	
	uint64 GetPerformanceCounterFrequency()
	{
		mach_timebase_info_data_t tbInfo;
		mach_timebase_info(&tbInfo);
		return tbInfo.denom * 1000000000 / tbInfo.numer;
	}

#if 0 // PLATFORMOS_TARGET == PLATFORMOS_OSX
	struct MonitorEntry {
		std::string filepath;
		int fd;
		bool is_dir;
		std::vector<std::weak_ptr<OnChangeCallback>> callbacks;
	};
	
	template<typename T>
	static std::string FilePathFromDirectoryNameAndFileName(StringSection<T> directoryName, StringSection<T> filename) {
		std::basic_string<T> filepath = directoryName.begin();
		if (filepath.length() > 0 && filename.size() > 0) {
			filepath += "/";
		}
		filepath += filename.begin();
		const char *cpath = reinterpret_cast<const char *>(filepath.c_str());
		auto canonical_path = std::make_unique<char[]>(PATH_MAX);
		realpath(cpath, canonical_path.get());
		return canonical_path.get();
	}
	
	class KQueueMonitor {
	private:
		// Identifier for our kernel queue, retrived from a call to kqueue()
		int _kq;
		
		// Maps open file descriptions to the filepath opened
		std::unordered_map<int, std::string> _fd_to_path;
		
		// Maps filepaths to their monitor entries
		std::unordered_map<std::string, MonitorEntry> _entries;
		
		// Tracks the filepaths of deleted and not yet existing files that
		// we have been asked to track
		std::unordered_set<std::string> _deleted_files;
		
		// Lock for working with any property of this class
		Threading::RecursiveMutex _lock;
		
		// Monitor thread
		std::thread _thread;
		
		// When true, the monitor thread should end
		bool _shouldStop;
		
		// Gets the MonitorEntry for a given file description, it one exists
		std::optional<MonitorEntry*> EntryForFD(int fd);
		
		// Scans the directory at the given filepath and tracks new files if they are in
		// _deleted_files
		void DirScan(StringSection<char> path);
		
		// Runs the callbacks for the given entry
		void RunCallbacks(MonitorEntry &entry);
		
		// Loop for the monitor thread
		void MonitorLoop();
	public:
		KQueueMonitor();
		~KQueueMonitor();
		
		// Start monitoring the given filepath and run the given callback when it changes
		// This will add another callback if the filepath is already being monitored
		void Monitor(StringSection<char> filepath, std::shared_ptr<OnChangeCallback> callback);
		
		// Simulates what would happen if the given filepath was changed
		void FakeFileChange(StringSection<char> filepath);
	};
	
	std::optional<MonitorEntry*> KQueueMonitor::EntryForFD(int fd) {
		auto it = _fd_to_path.find(fd);
		if (it == _fd_to_path.end()) {
			return {};
		}
		auto maybeEntry = _entries.find(it->second);
		assert(maybeEntry != _entries.end());
		return &maybeEntry->second;
	}
	
	// Loop that watches for any file changes and triggers callbacks
	void KQueueMonitor::MonitorLoop() {
		while (!_shouldStop) {
			kevent64_s change;
			
			// Timeout is 500ms. After that it checks if the thread
			// should exit (the application has been closed)
			timespec t;
			t.tv_sec = 0;
			t.tv_nsec = 500 * 1000 * 1000; // 500ms
			int num_changes = kevent64(_kq, NULL, 0, &change, 1, 0, &t);
			if (num_changes > 0) {
				int fd = static_cast<int>(change.ident);
				ScopedLock(_lock);
				auto maybeEntry = EntryForFD(fd);
				if (maybeEntry) {
					MonitorEntry &entry = **maybeEntry;
					if (!entry.is_dir) {
						// If it's not a directory, we check if it's been written to
						// or if it's been deleted
						if (change.fflags & NOTE_DELETE && entry.fd != 0) {
							// The file has been deleted, let's remove references to it,
							// close the file descriptor, and trigger a directory search in
							// case a new file with the same name has been created
							_fd_to_path.erase(entry.fd);
							close(entry.fd);
							entry.fd = 0;
							_deleted_files.emplace(entry.filepath);
							
							// Trigger search
							auto dname = std::make_unique<char[]>(PATH_MAX);
							dirname_r(entry.filepath.c_str(), dname.get());
							DirScan(dname.get());
						} else {
							// The file has been written to. Let's trigger the callbacks for it
							RunCallbacks(entry);
						}
					} else {
						// A directory has been changed, let's trigger a search of it
						DirScan(entry.filepath);
					}
				}
			}
		}
	}
	
	void KQueueMonitor::DirScan(StringSection<char> path) {
		ScopedLock(_lock);
		auto entryIt = _entries.find(path.begin());
		if (entryIt == _entries.end()) {
			return;
		}
		auto &entry = entryIt->second;
		// List the dir and look for new files
		DIR *dir = opendir(entry.filepath.c_str());
		if (dir != NULL) {
			dirent *d_entry;
			while ((d_entry = readdir(dir)) != NULL) {
				// Skip "." and ".." entries
				if (strcmp(d_entry->d_name, ".") == 0 || strcmp(d_entry->d_name, "..") == 0) {
					continue;
				}
				
				// Construct the canonical path
				std::string full_path;
				if (entry.filepath.size()) {
					full_path = entry.filepath + "/" + d_entry->d_name;
					auto buffer = std::make_unique<char[]>(PATH_MAX);
					realpath(full_path.c_str(), buffer.get());
					full_path = buffer.get();
				} else {
					full_path = d_entry->d_name;
				}
				
				// Check if this is a new file that we care about
				auto it = _deleted_files.find(full_path);
				if (it != _deleted_files.end()) {
					// One of the files we've been tracking has been created
					// Let's start tracking it too
					_deleted_files.erase(it);
					Monitor(full_path, nullptr);
					auto entry = _entries[full_path];
					if (entry.fd) {
						RunCallbacks(entry);
					}
				}
			}
		}
		closedir(dir);
	}
	
	void KQueueMonitor::RunCallbacks(MonitorEntry &entry) {
		for (unsigned i = 0; i < entry.callbacks.size(); ++i) {
			auto weakCallback = entry.callbacks[i];
			auto callback = weakCallback.lock();
			if (callback) {
				callback->OnChange();
			} else {
				// The callback no longer exists, let's remove it
				entry.callbacks.erase(entry.callbacks.begin() + i);
				--i;
			}
		}
	}
	
	KQueueMonitor::KQueueMonitor() : _kq(kqueue()),
									 _shouldStop(false),
									 _thread(&KQueueMonitor::MonitorLoop, this) {
		assert(_kq != -1);
	}
	
	KQueueMonitor::~KQueueMonitor() {
		_shouldStop = true;
		for (auto &pair : _fd_to_path) {
			close(pair.first);
		}
		_thread.join();
	}
	
	void KQueueMonitor::Monitor(StringSection<char> filepath, std::shared_ptr<OnChangeCallback> callback) {
		ScopedLock(_lock);
		const char *cpath = filepath.begin();
		auto entryIt = _entries.find(cpath);
		
		// Create the entry if it doesn't already exist
		if (entryIt == _entries.end()) {
			_entries[cpath] = {};
			_entries[cpath].filepath = cpath;
			_entries[cpath].fd = 0;
			
			// Check if this path is a directory
			DIR *dir = opendir(cpath);
			_entries[cpath].is_dir = dir != NULL;
			if (dir) {
				closedir(dir);
			}
		}
		
		// Add the callback if given
		MonitorEntry &entry = _entries[cpath];
		if (callback) {
			entry.callbacks.push_back(callback);
		}
		if (entry.fd != 0) {
			// We are already tracking this file, so we don't need
			// to register it
			return;
		}
		
		// Start tracking the file
		int fd = open(cpath, O_EVTONLY);
		if (fd != -1) {
			entry.fd = fd;
			_fd_to_path[fd] = cpath;
			kevent64_s new_event;
			new_event.ident = fd;
			new_event.filter = EVFILT_VNODE;
			new_event.flags = EV_ADD | EV_CLEAR | EV_ENABLE;
			new_event.fflags = NOTE_WRITE | (entry.is_dir ? 0 : NOTE_DELETE);
			new_event.data = 0;
			new_event.udata = 0;
			// Register that we are tracking this file with the kernel
			kevent64(_kq, &new_event, 1, NULL, 0, 0, NULL);
		} else {
			// This file doesn't exist yet, add it to the list
			// of files to check for when a directory is updated
			_deleted_files.emplace(cpath);
		}
	}
	
	void KQueueMonitor::FakeFileChange(StringSection<char> filepath) {
		ScopedLock(_lock);
		auto maybeEntry = _entries.find(filepath.begin());
		if (maybeEntry == _entries.end()) {
			return;
		}
		RunCallbacks(maybeEntry->second);
	}
	
	static std::unique_ptr<KQueueMonitor> s_kq_monitor;
	static Threading::RecursiveMutex s_monitorLock;

	void EnsureMonitorExists() {
		ScopedLock(s_monitorLock);
		if (s_kq_monitor == nullptr) {
			s_kq_monitor = std::make_unique<KQueueMonitor>();
		}
	}

	void AttachFileSystemMonitor(StringSection<utf16> directoryName,
								 StringSection<utf16> filename,
								 std::shared_ptr<OnChangeCallback> callback) {
	}
	
	void AttachFileSystemMonitor(StringSection<utf8> directoryName,
								 StringSection<utf8> filename,
								 std::shared_ptr<OnChangeCallback> callback) {
		ScopedLock(s_monitorLock);
		EnsureMonitorExists();

		// First monitor the directory
		const char *cpath = reinterpret_cast<const char *>(directoryName.begin());
		auto canonical_path = std::make_unique<char[]>(PATH_MAX);
		realpath(cpath, canonical_path.get());
		s_kq_monitor->Monitor(canonical_path.get(), nullptr);
		
		// Now monitor the file itself
		auto filepath = FilePathFromDirectoryNameAndFileName(directoryName, filename);
		s_kq_monitor->Monitor(filepath, callback);
	}
	
	void    FakeFileChange(StringSection<utf16> directoryName, StringSection<utf16> filename) {
		
	}
	
	void    FakeFileChange(StringSection<utf8> directoryName, StringSection<utf8> filename) {
		ScopedLock(s_monitorLock);
		EnsureMonitorExists();

		auto filepath = FilePathFromDirectoryNameAndFileName(directoryName, filename);
		s_kq_monitor->FakeFileChange(filepath);
	}
	
	void TerminateFileSystemMonitoring() {
		ScopedLock(s_monitorLock);
		s_kq_monitor = nullptr;
	}

#elif 1

	namespace Internal
	{
		class DirectoryChanges : public KEvent
		{
		public:
			std::vector<std::string> FindChanges()
			{
				ScopedLock(_cacheLock);

				// We know that something within the directory changed, we
				// just don't know exactly what.
				std::vector<std::string> filesCurrentlyInDir;
				// note -- don't use fdopendir(_fd) here, because that will tend take control of our _fd, and it
				// becomes undefined if we do something else with the _fd (such as watching it for notifications)
				// It seems to work, but might not be the most reliable option
				DIR *dir = opendir(_dirName.c_str());	
				if (dir != NULL) {
					rewinddir(dir);
					struct dirent *entry;
					while ((entry = readdir(dir)) != NULL)
						if (entry->d_type == DT_REG)
							filesCurrentlyInDir.push_back(entry->d_name);
				}
				closedir(dir);

				// This method is actually pretty awkward, because we can't do the next steps in
				// an atomic fashion. We know that something in the directory has changed, but we don't
				// know what. The only way to check is to iterate through all items and compare it to
				// our previous results.
				// But the directory could continue to be changed while we're doing this. Or, alternatively,
				// if there are 2 quick changes, we can end up skipping one
				// But when we do it like this, it matches behaviour on the other platforms much better...
				// which ultimately might reduce platform specific issues

				std::sort(filesCurrentlyInDir.begin(), filesCurrentlyInDir.end());

				auto cacheIterator = _statusCache.begin();
				auto newDirIterator = filesCurrentlyInDir.begin();
				std::vector<std::string> changedFiles;
				for (;;) {
					auto skipI = cacheIterator;
					while (skipI != _statusCache.end() && (newDirIterator == filesCurrentlyInDir.end() || skipI->_name < *newDirIterator)) {
						changedFiles.push_back(skipI->_name);
						++skipI;
					}
					if (cacheIterator != skipI)
						cacheIterator = _statusCache.erase(cacheIterator, skipI);

					while (newDirIterator != filesCurrentlyInDir.end() && (cacheIterator == _statusCache.end() || *newDirIterator < cacheIterator->_name)) {
						struct stat fdata;
						std::memset(&fdata, 0, sizeof(fdata));
						auto result = fstatat(_fd, newDirIterator->c_str(), &fdata, 0);
						if (result == 0) {
							cacheIterator = 1+_statusCache.insert(cacheIterator, { *newDirIterator, (uint64_t)fdata.st_mtimespec.tv_sec });							
							changedFiles.push_back(*newDirIterator);
						}
						++newDirIterator;
					}

					if (cacheIterator == _statusCache.end() && newDirIterator == filesCurrentlyInDir.end())
						break;

					assert(*newDirIterator == cacheIterator->_name);

					struct stat fdata;
					std::memset(&fdata, 0, sizeof(fdata));
					auto result = fstatat(_fd, newDirIterator->c_str(), &fdata, 0);
					if (result == 0 && cacheIterator->_lastModTime != (uint64_t)fdata.st_mtimespec.tv_sec) {
						changedFiles.push_back(*newDirIterator);
					}
					++cacheIterator;
					++newDirIterator;
				}

				return changedFiles;
			}

			std::any GeneratePayload(const KEventTriggerPayload&) override
			{
				return FindChanges();
			}

			DirectoryChanges(const char* dirName) : _dirName(dirName)
			{
				_fd = open(dirName, O_EVTONLY);
				
				// Setup the KEvent to monitor this directory
				_ident = _fd;
				_filter = EVFILT_VNODE;
				_fflags = NOTE_WRITE | NOTE_DELETE | NOTE_RENAME;

				// prime initial state
				FindChanges();
			}

			~DirectoryChanges()
			{
				close(_fd);
			}
		private:
			int _fd = -1;
			std::string _dirName;

			Threading::Mutex _cacheLock;
			struct CachedFileStatus { std::string _name; uint64_t _lastModTime; };
			std::vector<CachedFileStatus> _statusCache;
		};

		class MonitoredDirectory : public IConduitConsumer
		{
		public:
			virtual void OnEvent(std::any&& payload)
			{
				auto& changes = std::any_cast<const std::vector<std::string>&>(payload);
				for (auto c:changes)
					OnChange(MakeStringSection(c));
			}

			virtual void OnException(const std::exception_ptr& exception)
			{}

			void OnChange(StringSection<> filename)
			{
				auto hash = HashFilename(filename);
				ScopedLock(_callbacksLock);
				auto range = std::equal_range(
					_callbacks.begin(), _callbacks.end(),
					hash, CompareFirst<uint64, std::weak_ptr<OnChangeCallback>>());

				bool foundExpired = false;
				for (auto i2=range.first; i2!=range.second; ++i2) {
						// todo -- what happens if OnChange() results in a change to _callbacks?
					auto l = i2->second.lock();
					if (l) l->OnChange();
					else foundExpired = true;
				}

				if (foundExpired) {
						// Remove any pointers that have expired
						// (note that we only check matching pointers. Non-matching pointers
						// that have expired are untouched)
					_callbacks.erase(
						std::remove_if(range.first, range.second,
							[](std::pair<uint64, std::weak_ptr<OnChangeCallback>>& i)
							{ return i.second.expired(); }),
						range.second);
				}
			}
			
			void AttachCallback(
				uint64_t filenameHash,
				std::shared_ptr<OnChangeCallback> callback)
			{
				ScopedLock(_callbacksLock);
				_callbacks.insert(
					LowerBound(_callbacks, filenameHash),
					std::make_pair(filenameHash, std::move(callback)));
			}

			MonitoredDirectory()
			{}

			~MonitoredDirectory()
			{}

		private:
			std::vector<std::pair<uint64_t, std::weak_ptr<OnChangeCallback>>>  _callbacks;
			Threading::Mutex	_callbacksLock;
		};
	}

	class RawFSMonitor::Pimpl
	{
	public:
		std::shared_ptr<PollingThread> _pollingThread;
		Threading::Mutex _monitoredDirectoriesLock;
		std::vector<std::pair<uint64_t, std::shared_ptr<Internal::MonitoredDirectory>>> _monitoredDirectories;
	};
	
	void RawFSMonitor::Attach(StringSection<utf16>, std::shared_ptr<OnChangeCallback>)
	{
		assert(0);
	}

	void RawFSMonitor::Attach(StringSection<utf8> filename, std::shared_ptr<OnChangeCallback> callback)
	{
		auto split = MakeFileNameSplitter(filename);
		utf8 directoryName[MaxPath];
		MakeSplitPath(split.DriveAndPath()).Simplify().Rebuild(directoryName);
		auto hash = HashFilenameAndPath(MakeStringSection(directoryName));

		{
			ScopedLock(_pimpl->_monitoredDirectoriesLock);
			auto i = LowerBound(_pimpl->_monitoredDirectories, hash);
			if (i != _pimpl->_monitoredDirectories.cend() && i->first == hash) {
				i->second->AttachCallback(HashFilename(split.FileAndExtension()), std::move(callback));
				return;
			}
			
			auto newMonitoredDirectory = std::make_shared<Internal::MonitoredDirectory>();
			newMonitoredDirectory->AttachCallback(HashFilename(split.FileAndExtension()), std::move(callback));

			auto newConduitProducer = std::make_shared<Internal::DirectoryChanges>(directoryName);

			auto connectionFuture = _pimpl->_pollingThread->Connect(
				newConduitProducer,
				newMonitoredDirectory);

			_pimpl->_monitoredDirectories.insert(i, std::make_pair(hash, newMonitoredDirectory));
		}
	}

	void RawFSMonitor::FakeFileChange(StringSection<utf16>)
	{
		assert(0);
	}

	void RawFSMonitor::FakeFileChange(StringSection<utf8> filename)
	{
		auto split = MakeFileNameSplitter(filename);
		utf8 directoryName[MaxPath];
		MakeSplitPath(split.DriveAndPath()).Simplify().Rebuild(directoryName);
		auto hash = HashFilenameAndPath(MakeStringSection(directoryName));

		{
			ScopedLock(_pimpl->_monitoredDirectoriesLock);
			auto i = LowerBound(_pimpl->_monitoredDirectories, hash);
			if (i != _pimpl->_monitoredDirectories.cend() && i->first == hash) {
				i->second->OnChange(split.FileAndExtension());
				return;
			}
		}
	}

	RawFSMonitor::RawFSMonitor(const std::shared_ptr<PollingThread>& pollingThread)
	{
		_pimpl = std::make_unique<Pimpl>();
		_pimpl->_pollingThread = pollingThread;
	}
	RawFSMonitor::~RawFSMonitor() {}

#else
	class RawFSMonitor::Pimpl {};
	
	void RawFSMonitor::Attach(StringSection<utf16>, std::shared_ptr<OnChangeCallback>)
	{
	}

	void RawFSMonitor::Attach(StringSection<utf8>, std::shared_ptr<OnChangeCallback>)
	{
	}

	void RawFSMonitor::FakeFileChange(StringSection<utf16>)
	{
	}

	void RawFSMonitor::FakeFileChange(StringSection<utf8>)
	{
	}

	RawFSMonitor::RawFSMonitor(const std::shared_ptr<PollingThread>&) {}
	RawFSMonitor::~RawFSMonitor() {}
#endif
}

namespace OSServices
{
	bool GetCurrentDirectory(uint32_t dim, char dst[])
	{
		assert(0);
		if (dim > 0) dst[0] = '\0';
		return false;
	}

	void GetProcessPath(utf8 dst[], size_t bufferCount)
	{
		if (!bufferCount) return;

		uint32_t bufsize = bufferCount;
		if (_NSGetExecutablePath(dst, &bufsize) != 0)
			dst[0] = '\0';
	}

	void ChDir(const utf8 path[]) {}

	const char* GetCommandLine() { return ""; }

	ModuleId GetCurrentModuleId() { return 0; }

	void DeleteFile(const utf8 path[])
	{
		std::remove(path);
	}
}
