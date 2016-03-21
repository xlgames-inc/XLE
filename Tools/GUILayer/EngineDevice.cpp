// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "MarshalString.h"
#include "WindowRigInternal.h"
#include "DelayedDeleteQueue.h"
#include "ExportedNativeTypes.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderOverlays/Font.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../ConsoleRig/Console.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/SystemUtils.h"
#include "../../Utility/StringFormat.h"

#include "../../Tools/ToolsRig/GenerateAO.h"

namespace GUILayer
{
///////////////////////////////////////////////////////////////////////////////////////////////////
    void EngineDevice::SetDefaultWorkingDirectory()
    {
        nchar_t appPath     [MaxPath];
        nchar_t appDir      [MaxPath];
        nchar_t workingDir  [MaxPath];

        XlGetProcessPath    (appPath, dimof(appPath));
        XlDirname           (appDir, dimof(appDir), appPath);
        const auto* fn = a2n("..\\Working");
        XlConcatPath        (workingDir, dimof(workingDir), appDir, fn, XlStringEnd(fn));
        XlChDir             (workingDir);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    std::unique_ptr<IWindowRig> NativeEngineDevice::CreateWindowRig(const void* nativeWindowHandle)
    {
        std::unique_ptr<WindowRig> result(new WindowRig(*_renderDevice.get(), nativeWindowHandle));

        BufferUploads::IManager* bufferUploads = &_renderAssetsServices->GetBufferUploads();
        result->GetFrameRig().AddPostPresentCallback(
            [bufferUploads](RenderCore::IThreadContext& threadContext)
            { bufferUploads->Update(threadContext, false); });

        return std::move(result);
    }

    void NativeEngineDevice::AttachDefaultCompilers()
    {
        _renderAssetsServices->InitColladaCompilers();

            // add compiler for precalculated internal AO
        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        auto& compilers = asyncMan.GetIntermediateCompilers();
        auto aoGeoCompiler = std::make_shared<ToolsRig::AOSupplementCompiler>(_immediateContext);
        compilers.AddCompiler(
            ToolsRig::AOSupplementCompiler::CompilerType,
            std::move(aoGeoCompiler));
    }

    BufferUploads::IManager*    NativeEngineDevice::GetBufferUploads()
    {
        return &_renderAssetsServices->GetBufferUploads();
    }

    RenderCore::IThreadContext* NativeEngineDevice::GetImmediateContext()
    {
        return _renderDevice->GetImmediateContext().get();
    }

    NativeEngineDevice::NativeEngineDevice()
    {
        ConsoleRig::StartupConfig cfg;
        cfg._applicationName = clix::marshalString<clix::E_UTF8>(System::Windows::Forms::Application::ProductName);
        _services = std::make_unique<ConsoleRig::GlobalServices>(cfg);

        _renderDevice = RenderCore::CreateDevice();
        _immediateContext = _renderDevice->GetImmediateContext();

        _assetServices = std::make_unique<::Assets::Services>(::Assets::Services::Flags::RecordInvalidAssets);
        _renderAssetsServices = std::make_unique<RenderCore::Assets::Services>(_renderDevice.get());
        _creationThreadId = System::Threading::Thread::CurrentThread->ManagedThreadId;
    }

    NativeEngineDevice::~NativeEngineDevice()
    {
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
        
        RenderCore::Techniques::ResourceBoxes_Shutdown();
        RenderOverlays::CleanupFontSystem();
        if (_pimpl->GetAssetServices())
            _pimpl->GetAssetServices()->GetAssetSets().Clear();
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
        _pimpl = new NativeEngineDevice;
        RenderOverlays::InitFontSystem(_pimpl->GetRenderDevice().get(), _pimpl->GetBufferUploads());
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
        delete _pimpl;
        _pimpl = nullptr;
    }
}

