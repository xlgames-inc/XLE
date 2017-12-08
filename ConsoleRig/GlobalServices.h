// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/FunctionUtils.h"
#include <string>
#include <memory>

namespace Utility { class CompletionThreadPool; }

namespace ConsoleRig
{
    template<typename Obj>
        class AttachRef
    {
    public:
        void Detach();
        Obj& Get();
        operator bool() { return _attachedService != nullptr; }

        AttachRef(Obj&);
        AttachRef();
        AttachRef(AttachRef&& moveFrom);
        AttachRef& operator=(AttachRef&& moveFrom);
        ~AttachRef();

        AttachRef(const AttachRef&) = delete;
        AttachRef& operator=(const AttachRef&) = delete;
    protected:
        Obj* _attachedService;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class CrossModule : public std::enable_shared_from_this<CrossModule>
    {
    public:
        VariantFunctions _services;

        template<typename Object> auto Attach() -> AttachRef<Object>;
        template<typename Object> void Publish(Object& obj);
        template<typename Object> void Withhold(Object& obj);

        template<typename Object, typename... Args>
            std::shared_ptr<Object> CreateAndPublish(Args... a);
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class StartupConfig
    {
    public:
        std::string _applicationName;
        std::string _logConfigFile;
        bool _setWorkingDir;
        bool _redirectCout;
        unsigned _longTaskThreadPoolCount;
        unsigned _shortTaskThreadPoolCount;

        StartupConfig();
        StartupConfig(const char applicationName[]);
    };

    class GlobalServices
    {
    public:
        static CrossModule& GetCrossModule() { return *s_instance->_crossModule; }
        static CompletionThreadPool& GetShortTaskThreadPool() { return *s_instance->_shortTaskPool; }
        static CompletionThreadPool& GetLongTaskThreadPool() { return *s_instance->_longTaskPool; }
        static GlobalServices& GetInstance() { return *s_instance; }

        GlobalServices(const StartupConfig& cfg = StartupConfig());
        ~GlobalServices();

        GlobalServices(const GlobalServices&) = delete;
        GlobalServices& operator=(const GlobalServices&) = delete;

        void AttachCurrentModule();
        void DetachCurrentModule();
    protected:
        static GlobalServices* s_instance;
        std::shared_ptr<CrossModule> _crossModule;

        std::unique_ptr<CompletionThreadPool> _shortTaskPool;
        std::unique_ptr<CompletionThreadPool> _longTaskPool;
    };

}

