// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetServices.h"
#include "AssetSetManager.h"
#include "CompileAndAsyncManager.h"
#include "InvalidAssetManager.h"

namespace Assets
{
    Services* Services::s_instance = nullptr;

    Services::Services(Flags::BitField flags)
    {
        _assetSets = std::make_unique<AssetSetManager>();
        _asyncMan = std::make_unique<CompileAndAsyncManager>();
        _invalidAssetMan = std::make_unique<InvalidAssetManager>(!!(flags & Flags::RecordInvalidAssets));

        assert(!s_instance);
        s_instance = this;
    }

    Services::~Services() 
    {
        assert(s_instance == this);
        _invalidAssetMan.reset();
        _assetSets.reset();
        _asyncMan.reset();
        s_instance = nullptr;
    }
}

