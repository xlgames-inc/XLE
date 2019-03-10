// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PreviewRenderManager.h"
#include "TypeRules.h"
#include "ShaderGenerator.h"

#include "../GUILayer/MarshalString.h"
#include "../GUILayer/NativeEngineDevice.h"
#include "../GUILayer/CLIXAutoPtr.h"

#include "../ToolsRig/MaterialVisualisation.h"
#include "../ToolsRig/VisualisationUtils.h"

#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/DrawableDelegates.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Assets/RawMaterial.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/ObjectFactory.h"
#include "../../RenderCore/MinimalShaderSource.h"
#include "../../RenderCore/IDevice.h"

#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"

#include "../../Assets/AssetServices.h"
#include "../../Assets/IArtifact.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/AssetSetManager.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include "../../Utility/PtrUtils.h"

#include <memory>
#include <sstream>

#pragma warning(disable:4505) // 'ShaderPatcherLayer::CreatePreviewMaterial': unreferenced local function has been removed

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

    [Export(IPreviewBuilder::typeid)]
    [Export(Manager::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class Manager : IPreviewBuilder
    {
    public:
		virtual System::Drawing::Bitmap^ BuildPreviewImage(
            NodeGraphMetaData^ doc, 
			NodeGraphPreviewConfiguration^ previewConfig,
			System::Drawing::Size^ size, 
            PreviewGeometry geometry, 
			unsigned targetToVisualize);

        Manager();
    private:
        clix::auto_ptr<ManagerPimpl> _pimpl;

        ~Manager();
		System::Drawing::Bitmap^    GenerateErrorBitmap(const char str[], Size^ size);
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

	static RenderCore::Techniques::Material CreatePreviewMaterial(ShaderPatcherLayer::NodeGraphMetaData^ doc, const ::Assets::DirectorySearchRules& searchRules)
	{
		RenderCore::Techniques::Material result;

        // Our default material settings come from the "Document" object. This
        // give us our starting material and shader properties.
        if (doc != nullptr && !String::IsNullOrEmpty(doc->DefaultsMaterial)) {
            auto split = doc->DefaultsMaterial->Split(';');
			std::vector<::Assets::DependentFileState> finalMatDeps;
            for each(auto s in split) {
                auto nativeName = clix::marshalString<clix::E_UTF8>(s);
				RenderCore::Assets::MergeIn_Stall(
					result,
					MakeStringSection(nativeName),
					searchRules,
					finalMatDeps);
            }
        }
        result._matParams.SetParameter(u("SHADER_NODE_EDITOR"), MakeStringSection("1"));

        for each(auto i in doc->ShaderParameters)
            result._matParams.SetParameter(
                MakeStringSection(clix::marshalString<clix::E_UTF8>(i.Key)).Cast<utf8>(),
                MakeStringSection(clix::marshalString<clix::E_UTF8>(i.Value)));

		return result;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static RenderCore::CompiledShaderByteCode MakeCompiledShaderByteCode(
		RenderCore::ShaderService::IShaderSource& shaderSource,
		StringSection<> sourceCode, StringSection<> definesTable,
		RenderCore::ShaderStage stage)
	{
		const char* entryPoint = nullptr, *shaderModel = nullptr;
		switch (stage) {
		case RenderCore::ShaderStage::Vertex:
			entryPoint = "vs_main"; shaderModel = VS_DefShaderModel;
			break;
		case RenderCore::ShaderStage::Pixel:
			entryPoint = "ps_main"; shaderModel = PS_DefShaderModel;
			break;
		default:
			break;
		}
		if (!entryPoint || !shaderModel) return {};

		auto future = shaderSource.CompileFromMemory(sourceCode, entryPoint, shaderModel, definesTable);
		auto state = future->GetAssetState();
		auto artifacts = future->GetArtifacts();
		if (state == ::Assets::AssetState::Invalid || artifacts.empty()) {
			// try to find an artifact named "log". If it doesn't exist, just drop back to the first one
			::Assets::IArtifact* logArtifact = nullptr;
			for (const auto& e:artifacts)
				if (e.first == "log") {
					logArtifact = e.second.get();
					break;
				}
			if (!logArtifact && !artifacts.empty())
				logArtifact = artifacts[0].second.get();
			Throw(::Assets::Exceptions::InvalidAsset(entryPoint, artifacts[0].second->GetDependencyValidation(), logArtifact->GetBlob()));
		}

		return RenderCore::CompiledShaderByteCode{
			artifacts[0].second->GetBlob(), artifacts[0].second->GetDependencyValidation(), artifacts[0].second->GetRequestParameters()};
	}

	class TechniqueDelegate : public RenderCore::Techniques::ITechniqueDelegate
	{
	public:
		virtual RenderCore::Metal::ShaderProgram* GetShader(
			RenderCore::Techniques::ParsingContext& context,
			StringSection<::Assets::ResChar> techniqueCfgFile,
			const ParameterBox* shaderSelectors[],
			unsigned techniqueIndex)
		{
			auto previewShader = _config->_nodeGraph->GeneratePreviewShader(
				_config->_subGraphName,
				_config->_previewNodeId,
				_config->_settings,
				_config->_variableRestrictions);

			auto shaderCode = clix::marshalString<clix::E_UTF8>(previewShader->Item1);
			std::string definesTable;

			{
				std::vector<std::pair<const utf8*, std::string>> defines;
				for (unsigned c=0; c<RenderCore::Techniques::ShaderSelectors::Source::Max; ++c)
					BuildStringTable(defines, *shaderSelectors[c]);
				std::stringstream str;
				for (auto&d:defines) {
					str << d.first;
					if (!d.second.empty())
						str << "=" << d.second;
					str << ";";
				}
				if (_pretransformedFlag) str << "GEO_PRETRANSFORMED=1;";
				definesTable = str.str();
			}

			auto vsCode = MakeCompiledShaderByteCode(*_shaderSource, MakeStringSection(shaderCode), MakeStringSection(definesTable), RenderCore::ShaderStage::Vertex);
			auto psCode = MakeCompiledShaderByteCode(*_shaderSource, MakeStringSection(shaderCode), MakeStringSection(definesTable), RenderCore::ShaderStage::Pixel);
			if (vsCode.GetStage() != RenderCore::ShaderStage::Vertex || psCode.GetStage() != RenderCore::ShaderStage::Pixel) return nullptr;

			static RenderCore::Metal::ShaderProgram result;
			result = RenderCore::Metal::ShaderProgram { RenderCore::Metal::GetObjectFactory(), vsCode, psCode };
			return &result;
		}

		TechniqueDelegate(
			const std::shared_ptr<RenderCore::ShaderService::IShaderSource>& shaderSource, 
			NodeGraphPreviewConfiguration^ config,
			bool pretransformedFlag)
		: _shaderSource(shaderSource)
		, _config(config)
		, _pretransformedFlag(pretransformedFlag)
		{}
				
	private:
		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;
		gcroot<NodeGraphPreviewConfiguration^> _config;
		bool _pretransformedFlag;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	System::Drawing::Bitmap^ Manager::BuildPreviewImage(
        NodeGraphMetaData^ doc, 
		NodeGraphPreviewConfiguration^ previewConfig,
		System::Drawing::Size^ size, 
        PreviewGeometry geometry, 
		unsigned targetToVisualize)
    {
        using namespace RenderCore;

        const int width = std::max(0, int(size->Width));
        const int height = std::max(0, int(size->Height));
		auto& context = *_pimpl->_device->GetImmediateContext();
		auto& uploads = RenderCore::Assets::Services::GetBufferUploads();

		// We have to pump some services, or assets will never complete loading/compiling
		::Assets::Services::GetAsyncMan().Update();
		::Assets::GetAssetSetManager().OnFrameBarrier();
		uploads.Update(context, false);

		auto target = _pimpl->_device->CreateResource(
			CreateDesc(
                BindFlag::RenderTarget,
                0, GPUAccess::Write,
                TextureDesc::Plain2D(width, height, Format::R8G8B8A8_UNORM_SRGB),
                "PreviewBuilderTarget"));

		RenderCore::Techniques::AttachmentPool attachmentPool;
		RenderCore::Techniques::FrameBufferPool frameBufferPool;
		attachmentPool.Bind(FrameBufferProperties{(unsigned)width, (unsigned)height, TextureSamples::Create()});

            ////////////

		auto visSettings = std::make_shared<ToolsRig::MaterialVisSettings>();
        ToolsRig::VisCameraSettings camSettings;
        camSettings._position = Float3(-1.42f, 0, 0);  // note that the position of the camera affects the apparent color of normals when previewing world space normals
		camSettings._verticalFieldOfView = 90.f;

		bool pretransformed = false;

        // Select the geometry type to use.
        // In the "chart" mode, we are just going to run a pixel shader for every
        // output pixel, so we want to use a pretransformed quad covering the viewport
        switch (geometry) {
        case PreviewGeometry::Plane2D:
        case PreviewGeometry::Chart:
            visSettings->_geometryType = ToolsRig::MaterialVisSettings::GeometryType::Plane2D;
			pretransformed = true;
            break;

        case PreviewGeometry::Box:
            visSettings->_geometryType = ToolsRig::MaterialVisSettings::GeometryType::Cube;
			camSettings._position = Float3(-2.1f, 0, 0);
            break;

        default:
        case PreviewGeometry::Sphere:
            visSettings->_geometryType = ToolsRig::MaterialVisSettings::GeometryType::Sphere;
            break;

        case PreviewGeometry::Model:
            visSettings->_geometryType = ToolsRig::MaterialVisSettings::GeometryType::Model;
            break;
        };

		visSettings->_previewModelFile = clix::marshalString<clix::E_UTF8>(doc->PreviewModelFile);
		// visSettings->_searchRules = ::Assets::DefaultDirectorySearchRules(MakeStringSection(visSettings->_previewModelFile));
		// visSettings->_parameters = CreatePreviewMaterial(doc, visSettings->_searchRules);

		auto sceneFuture = ToolsRig::MakeScene(*visSettings);
		const auto& actualScene = ToolsRig::TryActualize(*sceneFuture);
		if (!actualScene) {
			auto errorLog = ToolsRig::GetActualizationError(*sceneFuture);
			if (errorLog) {
				return GenerateErrorBitmap(errorLog.value().c_str(), size);
			} else {
				return nullptr;		// pending
			}
		}

		ToolsRig::VisEnvSettings envSettings;
		envSettings._lightingType = ToolsRig::VisEnvSettings::LightingType::Direct;
		// envSettings->_activeSetting._toneMapSettings._flags = 0;		// (disable tonemap, because it doesn't work on small targets)

		Techniques::ParsingContext parserContext { *_pimpl->_globalTechniqueContext, &attachmentPool, &frameBufferPool };
		parserContext.SetTechniqueDelegate(std::make_shared<TechniqueDelegate>(_pimpl->_shaderSource, previewConfig, pretransformed));
		// Can no longer render to multiple output targets using this path. We only get to input the single "presentation target"
		// to the lighting parser.
        auto result = DrawPreview(
			context, target, parserContext, 
			camSettings, envSettings,
			*actualScene);
        if (result.first == ToolsRig::DrawPreviewResult::Error) {
            return GenerateErrorBitmap(result.second.c_str(), size);
        } else if (result.first == ToolsRig::DrawPreviewResult::Pending) {
            return nullptr;
        }

            ////////////

		auto readback = uploads.Resource_ReadBack(BufferUploads::ResourceLocator{std::move(target)});
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

	System::Drawing::Bitmap^    Manager::GenerateErrorBitmap(const char str[], Size^ size)
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
        ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _attachRef0;
		ConsoleRig::AttachablePtr<::Assets::Services> _attachRef1;
        ConsoleRig::AttachablePtr<RenderCore::Assets::Services> _attachRef2;
        ConsoleRig::AttachablePtr<RenderCore::Metal::ObjectFactory> _attachRef3;
    };

	LibraryAttachMarker::LibraryAttachMarker(GUILayer::EngineDevice^ engineDevice)
	{
		_pimpl = new AttachPimpl();

		ConsoleRig::CrossModule::SetInstance(*engineDevice->GetNative().GetCrossModule());
		_pimpl->_attachRef0 = ConsoleRig::GetAttachablePtr<ConsoleRig::GlobalServices>();
        _pimpl->_attachRef1 = ConsoleRig::GetAttachablePtr<::Assets::Services>();
        _pimpl->_attachRef2 = ConsoleRig::GetAttachablePtr<RenderCore::Assets::Services>();
        _pimpl->_attachRef3 = ConsoleRig::GetAttachablePtr<RenderCore::Metal::ObjectFactory>();
	}

	LibraryAttachMarker::~LibraryAttachMarker()
	{
		if (_pimpl) {
            System::GC::Collect();
            System::GC::WaitForPendingFinalizers();
            GUILayer::DelayedDeleteQueue::FlushQueue();
        
            ConsoleRig::ResourceBoxes_Shutdown();
            Assets::Dependencies_Shutdown();    //  (can't do this properly here!)

			_pimpl->_attachRef3.reset();
			_pimpl->_attachRef2.reset();
			_pimpl->_attachRef1.reset();
			_pimpl->_attachRef0.reset();
			ConsoleRig::CrossModule::ReleaseInstance();
			delete _pimpl; _pimpl = nullptr;
		}
	}

	LibraryAttachMarker::!LibraryAttachMarker() 
	{
		this->~LibraryAttachMarker();
	}


}

