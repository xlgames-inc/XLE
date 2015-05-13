// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GlobalServices.h"

namespace ConsoleRig
{
    GlobalServices* GlobalServices::s_instance = nullptr;

    GlobalServices::GlobalServices() 
    {
        assert(s_instance == nullptr);
        s_instance = this;
    }

    GlobalServices::~GlobalServices() 
    {
        assert(s_instance == this);
        s_instance = nullptr;
    }

    void GlobalServices::SetInstance(GlobalServices* instance)
    {
        assert(instance == nullptr || s_instance == nullptr);
        s_instance = instance;
    }

}
