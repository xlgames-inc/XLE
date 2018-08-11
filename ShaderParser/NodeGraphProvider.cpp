
#include "NodeGraphProvider.h"
#include "InterfaceSignature.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/DepVal.h"
#include "../Assets/Assets.h"

namespace ShaderPatcher
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
		auto GetParameterStruct(StringSection<char> structName) const -> const ParameterStructSignature*;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }
		ShaderFragment(StringSection<::Assets::ResChar> fn);
		~ShaderFragment();
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

	auto ShaderFragment::GetParameterStruct(StringSection<char> structName) const -> const ParameterStructSignature*
	{
		auto i = std::find_if(
			_sig._parameterStructs.cbegin(), _sig._parameterStructs.cend(),
            [structName](const ParameterStructSignature& signature) { return XlEqString(MakeStringSection(signature._name), structName); });
        if (i!=_sig._parameterStructs.cend())
			return AsPointer(i);
		return nullptr;
	}

	ShaderFragment::ShaderFragment(StringSection<::Assets::ResChar> fn)
	{
		auto shaderFile = LoadSourceFile(fn);
		_sig = ShaderSourceParser::BuildShaderFragmentSignature(MakeStringSection(shaderFile));
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
        auto splitName = SplitArchiveName(name);

        auto hash = Hash64(name.begin(), name.end());
        auto existing = _cache.find(hash);
        if (existing == _cache.end()) {
			char resolvedFile[MaxPath];
			_searchRules.ResolveFile(resolvedFile, std::get<0>(splitName));
			if (resolvedFile[0]) {
				auto& frag = ::Assets::GetAssetDep<ShaderFragment>(resolvedFile);
				auto* fn = frag.GetFunction(std::get<1>(splitName));
				if (fn != nullptr) {
					existing = _cache.insert({hash, Entry{std::get<1>(splitName).AsString(), *fn, std::string(resolvedFile)}}).first;
				}
			}
        }

		if (existing != _cache.end())        
			return Signature{ existing->second._name, existing->second._sig, existing->second._sourceFile };

		return {};
    }

	auto BasicNodeGraphProvider::FindGraph(StringSection<> name) -> std::optional<NodeGraph>
	{
		return {};
	}

    BasicNodeGraphProvider::BasicNodeGraphProvider(const ::Assets::DirectorySearchRules& searchRules)
    : _searchRules(searchRules) {}
        
    BasicNodeGraphProvider::~BasicNodeGraphProvider() {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	INodeGraphProvider::~INodeGraphProvider() {}

}

