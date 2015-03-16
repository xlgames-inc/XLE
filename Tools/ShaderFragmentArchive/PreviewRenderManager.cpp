// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "stdafx.h"
#include "PreviewRenderManager.h"
#include "TypeRules.h"
#include "ShaderDiagramDocument.h"
#include "../GUILayer/MarshalString.h"

#include "../../PlatformRig/MaterialVisualisation.h"
#include "../../SceneEngine/LightingParserContext.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../Math/Transformations.h"
#include "../../RenderCore/DX11/Metal/IncludeDX11.h"

namespace PreviewRender
{
    class ManagerPimpl
    {
    public:
        std::shared_ptr<RenderCore::IDevice>   _device;
        std::unique_ptr<::Assets::CompileAndAsyncManager> _asyncMan;
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _globalTechniqueContext;
    };

    static intrusive_ptr<ID3D::Texture2D> CreateTexture(
        RenderCore::IDevice* device, 
        const D3D11_TEXTURE2D_DESC& desc)
    {
        RenderCore::Metal::ObjectFactory objectFactory(device);

        intrusive_ptr<ID3D::Texture2D> result;
        ID3D::Texture2D* tempTarget = nullptr;
        HRESULT hresult = objectFactory.GetUnderlying()->CreateTexture2D(&desc, nullptr, &tempTarget);
        if (tempTarget) {
            if (SUCCEEDED(hresult)) {
                result = moveptr(tempTarget);
            } else tempTarget->Release();
        }
        return result;
    }

#if 0
    static System::String^ BuildTypeName(const D3D11_SHADER_TYPE_DESC& typeDesc)
    {
        System::String^ typeName;

            //      Convert from the D3D type into a string
            //      type name
        if (typeDesc.Type == D3D10_SVT_FLOAT) {
            typeName = "float";
        } else if (typeDesc.Type == D3D10_SVT_INT) {
            typeName = "int";
        } else if (typeDesc.Type == D3D10_SVT_BOOL) {
            typeName = "bool";
        } else if (typeDesc.Type == D3D10_SVT_UINT) {
            typeName = "uint";
        } else {
            return "";
        }

        if (typeDesc.Columns > 1 || (typeDesc.Columns == 1 && typeDesc.Rows > 1)) {
            typeName += typeDesc.Columns;

            if (typeDesc.Rows > 1) {
                typeName += 'x';
                typeName += typeDesc.Rows;
            }
        }

        return typeName;
    }

    class SystemConstantsContext
    {
    public:
        Float3      _lightNegativeDirection;
        Float3      _lightColour;
        unsigned    _outputWidth, _outputHeight;
    };

    static bool WriteSystemVariable(const char name[], ShaderDiagram::Document^ doc, const SystemConstantsContext& context, void* destination, void* destinationEnd)
    {
        size_t size = size_t(destinationEnd) - size_t(destination);
        if (!_stricmp(name, "SI_OutputDimensions") && size >= (sizeof(unsigned)*2)) {
            ((unsigned*)destination)[0] = context._outputWidth;
            ((unsigned*)destination)[1] = context._outputHeight;
            return true;
        } else if (!_stricmp(name, "SI_NegativeLightDirection") && size >= sizeof(Float3)) {
            *((Float3*)destination) = doc->NegativeLightDirection;
            return true;
        } else if (!_stricmp(name, "SI_LightColor") && size >= sizeof(Float3)) {
            *((Float3*)destination) = context._lightColour;
            return true;
        }
        return false;
    }
#endif
    
    class PreviewBuilderPimpl
    {
    public:
        std::unique_ptr<RenderCore::Metal::CompiledShaderByteCode>      _vertexShader;
        std::unique_ptr<RenderCore::Metal::CompiledShaderByteCode>      _pixelShader;
        std::unique_ptr<RenderCore::Metal::ShaderProgram>               _shaderProgram;
        std::string                                                     _errorString;

        RenderCore::Metal::ShaderProgram & GetShaderProgram()
        {
            if (_shaderProgram)         { return *_shaderProgram.get(); }

                // can throw a "PendingResource" or "InvalidResource"
            try {
                auto program = std::make_unique<RenderCore::Metal::ShaderProgram>(std::ref(*_vertexShader), std::ref(*_pixelShader));
                _shaderProgram = std::move(program);
                return *program;
            } catch (const ::Assets::Exceptions::InvalidResource& exception) {
                _errorString = exception.what();
                throw exception;
            }
        }
    };

#if 0

    RenderCore::Metal::ConstantBufferPacket SetupGlobalState(
        RenderCore::Metal::DeviceContext*   context,
        const RenderCore::Techniques::GlobalTransformConstants&  globalTransform)
    {
            // deprecated -- use scene engine to set default states
        // context->Bind(SceneEngine::CommonResources()._dssReadWrite);
        // context->Bind(SceneEngine::CommonResources()._blendStraightAlpha);
        // context->Bind(SceneEngine::CommonResources()._defaultRasterizer);
        // context->BindPS(MakeResourceList(SceneEngine::CommonResources()._defaultSampler));
        return RenderCore::MakeSharedPkt(globalTransform);
    }

    RenderCore::Metal::ConstantBufferPacket SetupGlobalState(
        RenderCore::Metal::DeviceContext*   context,
        const RenderCore::Techniques::CameraDesc&       camera)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;
        
        ViewportDesc viewportDesc(*context);
        Float4x4 worldToCamera = InvertOrthonormalTransform(camera._cameraToWorld);
        Float4x4 projectionMatrix = PerspectiveProjection(
            camera, viewportDesc.Width / float(viewportDesc.Height));

        Techniques::GlobalTransformConstants transformConstants;
        transformConstants._worldToClip = Combine(worldToCamera, projectionMatrix);
        transformConstants._viewToWorld = camera._cameraToWorld;

        transformConstants._worldSpaceView = ExtractTranslation(camera._cameraToWorld);
        transformConstants._minimalProjection = ExtractMinimalProjection(projectionMatrix);
        transformConstants._farClip = Techniques::CalculateNearAndFarPlane(transformConstants._minimalProjection, Techniques::GetDefaultClipSpaceType()).second;

        // auto right      = Normalize(ExtractRight_Cam(camera._cameraToWorld));
        // auto up         = Normalize(ExtractUp_Cam(camera._cameraToWorld));
        // auto forward    = Normalize(ExtractForward_Cam(camera._cameraToWorld));
        // auto fy         = XlTan(0.5f * camera._verticalFieldOfView);
        // auto fx         = fy * viewportDesc.Width / float(viewportDesc.Height);
        // transformConstants._frustumCorners[0] = Expand(Float3(camera._farClip * (forward - fx * right + fy * up)), 1.f);
        // transformConstants._frustumCorners[1] = Expand(Float3(camera._farClip * (forward - fx * right - fy * up)), 1.f);
        // transformConstants._frustumCorners[2] = Expand(Float3(camera._farClip * (forward + fx * right + fy * up)), 1.f);
        // transformConstants._frustumCorners[3] = Expand(Float3(camera._farClip * (forward + fx * right - fy * up)), 1.f);

        const float aspectRatio = viewportDesc.Width / float(viewportDesc.Height);
        const float top = camera._nearClip * XlTan(.5f * camera._verticalFieldOfView);
        const float right = top * aspectRatio;
        Float3 preTransformCorners[] = {
            Float3(-right,  top, -camera._nearClip),
            Float3(-right, -top, -camera._nearClip),
            Float3( right,  top, -camera._nearClip),
            Float3( right, -top, -camera._nearClip) 
        };
        for (unsigned c=0; c<4; ++c) {
            transformConstants._frustumCorners[c] = 
                Expand(TransformDirectionVector(camera._cameraToWorld, preTransformCorners[c]), 1.f);
        }
        
        return SetupGlobalState(context, transformConstants);
    }

#endif

    enum DrawPreviewResult
    {
        DrawPreviewResult_Error,
        DrawPreviewResult_Pending,
        DrawPreviewResult_Success
    };

    static DrawPreviewResult DrawPreview(
        RenderCore::IThreadContext& context, 
        PreviewBuilderPimpl& builder, ShaderDiagram::Document^ doc)
    {
        using namespace PlatformRig;

        try {
            MaterialVisObject visObject;
            visObject._shaderProgram = &builder.GetShaderProgram();
            visObject._systemConstants._lightNegativeDirection = Normalize(doc->NegativeLightDirection);
            visObject._systemConstants._lightColour = Float3(1,1,1);

                //  We need to convert the material parameters and resource bindings
                //  from the "doc" into native format in the "visObject._parameters" object.
                //  Any "string" parameter might be a texture binding
            for each (auto i in doc->PreviewMaterialState) {
                auto str = dynamic_cast<String^>(i.Value);
                if (str) {
                    visObject._parameters._bindings.push_back(
                        RenderCore::Assets::MaterialParameters::ResourceBinding(
                            Hash64(clix::marshalString<clix::E_UTF8>(i.Key)),
                            clix::marshalString<clix::E_UTF8>(str)));
                }
            }

                //  Shader constants are more difficult... we only support uint32 currently!
            for each (auto i in doc->PreviewMaterialState) {
                uint32 dest;
                ShaderPatcherLayer::TypeRules::CopyToBytes(
                    &dest, i.Value, "uint", 
                    ShaderPatcherLayer::TypeRules::ExtractTypeName(i.Value),
                    PtrAdd(&dest, sizeof(dest)));
            }

            MaterialVisSettings visSettings;
            visSettings._camera = std::make_shared<VisCameraSettings>();
            visSettings._camera->_position = Float3(-5, 0, 0);

            SceneEngine::LightingParserContext parserContext(
                *Manager::Instance->GetGlobalTechniqueContext());

            bool result = PlatformRig::MaterialVisLayer::Draw(
                context, parserContext, 
                visSettings, visObject);

            if (result) return DrawPreviewResult_Success;
            if (!parserContext._pendingResources.empty()) return DrawPreviewResult_Pending;
        } catch (::Assets::Exceptions::InvalidResource&) { return DrawPreviewResult_Error; }
        catch (::Assets::Exceptions::PendingResource&) { return DrawPreviewResult_Pending; }

        return DrawPreviewResult_Error;
    }

    System::Drawing::Bitmap^    PreviewBuilder::GenerateErrorBitmap(const char str[], Size^ size)
    {
            //      Previously, we got an error while rendering this item.
            //      Render some text to the bitmap with an error string. Just
            //      use the gdi for this (don't bother rendering via D3D)

        using System::Drawing::Bitmap;
        using namespace System::Drawing;
        Bitmap^ newBitmap = gcnew Bitmap(size->Width, size->Height, Imaging::PixelFormat::Format32bppArgb);

        Graphics^ dc = Graphics::FromImage(newBitmap);
        dc->FillRectangle(gcnew SolidBrush(Color::Black), 0, 0, newBitmap->Width, newBitmap->Height);
        dc->DrawString(gcnew String(str), gcnew Font("Arial", 9), gcnew SolidBrush(Color::White), RectangleF(0.f, 0.f, float(newBitmap->Width), float(newBitmap->Height)));
        delete dc;

        return newBitmap;
    }

    System::Drawing::Bitmap^    PreviewBuilder::GenerateBitmap(ShaderDiagram::Document^ doc, Size^ size)
    {
        const int width = std::max(0, int(size->Width));
        const int height = std::max(0, int(size->Height));

        if (!_pimpl->_errorString.empty()) {
            return GenerateErrorBitmap(_pimpl->_errorString.c_str(), size);
        }

            // note --  call GetShaderProgram() to check to see if our compile
            //          is ready. Do this before creating textures, etc -- to minimize
            //          overhead in the main thread while shaders are still compiling
        try {
            _pimpl->GetShaderProgram();
        } catch (const ::Assets::Exceptions::PendingResource&) {
            return nullptr; // still pending
        }
        catch (...) {}

        using namespace RenderCore;
        using namespace RenderCore::Metal;
        auto context = Manager::Instance->GetDevice()->GetImmediateContext();
        auto metalContext = DeviceContext::Get(*context);

        D3D11_TEXTURE2D_DESC targetDesc;
        targetDesc.Width                = width;
        targetDesc.Height               = height;
        targetDesc.MipLevels            = 1;
        targetDesc.ArraySize            = 1;
        targetDesc.Format               = DXGI_FORMAT_R8G8B8A8_TYPELESS;
        targetDesc.SampleDesc.Count     = 1;
        targetDesc.SampleDesc.Quality   = 0;
        targetDesc.Usage                = D3D11_USAGE_DEFAULT;
        targetDesc.BindFlags            = D3D11_BIND_RENDER_TARGET;
        targetDesc.CPUAccessFlags       = 0;
        targetDesc.MiscFlags            = 0;

        auto targetTexture = CreateTexture(Manager::Instance->GetDevice(), targetDesc);
        intrusive_ptr<ID3D::RenderTargetView> renderTargetView;
        if (targetTexture) {
            D3D11_RENDER_TARGET_VIEW_DESC viewDesc;
            viewDesc.Format                 = DXGI_FORMAT_R8G8B8A8_UNORM;       // \todo -- SRGB correction
            viewDesc.ViewDimension          = D3D11_RTV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MipSlice     = 0;
            {
                ID3D::RenderTargetView* tempView = nullptr;
                RenderCore::Metal::ObjectFactory objectFactory(Manager::Instance->GetDevice());
                HRESULT hresult = objectFactory.GetUnderlying()->CreateRenderTargetView(targetTexture.get(), &viewDesc, &tempView);
                if (tempView) {
                    if (SUCCEEDED(hresult)) {
                        renderTargetView = moveptr(tempView);
                    } else tempView->Release();
                }
            }
        }

        {
            ID3D::RenderTargetView* rtView = renderTargetView.get();
            metalContext->GetUnderlying()->OMSetRenderTargets(1, &rtView, nullptr);

            D3D11_VIEWPORT viewport;
            viewport.TopLeftX = viewport.TopLeftY = 0;
            viewport.Width = FLOAT(width); 
            viewport.Height = FLOAT(height);
            viewport.MinDepth = 0.f; viewport.MaxDepth = 1.f;
            metalContext->GetUnderlying()->RSSetViewports(1, &viewport);

            FLOAT clearColor[] = {0.05f, 0.05f, 0.2f, 1.f};
            metalContext->GetUnderlying()->ClearRenderTargetView(rtView, clearColor);
        }

            ////////////

        auto result = DrawPreview(*context, *_pimpl, doc);
        if (result == DrawPreviewResult_Error) {
            return GenerateErrorBitmap(_pimpl->_errorString.c_str(), size);
        } else if (result == DrawPreviewResult_Pending) {
            return nullptr;
        }

            ////////////

        D3D11_TEXTURE2D_DESC readableTargetDesc = targetDesc;
        readableTargetDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        readableTargetDesc.Usage = D3D11_USAGE_STAGING;
        readableTargetDesc.BindFlags = 0;
        readableTargetDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        intrusive_ptr<ID3D::Texture2D> readableTarget = CreateTexture(Manager::Instance->GetDevice(), readableTargetDesc);

        metalContext->GetUnderlying()->CopyResource(readableTarget.get(), targetTexture.get());

        D3D11_MAPPED_SUBRESOURCE mappedTexture;
        HRESULT hresult = metalContext->GetUnderlying()->Map(readableTarget.get(), 0, D3D11_MAP_READ, 0, &mappedTexture);
        if (SUCCEEDED(hresult)) {
            using System::Drawing::Bitmap;
            using namespace System::Drawing;
            Bitmap^ newBitmap = gcnew Bitmap(width, height, Imaging::PixelFormat::Format32bppArgb);
            auto data = newBitmap->LockBits(System::Drawing::Rectangle(0, 0, width, height), Imaging::ImageLockMode::WriteOnly, Imaging::PixelFormat::Format32bppArgb);
            try
            {
                    // we have to flip ABGR -> ARGB!
                for (int y=0; y<height; ++y) {
                    void* sourcePtr = PtrAdd(mappedTexture.pData, y * mappedTexture.RowPitch);
                    System::IntPtr destinationPtr = data->Scan0 + y * width * sizeof(unsigned);
                    for (int x=0; x<width; ++x) {
                        ((unsigned*)(void*)destinationPtr)[x] = 
                            (RenderCore::ARGBtoABGR(((unsigned*)sourcePtr)[x]) & 0x00ffffff) | 0xff000000;
                    }
                }
                // XlCopyMemory((void*)ptr, mappedTexture.pData, mappedTexture.DepthPitch);
            }
            finally
            {
                newBitmap->UnlockBits(data);
            }

            metalContext->GetUnderlying()->Unmap(readableTarget.get(), 0);
            return newBitmap;
        }

        return nullptr;
    }

    void    PreviewBuilder::Update(ShaderDiagram::Document^ doc, Size^ size)
    {
        _bitmap = GenerateBitmap(doc, size);
    }

    void    PreviewBuilder::Invalidate()
    {
        delete _bitmap;
        _bitmap = nullptr;
    }

    PreviewBuilder::PreviewBuilder(System::String^ shaderText)
    {
        _pimpl = new PreviewBuilderPimpl();

        std::string nativeShaderText = clix::marshalString<clix::E_UTF8>(shaderText);
        try {
            using namespace RenderCore::Metal;
            _pimpl->_vertexShader = std::make_unique<CompiledShaderByteCode>(nativeShaderText.c_str(), "VertexShaderEntry", VS_DefShaderModel, "SHADER_NODE_EDITOR=1");
            _pimpl->_pixelShader = std::make_unique<CompiledShaderByteCode>(nativeShaderText.c_str(), "PixelShaderEntry", PS_DefShaderModel, "SHADER_NODE_EDITOR=1");
        }
        catch (::Assets::Exceptions::PendingResource&) {}
        catch (::Assets::Exceptions::InvalidResource&) 
        {
            _pimpl->_errorString = "Compile failure";
        }
    }

    PreviewBuilder::~PreviewBuilder()
    {
        delete _pimpl;
    }

    PreviewBuilder^    Manager::CreatePreview(System::String^ shaderText)
    {
        return gcnew PreviewBuilder(shaderText);
    }

    void                    Manager::RotateLightDirection(ShaderDiagram::Document^ doc, System::Drawing::PointF rotationAmount)
    {
        try {
            float deltaCameraYaw    = -rotationAmount.Y * 1.f * gPI / 180.f;
            float deltaCameraPitch  =  rotationAmount.X * 1.f * gPI / 180.f;

            Float3x3 rotationPart;
            cml::matrix_rotation_euler(rotationPart, deltaCameraYaw, 0.f, deltaCameraPitch, cml::euler_order_yxz);

            auto negLightDir = doc->NegativeLightDirection;
            negLightDir = TransformDirectionVector(rotationPart, negLightDir);
            doc->NegativeLightDirection = Normalize(negLightDir);
        } catch(...) {
            doc->NegativeLightDirection = Float3(0.f, 0.f, 1.f);        // catch any math errors
        }
    }

    RenderCore::IDevice*        Manager::GetDevice()
    {
        return _pimpl->_device.get();
    }

    RenderCore::Techniques::TechniqueContext* Manager::GetGlobalTechniqueContext()
    {
        return _pimpl->_globalTechniqueContext.get();
    }

    static Manager::Manager()
    {
        _instance = nullptr;
    }

    Manager::Manager()
    {
        _pimpl = new ManagerPimpl;
        _pimpl->_device = RenderCore::CreateDevice();
        _pimpl->_asyncMan = RenderCore::Metal::CreateCompileAndAsyncManager();
        _pimpl->_globalTechniqueContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
    }

    Manager::~Manager()
    {
        delete _pimpl;
    }


}

