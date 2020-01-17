// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderPatchCollection.h"
#include "../../Assets/DepVal.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Conversion.h"

namespace RenderCore { namespace Assets
{
	void ShaderPatchCollection::MergeInto(ShaderPatchCollection& dest) const
	{
		for (const auto&p:_patches) {
			auto i = std::find_if(
				dest._patches.begin(), dest._patches.end(),
				[&p](const std::pair<std::string, ShaderSourceParser::InstantiationRequest>& q) { return q.first == p.first; });
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

	ShaderPatchCollection::ShaderPatchCollection(IteratorRange<const std::pair<std::string, ShaderSourceParser::InstantiationRequest>*> patches)
	: _patches(patches.begin(), patches.end())
	{
		SortAndCalculateHash();
	}

	ShaderPatchCollection::ShaderPatchCollection(std::vector<std::pair<std::string, ShaderSourceParser::InstantiationRequest>>&& patches)
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

		using Pair = std::pair<std::string, ShaderSourceParser::InstantiationRequest>;
		std::stable_sort(
			_patches.begin(), _patches.end(), 
			[](const Pair& lhs, const Pair& rhs) { return lhs.second._archiveName < rhs.second._archiveName; });

		_hash = DefaultSeed64;
		for (const auto&p:_patches) {
			// note that p.first doesn't actually contribute to the hash -- it's not used during the merge operation
			assert(!p.second._customProvider);
			_hash = HashCombine(Hash64(p.second._archiveName), _hash);
			_hash = HashCombine(p.second.CalculateInstanceHash(), _hash);
		}
	}

	bool operator<(const ShaderPatchCollection& lhs, const ShaderPatchCollection& rhs) { return lhs.GetHash() < rhs.GetHash(); }
	bool operator<(const ShaderPatchCollection& lhs, uint64_t rhs) { return lhs.GetHash() < rhs; }
	bool operator<(uint64_t lhs, const ShaderPatchCollection& rhs) { return lhs < rhs.GetHash(); }

	static void SerializeInstantiationRequest(
		OutputStreamFormatter& formatter, 
		const ShaderSourceParser::InstantiationRequest& instRequest)
	{
		formatter.WriteAttribute(
			AsPointer(instRequest._archiveName.begin()), AsPointer(instRequest._archiveName.end()), 
			(const char*)nullptr, (const char*)nullptr);

		for (const auto&p:instRequest._parameterBindings) {
			auto ele = formatter.BeginElement(p.first);
			SerializeInstantiationRequest(formatter, p.second);
			formatter.EndElement(ele);
		}
	}

	void ShaderPatchCollection::SerializeMethod(OutputStreamFormatter& formatter) const
	{
		for (const auto& p:_patches) {
			auto pele = formatter.BeginElement(p.first);
			SerializeInstantiationRequest(formatter, p.second);
			formatter.EndElement(pele);
		}
	}

	static ShaderSourceParser::InstantiationRequest DeserializeInstantiationRequest(InputStreamFormatter<utf8>& formatter)
	{
		ShaderSourceParser::InstantiationRequest result;

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

	ShaderPatchCollection::ShaderPatchCollection(InputStreamFormatter<utf8>& formatter)
	{
		for (;;) {
			auto next = formatter.PeekNext();
			if (next == InputStreamFormatter<utf8>::Blob::BeginElement) {

				StringSection<utf8> name;
				if (!formatter.TryBeginElement(name))
					Throw(FormatException("Could not parse element", formatter.GetLocation()));
				_patches.emplace_back(std::make_pair(name.Cast<char>().AsString(), DeserializeInstantiationRequest(formatter)));

				if (!formatter.TryEndElement())
					Throw(FormatException("Expecting end element", formatter.GetLocation()));

			} else if (	next == InputStreamFormatter<utf8>::Blob::EndElement
					||	next == InputStreamFormatter<utf8>::Blob::None) {
				break;
			} else {
				Throw(FormatException("Unexpected blob while parsing TechniqueFragment list", formatter.GetLocation()));
			}
		}

		SortAndCalculateHash();
	}

	std::vector<ShaderPatchCollection> DeserializeShaderPatchCollectionSet(InputStreamFormatter<utf8>& formatter)
	{
		std::vector<ShaderPatchCollection> result;
		for (;;) {
			auto next = formatter.PeekNext();
			switch (next) {
			case InputStreamFormatter<utf8>::Blob::BeginElement:
				{
					StringSection<utf8> name;
					if (!formatter.TryBeginElement(name))
						Throw(FormatException("Could not parse element", formatter.GetLocation()));
					result.emplace_back(ShaderPatchCollection(formatter));

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
		std::sort(result.begin(), result.end());
		return result;
	}

	void SerializeShaderPatchCollectionSet(OutputStreamFormatter& formatter, IteratorRange<const ShaderPatchCollection*> patchCollections)
	{
		for (const auto& p:patchCollections) {
			auto ele = formatter.BeginElement("ShaderPatchCollection");
			Serialize(formatter, p);
			formatter.EndElement(ele);
		}
	}	

	std::ostream& operator<<(std::ostream& str, const ShaderPatchCollection& patchCollection)
	{
		str << "PatchCollection[" << patchCollection.GetHash() << "]";
		return str;
	}

}}

