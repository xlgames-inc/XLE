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

#include "../../RenderCore/IDevice_Forward.h"
#include "../../RenderCore/ShaderService.h" // for RenderCore::ShaderService::IShaderSource
#include "../../RenderCore/Techniques/Techniques.h"
#include "../GUILayer/NativeEngineDevice.h"
#include "../GUILayer/CLIXAutoPtr.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/Resource.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/MinimalShaderSource.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../Math/Transformations.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/Conversion.h"
#include <memory>

using namespace System::ComponentModel::Composition;

namespace PreviewRender
{
    [Export(IManager::typeid)]
    [Export(Manager::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class Manager : IManager
    {
    public:
        virtual IPreviewBuilder^ CreatePreviewBuilder(System::String^ shaderText);

        void                    RotateLightDirection(ShaderDiagram::Document^ doc, System::Drawing::PointF rotationAmount);
        RenderCore::IDevice*    GetDevice();
        auto                    GetGlobalTechniqueContext() -> RenderCore::Techniques::TechniqueContext*;
        void                    Update();

        [ImportingConstructor]
        Manager(GUILayer::EngineDevice^ engineDevice);
    private:
        clix::auto_ptr<ManagerPimpl> _pimpl;

        ~Manager();
    };

    class ManagerPimpl
    {
    public:
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _globalTechniqueContext;
        // std::unique_ptr<RenderCore::Assets::Services> _renderAssetsServices;
        std::shared_ptr<RenderCore::IDevice> _device;
        // std::unique_ptr<::Assets::Services> _assetServices;
        // std::unique_ptr<ConsoleRig::GlobalServices> _services;
        std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;
        BufferUploads::IManager* _uploads;
    };

    public ref class PreviewBuilder : IPreviewBuilder
    {
    public:
        virtual System::Drawing::Bitmap^ Build(
            ShaderDiagram::Document^ doc, Size^ size, PreviewGeometry geometry);

        PreviewBuilder(
            std::shared_ptr<RenderCore::ShaderService::IShaderSource> shaderSource, 
            std::shared_ptr<RenderCore::Techniques::TechniqueContext> techContext,
            std::shared_ptr<RenderCore::IDevice> device,
            BufferUploads::IManager& uploads,
            System::String^ shaderText);
        ~PreviewBuilder();
    private:
        PreviewBuilderPimpl*        _pimpl;

        System::Drawing::Bitmap^    GenerateErrorBitmap(const char str[], Size^ size);
    };

    class PreviewBuilderPimpl
    {
    public:
        std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _techContext;
        std::shared_ptr<RenderCore::IDevice> _device;
        BufferUploads::IManager* _uploads;
        std::string _shaderText;
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
        virtual bool Apply(
            RenderCore::Metal::DeviceContext& metalContext,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex,
            const RenderCore::Assets::ResolvedMaterial& mat,
            const SystemConstants& sysConstants,
            const ::Assets::DirectorySearchRules& searchRules,
            const RenderCore::Metal::InputLayout& geoInputLayout);
        
        MaterialBinder(RenderCore::ShaderService::IShaderSource& shaderSource, const std::string& shaderText);
        ~MaterialBinder();
    private:
        std::string _shaderText;
        RenderCore::ShaderService::IShaderSource* _shaderSource;
    };

    bool MaterialBinder::Apply(
        RenderCore::Metal::DeviceContext& metalContext,
        RenderCore::Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex,
        const RenderCore::Assets::ResolvedMaterial& mat,
        const SystemConstants& sysConstants,
        const ::Assets::DirectorySearchRules& searchRules,
        const RenderCore::Metal::InputLayout& geoInputLayout)
    {
        // We need to build a shader program based on the input shader texture
        // and the material parameters given. We will need to stall waiting for
        // this shader to compile... Because we compile it on demand, this is the
        // only way. So, ideally we should use a shader service that isn't
        // asynchronous.

        auto matParams = RenderCore::Assets::TechParams_SetResHas(mat._matParams, mat._bindings, searchRules);
        auto geoParams = RenderCore::Techniques::TechParams_SetGeo(geoInputLayout);

        const ParameterBox* state[] = {
            &geoParams, 
            &parserContext.GetTechniqueContext()._globalEnvironmentState,
            &parserContext.GetTechniqueContext()._runtimeState, 
            &matParams
        };

        std::vector<std::pair<const utf8*, std::string>> defines;
        for (unsigned c=0; c<dimof(state); ++c)
            BuildStringTable(defines, *state[c]);
        std::string definesTable = FlattenStringTable(defines);

        auto vsCompileMarker = _shaderSource->CompileFromMemory(
            _shaderText.c_str(), "vs_main", VS_DefShaderModel, definesTable.c_str());
        auto psCompileMarker = _shaderSource->CompileFromMemory(
            _shaderText.c_str(), "ps_main", PS_DefShaderModel, definesTable.c_str());

        RenderCore::CompiledShaderByteCode vsCode(std::move(vsCompileMarker));
        RenderCore::CompiledShaderByteCode psCode(std::move(psCompileMarker));
        vsCode.StallWhilePending();
        psCode.StallWhilePending();
        
        RenderCore::Metal::ShaderProgram shaderProgram(vsCode, psCode);
        metalContext.Bind(shaderProgram);

        RenderCore::Metal::BoundInputLayout inputLayout(geoInputLayout, shaderProgram);
        metalContext.Bind(inputLayout);

        BindConstantsAndResources(
            metalContext, parserContext, mat, 
            sysConstants, searchRules, shaderProgram);

        return true;
    }

    MaterialBinder::MaterialBinder(RenderCore::ShaderService::IShaderSource& shaderSource, const std::string& shaderText) 
    : _shaderSource(&shaderSource), _shaderText(shaderText) {}
    MaterialBinder::~MaterialBinder() {}

    static std::pair<DrawPreviewResult, std::string> DrawPreview(
        RenderCore::IThreadContext& context,
        PreviewBuilderPimpl& builder, 
        PreviewGeometry geometry,
        ShaderDiagram::Document^ doc)
    {
        using namespace ToolsRig;

        try 
        {
            MaterialVisObject visObject;
            visObject._materialBinder = std::make_shared<MaterialBinder>(*builder._shaderSource, builder._shaderText);
            visObject._systemConstants._lightColour = Float3(1,1,1);
            visObject._parameters._matParams.SetParameter(u("SHADER_NODE_EDITOR"), "1");

            if (doc != nullptr) {
                visObject._systemConstants._lightNegativeDirection = Normalize(doc->NegativeLightDirection);

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
            }

            MaterialVisSettings visSettings;
            visSettings._camera = std::make_shared<VisCameraSettings>();
            visSettings._camera->_position = Float3(-5, 0, 0);

            // Select the geometry type to use.
            // In the "chart" mode, we are just going to run a pixel shader for every
            // output pixel, so we want to use a pretransformed quad covering the viewport
            switch (geometry) {
            case PreviewGeometry::Chart:
                visSettings._geometryType = MaterialVisSettings::GeometryType::Plane2D;
                visObject._parameters._matParams.SetParameter(u("GEO_PRETRANSFORMED"), "1");
                break;

            case PreviewGeometry::Box:
                visSettings._geometryType = MaterialVisSettings::GeometryType::Cube;
                break;

            default:
            case PreviewGeometry::Sphere:
                visSettings._geometryType = MaterialVisSettings::GeometryType::Sphere;
                break;

            case PreviewGeometry::Model:
                visSettings._geometryType = MaterialVisSettings::GeometryType::Model;
                break;
            };

            SceneEngine::LightingParserContext parserContext(*builder._techContext);
            bool result = ToolsRig::MaterialVisLayer::Draw(
                context, parserContext, 
                visSettings, VisEnvSettings(), visObject);
            if (result)
                return std::make_pair(DrawPreviewResult_Success, std::string());

            if (parserContext.HasPendingAssets()) return std::make_pair(DrawPreviewResult_Pending, std::string());
        }
        catch (::Assets::Exceptions::InvalidAsset&) { return std::make_pair(DrawPreviewResult_Error, std::string()); }
        catch (::Assets::Exceptions::PendingAsset&) { return std::make_pair(DrawPreviewResult_Pending, std::string()); }

        return std::make_pair(DrawPreviewResult_Error, std::string());
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

    System::Drawing::Bitmap^    PreviewBuilder::Build(
        ShaderDiagram::Document^ doc, Size^ size, PreviewGeometry geometry)
    {
        using namespace RenderCore;

        const int width = std::max(0, int(size->Width));
        const int height = std::max(0, int(size->Height));

        auto& uploads = *_pimpl->_uploads;
        auto targetTexture = uploads.Transaction_Immediate(
            BufferUploads::CreateDesc(
                BufferUploads::BindFlag::RenderTarget,
                0, BufferUploads::GPUAccess::Write,
                BufferUploads::TextureDesc::Plain2D(
                    width, height, 
                    Metal::NativeFormat::R8G8B8A8_UNORM_SRGB),
                "PreviewBuilderTarget"));

        Metal::RenderTargetView targetRTV(targetTexture->GetUnderlying());

        auto context = _pimpl->_device->GetImmediateContext();
        auto metalContext = Metal::DeviceContext::Get(*context);
        float clearColor[] = { 0.05f, 0.05f, 0.2f, 1.f };
        metalContext->Clear(targetRTV, clearColor);

        metalContext->Bind(RenderCore::MakeResourceList(targetRTV), nullptr);
        metalContext->Bind(Metal::ViewportDesc(0.f, 0.f, float(width), float(height), 0.f, 1.f));

            ////////////

        auto result = DrawPreview(*context, *_pimpl, geometry, doc);
        if (result.first == DrawPreviewResult_Error) {
            return GenerateErrorBitmap(result.second.c_str(), size);
        } else if (result.first == DrawPreviewResult_Pending) {
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

    PreviewBuilder::PreviewBuilder(
        std::shared_ptr<RenderCore::ShaderService::IShaderSource> shaderSource, 
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> techContext,
        std::shared_ptr<RenderCore::IDevice> device,
        BufferUploads::IManager& uploads,
        System::String^ shaderText)
    {
        _pimpl = new PreviewBuilderPimpl();
        _pimpl->_shaderText = clix::marshalString<clix::E_UTF8>(shaderText);
        _pimpl->_shaderSource = std::move(shaderSource);
        _pimpl->_techContext = std::move(techContext);
        _pimpl->_device = std::move(device);
        _pimpl->_uploads = &uploads;
    }

    PreviewBuilder::~PreviewBuilder()
    {
        delete _pimpl;
    }

    IPreviewBuilder^    Manager::CreatePreviewBuilder(System::String^ shaderText)
    {
        return gcnew PreviewBuilder(
            _pimpl->_shaderSource, _pimpl->_globalTechniqueContext, 
            _pimpl->_device, *_pimpl->_uploads,
            shaderText);
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

    void Manager::Update()
    {
        // ::Assets::Services::GetAsyncMan().Update();
        // RenderCore::Assets::Services::GetBufferUploads().Update(
        //     *_pimpl->_device->GetImmediateContext());
    }

    RenderCore::Techniques::TechniqueContext* Manager::GetGlobalTechniqueContext()
    {
        return _pimpl->_globalTechniqueContext.get();
    }

    static ConsoleRig::AttachRef<ConsoleRig::GlobalServices> s_attachRef;
    static ConsoleRig::AttachRef<RenderCore::Metal::ObjectFactory> s_attachRef2;
    static ConsoleRig::AttachRef<RenderCore::Assets::Services> s_attachRef3;

    Manager::Manager(GUILayer::EngineDevice^ engineDevice)
    {
        s_attachRef = engineDevice->GetNative().GetGlobalServices()->Attach();
        
        _pimpl.reset(new ManagerPimpl());
        _pimpl->_device = engineDevice->GetNative().GetRenderDevice();
        _pimpl->_uploads = &engineDevice->GetNative().GetRenderAssetServices()->GetBufferUploadsInstance();

        s_attachRef2 = 
            ConsoleRig::GlobalServices::GetCrossModule().Attach<RenderCore::Metal::ObjectFactory>();

        s_attachRef3 = 
            ConsoleRig::GlobalServices::GetCrossModule().Attach<RenderCore::Assets::Services>();

        // ConsoleRig::StartupConfig cfg;
        // cfg._applicationName = clix::marshalString<clix::E_UTF8>(System::Windows::Forms::Application::ProductName);
        // _pimpl->_services = std::make_unique<ConsoleRig::GlobalServices>(cfg);

        // _pimpl->_device = RenderCore::CreateDevice();
        // _pimpl->_assetServices = std::make_unique<::Assets::Services>(::Assets::Services::Flags::RecordInvalidAssets);
        // _pimpl->_renderAssetsServices = std::make_unique<RenderCore::Assets::Services>(_pimpl->_device.get());
        _pimpl->_globalTechniqueContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
        _pimpl->_shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(
            RenderCore::Metal::CreateLowLevelShaderCompiler());
    }

    Manager::~Manager()
    {
        s_attachRef3.Detach();
        s_attachRef2.Detach();
        s_attachRef.Detach();

        _pimpl->_shaderSource.reset();
        _pimpl->_globalTechniqueContext.reset();

        // System::GC::Collect();
        // System::GC::WaitForPendingFinalizers();
        // // DelayedDeleteQueue::FlushQueue();
        // 
        // RenderCore::Techniques::ResourceBoxes_Shutdown();
        // // RenderOverlays::CleanupFontSystem();
        // _pimpl->_assetServices->GetAssetSets().Clear();
        // Assets::Dependencies_Shutdown();
        // _pimpl->_globalTechniqueContext.reset();
        // _pimpl->_renderAssetsServices.reset();
        // _pimpl->_assetServices.reset();
        // _pimpl->_device.reset();
        // _pimpl->_services.reset();
        // delete _pimpl;
        // TerminateFileSystemMonitoring();
    }


}

