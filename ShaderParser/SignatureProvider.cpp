
#include "SignatureProvider.h"
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
		auto GetFunction(StringSection<char> fnName) const -> const ShaderSourceParser::FunctionSignature*;
		auto GetParameterStruct(StringSection<char> structName) const -> const ShaderSourceParser::ParameterStructSignature*;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }
		ShaderFragment(StringSection<::Assets::ResChar> fn);
		~ShaderFragment();
	private:
		ShaderSourceParser::ShaderFragmentSignature _sig;
		::Assets::DepValPtr _depVal;
	};

	auto ShaderFragment::GetFunction(StringSection<char> fnName) const -> const ShaderSourceParser::FunctionSignature*
	{
		auto i = std::find_if(
			_sig._functions.cbegin(), _sig._functions.cend(),
            [fnName](const ShaderSourceParser::FunctionSignature& signature) { return XlEqString(MakeStringSection(signature._name), fnName); });
        if (i!=_sig._functions.cend())
			return AsPointer(i);
		return nullptr;
	}

	auto ShaderFragment::GetParameterStruct(StringSection<char> structName) const -> const ShaderSourceParser::ParameterStructSignature*
	{
		auto i = std::find_if(
			_sig._parameterStructs.cbegin(), _sig._parameterStructs.cend(),
            [structName](const ShaderSourceParser::ParameterStructSignature& signature) { return XlEqString(MakeStringSection(signature._name), structName); });
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

	static const ShaderSourceParser::FunctionSignature& LoadFunctionSignature(const std::tuple<StringSection<>, StringSection<>>& splitName, const ::Assets::DirectorySearchRules& searchRules)
    {
        TRY {
			char resolvedFile[MaxPath];
			searchRules.ResolveFile(resolvedFile, std::get<0>(splitName));
			auto& frag = ::Assets::GetAssetDep<ShaderFragment>(resolvedFile);
			auto* fn = frag.GetFunction(std::get<1>(splitName));
			if (fn != nullptr) return *fn;
        } CATCH (...) {
        } CATCH_END
		static ShaderSourceParser::FunctionSignature blank;
        return blank;
    }

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

    auto BasicSignatureProvider::FindSignature(StringSection<> name) -> Result
    {
        auto splitName = SplitArchiveName(name);

        auto hash = Hash64(name.begin(), name.end());
        auto existing = _cache.find(hash);
        if (existing == _cache.end()) {
            auto res = LoadFunctionSignature(splitName, _searchRules);
            auto sig = AsNodeGraphSignature(res);
            existing = _cache.insert({hash, sig}).first;
        }
        
        return { std::get<1>(splitName).AsString(), &existing->second };
    }

    BasicSignatureProvider::BasicSignatureProvider(const ::Assets::DirectorySearchRules& searchRules)
    : _searchRules(searchRules) {}
        
    BasicSignatureProvider::~BasicSignatureProvider() {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    NodeGraphSignature AsNodeGraphSignature(const ShaderSourceParser::FunctionSignature& sig)
    {
        NodeGraphSignature result;
        for (auto& p:sig._parameters) {
            ParameterDirection dir = ParameterDirection::In;
            switch (p._direction) {
            case ShaderSourceParser::FunctionSignature::Parameter::In: dir = ParameterDirection::In; break;
            case ShaderSourceParser::FunctionSignature::Parameter::Out: dir = ParameterDirection::Out; break;
            default: assert(0); // in/out not supported
            }

            result.AddParameter(NodeGraphSignature::Parameter{p._type, p._name, dir, p._semantic});
        }
        if (!sig._returnType.empty())
            result.AddParameter(NodeGraphSignature::Parameter{sig._returnType, "result", ParameterDirection::Out, sig._returnSemantic});
        return result;
    }

}

