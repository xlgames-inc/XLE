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

        ConsoleRig::GlobalServices::GetCrossModule().Publish(*this);
    }

    Services::~Services() 
    {
        _invalidAssetMan.reset();
        _assetSets.reset();
        _asyncMan.reset();
        ConsoleRig::GlobalServices::GetCrossModule().Withhold(*this);
    }

    void Services::AttachCurrentModule()
    {
        assert(s_instance==nullptr);
        s_instance = this;
    }

    void Services::DetachCurrentModule()
    {
        assert(s_instance==this);
        s_instance = nullptr;
    }
}

