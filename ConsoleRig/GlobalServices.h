// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/FunctionUtils.h"
#include <string>

namespace ConsoleRig
{
    class StartupConfig
    {
    public:
        std::string _applicationName;
        std::string _logConfigFile;
        bool _setWorkingDir;

        StartupConfig();
        StartupConfig(const char applicationName[]);
    };

    class GlobalServices
    {
    public:
        VariantFunctions _services;

        class AttachReference
        {
        public:
            void Detach();

            AttachReference();
            AttachReference(AttachReference&& moveFrom);
            AttachReference& operator=(AttachReference&& moveFrom);
            ~AttachReference();

            AttachReference(const AttachReference&) = delete;
            AttachReference& operator=(const AttachReference&) = delete;
        protected:
            AttachReference(GlobalServices&);
            bool _isAttached;
            GlobalServices* _attachedServices;
            friend class GlobalServices;
        };

        AttachReference Attach();
        
        GlobalServices(const StartupConfig& cfg = StartupConfig());
        ~GlobalServices();

        static GlobalServices& GetInstance() { return *s_instance; }
        static void SetInstance(GlobalServices* instance);

        GlobalServices(const GlobalServices&) = delete;
        GlobalServices& operator=(const GlobalServices&) = delete;

    protected:
        static GlobalServices* s_instance;
        signed _attachReferenceCount;
        AttachReference _mainAttachReference;
    };
}

