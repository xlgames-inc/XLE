// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

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
        static InvalidAssetManager& GetInvalidAssetMan() { return *GetInstance()._invalidAssetMan; }

        static Services& GetInstance() { return *s_instance; }

        struct Flags 
        {
            enum Enum { RecordInvalidAssets = 1<<0 };
            typedef unsigned BitField;
        };

        Services(Flags::BitField flags);
        ~Services();
    protected:
        std::unique_ptr<AssetSetManager> _assetSets;
        std::unique_ptr<CompileAndAsyncManager> _asyncMan;
        std::unique_ptr<InvalidAssetManager> _invalidAssetMan;

        static Services* s_instance;
    };
}


