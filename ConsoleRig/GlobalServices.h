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
        class Attachable
    {
    public:
        class AttachRef;
        AttachRef Attach();

        class AttachRef
        {
        public:
            void Detach();
            Obj& Get();

            AttachRef();
            AttachRef(AttachRef&& moveFrom);
            AttachRef& operator=(AttachRef&& moveFrom);
            ~AttachRef();

            AttachRef(const AttachRef&) = delete;
            AttachRef& operator=(const AttachRef&) = delete;
        protected:
            AttachRef(Attachable<Obj>&);
            bool _isAttached;
            Attachable<Obj>* _attachedServices;
            friend class Attachable<Obj>;
        };

        Attachable(Obj& obj);
        ~Attachable();

        Attachable(const Attachable&) = delete;
        Attachable& operator=(const Attachable&) = delete;
    protected:
        signed _attachReferenceCount;
        AttachRef _mainAttachReference;
        Obj* _object;
    };

    template<typename Object>
        using AttachRef = typename Attachable<Object>::AttachRef;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class CrossModule
    {
    public:
        VariantFunctions _services;

        template<typename Object> auto Attach() -> typename Attachable<Object>::AttachRef;
        template<typename Object> void Publish(Object& obj);
        template<typename Object> void Withhold(Object& obj);
    };

    template<typename Object>
        auto CrossModule::Attach() -> typename Attachable<Object>::AttachRef
    {
        return _services.Call<Attachable<Object>*>(typeid(Object).hash_code())->Attach();
    }

    template<typename Object> 
        void CrossModule::Publish(Object& obj)
    {
        auto attachable = std::make_shared<Attachable<Object>>(obj);
        _services.Add(
            typeid(Object).hash_code(), 
            [attachable]() -> Attachable<Object>* { return attachable.get(); });
    }

    template<typename Object> 
        void CrossModule::Withhold(Object& obj)
    {
        _services.Remove(typeid(Object).hash_code());
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class StartupConfig
    {
    public:
        std::string _applicationName;
        std::string _logConfigFile;
        bool _setWorkingDir;
        unsigned _longTaskThreadPoolCount;
        unsigned _shortTaskThreadPoolCount;

        StartupConfig();
        StartupConfig(const char applicationName[]);
    };

    class GlobalServices
    {
    public:
        static CrossModule& GetCrossModule() { return s_instance->_crossModule; }
        static CompletionThreadPool& GetShortTaskThreadPool() { return *s_instance->_shortTaskPool; }
        static CompletionThreadPool& GetLongTaskThreadPool() { return *s_instance->_longTaskPool; }
        static GlobalServices& GetInstance() { return *s_instance; }

        AttachRef<GlobalServices> Attach();

        GlobalServices(const StartupConfig& cfg = StartupConfig());
        ~GlobalServices();

        GlobalServices(const GlobalServices&) = delete;
        GlobalServices& operator=(const GlobalServices&) = delete;

        void AttachCurrentModule();
        void DetachCurrentModule();

    protected:
        static GlobalServices* s_instance;
        CrossModule _crossModule;

        std::unique_ptr<CompletionThreadPool> _shortTaskPool;
        std::unique_ptr<CompletionThreadPool> _longTaskPool;
    };

}

