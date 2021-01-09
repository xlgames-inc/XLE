// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GlobalServices.h"
#include "AttachablePtr.h"
#include "Log.h"
#include "Console.h"
#include "ResourceBox.h"
#include "IProgress.h"
#include "Plugins.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
#include "../Utility/Threading/CompletionThreadPool.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/FileSystemMonitor.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/Conversion.h"
#include "../Core/SelectConfiguration.h"
#include <assert.h>
#include <random>
#include <typeinfo>

extern "C" const char ConsoleRig_VersionString[];
extern "C" const char ConsoleRig_BuildDateString[];

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
        return splitter.DriveAndPath().AsString() + "/../Working";
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static auto Fn_GetConsole = ConstHash64<'getc', 'onso', 'le'>::Value;
    static auto Fn_ConsoleMainModule = ConstHash64<'cons', 'olem', 'ain'>::Value;
    static auto Fn_GetAppName = ConstHash64<'appn', 'ame'>::Value;
    static auto Fn_GuidGen = ConstHash64<'guid', 'gen'>::Value;
    static auto Fn_RedirectCout = ConstHash64<'redi', 'rect', 'cout'>::Value;
	static auto Fn_GetAssetRoot = ConstHash64<'asse', 'troo', 't'>::Value;
	static auto Fn_DefaultFileSystem = ConstHash64<'defa', 'ultf', 's'>::Value;

	void DebugUtil_Startup();
	void DebugUtil_Shutdown();

    static void MainRig_Startup(const StartupConfig& cfg)
    {
		auto& serv = CrossModule::GetInstance()._services;

        std::string appNameString = cfg._applicationName;
        bool redirectCount = cfg._redirectCout;
        serv.Add<std::string()>(Fn_GetAppName, [appNameString](){ return appNameString; });
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

    static void MainRig_Attach()
    {
        auto& serv = CrossModule::GetInstance()._services;

		DebugUtil_Startup();

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

        std::shared_ptr<::Assets::IFileSystem> defaultFileSystem;
		if (!serv.Has<std::shared_ptr<::Assets::IFileSystem>()>(Fn_DefaultFileSystem)) {
			defaultFileSystem = ::Assets::CreateFileSystem_OS();
			serv.Add(Fn_DefaultFileSystem, [defaultFileSystem]() { return defaultFileSystem; });
		} else {
			defaultFileSystem = serv.Call<std::shared_ptr<::Assets::IFileSystem>>(Fn_DefaultFileSystem);
		}

		::Assets::MainFileSystem::Init(mountingTree, defaultFileSystem);
    }

    static void MainRig_Detach()
    {
            // this will throw an exception if no module has successfully initialised
            // logging
        auto& serv = CrossModule::GetInstance()._services;
		ModuleId mainModuleId = 0;
        if (serv.TryCall(Fn_ConsoleMainModule, mainModuleId) && mainModuleId == GetCurrentModuleId()) {
            serv.Remove(Fn_GetConsole);
            serv.Remove(Fn_ConsoleMainModule);
        }

		serv.InvalidateCurrentModule();

		Console::SetInstance(nullptr);

		ResourceBoxes_Shutdown();
		DebugUtil_Shutdown();
        ::Assets::MainFileSystem::Shutdown();
		TerminateFileSystemMonitoring();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	class GlobalServices::Pimpl
	{
	public:
		AttachablePtr<LogCentralConfiguration> _logCfg;
        std::unique_ptr<CompletionThreadPool> _shortTaskPool;
        std::unique_ptr<CompletionThreadPool> _longTaskPool;
		StartupConfig _cfg;
		std::unique_ptr<PluginSet> _pluginSet;
	};

    GlobalServices* GlobalServices::s_instance = nullptr;

    GlobalServices::GlobalServices(const StartupConfig& cfg)
    {
		_pimpl = std::make_unique<Pimpl>();
        _pimpl->_shortTaskPool = std::make_unique<CompletionThreadPool>(cfg._shortTaskThreadPoolCount);
        _pimpl->_longTaskPool = std::make_unique<CompletionThreadPool>(cfg._longTaskThreadPoolCount);
		_pimpl->_cfg = cfg;

        MainRig_Startup(cfg);

            // add "nsight" marker to global services when "-nsight" is on
            // the command line. This is an easy way to record a global (&cross-dll)
            // state to use the nsight configuration when the given flag is set.
        const auto* cmdLine = XlGetCommandLine();
        if (cmdLine && XlFindString(cmdLine, "-nsight"))
            CrossModule::GetInstance()._services.Add(Hash64("nsight"), []() { return true; });
    }

    GlobalServices::~GlobalServices() 
    {
        assert(s_instance == nullptr);  // (should already have been detached in the Withhold() call)
    }

	void GlobalServices::LoadDefaultPlugins()
	{
		if (!_pimpl->_pluginSet)
			_pimpl->_pluginSet = std::make_unique<PluginSet>();
	}

	void GlobalServices::UnloadDefaultPlugins()
	{
		_pimpl->_pluginSet.reset();
	}

    void GlobalServices::AttachCurrentModule()
    {
        assert(s_instance == nullptr);
        s_instance = this;
        MainRig_Attach();
		_pimpl->_logCfg = GetAttachablePtr<LogCentralConfiguration>();
		if (!_pimpl->_logCfg)
			_pimpl->_logCfg = MakeAttachablePtr<LogCentralConfiguration>(_pimpl->_cfg._logConfigFile);
    }

    void GlobalServices::DetachCurrentModule()
    {
        MainRig_Detach();
        assert(s_instance == this);
        s_instance = nullptr;
    }

	CompletionThreadPool& GlobalServices::GetShortTaskThreadPool() { return *_pimpl->_shortTaskPool; }
    CompletionThreadPool& GlobalServices::GetLongTaskThreadPool() { return *_pimpl->_longTaskPool; }

	CrossModule* CrossModule::s_instance = nullptr;

	CrossModule& CrossModule::GetInstance()
	{
		if (!s_instance) {
			s_instance = new CrossModule();
			std::atexit([]() { delete s_instance; s_instance = nullptr; });
		}
		return *s_instance;
	}

	void CrossModule::SetInstance(CrossModule& crossModule)
	{
		assert(!s_instance);
		s_instance = &crossModule;
	}

	void CrossModule::ReleaseInstance()
	{
		assert(s_instance);
		s_instance = nullptr;
	}


    IStep::~IStep() {}
    IProgress::~IProgress() {}


	namespace Internal
	{
		std::vector<std::unique_ptr<IBoxTable>> BoxTables;
		IBoxTable::~IBoxTable() {}
	}

	void ResourceBoxes_Shutdown()
	{
		// Destroy the box tables in reverse order
		while (!Internal::BoxTables.empty())
			Internal::BoxTables.erase(Internal::BoxTables.end()-1);
		Internal::BoxTables = std::vector<std::unique_ptr<Internal::IBoxTable>>();
	}


	StartupConfig::StartupConfig()
    {
        _applicationName = "XLEApp";
        _logConfigFile = "log.dat";
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

	LibVersionDesc GetLibVersionDesc()
	{
		return LibVersionDesc { ConsoleRig_VersionString, ConsoleRig_BuildDateString };
	}

}
