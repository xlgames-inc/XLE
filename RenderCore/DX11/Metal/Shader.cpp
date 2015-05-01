// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "DeviceContext.h"
#include "InputLayout.h"
#include "../../RenderUtils.h"
#include "../../../Assets/IntermediateResources.h"
#include "../../../Core/Exceptions.h"

#include "IncludeDX11.h"
#include <D3DX11.h>

namespace RenderCore { namespace Metal_DX11
{
    ID3DX11ThreadPump* GetThreadPump();
    void FlushThreadPump();

        ////////////////////////////////////////////////////////////////////////////////////////////////

    VertexShader::VertexShader(const ResChar initializer[])
    {
            //
            //      We have to append the shader model to the resource name
            //      (if it's not already there)
            //
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "vs_")) {
            XlCopyString(temp, initializer);
            size_t len = XlStringLen(temp);
            XlCopyString(&temp[len], dimof(temp)-len-1, ":" VS_DefShaderModel);
            initializer = temp;
        }

		const auto& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
        const size_t byteCodeSize = byteCode.GetSize();
        if (byteCodeSize) {
            const void* byteCodeData = byteCode.GetByteCode();
            _underlying = ObjectFactory().CreateVertexShader(byteCodeData, byteCodeSize);
        }
    }

    VertexShader::VertexShader(const CompiledShaderByteCode& byteCode)
    {
        if (byteCode.GetStage() != ShaderStage::Null) {
            const size_t byteCodeSize = byteCode.GetSize();
            if (byteCodeSize) {
                const void* byteCodeData = byteCode.GetByteCode();
                _underlying = ObjectFactory().CreateVertexShader(byteCodeData, byteCodeSize);
            }
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
            XlCopyString(temp, initializer);
            size_t len = XlStringLen(temp);
            XlCopyString(&temp[len], dimof(temp)-len-1, ":" PS_DefShaderModel);
            initializer = temp;
        }

        const auto& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
        _underlying = ObjectFactory().CreatePixelShader(byteCode.GetByteCode(), byteCode.GetSize());
    }

    PixelShader::PixelShader(const CompiledShaderByteCode& byteCode)
    {
        if (byteCode.GetStage() != ShaderStage::Null) {
            const size_t byteCodeSize = byteCode.GetSize();
            if (byteCodeSize) {
                const void* byteCodeData = byteCode.GetByteCode();
                _underlying = ObjectFactory().CreatePixelShader(byteCodeData, byteCodeSize);
            }
        }
    }

    PixelShader::PixelShader() {}

    PixelShader::~PixelShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    GeometryShader::GeometryShader( const ResChar initializer[],
                                    const StreamOutputInitializers& soInitializers)
    {
            //
            //      We have to append the shader model to the resource name
            //      (if it's not already there)
            //
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "gs_")) {
            XlCopyString(temp, initializer);
            size_t len = XlStringLen(temp);
            XlCopyString(&temp[len], dimof(temp)-len-1, ":" GS_DefShaderModel);
            initializer = temp;
        }

        intrusive_ptr<ID3D::GeometryShader> underlying;

        if (soInitializers._outputBufferCount == 0) {

			const auto& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
            underlying = ObjectFactory().CreateGeometryShader(byteCode.GetByteCode(), byteCode.GetSize());

        } else {

            assert(soInitializers._outputBufferCount < D3D11_SO_BUFFER_SLOT_COUNT);
            D3D11_SO_DECLARATION_ENTRY nativeDeclaration[D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT];
            for (unsigned c=0; c<std::min(unsigned(dimof(nativeDeclaration)), soInitializers._outputElementCount); ++c) {
                auto& ele = soInitializers._outputElements[c];
                nativeDeclaration[c].Stream = 0;
                nativeDeclaration[c].SemanticName = ele._semanticName.c_str();
                nativeDeclaration[c].SemanticIndex = ele._semanticIndex;
                nativeDeclaration[c].StartComponent = 0;
                nativeDeclaration[c].ComponentCount = (BYTE)GetComponentCount(GetComponents(ele._nativeFormat));
                    // hack -- treat "R16G16B16A16_FLOAT" as a 3 dimensional vector
                if (ele._nativeFormat == NativeFormat::Enum::R16G16B16A16_FLOAT)
                    nativeDeclaration[c].ComponentCount = 3;
                nativeDeclaration[c].OutputSlot = (BYTE)ele._inputSlot;
                assert(nativeDeclaration[c].OutputSlot < soInitializers._outputBufferCount);
            }

            ObjectFactory objFactory;
            auto featureLevel = objFactory.GetUnderlying()->GetFeatureLevel();

			const auto& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
            underlying = objFactory.CreateGeometryShaderWithStreamOutput( 
                byteCode.GetByteCode(), byteCode.GetSize(),
                nativeDeclaration, std::min(unsigned(dimof(nativeDeclaration)), soInitializers._outputElementCount),
                soInitializers._outputBufferStrides, soInitializers._outputBufferCount,
                    //      Note --     "NO_RASTERIZED_STREAM" is only supported on feature level 11. For other feature levels
                    //                  we must disable the rasterization step some other way
                (featureLevel>=D3D_FEATURE_LEVEL_11_0)?D3D11_SO_NO_RASTERIZED_STREAM:0);

        }

            //  (creation successful; we can commit to member now)
        _underlying = std::move(underlying);
    }

    GeometryShader::GeometryShader(const CompiledShaderByteCode& byteCode, const StreamOutputInitializers& soInitializers)
    {
        if (byteCode.GetStage() != ShaderStage::Null) {
            const size_t byteCodeSize = byteCode.GetSize();
            if (byteCodeSize) {

                intrusive_ptr<ID3D::GeometryShader> underlying;

                if (soInitializers._outputBufferCount == 0) {

                    underlying = ObjectFactory().CreateGeometryShader(byteCode.GetByteCode(), byteCodeSize);

                } else {

                    assert(soInitializers._outputBufferCount <= D3D11_SO_BUFFER_SLOT_COUNT);
                    D3D11_SO_DECLARATION_ENTRY nativeDeclaration[D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT];
                    for (unsigned c=0; c<std::min(unsigned(dimof(nativeDeclaration)), soInitializers._outputElementCount); ++c) {
                        auto& ele = soInitializers._outputElements[c];
                        nativeDeclaration[c].Stream = 0;
                        nativeDeclaration[c].SemanticName = ele._semanticName.c_str();
                        nativeDeclaration[c].SemanticIndex = ele._semanticIndex;
                        nativeDeclaration[c].StartComponent = 0;
                        nativeDeclaration[c].ComponentCount = (BYTE)GetComponentCount(GetComponents(ele._nativeFormat));
                        nativeDeclaration[c].OutputSlot = (BYTE)ele._inputSlot;
                        assert(nativeDeclaration[c].OutputSlot < soInitializers._outputBufferCount);
                    }

                    ObjectFactory objFactory;
                    auto featureLevel = objFactory.GetUnderlying()->GetFeatureLevel();
                    underlying = objFactory.CreateGeometryShaderWithStreamOutput( 
                        byteCode.GetByteCode(), byteCode.GetSize(),
                        nativeDeclaration, std::min(unsigned(dimof(nativeDeclaration)), soInitializers._outputElementCount),
                        soInitializers._outputBufferStrides, soInitializers._outputBufferCount,
                            //      Note --     "NO_RASTERIZED_STREAM" is only supported on feature level 11. For other feature levels
                            //                  we must disable the rasterization step some other way
                        (featureLevel>=D3D_FEATURE_LEVEL_11_0)?D3D11_SO_NO_RASTERIZED_STREAM:0);

                }

                _underlying = std::move(underlying);
            }
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
            XlCopyString(temp, initializer);
            size_t len = XlStringLen(temp);
            XlCopyString(&temp[len], dimof(temp)-len-1, ":" CS_DefShaderModel);
            initializer = temp;
        }

		const auto& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable?definesTable:"");
        auto underlying = ObjectFactory().CreateComputeShader(byteCode.GetByteCode(), byteCode.GetSize());

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependencyValidation());

            //  (creation successful; we can commit to member now)
        _underlying = std::move(underlying);
    }

    ComputeShader::ComputeShader(const CompiledShaderByteCode& byteCode)
    {
        if (byteCode.GetStage() != ShaderStage::Null) {
            const size_t byteCodeSize = byteCode.GetSize();
            const void* byteCodeData = byteCode.GetByteCode();
            _underlying = ObjectFactory().CreateComputeShader(byteCodeData, byteCodeSize);
        }

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependencyValidation());
    }

    ComputeShader::~ComputeShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    DomainShader::DomainShader(const ResChar initializer[], const ResChar definesTable[])
    {
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "ds_")) {
            XlCopyString(temp, initializer);
            size_t len = XlStringLen(temp);
            XlCopyString(&temp[len], dimof(temp)-len-1, ":" DS_DefShaderModel);
            initializer = temp;
        }

		const auto& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable);
        auto underlying = ObjectFactory().CreateDomainShader(byteCode.GetByteCode(), byteCode.GetSize());

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependencyValidation());

            //  (creation successful; we can commit to member now)
        _underlying = std::move(underlying);
    }

    DomainShader::DomainShader(const CompiledShaderByteCode& byteCode)
    {
        if (byteCode.GetStage() != ShaderStage::Null) {
            const size_t byteCodeSize = byteCode.GetSize();
            const void* byteCodeData = byteCode.GetByteCode();
            _underlying = ObjectFactory().CreateDomainShader(byteCodeData, byteCodeSize);
        }

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependencyValidation());
    }

    DomainShader::~DomainShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    HullShader::HullShader(const ResChar initializer[], const ResChar definesTable[])
    {
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "hs_")) {
            XlCopyString(temp, initializer);
            size_t len = XlStringLen(temp);
            XlCopyString(&temp[len], dimof(temp)-len-1, ":" HS_DefShaderModel);
            initializer = temp;
        }

		const auto& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable);
        auto underlying = ObjectFactory().CreateHullShader(byteCode.GetByteCode(), byteCode.GetSize());

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependencyValidation());

            //  (creation successful; we can commit to member now)
        _underlying = std::move(underlying);
    }

    HullShader::HullShader(const CompiledShaderByteCode& byteCode)
    {
        if (byteCode.GetStage() != ShaderStage::Null) {
            const size_t byteCodeSize = byteCode.GetSize();
            const void* byteCodeData = byteCode.GetByteCode();
            _underlying = ObjectFactory().CreateHullShader(byteCodeData, byteCodeSize);
        }

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependencyValidation());
    }

    HullShader::~HullShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    ShaderProgram::ShaderProgram(   const ResChar vertexShaderInitializer[], 
                                    const ResChar fragmentShaderInitializer[])
    :   _compiledVertexShader(::Assets::GetAssetComp<CompiledShaderByteCode>(vertexShaderInitializer)) // (odd..?)
    ,   _compiledPixelShader(::Assets::GetAssetComp<CompiledShaderByteCode>(fragmentShaderInitializer)) // (odd..?)
    ,   _vertexShader(_compiledVertexShader)
    ,   _pixelShader(_compiledPixelShader)
    ,   _compiledGeometryShader(nullptr)
    {
        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &_compiledVertexShader.GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, &_compiledPixelShader.GetDependencyValidation());
    }
    
    ShaderProgram::ShaderProgram(   const ResChar vertexShaderInitializer[], 
                                    const ResChar fragmentShaderInitializer[],
                                    const ResChar definesTable[])
    :   _compiledVertexShader(::Assets::GetAssetComp<CompiledShaderByteCode>(vertexShaderInitializer, definesTable)) // (odd..?)
    ,   _compiledPixelShader(::Assets::GetAssetComp<CompiledShaderByteCode>(fragmentShaderInitializer, definesTable)) // (odd..?)
    ,   _vertexShader(_compiledVertexShader)
    ,   _pixelShader(_compiledPixelShader)
    ,   _compiledGeometryShader(nullptr)
    {
        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &_compiledVertexShader.GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, &_compiledPixelShader.GetDependencyValidation());
    }

    ShaderProgram::ShaderProgram(   const ResChar vertexShaderInitializer[], 
                                    const ResChar geometryShaderInitializer[],
                                    const ResChar fragmentShaderInitializer[],
                                    const ResChar definesTable[])
    :   _compiledVertexShader(::Assets::GetAssetComp<CompiledShaderByteCode>(vertexShaderInitializer, definesTable)) // (odd..?)
    ,   _compiledPixelShader(::Assets::GetAssetComp<CompiledShaderByteCode>(fragmentShaderInitializer, definesTable)) // (odd..?)
    ,   _vertexShader(_compiledVertexShader)
    ,   _pixelShader(_compiledPixelShader)
    ,   _compiledGeometryShader(nullptr)
    {
        if (geometryShaderInitializer && geometryShaderInitializer[0]) {
            _compiledGeometryShader = &::Assets::GetAssetComp<CompiledShaderByteCode>(geometryShaderInitializer, definesTable);
            _geometryShader = GeometryShader(*_compiledGeometryShader);
        }
        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &_compiledVertexShader.GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, &_compiledPixelShader.GetDependencyValidation());
        if (_compiledGeometryShader) {
            Assets::RegisterAssetDependency(_validationCallback, &_compiledGeometryShader->GetDependencyValidation());
        }
    }

    ShaderProgram::ShaderProgram(   const CompiledShaderByteCode& compiledVertexShader, 
                                    const CompiledShaderByteCode& compiledFragmentShader)
    :   _compiledVertexShader(compiledVertexShader)
    ,   _compiledPixelShader(compiledFragmentShader)
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
        Assets::RegisterAssetDependency(_validationCallback, &_compiledVertexShader.GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, &_compiledPixelShader.GetDependencyValidation());
    }

    ShaderProgram::~ShaderProgram() {}

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
        Assets::RegisterAssetDependency(_validationCallback, &_compiledHullShader.GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, &_compiledDomainShader.GetDependencyValidation());
    }

    DeepShaderProgram::~DeepShaderProgram() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    static D3DX11_IMAGE_INFO    LoadInfoFromTexture(const char textureName[])
    {
        D3DX11_IMAGE_INFO imageInfo;
        HRESULT hresult = D3DX11GetImageInfoFromFile(
            textureName, nullptr, &imageInfo, nullptr);
        if (!SUCCEEDED(hresult)) {
            throw Assets::Exceptions::InvalidResource(textureName, "Failure loading info from texture");
        }
        return imageInfo;
    }

    NativeFormat::Enum LoadTextureFormat(const char initializer[])
    {
        auto infoOnDisk = LoadInfoFromTexture(initializer);
        return (NativeFormat::Enum)infoOnDisk.Format;
    }

    intrusive_ptr<ID3D::Resource> LoadTextureImmediately(const char initializer[], bool sourceIsLinearSpace)
    {
            // load a texture from disk immediately
            //  use an SRGB format if required
        D3DX11_IMAGE_LOAD_INFO loadInfo;
        loadInfo.Width = D3DX11_DEFAULT;
        loadInfo.Height = D3DX11_DEFAULT;
        loadInfo.Depth = D3DX11_DEFAULT;
        loadInfo.FirstMipLevel = D3DX11_DEFAULT;
        loadInfo.MipLevels = D3DX11_DEFAULT;
        loadInfo.Usage = (D3D11_USAGE) D3DX11_DEFAULT;
        loadInfo.BindFlags = D3DX11_DEFAULT;
        loadInfo.CpuAccessFlags = D3DX11_DEFAULT;
        loadInfo.MiscFlags = D3DX11_DEFAULT;
        loadInfo.Format = DXGI_FORMAT_FROM_FILE;
        loadInfo.Filter = D3DX11_DEFAULT;
        loadInfo.MipFilter = D3DX11_DEFAULT;
        loadInfo.pSrcInfo = NULL;

        // auto infoOnDisk = LoadInfoFromTexture(initializer);
        // if (!sourceIsLinearSpace) {
        //     auto srgbFormat = AsSRGBFormat(infoOnDisk.Format);
        //     if (srgbFormat != infoOnDisk.Format) {
        //         loadInfo.Format = srgbFormat;       // it doesn't work! Seems to invoke a conversion within the loading code
        //     }
        // }

        ID3D::Resource* texture = nullptr;
        auto hresult = D3DX11CreateTextureFromFile(ObjectFactory().GetUnderlying(), initializer, &loadInfo, nullptr, &texture, nullptr);
        if (!SUCCEEDED(hresult)) {
            ThrowException(
                Assets::Exceptions::InvalidResource(initializer, "Failure creating texture resource"));
        }

        return intrusive_ptr<ID3D::Resource>(texture, false);
    }
}}

