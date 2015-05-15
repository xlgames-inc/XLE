// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "MarshalString.h"
#include "WindowRigInternal.h"
#include "ExportedNativeTypes.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Assets/ColladaCompilerInterface.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderOverlays/Font.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../ConsoleRig/Console.h"
// #include "../../ConsoleRig/Log.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/SystemUtils.h"
#include "../../Utility/StringFormat.h"

namespace RenderCore { namespace Assets { void SetBufferUploads(BufferUploads::IManager* bufferUploads); }}

namespace GUILayer
{
///////////////////////////////////////////////////////////////////////////////////////////////////
    void EngineDevice::SetDefaultWorkingDirectory()
    {
        nchar_t appPath     [MaxPath];
        nchar_t appDir      [MaxPath];
        nchar_t workingDir  [MaxPath];

        XlGetProcessPath    (appPath, dimof(appPath));
        XlSimplifyPath      (appPath, dimof(appPath), appPath, a2n("\\/"));
        XlDirname           (appDir, dimof(appDir), appPath);
        const auto* fn = a2n("..\\Working");
        XlConcatPath        (workingDir, dimof(workingDir), appDir, fn, &fn[XlStringLen(fn)]);
        XlSimplifyPath      (workingDir, dimof(workingDir), workingDir, a2n("\\/"));
        XlChDir             (workingDir);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

        //      hack to make it easy to spawn a thread to process
        //      items in the background
    ref class ResourceCompilerThread_Hack
    {
    public:
        static System::Threading::Thread^ BackgroundThread = nullptr;

        static void BackgroundThreadFunction()
        {
            while (Active) {
                Assets::Services::GetAsyncMan().Update();
                #undef Yield
                // System::Threading::Thread::Yield();
                System::Threading::Thread::Sleep(100);
            }
        }

        static void Kick() 
        {
            Active = true;
            if (!BackgroundThread) {
                    //  this thread never dies -- and it keeps 
                    //      the program alive after it's finished
                BackgroundThread = gcnew System::Threading::Thread(gcnew System::Threading::ThreadStart(&BackgroundThreadFunction));
                BackgroundThread->Start();
            }
        }

        static void Shutdown()
        {
            Active = false;
            BackgroundThread->Join();
            delete BackgroundThread;
            BackgroundThread = nullptr;
        }

    private:
        static bool Active = true;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
    std::unique_ptr<IWindowRig> NativeEngineDevice::CreateWindowRig(const void* nativeWindowHandle)
    {
        std::unique_ptr<WindowRig> result(new WindowRig(*_renderDevice.get(), nativeWindowHandle));

        std::shared_ptr<BufferUploads::IManager> bufferUploads = _bufferUploads;
        result->GetFrameRig().AddPostPresentCallback(
            [=](RenderCore::IThreadContext& threadContext)
            { bufferUploads->Update(threadContext); });

        return std::move(result);
    }

    void NativeEngineDevice::AttachDefaultCompilers()
    {
        auto& compilers = _assetServices->GetAsyncMan().GetIntermediateCompilers();
        using RenderCore::Assets::ColladaCompiler;
		auto colladaProcessor = std::make_shared<ColladaCompiler>();
		compilers.AddCompiler(ColladaCompiler::Type_Model, colladaProcessor);
		compilers.AddCompiler(ColladaCompiler::Type_AnimationSet, colladaProcessor);
		compilers.AddCompiler(ColladaCompiler::Type_Skeleton, colladaProcessor);
        compilers.AddCompiler(
            RenderCore::Assets::MaterialScaffold::CompileProcessType,
            std::make_shared<RenderCore::Assets::MaterialScaffoldCompiler>());
    }

    NativeEngineDevice::NativeEngineDevice()
    {
        ConsoleRig::StartupConfig cfg;
        cfg._applicationName = clix::marshalString<clix::E_UTF8>(System::Windows::Forms::Application::ProductName);
        _services = std::make_unique<ConsoleRig::GlobalServices>(cfg);

        _renderDevice = RenderCore::CreateDevice();
        _immediateContext = _renderDevice->GetImmediateContext();

        _assetServices = std::make_unique<::Assets::Services>(::Assets::Services::Flags::RecordInvalidAssets);
        RenderCore::Metal::InitCompileAndAsyncManager();

        BufferUploads::Attach(ConsoleRig::GlobalServices::GetInstance());
        _bufferUploads = BufferUploads::CreateManager(_renderDevice.get());
        SceneEngine::SetBufferUploads(_bufferUploads.get());
        RenderCore::Assets::SetBufferUploads(_bufferUploads.get());
    }

    NativeEngineDevice::~NativeEngineDevice()
    {
        _bufferUploads.reset();
        BufferUploads::Detach();

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
    
    EngineDevice::EngineDevice()
    {
        assert(s_instance == nullptr);

        _pimpl.reset(new NativeEngineDevice);
        
        RenderOverlays::InitFontSystem(_pimpl->GetRenderDevice(), _pimpl->GetBufferUploads());
        ResourceCompilerThread_Hack::Kick();

        s_instance = this;
    }

    EngineDevice::~EngineDevice()
    {
        assert(s_instance == this);
        s_instance = nullptr;

            // it's a good idea to force a GC collect here...
            // it will help flush out managed references to native objects
            // before we go through the shutdown steps
        System::GC::Collect();
        System::GC::WaitForPendingFinalizers();
        
        RenderCore::Techniques::ResourceBoxes_Shutdown();
        RenderOverlays::CleanupFontSystem();
        if (_pimpl->GetAssetServices())
            _pimpl->GetAssetServices()->GetAssetSets().Clear();
        ResourceCompilerThread_Hack::Shutdown();
        Assets::Dependencies_Shutdown();
        _pimpl.reset();
        TerminateFileSystemMonitoring();
    }
}

