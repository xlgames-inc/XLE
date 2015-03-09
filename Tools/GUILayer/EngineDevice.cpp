// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EngineDevice.h"
#include "MarshalString.h"
#include "WindowRigInternal.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderOverlays/Font.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../../Utility/Streams/PathUtils.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/SystemUtils.h"
#include "../../../Utility/StringFormat.h"

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
        XlConcatPath        (workingDir, dimof(workingDir), appDir, a2n("..\\Working"));
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
                Assets::CompileAndAsyncManager::GetInstance().Update();
                #undef Yield
                System::Threading::Thread::Yield();
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
    RenderCore::IDevice* NativeEngineDevice::GetRenderDevice()
    {
        return _renderDevice.get();
    }

    BufferUploads::IManager* NativeEngineDevice::GetBufferUploads()
    {
        return _bufferUploads.get();
    }
    
    std::unique_ptr<IWindowRig> NativeEngineDevice::CreateWindowRig(const void* nativeWindowHandle)
    {
        std::unique_ptr<WindowRig> result(new WindowRig(*_renderDevice.get(), nativeWindowHandle));

        std::shared_ptr<BufferUploads::IManager> bufferUploads = _bufferUploads;
        result->GetFrameRig().AddPostPresentCallback(
            [=](RenderCore::IThreadContext& threadContext)
            { bufferUploads->Update(threadContext); });

        return std::move(result);
    }

    NativeEngineDevice::NativeEngineDevice()
    {
        _console = std::make_unique<ConsoleRig::Console>();
        _renderDevice = RenderCore::CreateDevice();
        _immediateContext = _renderDevice->GetImmediateContext();
        _asyncMan = RenderCore::Metal::CreateCompileAndAsyncManager();
        _bufferUploads = BufferUploads::CreateManager(_renderDevice.get());
    }

    NativeEngineDevice::~NativeEngineDevice()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////
    EngineDevice::EngineDevice()
    {
        assert(s_instance == nullptr);

            // initialise logging first...
        auto appName = clix::marshalString<clix::E_UTF8>(System::Windows::Forms::Application::ProductName);
        CreateDirectoryRecursive("int");
        ConsoleRig::Logging_Startup("log.cfg", 
            StringMeld<128>() << "int/" << appName << ".txt");

        _pimpl.reset(new NativeEngineDevice);
        
        RenderOverlays::InitFontSystem(_pimpl->GetRenderDevice(), _pimpl->GetBufferUploads());
        ResourceCompilerThread_Hack::Kick();

        s_instance = this;
    }

    EngineDevice::~EngineDevice()
    {
        assert(s_instance == this);
        s_instance = nullptr;
        
        RenderCore::Techniques::ResourceBoxes_Shutdown();
        RenderOverlays::CleanupFontSystem();
        ResourceCompilerThread_Hack::Shutdown();
        delete _pimpl;
        TerminateFileSystemMonitoring();
    }
}

