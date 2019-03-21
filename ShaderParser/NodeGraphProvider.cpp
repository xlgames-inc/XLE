// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NodeGraphProvider.h"
#include "GraphSyntax.h"
#include "ShaderSignatureParser.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/DepVal.h"
#include "../Assets/Assets.h"
#include "../Utility/Streams/PathUtils.h"

namespace GraphLanguage
{
    static std::string LoadSourceFile(StringSection<char> sourceFileName)
    {
        TRY {
			auto file = ::Assets::MainFileSystem::OpenBasicFile(sourceFileName.AsString().c_str(), "rb");

            file.Seek(0, FileSeekAnchor::End);
            size_t size = file.TellP();
            file.Seek(0, FileSeekAnchor::Start);

            std::string result;
            result.resize(size, '\0');
            file.Read(&result.at(0), 1, size);
            return result;

        } CATCH(const std::exception& ) {
            return std::string();
        } CATCH_END
    }

	class ShaderFragment
	{
	public:
		auto GetFunction(StringSection<char> fnName) const -> const NodeGraphSignature*;
		auto GetUniformBuffer(StringSection<char> structName) const -> const UniformBufferSignature*;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }
		ShaderFragment(StringSection<::Assets::ResChar> fn);
		~ShaderFragment();

		bool _isGraphSyntaxFile = false;
	private:
		ShaderFragmentSignature _sig;
		::Assets::DepValPtr _depVal;
	};

	auto ShaderFragment::GetFunction(StringSection<char> fnName) const -> const NodeGraphSignature*
	{
		auto i = std::find_if(
			_sig._functions.cbegin(), _sig._functions.cend(),
            [fnName](const std::pair<std::string, NodeGraphSignature>& signature) { return XlEqString(MakeStringSection(signature.first), fnName); });
        if (i!=_sig._functions.cend())
			return &i->second;
		return nullptr;
	}

	auto ShaderFragment::GetUniformBuffer(StringSection<char> structName) const -> const UniformBufferSignature*
	{
		auto i = std::find_if(
			_sig._uniformBuffers.cbegin(), _sig._uniformBuffers.cend(),
            [structName](const std::pair<std::string, UniformBufferSignature>& signature) { return XlEqString(MakeStringSection(signature.first), structName); });
        if (i!=_sig._uniformBuffers.cend())
			return &i->second;
		return nullptr;
	}

	ShaderFragment::ShaderFragment(StringSection<::Assets::ResChar> fn)
	{
		auto shaderFile = LoadSourceFile(fn);
		if (XlEqStringI(MakeFileNameSplitter(fn).Extension(), "graph")) {
			auto graphSyntax = ParseGraphSyntax(shaderFile);
			for (auto& subGraph:graphSyntax._subGraphs)
				_sig._functions.emplace_back(std::make_pair(subGraph.first, std::move(subGraph.second._signature)));
			_isGraphSyntaxFile = true;
		} else {
			_sig = ShaderSourceParser::ParseHLSL(MakeStringSection(shaderFile));
		}
		_depVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_depVal, fn);
	}

	ShaderFragment::~ShaderFragment() {}

    static std::tuple<StringSection<>, StringSection<>> SplitArchiveName(StringSection<> archiveName)
    {
        auto pos = std::find(archiveName.begin(), archiveName.end(), ':');
        if (pos != archiveName.end()) {
            return std::make_tuple(MakeStringSection(archiveName.begin(), pos), MakeStringSection(pos+1, archiveName.end()));
        } else {
            return std::make_tuple(StringSection<>(), archiveName);
        }
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    auto BasicNodeGraphProvider::FindSignature(StringSection<> name) -> std::optional<Signature>
    {
        auto hash = Hash64(name.begin(), name.end());
        auto existing = _cache.find(hash);
        if (existing == _cache.end()) {
			auto splitName = SplitArchiveName(name);
			if (!std::get<0>(splitName).IsEmpty()) {
				char resolvedFile[MaxPath];
				_searchRules.ResolveFile(resolvedFile, std::get<0>(splitName));
				if (resolvedFile[0]) {
					TRY {
						auto& frag = ::Assets::GetAssetDep<ShaderFragment>(resolvedFile);
						auto* fn = frag.GetFunction(std::get<1>(splitName));
						if (fn != nullptr) {
							existing = _cache.insert({hash, Entry{std::get<1>(splitName).AsString(), *fn, std::string(resolvedFile), frag._isGraphSyntaxFile}}).first;
						}
					} CATCH (const ::Assets::Exceptions::RetrievalError&) {
					} CATCH_END
				}
			}
        }

		if (existing != _cache.end())        
			return Signature{ existing->second._name, existing->second._sig, existing->second._sourceFile, existing->second._isGraphSyntaxFile };

		return {};
    }

	auto BasicNodeGraphProvider::FindGraph(StringSection<> name) -> std::optional<NodeGraph>
	{
		auto splitName = SplitArchiveName(name);
		return LoadGraphSyntaxFile(std::get<0>(splitName), std::get<1>(splitName));
	}

    BasicNodeGraphProvider::BasicNodeGraphProvider(const ::Assets::DirectorySearchRules& searchRules)
    : _searchRules(searchRules) {}
        
    BasicNodeGraphProvider::~BasicNodeGraphProvider() {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	INodeGraphProvider::~INodeGraphProvider() {}

}

