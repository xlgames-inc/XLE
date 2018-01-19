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

    template<typename Type>
        class AttachableSingleton
    {
    public:
        static Type& GetInstance() { assert(s_instance); return *s_instance; }

        void AttachCurrentModule();
        void DetachCurrentModule();

        AttachableSingleton();
        ~AttachableSingleton();
    protected:
        static Type* s_instance;
    };

    class Services : public AttachableSingleton<Services>
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

        void AttachCurrentModule() { AttachableSingleton<Services>::AttachCurrentModule(); }
        void DetachCurrentModule() { AttachableSingleton<Services>::DetachCurrentModule(); }

        Services(Flags::BitField flags=0);
        ~Services();

        Services(const Services&) = delete;
        const Services& operator=(const Services&) = delete;
    protected:
        std::unique_ptr<AssetSetManager>		_assetSets;
        std::unique_ptr<CompileAndAsyncManager> _asyncMan;
        std::unique_ptr<InvalidAssetManager>	_invalidAssetMan;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Type>
        AttachableSingleton<Type>::AttachableSingleton()
    {
        assert(s_instance == nullptr); 
    }

    template<typename Type>
        AttachableSingleton<Type>::~AttachableSingleton()
    {
        assert(s_instance == nullptr);
    }

    template<typename Type>
        void AttachableSingleton<Type>::AttachCurrentModule()
    {
        assert(s_instance==nullptr);
        s_instance = (Type*)this;
    }

    template<typename Type>
        void AttachableSingleton<Type>::DetachCurrentModule()
    {
        assert(s_instance==this);
        s_instance = nullptr;
    }

    template<typename Type>
        Type* AttachableSingleton<Type>::s_instance = nullptr;
}


