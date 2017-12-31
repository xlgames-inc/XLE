// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "ObjectFactory.h"
#include "../../ShaderService.h"
#include "../../Types.h"
#include "../../../Assets/Assets.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/StringFormat.h"

#pragma warning(disable:4702)

namespace RenderCore { namespace Metal_Vulkan
{
    using ::Assets::ResChar;

        ////////////////////////////////////////////////////////////////////////////////////////////////

    Shader::Shader(const CompiledShaderByteCode& compiledShader)
    {
        if (compiledShader.GetStage() != ShaderStage::Null) {
            auto byteCode = compiledShader.GetByteCode();
            _underlying = GetObjectFactory().CreateShaderModule(byteCode.begin(), byteCode.size());
        }
    }

    Shader::Shader() {}
    Shader::~Shader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    VertexShader::VertexShader(StringSection<ResChar> initializer)
    {
            //
            //      We have to append the shader model to the resource name
            //      (if it's not already there)
            //
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "vs_")) {
            StringMeldInPlace(temp) << initializer << ":" VS_DefShaderModel;
            initializer = temp;
        }

		const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
        assert(compiledShader.GetStage() == ShaderStage::Vertex);
        auto byteCode = compiledShader.GetByteCode();
        _underlying = GetObjectFactory().CreateShaderModule(byteCode.begin(), byteCode.size());
    }

    VertexShader::VertexShader(const CompiledShaderByteCode& compiledShader)
    : Shader(compiledShader)
    {
        assert(compiledShader.GetStage() == ShaderStage::Vertex || compiledShader.GetStage() ==  ShaderStage::Null);
    }

    VertexShader::VertexShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    PixelShader::PixelShader(StringSection<ResChar> initializer)
    {
            //
            //      We have to append the shader model to the resource name
            //      (if it's not already there)
            //
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "ps_")) {
            StringMeldInPlace(temp) << initializer << ":" PS_DefShaderModel;
            initializer = temp;
        }

        const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
        assert(compiledShader.GetStage() == ShaderStage::Pixel);
        auto byteCode = compiledShader.GetByteCode();
        _underlying = GetObjectFactory().CreateShaderModule(byteCode.begin(), byteCode.size());
    }

    PixelShader::PixelShader(const CompiledShaderByteCode& compiledShader)
    : Shader(compiledShader)
    {
        assert(compiledShader.GetStage() == ShaderStage::Pixel || compiledShader.GetStage() ==  ShaderStage::Null);
    }

    PixelShader::PixelShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    GeometryShader::GeometryShader( StringSection<ResChar> initializer,
                                    const StreamOutputInitializers& soInitializers)
    {
            //
            //      We have to append the shader model to the resource name
            //      (if it's not already there)
            //
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "gs_")) {
            StringMeldInPlace(temp) << initializer << ":" GS_DefShaderModel;
            initializer = temp;
        }

        if (soInitializers._outputBufferCount != 0)
            Throw(::Exceptions::BasicLabel("Unimplemented"));

        const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
        assert(compiledShader.GetStage() == ShaderStage::Geometry);
        auto byteCode = compiledShader.GetByteCode();
        _underlying = GetObjectFactory().CreateShaderModule(byteCode.begin(), byteCode.size());
    }

    GeometryShader::GeometryShader(const CompiledShaderByteCode& compiledShader, const StreamOutputInitializers& soInitializers)
    {
        if (soInitializers._outputBufferCount != 0)
            Throw(::Exceptions::BasicLabel("Unimplemented"));

        assert(compiledShader.GetStage() == ShaderStage::Geometry);
        auto byteCode = compiledShader.GetByteCode();
        _underlying = GetObjectFactory().CreateShaderModule(byteCode.begin(), byteCode.size());
    }

    GeometryShader::GeometryShader() {}

    void GeometryShader::SetDefaultStreamOutputInitializers(const StreamOutputInitializers& initializers)
    {
    }

    auto GeometryShader::GetDefaultStreamOutputInitializers() -> const StreamOutputInitializers&
    {
        static StreamOutputInitializers s_defaults;
        return s_defaults;
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    ComputeShader::ComputeShader(StringSection<ResChar> initializer, StringSection<ResChar> definesTable)
    {
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "cs_")) {
            StringMeldInPlace(temp) << initializer << ":" CS_DefShaderModel;
            initializer = temp;
        }

        const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable);
        assert(compiledShader.GetStage() == ShaderStage::Compute);
        auto byteCode = compiledShader.GetByteCode();
        _underlying = GetObjectFactory().CreateShaderModule(byteCode.begin(), byteCode.size());

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, compiledShader.GetDependencyValidation());
    }

    ComputeShader::ComputeShader(const CompiledShaderByteCode& compiledShader)
    : Shader(compiledShader)
    {
        assert(compiledShader.GetStage() == ShaderStage::Compute || compiledShader.GetStage() == ShaderStage::Null);
        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, compiledShader.GetDependencyValidation());
    }

    ComputeShader::ComputeShader() {}

    ComputeShader::~ComputeShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    DomainShader::DomainShader(StringSection<ResChar> initializer, StringSection<ResChar> definesTable)
    {
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "ds_")) {
            StringMeldInPlace(temp) << initializer << ":" DS_DefShaderModel;
            initializer = temp;
        }

		const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable);
        assert(compiledShader.GetStage() == ShaderStage::Domain);
        auto byteCode = compiledShader.GetByteCode();
        _underlying = GetObjectFactory().CreateShaderModule(byteCode.begin(), byteCode.size());
    }

    DomainShader::DomainShader(const CompiledShaderByteCode& compiledShader)
    : Shader(compiledShader)
    {
        assert(compiledShader.GetStage() == ShaderStage::Domain || compiledShader.GetStage() == ShaderStage::Null);
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    HullShader::HullShader(StringSection<ResChar> initializer, StringSection<ResChar> definesTable)
    {
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "hs_")) {
            StringMeldInPlace(temp) << initializer << ":" HS_DefShaderModel;
            initializer = temp;
        }

		const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable);
        assert(compiledShader.GetStage() == ShaderStage::Hull);
        auto byteCode = compiledShader.GetByteCode();
        _underlying = ObjectFactory().CreateShaderModule(byteCode.begin(), byteCode.size());
    }

    HullShader::HullShader(const CompiledShaderByteCode& compiledShader)
    : Shader(compiledShader)
    {
        assert(compiledShader.GetStage() == ShaderStage::Hull || compiledShader.GetStage() != ShaderStage::Null);
    }


        ////////////////////////////////////////////////////////////////////////////////////////////////

    ShaderProgram::ShaderProgram(   StringSection<ResChar> vertexShaderInitializer, 
                                    StringSection<ResChar> fragmentShaderInitializer)
    :   _compiledVertexShader(&::Assets::GetAssetComp<CompiledShaderByteCode>(vertexShaderInitializer)) // (odd..?)
    ,   _compiledPixelShader(&::Assets::GetAssetComp<CompiledShaderByteCode>(fragmentShaderInitializer)) // (odd..?)
    ,   _vertexShader(*_compiledVertexShader)
    ,   _pixelShader(*_compiledPixelShader)
    ,   _compiledGeometryShader(nullptr)
    {
        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, _compiledVertexShader->GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, _compiledPixelShader->GetDependencyValidation());
    }
    
    ShaderProgram::ShaderProgram(   StringSection<ResChar> vertexShaderInitializer, 
                                    StringSection<ResChar> fragmentShaderInitializer,
                                    StringSection<ResChar> definesTable)
    :   _compiledVertexShader(&::Assets::GetAssetComp<CompiledShaderByteCode>(vertexShaderInitializer, definesTable)) // (odd..?)
    ,   _compiledPixelShader(&::Assets::GetAssetComp<CompiledShaderByteCode>(fragmentShaderInitializer, definesTable)) // (odd..?)
    ,   _vertexShader(*_compiledVertexShader)
    ,   _pixelShader(*_compiledPixelShader)
    ,   _compiledGeometryShader(nullptr)
    {
        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, _compiledVertexShader->GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, _compiledPixelShader->GetDependencyValidation());
    }

    ShaderProgram::ShaderProgram(   StringSection<ResChar> vertexShaderInitializer, 
                                    StringSection<ResChar> geometryShaderInitializer,
                                    StringSection<ResChar> fragmentShaderInitializer,
                                    StringSection<ResChar> definesTable)
    :   _compiledVertexShader(&::Assets::GetAssetComp<CompiledShaderByteCode>(vertexShaderInitializer, definesTable)) // (odd..?)
    ,   _compiledPixelShader(&::Assets::GetAssetComp<CompiledShaderByteCode>(fragmentShaderInitializer, definesTable)) // (odd..?)
    ,   _vertexShader(*_compiledVertexShader)
    ,   _pixelShader(*_compiledPixelShader)
    ,   _compiledGeometryShader(nullptr)
    {
        if (!geometryShaderInitializer.IsEmpty()) {
            _compiledGeometryShader = &::Assets::GetAssetComp<CompiledShaderByteCode>(geometryShaderInitializer, definesTable);
            _geometryShader = GeometryShader(*_compiledGeometryShader);
        }
        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, _compiledVertexShader->GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, _compiledPixelShader->GetDependencyValidation());
        if (_compiledGeometryShader) {
            Assets::RegisterAssetDependency(_validationCallback, _compiledGeometryShader->GetDependencyValidation());
        }
    }

    ShaderProgram::ShaderProgram(   const CompiledShaderByteCode& compiledVertexShader, 
                                    const CompiledShaderByteCode& compiledFragmentShader)
    :   _compiledVertexShader(&compiledVertexShader)
    ,   _compiledPixelShader(&compiledFragmentShader)
    ,   _compiledGeometryShader(nullptr)
    {
            //  make sure both the vertex shader and the pixel shader
            //  have completed compiling. If one completes early, we don't
            //  want to create the vertex shader object, and then just delete it.
        compiledVertexShader.GetByteCode();
        compiledFragmentShader.GetByteCode();

        auto vertexShader = VertexShader(compiledVertexShader);
        auto pixelShader = PixelShader(compiledFragmentShader);

        _vertexShader = std::move(vertexShader);
        _pixelShader = std::move(pixelShader);
        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, _compiledVertexShader->GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, _compiledPixelShader->GetDependencyValidation());
    }

	ShaderProgram::ShaderProgram()
	{
		_compiledVertexShader = nullptr;
		_compiledPixelShader = nullptr;
		_compiledGeometryShader = nullptr;
	}
    ShaderProgram::~ShaderProgram() {}

    bool ShaderProgram::DynamicLinkingEnabled() const
    {
        return _compiledVertexShader->DynamicLinkingEnabled() || _compiledPixelShader->DynamicLinkingEnabled();
    }

	ShaderProgram::ShaderProgram(ShaderProgram&& moveFrom) never_throws
	: _compiledVertexShader(moveFrom._compiledVertexShader)
	, _compiledPixelShader(moveFrom._compiledPixelShader)
	, _compiledGeometryShader(moveFrom._compiledGeometryShader)
	, _vertexShader(std::move(moveFrom._vertexShader))
	, _pixelShader(std::move(moveFrom._pixelShader))
	, _geometryShader(std::move(moveFrom._geometryShader))
	, _validationCallback(std::move(moveFrom._validationCallback))
	{
		moveFrom._compiledVertexShader = nullptr;
		moveFrom._compiledPixelShader = nullptr;
		moveFrom._compiledGeometryShader = nullptr;
	}

    ShaderProgram& ShaderProgram::operator=(ShaderProgram&& moveFrom) never_throws
	{
		_compiledVertexShader = moveFrom._compiledVertexShader;
		_compiledPixelShader = moveFrom._compiledPixelShader;
		_compiledGeometryShader = moveFrom._compiledGeometryShader;
		moveFrom._compiledVertexShader = nullptr;
		moveFrom._compiledPixelShader = nullptr;
		moveFrom._compiledGeometryShader = nullptr;

		_vertexShader = std::move(moveFrom._vertexShader);
		_pixelShader = std::move(moveFrom._pixelShader);
		_geometryShader = std::move(moveFrom._geometryShader);
		_validationCallback = std::move(moveFrom._validationCallback);
		return *this;
	}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    DeepShaderProgram::DeepShaderProgram(  
        StringSection<ResChar> vertexShaderInitializer, 
        StringSection<ResChar> geometryShaderInitializer,
        StringSection<ResChar> fragmentShaderInitializer,
        StringSection<ResChar> hullShaderInitializer,
        StringSection<ResChar> domainShaderInitializer,
        StringSection<ResChar> definesTable)
    :   ShaderProgram(vertexShaderInitializer, geometryShaderInitializer, fragmentShaderInitializer, definesTable)
    ,   _compiledHullShader(::Assets::GetAssetComp<CompiledShaderByteCode>(hullShaderInitializer, definesTable))
    ,   _compiledDomainShader(::Assets::GetAssetComp<CompiledShaderByteCode>(domainShaderInitializer, definesTable))
    ,   _hullShader(_compiledHullShader)
    ,   _domainShader(_compiledDomainShader)
    {
        Assets::RegisterAssetDependency(_validationCallback, _compiledHullShader.GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, _compiledDomainShader.GetDependencyValidation());
    }

    DeepShaderProgram::~DeepShaderProgram() {}

}}

