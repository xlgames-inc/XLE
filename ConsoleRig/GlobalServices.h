// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/FunctionUtils.h"

namespace ConsoleRig
{
    class GlobalServices
    {
    public:
        VariantFunctions _services;
        
        GlobalServices();
        ~GlobalServices();

        static GlobalServices& GetInstance() { return *s_instance; }
        static void SetInstance(GlobalServices* instance);

        GlobalServices(const GlobalServices&) = delete;
        GlobalServices& operator=(const GlobalServices&) = delete;

    protected:
        static GlobalServices* s_instance;
    };
}

