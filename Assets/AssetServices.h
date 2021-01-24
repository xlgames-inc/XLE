// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>
#include <assert.h>

namespace Assets
{
    class AssetSetManager;
    class CompileAndAsyncManager;
    class InvalidAssetManager;


    class Services
    {
    public:
        static AssetSetManager& GetAssetSets() { return *GetInstance()._assetSets; }
        static CompileAndAsyncManager& GetAsyncMan() { return *GetInstance()._asyncMan; }

        struct Flags 
        {
            enum Enum { RecordInvalidAssets = 1<<0 };
            typedef unsigned BitField;
        };

        static Services& GetInstance();
        static bool HasInstance();

        Services(Flags::BitField flags=0);
        ~Services();

        Services(const Services&) = delete;
        const Services& operator=(const Services&) = delete;
    protected:
        std::unique_ptr<AssetSetManager>		_assetSets;
        std::unique_ptr<CompileAndAsyncManager> _asyncMan;
    };

}


