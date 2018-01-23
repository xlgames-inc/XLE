// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetServices.h"
#include "AssetSetManager.h"
#include "CompileAndAsyncManager.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../ConsoleRig/AttachableInternal.h"

namespace Assets
{
    Services::Services(Flags::BitField flags)
    {
        _assetSets = std::make_unique<AssetSetManager>();
        _asyncMan = std::make_unique<CompileAndAsyncManager>();
    }

    Services::~Services() 
    {
        _assetSets.reset();
        _asyncMan.reset();
    }
}

