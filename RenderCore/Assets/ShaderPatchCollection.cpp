// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderPatchCollection.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../ShaderParser/NodeGraphProvider.h"
#include "../../Assets/DepVal.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"

namespace RenderCore { namespace Assets
{
	void ShaderPatchCollection::MergeInto(ShaderPatchCollection& dest) const
	{
		for (const auto&p:_patches) {
			auto i = std::find_if(
				dest._patches.begin(), dest._patches.end(),
				[&p](const std::pair<std::string, ShaderSourceParser::InstantiationRequest_ArchiveName>& q) { return q.first == p.first; });
			if (i == dest._patches.end()) {
				dest._patches.push_back(p);
			} else {
				i->second = p.second;
			}
		}

		dest.SortAndCalculateHash();
	}

	ShaderPatchCollection::ShaderPatchCollection()
	{
		_hash = 0;
	}

	ShaderPatchCollection::ShaderPatchCollection(IteratorRange<const std::pair<std::string, ShaderSourceParser::InstantiationRequest_ArchiveName>*> patches)
	: _patches(patches.begin(), patches.end())
	{
		SortAndCalculateHash();
	}

	ShaderPatchCollection::ShaderPatchCollection(std::vector<std::pair<std::string, ShaderSourceParser::InstantiationRequest_ArchiveName>>&& patches)
	: _patches(std::move(patches))
	{
		SortAndCalculateHash();
	}

	ShaderPatchCollection::~ShaderPatchCollection() {}

	void ShaderPatchCollection::SortAndCalculateHash()
	{
		if (_patches.empty()) {
			_hash = 0;
			return;
		}

		using Pair = std::pair<std::string, ShaderSourceParser::InstantiationRequest_ArchiveName>;
		std::stable_sort(
			_patches.begin(), _patches.end(), 
			[](const Pair& lhs, const Pair& rhs) { return lhs.second._archiveName < rhs.second._archiveName; });

		_hash = DefaultSeed64;
		for (const auto&p:_patches) {
			// note that p.first doesn't actually contribute to the hash -- it's not used during the merge operation
			assert(!p.second._customProvider);
			_hash = HashCombine(Hash64(p.second._archiveName), _hash);
			_hash = HashCombine(p.second.CalculateHash(), _hash);
		}
	}

	static ShaderSourceParser::InstantiationRequest_ArchiveName DeserializeInstantiationRequest(InputStreamFormatter<utf8>& formatter)
	{
		ShaderSourceParser::InstantiationRequest_ArchiveName result;

		for (;;) {
			auto next = formatter.PeekNext();
			switch (next) {
			case InputStreamFormatter<utf8>::Blob::AttributeName:
				{
					StringSection<utf8> name, value;
					if (!formatter.TryAttribute(name, value))
						Throw(FormatException("Could not parse attribute", formatter.GetLocation()));
					if (!value.IsEmpty())
						Throw(FormatException("Expecting only a single attribute in each fragment, which is the entry point name, with no value", formatter.GetLocation()));
					if (!result._archiveName.empty())
						Throw(FormatException("Multiple entry points found for a single technique fragment declaration", formatter.GetLocation()));
					result._archiveName = name.Cast<char>().AsString();
					continue;
				}

			case InputStreamFormatter<utf8>::Blob::BeginElement:
				{
					StringSection<utf8> name;
					if (!formatter.TryBeginElement(name))
						Throw(FormatException("Could not parse element", formatter.GetLocation()));
					result._parameterBindings.emplace(
						std::make_pair(
							name.Cast<char>().AsString(),
							DeserializeInstantiationRequest(formatter)));

					if (!formatter.TryEndElement())
						Throw(FormatException("Expecting end element", formatter.GetLocation()));
					continue;
				}

			case InputStreamFormatter<utf8>::Blob::EndElement:
			case InputStreamFormatter<utf8>::Blob::None:
				if (result._archiveName.empty())
					Throw(FormatException("No entry point was specified for fragment ending at marked location", formatter.GetLocation()));
				return result;

			default:
				Throw(FormatException("Unexpected blob while parsing TechniqueFragment", formatter.GetLocation()));
			}
		}
	}

	ShaderPatchCollection DeserializeShaderPatchCollection(InputStreamFormatter<utf8>& formatter)
	{
		std::vector<std::pair<std::string, ShaderSourceParser::InstantiationRequest_ArchiveName>> result;
		for (;;) {
			auto next = formatter.PeekNext();
			switch (next) {
			case InputStreamFormatter<utf8>::Blob::BeginElement:
				{
					StringSection<utf8> name;
					if (!formatter.TryBeginElement(name))
						Throw(FormatException("Could not parse element", formatter.GetLocation()));
					result.emplace_back(std::make_pair(name.Cast<char>().AsString(), DeserializeInstantiationRequest(formatter)));

					if (!formatter.TryEndElement())
						Throw(FormatException("Expecting end element", formatter.GetLocation()));
					continue;
				}

			case InputStreamFormatter<utf8>::Blob::EndElement:
			case InputStreamFormatter<utf8>::Blob::None:
				return result;

			default:
				Throw(FormatException("Unexpected blob while parsing TechniqueFragment list", formatter.GetLocation()));
			}
		}
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static std::string Merge(const std::vector<std::string>& v)
	{
		size_t size=0;
		for (const auto&q:v) size += q.size();
		std::string result;
		result.reserve(size);
		for (const auto&q:v) result.insert(result.end(), q.begin(), q.end());
		return result;
	}

	CompiledShaderPatchCollection::CompiledShaderPatchCollection(const ShaderPatchCollection& src)
	{
		// With the given shader patch collection, build the source code and the 
		// patching functions associated

		std::vector<ShaderSourceParser::InstantiationRequest_ArchiveName> finalInstRequests;
		finalInstRequests.reserve(src.GetPatches().size());
		for (const auto&i:src.GetPatches()) finalInstRequests.push_back(i.second);

		auto inst = InstantiateShader(MakeIteratorRange(finalInstRequests));
		_srcCode = Merge(inst._sourceFragments);

		_patches.reserve(inst._entryPoints.size());
		for (const auto&patch:inst._entryPoints) {
			if (patch._implementsName.empty()) continue;

			Patch p;
			p._implementsHash = Hash64(patch._signature.GetImplements());

			if (patch._implementsName != patch._name) {
				p._scaffoldInFunction = ShaderSourceParser::GenerateScaffoldFunction(
					patch._implementsSignature, patch._signature,
					patch._implementsName, patch._name, 
					ShaderSourceParser::ScaffoldFunctionFlags::ScaffoldeeUsesReturnSlot);
			}

			_patches.emplace_back(std::move(p));
		}

		_depVal = std::make_shared<::Assets::DependencyValidation>();
		for (const auto&d:inst._depVals)
			if (d)
				::Assets::RegisterAssetDependency(_depVal, d);
	}

	CompiledShaderPatchCollection::~CompiledShaderPatchCollection() {}

}}

