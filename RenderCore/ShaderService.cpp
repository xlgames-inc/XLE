// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderService.h"
#include "Types.h"	// For PS_DefShaderModel
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/MemoryUtils.h"
#include <assert.h>

namespace RenderCore
{

        ////////////////////////////////////////////////////////////

    static ShaderStage AsShaderStage(StringSection<::Assets::ResChar> shaderModel)
    {
		assert(shaderModel.size() >= 1);
        switch (shaderModel[0]) {
        case 'v':   return ShaderStage::Vertex;
        case 'p':   return ShaderStage::Pixel;
        case 'g':   return ShaderStage::Geometry;
        case 'd':   return ShaderStage::Domain;
        case 'h':   return ShaderStage::Hull;
        case 'c':   return ShaderStage::Compute;
        default:    return ShaderStage::Null;
        }
    }

    ShaderStage ShaderService::ResId::AsShaderStage() const { return RenderCore::AsShaderStage(_shaderModel); }

	CompiledShaderByteCode::CompiledShaderByteCode(const ::Assets::Blob& shader, const ::Assets::DepValPtr& depVal, StringSection<Assets::ResChar>)
	: _shader(shader)
	, _depVal(depVal)
	{
		if (_shader && !_shader->empty()) {
			if (_shader->size() < sizeof(ShaderService::ShaderHeader))
				Throw(::Exceptions::BasicLabel("Shader byte code is too small for shader header"));
			const auto& hdr = *(const ShaderService::ShaderHeader*)_shader->data();
			if (hdr._version != ShaderService::ShaderHeader::Version)
				Throw(::Exceptions::BasicLabel("Unexpected version in shader header. Found (%i), expected (%i)", hdr._version, ShaderService::ShaderHeader::Version));
		}
	}

	CompiledShaderByteCode::CompiledShaderByteCode()
	{
		_depVal = std::make_shared<::Assets::DependencyValidation>();
	}

    CompiledShaderByteCode::~CompiledShaderByteCode() {}

	IteratorRange<const void*> CompiledShaderByteCode::GetByteCode() const
	{
		if (!_shader || _shader->empty()) return {};
		return MakeIteratorRange(
			PtrAdd(AsPointer(_shader->begin()), sizeof(ShaderService::ShaderHeader)), 
			AsPointer(_shader->end()));
	}

    bool CompiledShaderByteCode::DynamicLinkingEnabled() const
    {
        if (!_shader || _shader->empty()) return false;

		assert(_shader->size() >= sizeof(ShaderService::ShaderHeader));
        auto& hdr = *(const ShaderService::ShaderHeader*)AsPointer(_shader->begin());
        assert(hdr._version == ShaderService::ShaderHeader::Version);
        return hdr._dynamicLinkageEnabled != 0;
    }

	ShaderStage		CompiledShaderByteCode::GetStage() const
	{
		if (!_shader || _shader->empty()) return ShaderStage::Null;

		assert(_shader->size() >= sizeof(ShaderService::ShaderHeader));
		auto& hdr = *(const ShaderService::ShaderHeader*)AsPointer(_shader->begin());
		assert(hdr._version == ShaderService::ShaderHeader::Version);
		return AsShaderStage(hdr._shaderModel);
	}

    StringSection<>             CompiledShaderByteCode::GetIdentifier() const
    {
        if (!_shader || _shader->empty()) return {};

		assert(_shader->size() >= sizeof(ShaderService::ShaderHeader));
		auto& hdr = *(const ShaderService::ShaderHeader*)AsPointer(_shader->begin());
		assert(hdr._version == ShaderService::ShaderHeader::Version);
		return MakeStringSection(hdr._identifier);
    }

    const uint64 CompiledShaderByteCode::CompileProcessType = ConstHash64<'Shad', 'erCo', 'mpil', 'e'>::Value;

        ////////////////////////////////////////////////////////////

    ShaderService::ResId::ResId(StringSection<ResChar> filename, StringSection<ResChar> entryPoint, StringSection<ResChar> shaderModel)
    {
        XlCopyString(_filename, filename);
        XlCopyString(_entryPoint, entryPoint);

        _dynamicLinkageEnabled = shaderModel[0] == '!';
        if (_dynamicLinkageEnabled) {
			XlCopyString(_shaderModel, MakeStringSection(shaderModel.begin()+1, shaderModel.end()));
		} else {
			XlCopyString(_shaderModel, shaderModel);
		}
    }

    ShaderService::ResId::ResId()
    {
        _filename[0] = '\0'; _entryPoint[0] = '\0'; _shaderModel[0] = '\0';
        _dynamicLinkageEnabled = false;
    }


    auto ShaderService::MakeResId(
        StringSection<::Assets::ResChar> initializer,
        const ILowLevelCompiler* compiler) -> ResId
    {
        ResId shaderId;

        const ::Assets::ResChar* startShaderModel = nullptr;
        auto splitter = MakeFileNameSplitter(initializer);
        XlCopyString(shaderId._filename, splitter.AllExceptParameters());

        if (splitter.Parameters().IsEmpty()) {
            XlCopyString(shaderId._entryPoint, "main");
        } else {
            startShaderModel = XlFindChar(splitter.Parameters().begin(), ':');

            if (!startShaderModel) {
                XlCopyString(shaderId._entryPoint, splitter.Parameters().begin());
            } else {
                XlCopyNString(shaderId._entryPoint, splitter.Parameters().begin(), startShaderModel - splitter.Parameters().begin());
                if (*(startShaderModel+1) == '!') {
                    shaderId._dynamicLinkageEnabled = true;
                    ++startShaderModel;
                }
                XlCopyString(shaderId._shaderModel, startShaderModel+1);
            }
        }

        if (!startShaderModel)
            XlCopyString(shaderId._shaderModel, PS_DefShaderModel);

            //  we have to do the "AdaptShaderModel" shader model here to convert
            //  the default shader model string (etc, "vs_*) to a resolved shader model
            //  this is because we want the archive name to be correct
        if (compiler)
            compiler->AdaptShaderModel(shaderId._shaderModel, dimof(shaderId._shaderModel), shaderId._shaderModel);

        return shaderId;
    }

    auto ShaderService::CompileFromFile(
        StringSection<::Assets::ResChar> resId, 
        StringSection<::Assets::ResChar> definesTable) const -> std::shared_ptr<::Assets::CompileFuture>
    {
        for (const auto& i:_shaderSources) {
            auto r = i->CompileFromFile(resId, definesTable);
            if (r) return r;
        }
        return nullptr;
    }

    auto ShaderService::CompileFromMemory(
        StringSection<char> shaderInMemory, 
        StringSection<char> entryPoint, StringSection<char> shaderModel, 
        StringSection<::Assets::ResChar> definesTable) const -> std::shared_ptr<::Assets::CompileFuture>
    {
        for (const auto& i:_shaderSources) {
            auto r = i->CompileFromMemory(shaderInMemory, entryPoint, shaderModel, definesTable);
            if (r) return r;
        }
        return nullptr;
    }

    void ShaderService::AddShaderSource(std::shared_ptr<IShaderSource> shaderSource)
    {
        _shaderSources.push_back(shaderSource);
    }

    ShaderService* ShaderService::s_instance = nullptr;
    void ShaderService::SetInstance(ShaderService* instance)
    {
        if (instance) {
            assert(!s_instance);
            s_instance = instance;
        } else {
            s_instance = nullptr;
        }
    }

    ShaderService::ShaderService() {}
    ShaderService::~ShaderService() {}

    ShaderService::IShaderSource::~IShaderSource() {}
    ShaderService::ILowLevelCompiler::~ILowLevelCompiler() {}


	ShaderService::ShaderHeader::ShaderHeader(StringSection<char> identifier, StringSection<char> shaderModel, bool dynamicLinkageEnabled)
	: _version(Version)
	, _dynamicLinkageEnabled(unsigned(dynamicLinkageEnabled))
	{
        XlCopyString(_identifier, identifier);
		XlCopyString(_shaderModel, shaderModel);
	}
}

