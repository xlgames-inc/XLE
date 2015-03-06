// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EngineDevice.h"
#include "MarshalString.h"
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
    EngineDeviceInternal::~EngineDeviceInternal() {}

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
    RenderCore::IDevice* EngineDevice::GetRenderDevice()
    {
        return _pimpl->_renderDevice.get();
    }

    BufferUploads::IManager* EngineDevice::GetBufferUploads()
    {
        return _pimpl->_bufferUploads.get();
    }
    
    EngineDevice::EngineDevice()
    {
        assert(s_instance == nullptr);

            // initialise logging first...
        auto appName = clix::marshalString<clix::E_UTF8>(System::Windows::Forms::Application::ProductName);
        CreateDirectoryRecursive("int");
        ConsoleRig::Logging_Startup("log.cfg", 
            StringMeld<128>() << "int/" << appName << ".txt");

        _pimpl.reset(new EngineDeviceInternal);
        _pimpl->_console = std::make_unique<ConsoleRig::Console>();
        _pimpl->_renderDevice = RenderCore::CreateDevice();
        _pimpl->_immediateContext = _pimpl->_renderDevice->GetImmediateContext();
        _pimpl->_asyncMan = RenderCore::Metal::CreateCompileAndAsyncManager();
        _pimpl->_bufferUploads = BufferUploads::CreateManager(_pimpl->_renderDevice.get());

        RenderOverlays::InitFontSystem(_pimpl->_renderDevice.get(), _pimpl->_bufferUploads.get());
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

