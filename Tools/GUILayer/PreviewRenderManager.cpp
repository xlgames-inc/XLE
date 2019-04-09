// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EngineDevice.h"
#include "UITypesBinding.h"
#include "GUILayerUtil.h"
#include "MarshalString.h"
#include "NativeEngineDevice.h"
#include "CLIXAutoPtr.h"

#include "../ToolsRig/MaterialVisualisation.h"
#include "../ToolsRig/ModelVisualisation.h"
#include "../ToolsRig/VisualisationUtils.h"

#include "../../SceneEngine/SceneParser.h"

#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Assets/RawMaterial.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/MinimalShaderSource.h"
#include "../../RenderCore/IDevice.h"

#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"

#include "../../Assets/AssetServices.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/AssetSetManager.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Utility/PtrUtils.h"

#include <memory>

using System::Collections::Generic::Dictionary;
using System::String;
using System::Object;

namespace GUILayer
{
	using System::Drawing::Size;

	class ManagerPimpl
    {
    public:
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _globalTechniqueContext;
        std::shared_ptr<RenderCore::IDevice> _device;
    };

    public ref class PreviewBuilder
    {
    public:
		virtual System::Drawing::Bitmap^ BuildPreviewImage(
            GUILayer::MaterialVisSettings^ visSettings,
			String^ materialNames,
			GUILayer::TechniqueDelegateWrapper^ techniqueDelegate,
			GUILayer::MaterialDelegateWrapper^ materialDelegate,
			System::Drawing::Size^ size);

        PreviewBuilder();
		~PreviewBuilder();
    private:
        clix::auto_ptr<ManagerPimpl> _pimpl;
		System::Drawing::Bitmap^    GenerateErrorBitmap(const char str[], Size^ size);
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

	static std::shared_ptr<RenderCore::Techniques::Material> CreatePreviewMaterial(String^ materialNames, const ::Assets::DirectorySearchRules& searchRules)
	{
		auto result = std::make_shared<RenderCore::Techniques::Material>();

        // Our default material settings come from the "Document" object. This
        // give us our starting material and shader properties.
        if (!String::IsNullOrEmpty(materialNames)) {
            auto split = materialNames->Split(';');
			std::vector<::Assets::DependentFileState> finalMatDeps;
            for each(auto s in split) {
                auto nativeName = clix::marshalString<clix::E_UTF8>(s);
				RenderCore::Assets::MergeIn_Stall(
					*result,
					MakeStringSection(nativeName),
					searchRules,
					finalMatDeps);
            }
        }
        result->_matParams.SetParameter(u("SHADER_NODE_EDITOR"), MakeStringSection("1"));

        /*
		for each(auto i in doc->ShaderParameters)
            result->_matParams.SetParameter(
                MakeStringSection(clix::marshalString<clix::E_UTF8>(i.Key)).Cast<utf8>(),
                MakeStringSection(clix::marshalString<clix::E_UTF8>(i.Value)));
		*/

		return result;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	System::Drawing::Bitmap^ PreviewBuilder::BuildPreviewImage(
		GUILayer::MaterialVisSettings^ visSettings,
		String^ materialNames,
		GUILayer::TechniqueDelegateWrapper^ techniqueDelegate,
		GUILayer::MaterialDelegateWrapper^ materialDelegate,
		System::Drawing::Size^ size)
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

        ToolsRig::VisCameraSettings camSettings;
        camSettings._position = Float3(-1.42f, 0, 0);  // note that the position of the camera affects the apparent color of normals when previewing world space normals
		camSettings._verticalFieldOfView = 90.f;

        // Select the geometry type to use.
        // In the "chart" mode, we are just going to run a pixel shader for every
        // output pixel, so we want to use a pretransformed quad covering the viewport
		::Assets::FuturePtr<SceneEngine::IScene> sceneFuture;

		auto geometry = visSettings->Geometry;
		if (geometry != MaterialVisSettings::GeometryType::Model) {
			bool pretransformed = false;
			ToolsRig::MaterialVisSettings nativeVisSettings;
			switch (geometry) {
			case MaterialVisSettings::GeometryType::Plane2D:
			case MaterialVisSettings::GeometryType::Chart:
				nativeVisSettings._geometryType = ToolsRig::MaterialVisSettings::GeometryType::Plane2D;
				pretransformed = true;
				break;

			case MaterialVisSettings::GeometryType::Cube:
				nativeVisSettings._geometryType = ToolsRig::MaterialVisSettings::GeometryType::Cube;
				camSettings._position = Float3(-2.1f, 0, 0);
				break;

			default:
			case MaterialVisSettings::GeometryType::Sphere:
				nativeVisSettings._geometryType = ToolsRig::MaterialVisSettings::GeometryType::Sphere;
				break;
			}

			auto material = CreatePreviewMaterial(materialNames, ::Assets::DirectorySearchRules{});

			if (pretransformed)
				material->_matParams.SetParameter(u("GEO_PRETRANSFORMED"), MakeStringSection("1"));

			sceneFuture = ToolsRig::MakeScene(nativeVisSettings, material);
		} else {
			ToolsRig::ModelVisSettings modelSettings;
			// modelSettings._modelName = clix::marshalString<clix::E_UTF8>(doc->PreviewModelFile);
			// modelSettings._materialName = clix::marshalString<clix::E_UTF8>(doc->PreviewModelFile);
			sceneFuture = ToolsRig::MakeScene(modelSettings);
		}

		const auto& actualScene = ToolsRig::TryActualize(*sceneFuture);
		if (!actualScene) {
			auto errorLog = ToolsRig::GetActualizationError(*sceneFuture);
			if (errorLog) {
				return GenerateErrorBitmap(errorLog.value().c_str(), size);
			} else {
				return nullptr;		// pending
			}
		}

		if (geometry == MaterialVisSettings::GeometryType::Model) {
			auto* visContent = dynamic_cast<ToolsRig::IVisContent*>(actualScene.get());
			if (visContent) {
				camSettings = 
					ToolsRig::AlignCameraToBoundingBox(
						camSettings._verticalFieldOfView,
						visContent->GetBoundingBox());
			}
		}

		ToolsRig::VisEnvSettings envSettings;
		envSettings._lightingType = ToolsRig::VisEnvSettings::LightingType::Direct;

		Techniques::ParsingContext parserContext { *_pimpl->_globalTechniqueContext, &attachmentPool, &frameBufferPool };

		if (techniqueDelegate)
			parserContext.SetTechniqueDelegate(techniqueDelegate->_techniqueDelegate.GetNativePtr());
		if (materialDelegate)
			parserContext.SetMaterialDelegate(materialDelegate->_materialDelegate.GetNativePtr());

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

    PreviewBuilder::PreviewBuilder()
    {
        _pimpl.reset(new ManagerPimpl());

		auto engineDevice = EngineDevice::GetInstance();
        _pimpl->_device = engineDevice->GetNative().GetRenderDevice();
        
        _pimpl->_globalTechniqueContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
    }

    PreviewBuilder::~PreviewBuilder()
    {
		_pimpl.reset();
    }

}

