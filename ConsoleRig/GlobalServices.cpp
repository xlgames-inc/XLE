// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GlobalServices.h"
#include "AttachableInternal.h"
#include "LogStartup.h"
#include "Console.h"
#include "IProgress.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
#include "../Utility/Threading/CompletionThreadPool.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/Conversion.h"
#include "../Core/SelectConfiguration.h"
#include <assert.h>
#include <random>

namespace ConsoleRig
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::basic_string<utf8> GetAssetRoot()
    {
            //
            //      For convenience, set the working directory to be ../Working 
            //              (relative to the application path)
            //
        utf8 appPath[MaxPath];
        XlGetProcessPath(appPath, dimof(appPath));
		auto splitter = MakeFileNameSplitter(appPath);
        return splitter.DriveAndPath().AsString() + u("/../Working");
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static auto Fn_GetConsole = ConstHash64<'getc', 'onso', 'le'>::Value;
    static auto Fn_ConsoleMainModule = ConstHash64<'cons', 'olem', 'ain'>::Value;
    static auto Fn_GetAppName = ConstHash64<'appn', 'ame'>::Value;
    static auto Fn_LogCfg = ConstHash64<'logc', 'fg'>::Value;
    static auto Fn_GuidGen = ConstHash64<'guid', 'gen'>::Value;
    static auto Fn_RedirectCout = ConstHash64<'redi', 'rect', 'cout'>::Value;
	static auto Fn_GetAssetRoot = ConstHash64<'asse', 'troo', 't'>::Value;
	static auto Fn_DefaultFileSystem = ConstHash64<'defa', 'ultf', 's'>::Value;

    static void MainRig_Startup(const StartupConfig& cfg, VariantFunctions& serv)
    {
        std::string appNameString = cfg._applicationName;
        std::string logCfgString = cfg._logConfigFile;
        bool redirectCount = cfg._redirectCout;
        serv.Add<std::string()>(Fn_GetAppName, [appNameString](){ return appNameString; });
        serv.Add<std::string()>(Fn_LogCfg, [logCfgString](){ return logCfgString; });
        serv.Add<bool()>(Fn_RedirectCout, [redirectCount](){ return redirectCount; });

        srand(std::random_device().operator()());

        auto guidGen = std::make_shared<std::mt19937_64>(std::random_device().operator()());
        serv.Add<uint64()>(
            Fn_GuidGen, [guidGen](){ return (*guidGen)(); });

		auto assetRoot = GetAssetRoot();
        if (cfg._setWorkingDir)
			XlChDir(assetRoot.c_str());

		serv.Add<std::basic_string<utf8>()>(Fn_GetAssetRoot, [assetRoot](){ return assetRoot; });

            //
            //      We need to initialize logging output.
            //      The "int" directory stands for "intermediate." We cache processed 
            //      models and textures in this directory
            //      But it's also a convenient place for log files (since it's excluded from
            //      git and it contains only temporary data).
            //      Note that we overwrite the log file every time, destroying previous data.
            //
        RawFS::CreateDirectoryRecursive("int");
    }

    StartupConfig::StartupConfig()
    {
        _applicationName = "XLEApp";
        _logConfigFile = "log.cfg";
        _setWorkingDir = true;
        _redirectCout = true;
        // Hack -- these thread pools are only useful/efficient on windows
        #if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
            _longTaskThreadPoolCount = 4;
            _shortTaskThreadPoolCount = 2;
        #else
            _longTaskThreadPoolCount = 0;
            _shortTaskThreadPoolCount = 0;
        #endif
    }

    StartupConfig::StartupConfig(const char applicationName[]) : StartupConfig()
    {
        _applicationName = applicationName;
    }

    static void MainRig_Attach()
    {
        auto& serv = GlobalServices::GetCrossModule()._services;

        Logging_Startup(
            serv.Call<std::string>(Fn_LogCfg).c_str(), 
            StringMeld<MaxPath>() << "int/log_" << serv.Call<std::string>(Fn_GetAppName) << ".txt");

        if (!serv.Has<ModuleId()>(Fn_ConsoleMainModule)) {
            auto console = std::make_shared<Console>();
            auto currentModule = GetCurrentModuleId();
            serv.Add(
                Fn_GetConsole, 
                [console]() { return console.get(); });
            serv.Add(
                Fn_ConsoleMainModule, 
                [currentModule]() { return currentModule; });
        } else {
            Console::SetInstance(serv.Call<Console*>(Fn_GetConsole));
        }

		std::shared_ptr<::Assets::MountingTree> mountingTree;
		if (!serv.Has<std::shared_ptr<::Assets::MountingTree>()>(typeid(::Assets::MountingTree).hash_code())) {
			mountingTree = std::make_shared<::Assets::MountingTree>(s_defaultFilenameRules);
			serv.Add(typeid(::Assets::MountingTree).hash_code(), [mountingTree]() { return mountingTree; });
		} else {
			mountingTree = serv.Call<std::shared_ptr<::Assets::MountingTree>>(typeid(::Assets::MountingTree).hash_code());
		}

		#if defined(TEMP_HACK)
        std::shared_ptr<::Assets::IFileSystem> defaultFileSystem;
		if (!serv.Has<std::shared_ptr<::Assets::IFileSystem>()>(Fn_DefaultFileSystem)) {
			defaultFileSystem = ::Assets::CreateFileSystem_OS();
			serv.Add(Fn_DefaultFileSystem, [defaultFileSystem]() { return defaultFileSystem; });
		} else {
			defaultFileSystem = serv.Call<std::shared_ptr<::Assets::IFileSystem>>(Fn_DefaultFileSystem);
		}

		::Assets::MainFileSystem::Init(mountingTree, defaultFileSystem);
        #endif
    }

    static void MainRig_Detach()
    {
            // this will throw an exception if no module has successfully initialised
            // logging
        auto& serv = GlobalServices::GetCrossModule()._services;
        if (serv.Call<ModuleId>(Fn_ConsoleMainModule) == GetCurrentModuleId()) {
            serv.Remove(Fn_GetConsole);
            serv.Remove(Fn_ConsoleMainModule);
        } else {
            Console::SetInstance(nullptr);
        }

        Logging_Shutdown();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    GlobalServices* GlobalServices::s_instance = nullptr;

    GlobalServices::GlobalServices(const StartupConfig& cfg)
    {
        if (cfg._shortTaskThreadPoolCount)
            _shortTaskPool = std::make_unique<CompletionThreadPool>(cfg._shortTaskThreadPoolCount);
        if (cfg._longTaskThreadPoolCount)
            _longTaskPool = std::make_unique<CompletionThreadPool>(cfg._longTaskThreadPoolCount);

        _crossModule = std::make_shared<CrossModule>();
        MainRig_Startup(cfg, _crossModule->_services);
        _crossModule->Publish(*this);
        AttachCurrentModule();

            // add "nsight" marker to global services when "-nsight" is on
            // the command line. This is an easy way to record a global (&cross-dll)
            // state to use the nsight configuration when the given flag is set.
        const auto* cmdLine = XlGetCommandLine();
        if (cmdLine && XlFindString(cmdLine, "-nsight"))
            _crossModule->_services.Add(Hash64("nsight"), []() { return true; });
    }

    GlobalServices::~GlobalServices() 
    {
        _crossModule->Withhold(*this);
        DetachCurrentModule();
    }

    void GlobalServices::AttachCurrentModule()
    {
        assert(s_instance == nullptr);
        s_instance = this;
        MainRig_Attach();
    }

    void GlobalServices::DetachCurrentModule()
    {
        MainRig_Detach();
        assert(s_instance == this);
        s_instance = nullptr;
    }


    IStep::~IStep() {}
    IProgress::~IProgress() {}


    void Logging_Startup(const char configFile[], const char logFileName[]) {}
    void Logging_Shutdown() {}
}
