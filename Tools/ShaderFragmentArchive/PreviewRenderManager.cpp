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

#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Assets/RawMaterial.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/MinimalShaderSource.h"

#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"

#include "../../Assets/AssetServices.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../ConsoleRig/AttachableInternal.h"
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

///////////////////////////////////////////////////////////////////////////////////////////////////

	static RenderCore::Techniques::Material CreatePreviewMaterial(ShaderPatcherLayer::NodeGraphContext^ doc, const ::Assets::DirectorySearchRules& searchRules)
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
        result._matParams.SetParameter(u("SHADER_NODE_EDITOR"), "1");

        for each(auto i in doc->ShaderParameters)
            result._matParams.SetParameter(
                (const utf8*)clix::marshalString<clix::E_UTF8>(i.Key).c_str(),
                clix::marshalString<clix::E_UTF8>(i.Value));

		return result;
	}

	ToolsRig::PreviewGeometry AsNative(PreviewGeometry geo)
    {
		switch (geo) {
		case PreviewGeometry::Chart: return ToolsRig::PreviewGeometry::Chart;
		default:
		case PreviewGeometry::Plane2D: return ToolsRig::PreviewGeometry::Plane2D;
		case PreviewGeometry::Box: return ToolsRig::PreviewGeometry::Box;
		case PreviewGeometry::Sphere: return ToolsRig::PreviewGeometry::Sphere;
		case PreviewGeometry::Model: return ToolsRig::PreviewGeometry::Model;
		}
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

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
            std::initializer_list<Metal::RenderTargetView>(rtvs, ArrayEnd(rtvs)));
        metalContext->Bind(rtvList, &dsv);

        metalContext->Bind(Metal::ViewportDesc(0.f, 0.f, float(width), float(height), 0.f, 1.f));

            ////////////

		ToolsRig::MaterialVisObject visObject;
		visObject._previewModelFile = clix::marshalString<clix::E_UTF8>(doc->PreviewModelFile);
		visObject._searchRules = ::Assets::DefaultDirectorySearchRules(MakeStringSection(visObject._previewModelFile));
		visObject._parameters = CreatePreviewMaterial(doc, visObject._searchRules);

        auto result = DrawPreview(*context, *_pimpl->_techContext, AsNative(geometry), visObject);
        if (result.first == ToolsRig::DrawPreviewResult::Error) {
            return GenerateErrorBitmap(result.second.c_str(), size);
        } else if (result.first == ToolsRig::DrawPreviewResult::Pending) {
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
        // ConsoleRig::AttachRef<RenderCore::Metal::ObjectFactory> _attachRef3;
    };

	LibraryAttachMarker::LibraryAttachMarker(GUILayer::EngineDevice^ engineDevice)
	{
		_pimpl = new AttachPimpl();

        engineDevice->GetNative().GetGlobalServices()->AttachCurrentModule();
        auto& crossModule = ConsoleRig::GlobalServices::GetCrossModule();
        _pimpl->_attachRef1 = crossModule.Attach<::Assets::Services>();
        _pimpl->_attachRef2 = crossModule.Attach<RenderCore::Assets::Services>();
        // _pimpl->_attachRef3 = crossModule.Attach<RenderCore::Metal::ObjectFactory>();
	}

	LibraryAttachMarker::~LibraryAttachMarker()
	{
		if (_pimpl) {
            System::GC::Collect();
            System::GC::WaitForPendingFinalizers();
            GUILayer::DelayedDeleteQueue::FlushQueue();
        
            ConsoleRig::ResourceBoxes_Shutdown();
            // Assets::Dependencies_Shutdown();     (can't do this properly here!)

			// _pimpl->_attachRef3.Detach();
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

