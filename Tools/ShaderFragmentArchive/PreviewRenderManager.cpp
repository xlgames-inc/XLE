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
#include "../GUILayer/NativeEngineDevice.h"
#include "../GUILayer/CLIXAutoPtr.h"
#include "../ToolsRig/MaterialVisualisation.h"
#include "../ToolsRig/VisualisationUtils.h"

#include "../../SceneEngine/LightingParserContext.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/ShaderService.h" // for RenderCore::ShaderService::IShaderSource
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Assets/ModelRunTime.h"   // for aligning preview camera to model
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../../RenderCore/MinimalShaderSource.h"

#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"

#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/ConfigFileContainer.h"

#include "../../Utility/PtrUtils.h"

#include <memory>

using namespace System::ComponentModel::Composition;

namespace ShaderPatcherLayer
{
	using System::Drawing::Size;

	class ManagerPimpl
    {
    public:
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _globalTechniqueContext;
        std::shared_ptr<RenderCore::IDevice> _device;
        std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;
    };

    [Export(IManager::typeid)]
    [Export(Manager::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class Manager : IManager
    {
    public:
        virtual IPreviewBuilder^ CreatePreviewBuilder(IManager::ShaderText^ shaderText);

        Manager();
    private:
        clix::auto_ptr<ManagerPimpl> _pimpl;

        ~Manager();
    };

	class PreviewBuilderPimpl
    {
    public:
        std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _techContext;
        std::shared_ptr<RenderCore::IDevice> _device;
        std::string _shaderText;
        RenderCore::Techniques::PredefinedCBLayout _cbLayout;
    };

    public ref class PreviewBuilder : IPreviewBuilder
    {
    public:
        virtual System::Drawing::Bitmap^ Build(
            NodeGraphContext^ doc, Size^ size, PreviewGeometry geometry, unsigned targetToVisualize);

        PreviewBuilder(
            std::shared_ptr<RenderCore::ShaderService::IShaderSource> shaderSource, 
            std::shared_ptr<RenderCore::Techniques::TechniqueContext> techContext,
            std::shared_ptr<RenderCore::IDevice> device,
            System::String^ shaderText, System::String^ cbLayout);
        ~PreviewBuilder();
    private:
        PreviewBuilderPimpl*        _pimpl;

        System::Drawing::Bitmap^    GenerateErrorBitmap(const char str[], Size^ size);
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
        
        MaterialBinder(
            RenderCore::ShaderService::IShaderSource& shaderSource, 
            const std::string& shaderText,
            const RenderCore::Techniques::PredefinedCBLayout& cbLayout);
        ~MaterialBinder();
    private:
        std::string _shaderText;
        RenderCore::ShaderService::IShaderSource* _shaderSource;
        const RenderCore::Techniques::PredefinedCBLayout* _cbLayout;
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

        // We need to build a PredefinedCBLayout, also. Normally, this is built from the 
        // node graph directly. We have 2 choices:
        //      1) build from the node graph (using a similiar path to what happens in non-preview modes)
        //      2) use reflection to get the cb details from compiled shader code, and build from there.
        // Method 1 is a little more difficult to implement, but should provide more robust results in
        // general (because it will match the technique config case better). So let's do that...

        BindConstantsAndResources(
            metalContext, parserContext, mat, 
            sysConstants, searchRules, shaderProgram, *_cbLayout);

        return true;
    }

    MaterialBinder::MaterialBinder(
        RenderCore::ShaderService::IShaderSource& shaderSource, 
        const std::string& shaderText,
        const RenderCore::Techniques::PredefinedCBLayout& cbLayout) 
    : _shaderSource(&shaderSource), _shaderText(shaderText), _cbLayout(&cbLayout) {}
    MaterialBinder::~MaterialBinder() {}

    static std::pair<DrawPreviewResult, std::string> DrawPreview(
        RenderCore::IThreadContext& context,
        PreviewBuilderPimpl& builder, 
        PreviewGeometry geometry,
        ShaderPatcherLayer::NodeGraphContext^ doc)
    {
        using namespace ToolsRig;

        try 
        {
            MaterialVisObject visObject;
            visObject._materialBinder = std::make_shared<MaterialBinder>(*builder._shaderSource, builder._shaderText, builder._cbLayout);
            visObject._systemConstants._lightColour = Float3(1,1,1);
            visObject._previewModelFile = clix::marshalString<clix::E_UTF8>(doc->PreviewModelFile);
            visObject._searchRules = Assets::DefaultDirectorySearchRules(MakeStringSection(visObject._previewModelFile));

            // Our default material settings come from the "Document" object. This
            // give us our starting material and shader properties.
            if (doc != nullptr && !String::IsNullOrEmpty(doc->DefaultsMaterial)) {
                auto split = doc->DefaultsMaterial->Split(';');
                for each(auto s in split) {
                    auto nativeName = clix::marshalString<clix::E_UTF8>(s);
                    auto rawMat = RenderCore::Assets::RawMaterial::GetDivergentAsset(nativeName.c_str());
                    // auto rawMat = GUILayer::RawMaterial::Get(s)->GetUnderlying();
                    if (!rawMat) continue;

                    auto searchRules = Assets::DefaultDirectorySearchRules(MakeStringSection(nativeName));
                    rawMat->GetAsset()._asset.Resolve(visObject._parameters, searchRules);
                }
            }
            visObject._parameters._matParams.SetParameter(u("SHADER_NODE_EDITOR"), "1");

            for each(auto i in doc->ShaderParameters)
                visObject._parameters._matParams.SetParameter(
                    (const utf8*)clix::marshalString<clix::E_UTF8>(i.Key).c_str(),
                    clix::marshalString<clix::E_UTF8>(i.Value));

            MaterialVisSettings visSettings;
            visSettings._camera = std::make_shared<VisCameraSettings>();
            visSettings._camera->_position = Float3(-5, 0, 0);

            // Select the geometry type to use.
            // In the "chart" mode, we are just going to run a pixel shader for every
            // output pixel, so we want to use a pretransformed quad covering the viewport
            switch (geometry) {
            case PreviewGeometry::Plane2D:
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

            // Align the camera if we're drawing with a model...
            if (geometry == PreviewGeometry::Model && !visObject._previewModelFile.empty()) {
                const auto& model = ::Assets::GetAssetComp<RenderCore::Assets::ModelScaffold>(
                    visObject._previewModelFile.c_str());
                model.StallAndResolve();
                *visSettings._camera = ToolsRig::AlignCameraToBoundingBox(
                    visSettings._camera->_verticalFieldOfView, 
                    model.GetStaticBoundingBox());
            }

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

    System::Drawing::Bitmap^ PreviewBuilder::Build(NodeGraphContext^ doc, Size^ size, PreviewGeometry geometry, uint targetToVisualize)
    {
        using namespace RenderCore;

        const int width = std::max(0, int(size->Width));
        const int height = std::max(0, int(size->Height));

        const unsigned maxTargets = 4;
        targetToVisualize = std::min(targetToVisualize, maxTargets-1);

        auto& uploads = RenderCore::Assets::Services::GetBufferUploads();
        intrusive_ptr<BufferUploads::ResourceLocator> targets[maxTargets];
        for (unsigned c=0; c<(targetToVisualize+1); ++c)
            targets[c] = uploads.Transaction_Immediate(
                BufferUploads::CreateDesc(
                    BufferUploads::BindFlag::RenderTarget,
                    0, BufferUploads::GPUAccess::Write,
                    BufferUploads::TextureDesc::Plain2D(width, height, Metal::NativeFormat::R8G8B8A8_UNORM_SRGB),
                    "PreviewBuilderTarget"));

        auto depthBuffer = uploads.Transaction_Immediate(
            BufferUploads::CreateDesc(
                BufferUploads::BindFlag::DepthStencil,
                0, BufferUploads::GPUAccess::Write,
                BufferUploads::TextureDesc::Plain2D(width, height, Metal::NativeFormat::D24_UNORM_S8_UINT),
                "PreviewBuilderDepthBuffer"));

        auto context = _pimpl->_device->GetImmediateContext();
        auto metalContext = Metal::DeviceContext::Get(*context);
        float clearColor[] = { 0.05f, 0.05f, 0.2f, 1.f };

        Metal::RenderTargetView rtvs[maxTargets];
        for (unsigned c=0; c<(targetToVisualize+1); ++c) {
            rtvs[c] = Metal::RenderTargetView(targets[c]->GetUnderlying());
            metalContext->Clear(rtvs[c], clearColor);
        }

        Metal::DepthStencilView dsv(depthBuffer->GetUnderlying());
        metalContext->Clear(dsv, 1.f, 0x0);
        RenderCore::ResourceList<Metal::RenderTargetView, maxTargets> rtvList(
            std::initializer_list<const Metal::RenderTargetView>(rtvs, ArrayEnd(rtvs)));
        metalContext->Bind(rtvList, &dsv);

        metalContext->Bind(Metal::ViewportDesc(0.f, 0.f, float(width), float(height), 0.f, 1.f));

            ////////////

        auto result = DrawPreview(*context, *_pimpl, geometry, doc);
        if (result.first == DrawPreviewResult_Error) {
            return GenerateErrorBitmap(result.second.c_str(), size);
        } else if (result.first == DrawPreviewResult_Pending) {
            return nullptr;
        }

            ////////////

        auto readback = uploads.Resource_ReadBack(*targets[targetToVisualize]);
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
        System::String^ shaderText, System::String^ cbLayout)
    {
        _pimpl = new PreviewBuilderPimpl();
        _pimpl->_shaderText = clix::marshalString<clix::E_UTF8>(shaderText);
        auto nativeCBLayout = clix::marshalString<clix::E_UTF8>(cbLayout);
        _pimpl->_cbLayout = RenderCore::Techniques::PredefinedCBLayout(MakeStringSection(nativeCBLayout), true);
        _pimpl->_shaderSource = std::move(shaderSource);
        _pimpl->_techContext = std::move(techContext);
        _pimpl->_device = std::move(device);
    }

    PreviewBuilder::~PreviewBuilder()
    {
        delete _pimpl;
    }

    IPreviewBuilder^    Manager::CreatePreviewBuilder(IManager::ShaderText^ shaderText)
    {
        return gcnew PreviewBuilder(
            _pimpl->_shaderSource, _pimpl->_globalTechniqueContext, 
            _pimpl->_device,
            shaderText->Item1, shaderText->Item2);
    }
    
    Manager::Manager()
    {
        _pimpl.reset(new ManagerPimpl());

		auto engineDevice = GUILayer::EngineDevice::GetInstance();
        _pimpl->_device = engineDevice->GetNative().GetRenderDevice();
        
        _pimpl->_globalTechniqueContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
        _pimpl->_shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(
            RenderCore::Metal::CreateLowLevelShaderCompiler());
    }

    Manager::~Manager()
    {
        _pimpl->_shaderSource.reset();
        _pimpl->_globalTechniqueContext.reset();
    }

	class AttachPimpl
    {
    public:
        ConsoleRig::AttachRef<ConsoleRig::GlobalServices> _attachRef;
        ConsoleRig::AttachRef<::Assets::Services> _attachRef1;
        ConsoleRig::AttachRef<RenderCore::Assets::Services> _attachRef2;
        ConsoleRig::AttachRef<RenderCore::Metal::ObjectFactory> _attachRef3;
    };

	LibraryAttachMarker::LibraryAttachMarker(GUILayer::EngineDevice^ engineDevice)
	{
		_pimpl = new AttachPimpl();

        _pimpl->_attachRef = engineDevice->GetNative().GetGlobalServices()->Attach();
        auto& crossModule = ConsoleRig::GlobalServices::GetCrossModule();
        _pimpl->_attachRef1 = crossModule.Attach<::Assets::Services>();
        _pimpl->_attachRef2 = crossModule.Attach<RenderCore::Assets::Services>();
        _pimpl->_attachRef3 = crossModule.Attach<RenderCore::Metal::ObjectFactory>();
	}

	LibraryAttachMarker::~LibraryAttachMarker()
	{
		if (_pimpl) {
			_pimpl->_attachRef3.Detach();
			_pimpl->_attachRef2.Detach();
			_pimpl->_attachRef1.Detach();
			_pimpl->_attachRef.Detach();
			delete _pimpl; _pimpl = nullptr;
		}
	}

	LibraryAttachMarker::!LibraryAttachMarker() 
	{
		this->~LibraryAttachMarker();
	}


}

