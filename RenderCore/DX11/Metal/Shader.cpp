// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "DeviceContext.h"
#include "ObjectFactory.h"
#include "InputLayout.h"
#include "../../RenderUtils.h"
#include "../../Format.h"
#include "../../../Assets/IntermediateAssets.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/StringFormat.h"
#include "../../../Core/Exceptions.h"

#include "IncludeDX11.h"
#include <D3D11Shader.h>

namespace RenderCore { namespace Metal_DX11
{
    using ::Assets::ResChar;

        ////////////////////////////////////////////////////////////////////////////////////////////////

    VertexShader::VertexShader(const ResChar initializer[])
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
        _underlying = GetObjectFactory().CreateVertexShader(byteCode.first, byteCode.second);
    }

    VertexShader::VertexShader(const CompiledShaderByteCode& compiledShader)
    {
		auto& objFactory = GetObjectFactory();
        if (compiledShader.DynamicLinkingEnabled())
            _classLinkage = objFactory.CreateClassLinkage();

        if (compiledShader.GetStage() != ShaderStage::Null) {
            assert(compiledShader.GetStage() == ShaderStage::Vertex);
            auto byteCode = compiledShader.GetByteCode();
            _underlying = objFactory.CreateVertexShader(byteCode.first, byteCode.second, _classLinkage.get());
        }
    }

    VertexShader::VertexShader() {}
    
    VertexShader::~VertexShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    PixelShader::PixelShader(const ResChar initializer[])
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
        _underlying = GetObjectFactory().CreatePixelShader(byteCode.first, byteCode.second);
    }

    PixelShader::PixelShader(const CompiledShaderByteCode& compiledShader)
    {
        auto& objFactory = GetObjectFactory();
        if (compiledShader.DynamicLinkingEnabled())
            _classLinkage = objFactory.CreateClassLinkage();

        if (compiledShader.GetStage() != ShaderStage::Null) {
            assert(compiledShader.GetStage() == ShaderStage::Pixel);
            auto byteCode = compiledShader.GetByteCode();
            _underlying = objFactory.CreatePixelShader(byteCode.first, byteCode.second, _classLinkage.get());
        }
    }

    PixelShader::PixelShader() {}
    PixelShader::~PixelShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned BuildNativeDeclaration(
        D3D11_SO_DECLARATION_ENTRY nativeDeclaration[], unsigned nativeDeclarationCount,
        const GeometryShader::StreamOutputInitializers& soInitializers)
    {
        auto finalCount = std::min(nativeDeclarationCount, soInitializers._outputElementCount);
        for (unsigned c=0; c<finalCount; ++c) {
            auto& ele = soInitializers._outputElements[c];
            nativeDeclaration[c].Stream = 0;
            nativeDeclaration[c].SemanticName = ele._semanticName.c_str();
            nativeDeclaration[c].SemanticIndex = ele._semanticIndex;
            nativeDeclaration[c].StartComponent = 0;
            nativeDeclaration[c].ComponentCount = (BYTE)GetComponentCount(GetComponents(ele._nativeFormat));
                // hack -- treat "R16G16B16A16_FLOAT" as a 3 dimensional vector
            if (ele._nativeFormat == Format::R16G16B16A16_FLOAT)
                nativeDeclaration[c].ComponentCount = 3;
            nativeDeclaration[c].OutputSlot = (BYTE)ele._inputSlot;
            assert(nativeDeclaration[c].OutputSlot < soInitializers._outputBufferCount);
        }
        return finalCount;
    }

    GeometryShader::GeometryShader( const ResChar initializer[],
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

        intrusive_ptr<ID3D::GeometryShader> underlying;

        if (soInitializers._outputBufferCount == 0) {

			const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
            assert(compiledShader.GetStage() == ShaderStage::Geometry);
            auto byteCode = compiledShader.GetByteCode();
            underlying = GetObjectFactory().CreateGeometryShader(byteCode.first, byteCode.second);

        } else {

            assert(soInitializers._outputBufferCount < D3D11_SO_BUFFER_SLOT_COUNT);
            D3D11_SO_DECLARATION_ENTRY nativeDeclaration[D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT];
            auto delcCount = BuildNativeDeclaration(nativeDeclaration, dimof(nativeDeclaration), soInitializers);

            auto& objFactory = GetObjectFactory();
            auto featureLevel = objFactory.GetUnderlying()->GetFeatureLevel();

			const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
            assert(compiledShader.GetStage() == ShaderStage::Geometry);
            auto byteCode = compiledShader.GetByteCode();
            underlying = objFactory.CreateGeometryShaderWithStreamOutput( 
                byteCode.first, byteCode.second,
                nativeDeclaration, delcCount,
                soInitializers._outputBufferStrides, soInitializers._outputBufferCount,
                    //      Note --     "NO_RASTERIZED_STREAM" is only supported on feature level 11. For other feature levels
                    //                  we must disable the rasterization step some other way
                (featureLevel>=D3D_FEATURE_LEVEL_11_0)?D3D11_SO_NO_RASTERIZED_STREAM:0);

        }

            //  (creation successful; we can commit to member now)
        _underlying = std::move(underlying);
    }

    GeometryShader::GeometryShader(const CompiledShaderByteCode& compiledShader, const StreamOutputInitializers& soInitializers)
    {
        if (compiledShader.GetStage() != ShaderStage::Null) {
            assert(compiledShader.GetStage() == ShaderStage::Geometry);

            auto byteCode = compiledShader.GetByteCode();

            intrusive_ptr<ID3D::GeometryShader> underlying;
            if (soInitializers._outputBufferCount == 0) {

                underlying = GetObjectFactory().CreateGeometryShader(byteCode.first, byteCode.second);

            } else {

                assert(soInitializers._outputBufferCount <= D3D11_SO_BUFFER_SLOT_COUNT);
                D3D11_SO_DECLARATION_ENTRY nativeDeclaration[D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT];
                auto delcCount = BuildNativeDeclaration(nativeDeclaration, dimof(nativeDeclaration), soInitializers);

                auto& objFactory = GetObjectFactory();
                auto featureLevel = objFactory.GetUnderlying()->GetFeatureLevel();
                underlying = objFactory.CreateGeometryShaderWithStreamOutput( 
                    byteCode.first, byteCode.second,
                    nativeDeclaration, delcCount,
                    soInitializers._outputBufferStrides, soInitializers._outputBufferCount,
                        //      Note --     "NO_RASTERIZED_STREAM" is only supported on feature level 11. For other feature levels
                        //                  we must disable the rasterization step some other way
                    (featureLevel>=D3D_FEATURE_LEVEL_11_0)?D3D11_SO_NO_RASTERIZED_STREAM:0);

            }

            _underlying = std::move(underlying);
        }
    }

    GeometryShader::GeometryShader(GeometryShader&& moveFrom)
    {
        _underlying = std::move(moveFrom._underlying);
    }
    
    GeometryShader& GeometryShader::operator=(GeometryShader&& moveFrom)
    {
        _underlying = std::move(moveFrom._underlying);
        return *this;
    }

    GeometryShader::GeometryShader() {}
    GeometryShader::~GeometryShader() {}

    GeometryShader::StreamOutputInitializers GeometryShader::_hackInitializers;

    void GeometryShader::SetDefaultStreamOutputInitializers(const StreamOutputInitializers& initializers)
    {
        _hackInitializers = initializers;
    }

    auto GeometryShader::GetDefaultStreamOutputInitializers() -> const StreamOutputInitializers&
    {
        return _hackInitializers;
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    ComputeShader::ComputeShader(const ResChar initializer[], const ResChar definesTable[])
    {
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "cs_")) {
            StringMeldInPlace(temp) << initializer << ":" CS_DefShaderModel;
            initializer = temp;
        }

        const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable?definesTable:"");
        assert(compiledShader.GetStage() == ShaderStage::Compute);
        auto byteCode = compiledShader.GetByteCode();
        _underlying = GetObjectFactory().CreateComputeShader(byteCode.first, byteCode.second);

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, compiledShader.GetDependencyValidation());
    }

    ComputeShader::ComputeShader(const CompiledShaderByteCode& compiledShader)
    {
        if (compiledShader.GetStage() != ShaderStage::Null) {
            assert(compiledShader.GetStage() == ShaderStage::Compute);
            auto byteCode = compiledShader.GetByteCode();
            _underlying = GetObjectFactory().CreateComputeShader(byteCode.first, byteCode.second);
        }

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, compiledShader.GetDependencyValidation());
    }

    ComputeShader::ComputeShader() {}

    ComputeShader::~ComputeShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    DomainShader::DomainShader(const ResChar initializer[], const ResChar definesTable[])
    {
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "ds_")) {
            StringMeldInPlace(temp) << initializer << ":" DS_DefShaderModel;
            initializer = temp;
        }

		const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable?definesTable:"");
        assert(compiledShader.GetStage() == ShaderStage::Domain);
        auto byteCode = compiledShader.GetByteCode();
        _underlying = GetObjectFactory().CreateDomainShader(byteCode.first, byteCode.second);

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, compiledShader.GetDependencyValidation());
    }

    DomainShader::DomainShader(const CompiledShaderByteCode& compiledShader)
    {
        if (compiledShader.GetStage() != ShaderStage::Null) {
            assert(compiledShader.GetStage() == ShaderStage::Domain);
            auto byteCode = compiledShader.GetByteCode();
            _underlying = GetObjectFactory().CreateDomainShader(byteCode.first, byteCode.second);
        }

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, compiledShader.GetDependencyValidation());
    }

    DomainShader::~DomainShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    HullShader::HullShader(const ResChar initializer[], const ResChar definesTable[])
    {
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "hs_")) {
            StringMeldInPlace(temp) << initializer << ":" HS_DefShaderModel;
            initializer = temp;
        }

		const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable?definesTable:"");
        assert(compiledShader.GetStage() == ShaderStage::Hull);
        auto byteCode = compiledShader.GetByteCode();
        _underlying = GetObjectFactory().CreateHullShader(byteCode.first, byteCode.second);

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, compiledShader.GetDependencyValidation());
    }

    HullShader::HullShader(const CompiledShaderByteCode& compiledShader)
    {
        if (compiledShader.GetStage() != ShaderStage::Null) {
            assert(compiledShader.GetStage() == ShaderStage::Hull);
            auto byteCode = compiledShader.GetByteCode();
            _underlying = GetObjectFactory().CreateHullShader(byteCode.first, byteCode.second);
        }

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, compiledShader.GetDependencyValidation());
    }

    HullShader::~HullShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    ShaderProgram::ShaderProgram(   const ResChar vertexShaderInitializer[], 
                                    const ResChar fragmentShaderInitializer[])
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
    
    ShaderProgram::ShaderProgram(   const ResChar vertexShaderInitializer[], 
                                    const ResChar fragmentShaderInitializer[],
                                    const ResChar definesTable[])
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

    ShaderProgram::ShaderProgram(   const ResChar vertexShaderInitializer[], 
                                    const ResChar geometryShaderInitializer[],
                                    const ResChar fragmentShaderInitializer[],
                                    const ResChar definesTable[])
    :   _compiledVertexShader(&::Assets::GetAssetComp<CompiledShaderByteCode>(vertexShaderInitializer, definesTable)) // (odd..?)
    ,   _compiledPixelShader(&::Assets::GetAssetComp<CompiledShaderByteCode>(fragmentShaderInitializer, definesTable)) // (odd..?)
    ,   _vertexShader(*_compiledVertexShader)
    ,   _pixelShader(*_compiledPixelShader)
    ,   _compiledGeometryShader(nullptr)
    {
        if (geometryShaderInitializer && geometryShaderInitializer[0]) {
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
        const ResChar vertexShaderInitializer[], 
        const ResChar geometryShaderInitializer[],
        const ResChar fragmentShaderInitializer[],
        const ResChar hullShaderInitializer[],
        const ResChar domainShaderInitializer[],
        const ResChar definesTable[])
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

    template intrusive_ptr<ID3D::ShaderReflection>;
}}

