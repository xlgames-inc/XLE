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
// #include "../GUILayer/NativeEngineDevice.h"
#include "../GUILayer/CLIXAutoPtr.h"
#include "../GUILayer/DelayedDeleteQueue.h"
#include "../ToolsRig/MaterialVisualisation.h"
#include "../ToolsRig/VisualisationUtils.h"

#include "../../SceneEngine/LightingParserContext.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/ShaderService.h" // for RenderCore::ShaderService::IShaderSource
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Assets/ModelRunTime.h"   // for aligning preview camera to model
#include "../../RenderCore/Assets/ShaderVariationSet.h"
#include "../../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../../RenderCore/MinimalShaderSource.h"
#include "../../RenderCore/BufferView.h"

// #include "../../BufferUploads/IBufferUploads.h"
// #include "../../BufferUploads/DataPacket.h"
// #include "../../BufferUploads/ResourceLocator.h"

#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/IAssetCompiler.h"
#include "../../Assets/Assets.h"

// #include "../../ConsoleRig/ResourceBox.h"
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

    class MaterialBinder : public ToolsRig::IMaterialBinder
    {
    public:
        virtual bool Apply(
            RenderCore::Metal::DeviceContext& metalContext,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex,
            const RenderCore::Techniques::Material& mat,
            const SystemConstants& sysConstants,
            const ::Assets::DirectorySearchRules& searchRules,
            const RenderCore::InputLayout& geoInputLayout);
        
        MaterialBinder(
            RenderCore::ShaderService::IShaderSource& shaderSource, 
            const std::string& shaderText,
            const RenderCore::Techniques::PredefinedCBLayout& cbLayout);
        ~MaterialBinder();
    private:
        std::string _shaderText;
        RenderCore::ShaderService::IShaderSource* _shaderSource;
        const RenderCore::Techniques::PredefinedCBLayout* _cbLayout;

		class CachedShader
		{
		public:
			RenderCore::Metal::ShaderProgram _shaderProgram;
			RenderCore::Metal::BoundInputLayout _inputLayout;
			RenderCore::Metal::BoundUniforms _uniforms;

			CachedShader& operator=(const CachedShader&) = delete;
			CachedShader(const CachedShader&) = delete;

			CachedShader& operator=(CachedShader&& moveFrom) never_throws
			{
				_shaderProgram = std::move(moveFrom._shaderProgram);
				_inputLayout = std::move(moveFrom._inputLayout);
				_uniforms = std::move(moveFrom._uniforms);
				return *this;
			}

			CachedShader(CachedShader&& moveFrom) never_throws
			: _shaderProgram(std::move(moveFrom._shaderProgram))
			, _inputLayout(std::move(moveFrom._inputLayout))
			, _uniforms(std::move(moveFrom._uniforms))
			{}

			CachedShader() {}
			~CachedShader() {}
		};

		std::vector<std::pair<uint64, CachedShader>> _cachedShaders;
    };

    bool MaterialBinder::Apply(
        RenderCore::Metal::DeviceContext& metalContext,
        RenderCore::Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex,
        const RenderCore::Techniques::Material& mat,
        const SystemConstants& sysConstants,
        const ::Assets::DirectorySearchRules& searchRules,
        const RenderCore::InputLayout& geoInputLayout)
    {
        // We need to build a shader program based on the input shader texture
        // and the material parameters given. We will need to stall waiting for
        // this shader to compile... Because we compile it on demand, this is the
        // only way. So, ideally we should use a shader service that isn't
        // asynchronous.

        auto matParams = RenderCore::Assets::TechParams_SetResHas(mat._matParams, mat._bindings, searchRules);
        auto geoParams = RenderCore::Assets::TechParams_SetGeo(geoInputLayout);

        const ParameterBox* state[] = {
            &geoParams, 
            &parserContext.GetTechniqueContext()._globalEnvironmentState,
            &parserContext.GetTechniqueContext()._runtimeState, 
            &matParams
        };

		uint64 hash = 0;
		for (unsigned c=0; c<dimof(state); ++c)
			hash = HashCombine(hash, HashCombine(state[c]->GetHash(), state[c]->GetParameterNamesHash()));

		const CachedShader* shader = nullptr;
		auto i = LowerBound(_cachedShaders, hash);
		if (i != _cachedShaders.end() && i->first == hash)
			shader = &i->second;

		if (shader == nullptr) {
			std::vector<std::pair<const utf8*, std::string>> defines;
			for (unsigned c=0; c<dimof(state); ++c)
				BuildStringTable(defines, *state[c]);
			std::string definesTable = FlattenStringTable(defines);

			auto vsCompileMarker = _shaderSource->CompileFromMemory(
				_shaderText.c_str(), "vs_main", VS_DefShaderModel, definesTable.c_str());
			auto psCompileMarker = _shaderSource->CompileFromMemory(
				_shaderText.c_str(), "ps_main", PS_DefShaderModel, definesTable.c_str());

			vsCompileMarker->StallWhilePending();
			psCompileMarker->StallWhilePending();

			using namespace RenderCore;
			CompiledShaderByteCode vsCode(vsCompileMarker->GetArtifacts()[0].second->GetBlob(), nullptr, "InMemoryShader");
			CompiledShaderByteCode psCode(psCompileMarker->GetArtifacts()[0].second->GetBlob(), nullptr, "InMemoryShader");
        
			CachedShader cs;
			cs._shaderProgram = Metal::ShaderProgram(Metal::GetObjectFactory(), vsCode, psCode);
			cs._inputLayout = Metal::BoundInputLayout(geoInputLayout, cs._shaderProgram);
			cs._uniforms = Metal::BoundUniforms(cs._shaderProgram);

			i = _cachedShaders.insert(i, std::make_pair(hash, std::move(cs)));
			shader = &i->second;
		}

		// RenderCore::VertexBufferView vbvs[] = {};
		shader->_inputLayout.Apply(metalContext, {});
		metalContext.Bind(shader->_shaderProgram);

        // We need to build a PredefinedCBLayout, also. Normally, this is built from the 
        // node graph directly. We have 2 choices:
        //      1) build from the node graph (using a similiar path to what happens in non-preview modes)
        //      2) use reflection to get the cb details from compiled shader code, and build from there.
        // Method 1 is a little more difficult to implement, but should provide more robust results in
        // general (because it will match the technique config case better). So let's do that...

        BindConstantsAndResources(
            metalContext, parserContext, mat, 
            sysConstants, searchRules, 
			shader->_uniforms, *_cbLayout);

        return true;
    }

    MaterialBinder::MaterialBinder(
        RenderCore::ShaderService::IShaderSource& shaderSource, 
        const std::string& shaderText,
        const RenderCore::Techniques::PredefinedCBLayout& cbLayout) 
    : _shaderSource(&shaderSource), _shaderText(shaderText), _cbLayout(&cbLayout) {}
    MaterialBinder::~MaterialBinder() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	enum DrawPreviewResult
    {
        DrawPreviewResult_Error,
        DrawPreviewResult_Pending,
        DrawPreviewResult_Success
    };

    static std::pair<DrawPreviewResult, std::string> DrawPreview(
        RenderCore::IThreadContext& context,
        const RenderCore::Techniques::TechniqueContext& techContext,
		const std::shared_ptr<MaterialBinder>& binder,
        PreviewGeometry geometry,
        ShaderPatcherLayer::NodeGraphContext^ doc)
    {
        using namespace ToolsRig;

        try 
        {
            MaterialVisObject visObject;
            visObject._materialBinder = binder;
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
                    auto resolveAttempt = rawMat->GetAsset().TryResolve(visObject._parameters, searchRules);
					assert(resolveAttempt == ::Assets::AssetState::Ready);

                    visObject._searchRules.AddSearchDirectoryFromFilename(MakeStringSection(nativeName));
                }
            }
            visObject._parameters._matParams.SetParameter(u("SHADER_NODE_EDITOR"), "1");

            for each(auto i in doc->ShaderParameters)
                visObject._parameters._matParams.SetParameter(
                    (const utf8*)clix::marshalString<clix::E_UTF8>(i.Key).c_str(),
                    clix::marshalString<clix::E_UTF8>(i.Value));

            MaterialVisSettings visSettings;
            visSettings._camera = std::make_shared<VisCameraSettings>();
            visSettings._camera->_position = Float3(-4, 0, 0);  // note that the position of the camera affects the apparent color of normals when previewing world space normals

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
                auto model = ::Assets::MakeAsset<RenderCore::Assets::ModelScaffold>(
                    visObject._previewModelFile.c_str());
                model->StallWhilePending();
                *visSettings._camera = ToolsRig::AlignCameraToBoundingBox(
                    visSettings._camera->_verticalFieldOfView, 
                    model->Actualize()->GetStaticBoundingBox());
            }

            SceneEngine::LightingParserContext parserContext(techContext);
            bool result = ToolsRig::MaterialVisLayer::Draw(
                context, parserContext, 
                visSettings, VisEnvSettings(), visObject);
            if (result)
                return std::make_pair(DrawPreviewResult_Success, std::string());

            if (parserContext.HasPendingAssets()) return std::make_pair(DrawPreviewResult_Pending, std::string());
        }
        catch (::Assets::Exceptions::InvalidAsset& e) { return std::make_pair(DrawPreviewResult_Error, e.what()); }
        catch (::Assets::Exceptions::PendingAsset& e) { return std::make_pair(DrawPreviewResult_Pending, e.Initializer()); }

        return std::make_pair(DrawPreviewResult_Error, std::string());
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	class PreviewBuilderPimpl
    {
    public:
        std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _techContext;
        std::shared_ptr<RenderCore::IDevice> _device;
        std::string _shaderText;
        RenderCore::Techniques::PredefinedCBLayout _cbLayout;
		std::shared_ptr<MaterialBinder> _materialBinder;
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
                CreateDesc(
                    BindFlag::RenderTarget,
                    0, GPUAccess::Write,
                    TextureDesc::Plain2D(width, height, Format::R8G8B8A8_UNORM_SRGB),
                    "PreviewBuilderTarget"));

        auto depthBuffer = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::DepthStencil,
                0, GPUAccess::Write,
                TextureDesc::Plain2D(width, height, Format::D24_UNORM_S8_UINT),
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
        metalContext->Clear(dsv, Metal::DeviceContext::ClearFilter::Depth|Metal::DeviceContext::ClearFilter::Stencil, 1.f, 0x0);
        RenderCore::ResourceList<Metal::RenderTargetView, maxTargets> rtvList(
            std::initializer_list<const Metal::RenderTargetView>(rtvs, ArrayEnd(rtvs)));
        metalContext->Bind(rtvList, &dsv);

        metalContext->Bind(Metal::ViewportDesc(0.f, 0.f, float(width), float(height), 0.f, 1.f));

            ////////////

        auto result = DrawPreview(*context, *_pimpl->_techContext, _pimpl->_materialBinder, geometry, doc);
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
		_pimpl->_materialBinder = std::make_shared<MaterialBinder>(*_pimpl->_shaderSource, _pimpl->_shaderText, _pimpl->_cbLayout);
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
            RenderCore::Metal::CreateLowLevelShaderCompiler(*_pimpl->_device));
    }

    Manager::~Manager()
    {
        _pimpl->_shaderSource.reset();
        _pimpl->_globalTechniqueContext.reset();
    }

	class AttachPimpl
    {
    public:
        ConsoleRig::AttachRef<::Assets::Services> _attachRef1;
        ConsoleRig::AttachRef<RenderCore::Assets::Services> _attachRef2;
        ConsoleRig::AttachRef<RenderCore::Metal::ObjectFactory> _attachRef3;
    };

	LibraryAttachMarker::LibraryAttachMarker(GUILayer::EngineDevice^ engineDevice)
	{
		_pimpl = new AttachPimpl();

        engineDevice->GetNative().GetGlobalServices()->AttachCurrentModule();
        auto& crossModule = ConsoleRig::GlobalServices::GetCrossModule();
        _pimpl->_attachRef1 = crossModule.Attach<::Assets::Services>();
        _pimpl->_attachRef2 = crossModule.Attach<RenderCore::Assets::Services>();
        _pimpl->_attachRef3 = crossModule.Attach<RenderCore::Metal::ObjectFactory>();
	}

	LibraryAttachMarker::~LibraryAttachMarker()
	{
		if (_pimpl) {
            System::GC::Collect();
            System::GC::WaitForPendingFinalizers();
            GUILayer::DelayedDeleteQueue::FlushQueue();
        
            ConsoleRig::ResourceBoxes_Shutdown();
            // Assets::Dependencies_Shutdown();     (can't do this properly here!)

			_pimpl->_attachRef3.Detach();
			_pimpl->_attachRef2.Detach();
			_pimpl->_attachRef1.Detach();
			ConsoleRig::GlobalServices::GetInstance().DetachCurrentModule();
			delete _pimpl; _pimpl = nullptr;

            TerminateFileSystemMonitoring();
		}
	}

	LibraryAttachMarker::!LibraryAttachMarker() 
	{
		this->~LibraryAttachMarker();
	}


}

