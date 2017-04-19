// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../ConsoleRig/GlobalServices.h"
#include <memory>
#include <assert.h>

namespace Assets
{
    class AssetSetManager;
    class CompileAndAsyncManager;
    class InvalidAssetManager;

    template<typename Type>
        class CrossModuleSingleton
    {
    public:
        static Type& GetInstance() { assert(s_instance); return *s_instance; }

        void AttachCurrentModule();
        void DetachCurrentModule();

        CrossModuleSingleton();
        ~CrossModuleSingleton();
    protected:
        static Type* s_instance;
    };

    class Services : public CrossModuleSingleton<Services>
    {
    public:
        static AssetSetManager& GetAssetSets() { return *GetInstance()._assetSets; }
        static CompileAndAsyncManager& GetAsyncMan() { return *GetInstance()._asyncMan; }
        static InvalidAssetManager* GetInvalidAssetMan() { return s_instance?s_instance->_invalidAssetMan.get():nullptr; }

        struct Flags 
        {
            enum Enum { RecordInvalidAssets = 1<<0 };
            typedef unsigned BitField;
        };

        void AttachCurrentModule() { CrossModuleSingleton<Services>::AttachCurrentModule(); }
        void DetachCurrentModule() { CrossModuleSingleton<Services>::DetachCurrentModule(); }

        Services(Flags::BitField flags=0);
        ~Services();

        Services(const Services&) = delete;
        const Services& operator=(const Services&) = delete;
    protected:
        std::unique_ptr<AssetSetManager> _assetSets;
        std::unique_ptr<CompileAndAsyncManager> _asyncMan;
        std::unique_ptr<InvalidAssetManager> _invalidAssetMan;
    };

    template<typename Type>
        CrossModuleSingleton<Type>::CrossModuleSingleton() 
    {
        assert(s_instance == nullptr); 
        ConsoleRig::GlobalServices::GetCrossModule().Publish(*(Type*)this);
    }

    template<typename Type>
        CrossModuleSingleton<Type>::~CrossModuleSingleton() 
    {
        ConsoleRig::GlobalServices::GetCrossModule().Withhold(*(Type*)this);
        assert(s_instance == nullptr); 
    }

    template<typename Type>
        void CrossModuleSingleton<Type>::AttachCurrentModule()
    {
        assert(s_instance==nullptr);
        s_instance = (Type*)this;
    }

    template<typename Type>
        void CrossModuleSingleton<Type>::DetachCurrentModule()
    {
        assert(s_instance==this);
        s_instance = nullptr;
    }

    template<typename Type>
        Type* CrossModuleSingleton<Type>::s_instance = nullptr;
}


