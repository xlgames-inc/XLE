// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "MarshalString.h"
#include "CLIXAutoPtr.h"
#include "WindowRigInternal.h"
#include "DelayedDeleteQueue.h"
#include "ExportedNativeTypes.h"
#include "../ToolsRig/DivergentAsset.h"
#include "../ToolsRig/PreviewSceneRegistry.h"
#include "../../PlatformRig/WinAPI/RunLoop_WinAPI.h"
#include "../../RenderCore/Techniques/Apparatuses.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/Services.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Init.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/MountingTree.h"
#include "../../Assets/OSFileSystem.h"
#include "../../Assets/AssetServices.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/Streams/PathUtils.h"


namespace GUILayer
{
	ref class TimerMessageFilter : public System::Windows::Forms::IMessageFilter
	{
	public:
		virtual bool PreFilterMessage(System::Windows::Forms::Message% m)
		{
			if (m.Msg == WM_TIMER && _osRunLoop.get()) {
				// Return true to filter the event out of the message loop (which we will do if the timer id is recognized)
				return _osRunLoop->OnOSTrigger((UINT_PTR)m.WParam.ToPointer());
			}

			return false;
		}

		clix::shared_ptr<PlatformRig::OSRunLoop_BasicTimer> _osRunLoop;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////
    void EngineDevice::SetDefaultWorkingDirectory()
    {
        utf8 appPath[MaxPath];
        OSServices::GetProcessPath(appPath, dimof(appPath));
		auto splitter = MakeFileNameSplitter(appPath);
		OSServices::ChDir((splitter.DriveAndPath().AsString() + "/../Working").c_str());
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    RenderCore::IThreadContext*		NativeEngineDevice::GetImmediateContext()	{ return _renderDevice->GetImmediateContext().get(); }

    const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& NativeEngineDevice::GetMainPipelineAcceleratorPool()
    {
        return _drawingApparatus->_pipelineAccelerators;
    }

    const std::shared_ptr<RenderCore::Techniques::IImmediateDrawables>& NativeEngineDevice::GetImmediateDrawables()
    {
        return _immediateDrawingApparatus->_immediateDrawables;
    }

    const std::shared_ptr<RenderCore::Techniques::TechniqueContext>& NativeEngineDevice::GetTechniqueContext()
    {
        return _drawingApparatus->_techniqueContext;
    }

    void NativeEngineDevice::ResetFrameBufferPool()
    {
        _frameRenderingApparatus->_frameBufferPool->Reset();
    }

    NativeEngineDevice::NativeEngineDevice()
    {
        ConsoleRig::StartupConfig cfg;
        cfg._applicationName = clix::marshalString<clix::E_UTF8>(System::Windows::Forms::Application::ProductName);
        _services = std::make_shared<ConsoleRig::GlobalServices>(cfg);

		_assetServices = std::make_shared<::Assets::Services>();
        _mountId0 = ::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", ::Assets::CreateFileSystem_OS("Game/xleres"));
        _mountId1 = ::Assets::MainFileSystem::GetMountingTree()->Mount("res", ::Assets::CreateFileSystem_OS("C:/code/XLEExt/res"));

        _renderDevice = RenderCore::CreateDevice(RenderCore::Techniques::GetTargetAPI());
        _immediateContext = _renderDevice->GetImmediateContext();

        _techniquesServices = std::make_shared<RenderCore::Techniques::Services>(_renderDevice);

        _drawingApparatus = std::make_shared<RenderCore::Techniques::DrawingApparatus>(_renderDevice);
        _immediateDrawingApparatus = std::make_shared<RenderCore::Techniques::ImmediateDrawingApparatus>(_drawingApparatus);
        _primaryResourcesApparatus = std::make_shared<RenderCore::Techniques::PrimaryResourcesApparatus>(_renderDevice);
        _frameRenderingApparatus = std::make_shared<RenderCore::Techniques::FrameRenderingApparatus>(_renderDevice);
        _drawingApparatus->_techniqueContext->_frameBufferPool = _frameRenderingApparatus->_frameBufferPool;
        _drawingApparatus->_techniqueContext->_attachmentPool = _frameRenderingApparatus->_attachmentPool;
        _previewSceneRegistry = ToolsRig::CreatePreviewSceneRegistry();

        ::ConsoleRig::GlobalServices::GetInstance().LoadDefaultPlugins();

        _divAssets = std::make_unique<ToolsRig::DivergentAssetManager>();
        _creationThreadId = System::Threading::Thread::CurrentThread->ManagedThreadId;

		auto osRunLoop = std::make_shared<PlatformRig::OSRunLoop_BasicTimer>((HWND)0);
		PlatformRig::SetOSRunLoop(osRunLoop);

        auto messageFilter = gcnew TimerMessageFilter();
        messageFilter->_osRunLoop = osRunLoop;
		System::Windows::Forms::Application::AddMessageFilter(messageFilter);
        _messageFilter = messageFilter;
    }

    NativeEngineDevice::~NativeEngineDevice()
    {
		if (_messageFilter)
			System::Windows::Forms::Application::RemoveMessageFilter(_messageFilter.get());
		PlatformRig::SetOSRunLoop(nullptr);
        // ::Assets::Services::GetAssetSets().Clear();
		::ConsoleRig::GlobalServices::GetInstance().UnloadDefaultPlugins();
        ::Assets::MainFileSystem::GetMountingTree()->Unmount(_mountId1);
        ::Assets::MainFileSystem::GetMountingTree()->Unmount(_mountId0);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    RenderCore::IThreadContext* EngineDevice::GetNativeImmediateContext()
    {
        return _pimpl->GetImmediateContext();
    }

    void EngineDevice::PrepareForShutdown()
    {
        for each(auto i in _shutdownCallbacks) {
            auto callback = dynamic_cast<IOnEngineShutdown^>(i->Target);
            if (callback)
                callback->OnEngineShutdown();
        }
        _shutdownCallbacks->Clear();

        // It's a good idea to force a GC collect here...
        // it will help flush out managed references to native objects
        // before we go through the shutdown steps
        System::GC::Collect();
        System::GC::WaitForPendingFinalizers();
        DelayedDeleteQueue::FlushQueue();
        
        //if (_pimpl->GetAssetServices())
        //    _pimpl->GetAssetServices()->GetAssetSets().Clear();
    }

    void EngineDevice::AddOnShutdown(IOnEngineShutdown^ callback)
    {
        // It will be nicer to do this with delegates, but we can't create a 
        // delegate with captures in C++/CLR
        _shutdownCallbacks->Add(gcnew System::WeakReference(callback));
    }
    
    EngineDevice::EngineDevice()
    {
        assert(s_instance == nullptr);
        _shutdownCallbacks = gcnew System::Collections::Generic::List<System::WeakReference^>();
        _pimpl = new NativeEngineDevice();
        s_instance = this;
    }

    EngineDevice::~EngineDevice()
    {
        assert(s_instance == this);
        s_instance = nullptr;

        PrepareForShutdown();
        delete _pimpl;
		_pimpl = nullptr;
    }

    EngineDevice::!EngineDevice() 
    {
		if (_pimpl) {
			System::Diagnostics::Debug::Assert(false, "Non deterministic delete of EngineDevice");
		}
    }
}

