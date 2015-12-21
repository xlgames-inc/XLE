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

#include "../ToolsRig/MaterialVisualisation.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../../SceneEngine/LightingParserContext.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/Resource.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../Math/Transformations.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/Conversion.h"

namespace PreviewRender
{
    class ManagerPimpl
    {
    public:
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _globalTechniqueContext;
        std::unique_ptr<RenderCore::Assets::Services> _renderAssetsServices;
        std::shared_ptr<RenderCore::IDevice> _device;
        std::unique_ptr<::Assets::Services> _assetServices;
        std::unique_ptr<ConsoleRig::GlobalServices> _services;
    };

    class PreviewBuilderPimpl
    {
    public:
        std::unique_ptr<RenderCore::CompiledShaderByteCode>     _vertexShader;
        std::unique_ptr<RenderCore::CompiledShaderByteCode>     _pixelShader;
        std::unique_ptr<RenderCore::Metal::ShaderProgram>       _shaderProgram;
        std::string                                             _errorString;

        RenderCore::Metal::ShaderProgram & GetShaderProgram()
        {
            if (_shaderProgram)         { return *_shaderProgram.get(); }

                // can throw a "PendingAsset" or "InvalidAsset"
            try {
                auto program = std::make_unique<RenderCore::Metal::ShaderProgram>(std::ref(*_vertexShader), std::ref(*_pixelShader));
                _shaderProgram = std::move(program);
                return *program;
            } catch (const ::Assets::Exceptions::InvalidAsset& exception) {
                _errorString = exception.what();
                throw exception;
            }
        }
    };

    enum DrawPreviewResult
    {
        DrawPreviewResult_Error,
        DrawPreviewResult_Pending,
        DrawPreviewResult_Success
    };

    class MaterialBinder : public ToolsRig::IMaterialBinder
    {
    public:
        virtual RenderCore::Metal::ShaderProgram* Apply(
            RenderCore::Metal::DeviceContext& metalContext,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex,
            const RenderCore::Assets::ResolvedMaterial& mat,
            const SystemConstants& sysConstants,
            const ::Assets::DirectorySearchRules& searchRules,
            const RenderCore::Metal::InputLayout& geoInputLayout);
        
        MaterialBinder(RenderCore::Metal::ShaderProgram& shaderProgram);
        ~MaterialBinder();
    private:
        RenderCore::Metal::ShaderProgram* _shaderProgram;
    };

    RenderCore::Metal::ShaderProgram* MaterialBinder::Apply(
            RenderCore::Metal::DeviceContext& metalContext,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex,
            const RenderCore::Assets::ResolvedMaterial& mat,
            const SystemConstants& sysConstants,
            const ::Assets::DirectorySearchRules& searchRules,
            const RenderCore::Metal::InputLayout& geoInputLayout)
    {
        metalContext.Bind(*_shaderProgram);

        RenderCore::Metal::BoundInputLayout inputLayout(geoInputLayout, *_shaderProgram);
        metalContext.Bind(inputLayout);

        BindConstantsAndResources(
            metalContext, parserContext, mat, 
            sysConstants, searchRules, *_shaderProgram);

        return _shaderProgram;
    }

    MaterialBinder::MaterialBinder(RenderCore::Metal::ShaderProgram& shaderProgram) : _shaderProgram(&shaderProgram) {}
    MaterialBinder::~MaterialBinder() {}

    static DrawPreviewResult DrawPreview(
        RenderCore::IThreadContext& context, 
        PreviewBuilderPimpl& builder, ShaderDiagram::Document^ doc)
    {
        using namespace ToolsRig;

        try 
        {
            MaterialVisObject visObject;
            visObject._materialBinder = std::make_shared<MaterialBinder>(builder.GetShaderProgram());
            visObject._systemConstants._lightNegativeDirection = Normalize(doc->NegativeLightDirection);
            visObject._systemConstants._lightColour = Float3(1,1,1);

                //  We need to convert the material parameters and resource bindings
                //  from the "doc" into native format in the "visObject._parameters" object.
                //  Any "string" parameter might be a texture binding
            for each (auto i in doc->PreviewMaterialState) {
                auto str = dynamic_cast<String^>(i.Value);
                if (str) {
                    visObject._parameters._bindings.SetParameter(
                            (const utf8*)clix::marshalString<clix::E_UTF8>(i.Key).c_str(),
                            clix::marshalString<clix::E_UTF8>(str));
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
            visSettings._geometryType = MaterialVisSettings::GeometryType::Plane2D;

            SceneEngine::LightingParserContext parserContext(
                *Manager::Instance->GetGlobalTechniqueContext());

            bool result = ToolsRig::MaterialVisLayer::Draw(
                context, parserContext, 
                visSettings, VisEnvSettings(), visObject);

            if (result) return DrawPreviewResult_Success;
            if (parserContext.HasPendingAssets()) return DrawPreviewResult_Pending;
        } 
        catch (::Assets::Exceptions::InvalidAsset&) { return DrawPreviewResult_Error; }
        catch (::Assets::Exceptions::PendingAsset&) { return DrawPreviewResult_Pending; }
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
        } catch (const ::Assets::Exceptions::PendingAsset&) {
            return nullptr; // still pending
        }
        catch (...) {}

        using namespace RenderCore;
        auto context = Manager::Instance->GetDevice()->GetImmediateContext();

        auto& uploads = RenderCore::Assets::Services::GetBufferUploads();
        auto targetTexture = uploads.Transaction_Immediate(
            BufferUploads::CreateDesc(
                BufferUploads::BindFlag::RenderTarget,
                0, BufferUploads::GPUAccess::Write,
                BufferUploads::TextureDesc::Plain2D(
                    width, height, 
                    Metal::NativeFormat::R8G8B8A8_UNORM_SRGB),
                "PreviewBuilderTarget"));

        Metal::RenderTargetView targetRTV(targetTexture->GetUnderlying());

        auto metalContext = Metal::DeviceContext::Get(*context);
        float clearColor[] = { 0.05f, 0.05f, 0.2f, 1.f };
        metalContext->Clear(targetRTV, clearColor);

        metalContext->Bind(RenderCore::MakeResourceList(targetRTV), nullptr);
        metalContext->Bind(Metal::ViewportDesc(0.f, 0.f, float(width), float(height), 0.f, 1.f));

            ////////////

        auto result = DrawPreview(*context, *_pimpl, doc);
        if (result == DrawPreviewResult_Error) {
            return GenerateErrorBitmap(_pimpl->_errorString.c_str(), size);
        } else if (result == DrawPreviewResult_Pending) {
            return nullptr;
        }

            ////////////

        auto readback = uploads.Resource_ReadBack(*targetTexture);
        if (readback && readback->GetDataSize()) {
            using System::Drawing::Bitmap;
            using namespace System::Drawing;
            Bitmap^ newBitmap = gcnew Bitmap(width, height, Imaging::PixelFormat::Format32bppArgb);
            auto data = newBitmap->LockBits(
                System::Drawing::Rectangle(0, 0, width, height), 
                Imaging::ImageLockMode::WriteOnly, 
                Imaging::PixelFormat::Format32bppArgb);
            try
            {
                    // we have to flip ABGR -> ARGB!
                for (int y=0; y<height; ++y) {
                    void* sourcePtr = PtrAdd(readback->GetData(), y * readback->GetPitches()._rowPitch);
                    System::IntPtr destinationPtr = data->Scan0 + y * width * sizeof(unsigned);
                    for (int x=0; x<width; ++x) {
                        ((unsigned*)(void*)destinationPtr)[x] = 
                            (RenderCore::ARGBtoABGR(((unsigned*)sourcePtr)[x]) & 0x00ffffff) | 0xff000000;
                    }
                }
            }
            finally
            {
                newBitmap->UnlockBits(data);
            }
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
            using namespace RenderCore;
            _pimpl->_vertexShader = std::make_unique<CompiledShaderByteCode>(nativeShaderText.c_str(), "VertexShaderEntry", VS_DefShaderModel, "SHADER_NODE_EDITOR=1");
            _pimpl->_pixelShader = std::make_unique<CompiledShaderByteCode>(nativeShaderText.c_str(), "PixelShaderEntry", PS_DefShaderModel, "SHADER_NODE_EDITOR=1");
        }
        catch (::Assets::Exceptions::PendingAsset&) {}
        catch (::Assets::Exceptions::InvalidAsset&) 
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

    void Manager::Shutdown()
    {
        delete _instance;
        _instance = nullptr;
    }

    Manager::Manager()
    {
        _pimpl = new ManagerPimpl;
        
        ConsoleRig::StartupConfig cfg;
        cfg._applicationName = clix::marshalString<clix::E_UTF8>(System::Windows::Forms::Application::ProductName);
        _pimpl->_services = std::make_unique<ConsoleRig::GlobalServices>(cfg);

        _pimpl->_device = RenderCore::CreateDevice();
        _pimpl->_assetServices = std::make_unique<::Assets::Services>(::Assets::Services::Flags::RecordInvalidAssets);
        _pimpl->_renderAssetsServices = std::make_unique<RenderCore::Assets::Services>(_pimpl->_device.get());
        _pimpl->_globalTechniqueContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
    }

    Manager::~Manager()
    {
        System::GC::Collect();
        System::GC::WaitForPendingFinalizers();
        // DelayedDeleteQueue::FlushQueue();

        RenderCore::Techniques::ResourceBoxes_Shutdown();
        // RenderOverlays::CleanupFontSystem();
        _pimpl->_assetServices->GetAssetSets().Clear();
        Assets::Dependencies_Shutdown();
        _pimpl->_globalTechniqueContext.reset();
        _pimpl->_renderAssetsServices.reset();
        _pimpl->_assetServices.reset();
        _pimpl->_device.reset();
        _pimpl->_services.reset();
        delete _pimpl;
        TerminateFileSystemMonitoring();
    }


}

