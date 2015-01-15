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
#include "../../../Assets/CompileAndAsyncManager.h"
#include "../../../Core/Exceptions.h"

#include "DX11Utils.h"
#include "IncludeDX11.h"
#include <D3DX11.h>
#include <functional>

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

        CompiledShaderByteCode& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
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

        CompiledShaderByteCode& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
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

            CompiledShaderByteCode& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
            underlying = ObjectFactory().CreateGeometryShader(byteCode.GetByteCode(), byteCode.GetSize());

        } else {

            assert(soInitializers._outputBufferCount < D3D11_SO_BUFFER_SLOT_COUNT);
            D3D11_SO_DECLARATION_ENTRY nativeDeclaration[D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT];
            for (unsigned c=0; c<std::min(unsigned(dimof(nativeDeclaration)), soInitializers._outputElementCount); ++c) {
                nativeDeclaration[c].Stream = 0;
                nativeDeclaration[c].SemanticName = soInitializers._outputElements[c]._semanticName.c_str();
                nativeDeclaration[c].SemanticIndex = soInitializers._outputElements[c]._semanticIndex;
                nativeDeclaration[c].StartComponent = 0;
                nativeDeclaration[c].ComponentCount = 3;        // todo -- get the component count from the expected format (ie, typically either 3 or 4)
                nativeDeclaration[c].OutputSlot = (BYTE)soInitializers._outputElements[c]._inputSlot;
                assert(nativeDeclaration[c].OutputSlot < soInitializers._outputBufferCount);
            }

            ObjectFactory objFactory;
            auto featureLevel = objFactory.GetUnderlying()->GetFeatureLevel();

            CompiledShaderByteCode& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
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

    static unsigned GetComponentCount(NativeFormat::Enum format)
    {
        using namespace FormatComponents;
        auto type = GetComponents(format);
        switch (type) {
        case Unknown:
        case Alpha:
        case Luminance: 
        case Depth: return 1;

        case RG:
        case LuminanceAlpha: return 2;
        
        case RGB: return 3;

        case RGBE:
        case RGBAlpha: return 4;
        }
        return 0;
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
                        nativeDeclaration[c].Stream = 0;
                        nativeDeclaration[c].SemanticName = soInitializers._outputElements[c]._semanticName.c_str();
                        nativeDeclaration[c].SemanticIndex = soInitializers._outputElements[c]._semanticIndex;
                        nativeDeclaration[c].StartComponent = 0;
                        nativeDeclaration[c].ComponentCount = (BYTE)GetComponentCount(soInitializers._outputElements[c]._nativeFormat);        // todo -- get the component count from the expected format (ie, typically either 3 or 4)
                        nativeDeclaration[c].OutputSlot = (BYTE)soInitializers._outputElements[c]._inputSlot;
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

        CompiledShaderByteCode& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable);
        auto underlying = ObjectFactory().CreateComputeShader(byteCode.GetByteCode(), byteCode.GetSize());

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependancyValidation());

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
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependancyValidation());
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

        CompiledShaderByteCode& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable);
        auto underlying = ObjectFactory().CreateDomainShader(byteCode.GetByteCode(), byteCode.GetSize());

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependancyValidation());

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
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependancyValidation());
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

        CompiledShaderByteCode& byteCode = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer, definesTable);
        auto underlying = ObjectFactory().CreateHullShader(byteCode.GetByteCode(), byteCode.GetSize());

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependancyValidation());

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
        Assets::RegisterAssetDependency(_validationCallback, &byteCode.GetDependancyValidation());
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
        Assets::RegisterAssetDependency(_validationCallback, &_compiledVertexShader.GetDependancyValidation());
        Assets::RegisterAssetDependency(_validationCallback, &_compiledPixelShader.GetDependancyValidation());
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
        Assets::RegisterAssetDependency(_validationCallback, &_compiledVertexShader.GetDependancyValidation());
        Assets::RegisterAssetDependency(_validationCallback, &_compiledPixelShader.GetDependancyValidation());
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
        Assets::RegisterAssetDependency(_validationCallback, &_compiledVertexShader.GetDependancyValidation());
        Assets::RegisterAssetDependency(_validationCallback, &_compiledPixelShader.GetDependancyValidation());
        if (_compiledGeometryShader) {
            Assets::RegisterAssetDependency(_validationCallback, &_compiledGeometryShader->GetDependancyValidation());
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
        Assets::RegisterAssetDependency(_validationCallback, &_compiledVertexShader.GetDependancyValidation());
        Assets::RegisterAssetDependency(_validationCallback, &_compiledPixelShader.GetDependancyValidation());
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
        Assets::RegisterAssetDependency(_validationCallback, &_compiledHullShader.GetDependancyValidation());
        Assets::RegisterAssetDependency(_validationCallback, &_compiledDomainShader.GetDependancyValidation());
    }

    DeepShaderProgram::~DeepShaderProgram() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    const ResChar* DeferredShaderResource::LinearSpace = "linear"; 
    const ResChar* DeferredShaderResource::SRGBSpace = "srgb"; 

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

    DeferredShaderResource::DeferredShaderResource(const ResChar initializer[], const ResChar sourceSpace[])
    {
        _futureResource = nullptr;
        _futureResult = ~HRESULT(0x0);
        _sourceInSRGBSpace = sourceSpace == SRGBSpace;
        DEBUG_ONLY(XlCopyString(_initializer, dimof(_initializer), initializer);)

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

            //  we have to hit the disk and get the texture info. This is the only
            //  way to prevent the system from building new mips (if those mips don't
            //  already exist)
        auto infoOnDisk = LoadInfoFromTexture(initializer);

        if (_sourceInSRGBSpace) {

            // assert(0);  // DavidJ --    this isn't working. We need to write a custom DDS loader. the DX11 utils one
                        //              doesn't seem to have a way to mark the texture as an SRGB texture..?

                //     Hack --  we have to do a synchronous read of the file here. We need
                //              to get the real format, so that we can feed the SRGB format
                //              into "D3DX11CreateTextureFromFile"

            auto srgbFormat = AsSRGBFormat(infoOnDisk.Format);
            if (srgbFormat != infoOnDisk.Format) {
                loadInfo.Format = srgbFormat;       // it doesn't work! Seems to invoke a conversion within the loading code
            }

            auto hresult = D3DX11CreateTextureFromFile(
                ObjectFactory().GetUnderlying(),
                initializer, &loadInfo,
                GetThreadPump(), &_futureResource, &_futureResult);

            if (!SUCCEEDED(hresult)) {
                throw Assets::Exceptions::InvalidResource(initializer, "Failure creating texture resource");
            }

        } else {

                //
                //      The resource name should give us some hints as
                //      to what this object is. Typically, it should be a texture
                //          -- so it should be easy to load the texture and return
                //      a shader resource view.
                //
                //      We need to do 2 things -- load the texture off disk, and push the
                //      data into a GPU object.
                //
                //      There are a bunch of ways to do this. But the easiest is just to
                //      use "D3DX11CreateTextureFromFile" with a thread pump!
                //
            
            loadInfo.MipLevels = infoOnDisk.MipLevels;        // (prevent the system from building new mips, if they don't exist in the file already)

            auto hresult = D3DX11CreateTextureFromFile(
                ObjectFactory().GetUnderlying(),
                initializer, &loadInfo,
                GetThreadPump(), &_futureResource, &_futureResult);

            if (!SUCCEEDED(hresult)) {
                throw Assets::Exceptions::InvalidResource(initializer, "Failure creating texture resource");
            }
        }

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        RegisterFileDependency(_validationCallback, initializer);
    }

    DeferredShaderResource::~DeferredShaderResource()
    {
           //      We need to cancel the background compilation process, also
        while (_futureResult == HRESULT(~0)) { FlushThreadPump(); } // (best we can do now is just stall until we get a result)

            //      If the processing was completed since the last call to GetShaderResource(),
            //      then _futureResource might not be null. We have to delete free it in this
            //      case (it can't be protected by a intrusive_ptr, because of the way to pass
            //      the address of the pointer to the D3D thread pump.
        if (_futureResource) {
            _futureResource->Release();
        }
    }

    const ShaderResourceView&       DeferredShaderResource::GetShaderResource() const
    {
        if (_finalResource.GetUnderlying()) {
            return _finalResource;
        }

        if (_futureResult!=~HRESULT(0x0)) {
            if (!SUCCEEDED(_futureResult) || !_futureResource) {
                ThrowException(Assets::Exceptions::InvalidResource(Initializer(), "Unknown error during loading"));
            }

            //if (_sourceInSRGBSpace) {
            //        // we have to create a SRV desc so we can change the pixel format to an SRGB mode
            //         //  Note -- this doesn't work because the texture is created with a fully specified
            //         //          pixel format
            //    if (auto texture = QueryInterfaceCast<ID3D::Texture2D>(_futureResource)) {
            //        TextureDesc2D inputDesc(texture);
            //        auto srgbFormat = AsSRGBFormat(inputDesc.Format);
            //        if (srgbFormat != inputDesc.Format) {
            //            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            //            srvDesc.Format = srgbFormat;
            //            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            //            srvDesc.Texture2D.MostDetailedMip = 0;
            //            srvDesc.Texture2D.MipLevels = (UINT)-1;
            //            
            //            hresult = ObjectFactory::GetDefaultUnderlying()->CreateShaderResourceView(
            //                _futureResource, &srvDesc, &tempView);
            //        }
            //    }
            //}
                
            TRY {
                _finalResource = ShaderResourceView(_futureResource);
            } CATCH(RenderCore::Exceptions::GenericFailure&) {
                ThrowException(Assets::Exceptions::InvalidResource(Initializer(), "Failure while creating shader resource view."));
            } CATCH_END

            _futureResource->Release(); 
            _futureResource = nullptr;
            return _finalResource;
        }

#pragma warning(disable:4702)   // unreachable code
        ThrowException(Assets::Exceptions::PendingResource(Initializer(), ""));
        return _finalResource;
    }

    const char*                     DeferredShaderResource::Initializer() const
    {
        #if defined(_DEBUG)
            return _initializer;
        #else
            return "";
        #endif
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

