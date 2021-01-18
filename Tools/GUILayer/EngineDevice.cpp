// Copyright 2015 XLGAMES Inc.
//
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
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/WinAPI/RunLoop_WinAPI.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Init.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/Services.h"
#include "../../RenderOverlays/Font.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/MountingTree.h"
#include "../../Assets/OSFileSystem.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../OSServices/BasicFile.h"
#include "../../OSServices/SystemUtils.h"
#include "../../Utility/StringFormat.h"

#include "../../Tools/ToolsRig/GenerateAO.h"

namespace RenderCore { namespace Techniques
{
	std::shared_ptr<IPipelineAcceleratorPool> CreatePipelineAcceleratorPool();
}}

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
        XlGetProcessPath(appPath, dimof(appPath));
		auto splitter = MakeFileNameSplitter(appPath);
		XlChDir((splitter.DriveAndPath().AsString() + "/../Working").c_str());
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    void NativeEngineDevice::AttachDefaultCompilers()
    {
        _renderAssetsServices->InitModelCompilers();

            // add compiler for precalculated internal AO
		auto& asyncMan = ::Assets::Services::GetAsyncMan();
        asyncMan.GetIntermediateCompilers().AddCompiler(std::make_shared<ToolsRig::AOSupplementCompiler>(_immediateContext));
    }

    BufferUploads::IManager*		NativeEngineDevice::GetBufferUploads()		{ return &_techniquesServices->GetBufferUploads(); }
    RenderCore::IThreadContext*		NativeEngineDevice::GetImmediateContext()	{ return _renderDevice->GetImmediateContext().get(); }

    NativeEngineDevice::NativeEngineDevice()
    {
        ConsoleRig::StartupConfig cfg;
        cfg._applicationName = clix::marshalString<clix::E_UTF8>(System::Windows::Forms::Application::ProductName);
        _services = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(cfg);
		_crossModule = &ConsoleRig::CrossModule::GetInstance();

		::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", ::Assets::CreateFileSystem_OS("Game/xleres"));

        _renderDevice = RenderCore::CreateDevice(RenderCore::Techniques::GetTargetAPI());
        _immediateContext = _renderDevice->GetImmediateContext();
		RenderCore::Techniques::SetThreadContext(_immediateContext);

        _assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(::Assets::Services::Flags::RecordInvalidAssets);
        _renderAssetsServices = ConsoleRig::MakeAttachablePtr<RenderCore::Assets::Services>(_renderDevice);
		_techniquesServices = ConsoleRig::MakeAttachablePtr<RenderCore::Techniques::Services>(_renderDevice);
		_divAssets = std::make_unique<ToolsRig::DivergentAssetManager>();
        _creationThreadId = System::Threading::Thread::CurrentThread->ManagedThreadId;
		_pipelineAcceleratorPool = RenderCore::Techniques::CreatePipelineAcceleratorPool();

		// hack for plugin startup -- need to find the resources for the plugin:
		::Assets::MainFileSystem::GetMountingTree()->Mount("res", ::Assets::CreateFileSystem_OS("C:/code/XLEExt/res"));
		::ConsoleRig::GlobalServices::GetInstance().LoadDefaultPlugins();

		TimerMessageFilter^ messageFilter = gcnew TimerMessageFilter();
		auto osRunLoop = std::make_shared<PlatformRig::OSRunLoop_BasicTimer>((HWND)0);
		messageFilter->_osRunLoop = osRunLoop;
		PlatformRig::SetOSRunLoop(osRunLoop);

		_messageFilter = messageFilter;
		System::Windows::Forms::Application::AddMessageFilter(messageFilter);
    }

    NativeEngineDevice::~NativeEngineDevice()
    {
		if (_messageFilter)
			System::Windows::Forms::Application::RemoveMessageFilter(_messageFilter.get());
		PlatformRig::SetOSRunLoop(nullptr);
		::ConsoleRig::GlobalServices::GetInstance().UnloadDefaultPlugins();
		_pipelineAcceleratorPool.reset();
		_divAssets.reset();
        _renderAssetsServices.reset();
        _assetServices.reset();
        _immediateContext.reset();
        _renderDevice.reset();
        _console.reset();

        _services.reset();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    void EngineDevice::AttachDefaultCompilers()
    {
        _pimpl->AttachDefaultCompilers();
    }

    RenderCore::IThreadContext* EngineDevice::GetNativeImmediateContext()
    {
        return _pimpl->GetImmediateContext();
    }

    void EngineDevice::ForegroundUpdate()
    {
            // This is intended to be run in the foreground thread
            // It can be run inconsistantly... But typically it is
            // run approximately once per frame.
        assert(System::Threading::Thread::CurrentThread->ManagedThreadId == _pimpl->GetCreationThreadId());
        Assets::Services::GetAsyncMan().Update();

            // Some tools need buffer uploads to be updated from here
        _pimpl->GetBufferUploads()->Update(*_pimpl->GetImmediateContext(), false);
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
        
        ConsoleRig::ResourceBoxes_Shutdown();
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
        Assets::Dependencies_Shutdown();
        delete _pimpl;
		_pimpl = nullptr;
        TerminateFileSystemMonitoring();
    }

    EngineDevice::!EngineDevice() 
    {
		if (_pimpl) {
			System::Diagnostics::Debug::Assert(false, "Non deterministic delete of EngineDevice");
		}
    }
}

