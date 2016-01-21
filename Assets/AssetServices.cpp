// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetServices.h"
#include "AssetSetManager.h"
#include "CompileAndAsyncManager.h"
#include "InvalidAssetManager.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../ConsoleRig/AttachableInternal.h"

namespace Assets
{
    Services* Services::s_instance = nullptr;

    Services::Services(Flags::BitField flags)
    {
        _assetSets = std::make_unique<AssetSetManager>();
        _asyncMan = std::make_unique<CompileAndAsyncManager>();
        _invalidAssetMan = std::make_unique<InvalidAssetManager>(!!(flags & Flags::RecordInvalidAssets));
    }

    Services::~Services() 
    {
        _invalidAssetMan.reset();
        _assetSets.reset();
        _asyncMan.reset();
    }

    // Services* CrossModuleSingleton<Services>::s_instance = nullptr;
}

