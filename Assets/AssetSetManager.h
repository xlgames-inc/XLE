// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include <memory>
#include <string>

namespace Assets
{
	class IDefaultAssetHeap;
    template <typename AssetType> class DefaultAssetHeap;

    class AssetSetManager
    {
    public:
        template<typename Type>
			DefaultAssetHeap<Type>& GetSetForType();

        void Clear();
        void LogReport();

        unsigned GetAssetSetCount();
        const IDefaultAssetHeap* GetAssetSet(unsigned index);

		void OnFrameBarrier();

        void Lock();
        void Unlock();

        AssetSetManager();
        ~AssetSetManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

		IDefaultAssetHeap* GetSetForTypeCode(size_t typeCode);
        void Add(size_t typeCode, std::unique_ptr<IDefaultAssetHeap>&& set);
    };

	AssetSetManager& GetAssetSetManager();

    template<typename Type>
		DefaultAssetHeap<Type>& AssetSetManager::GetSetForType()
    {
            // The lock here is frustratingly redundant in 99% of cases. But 
            // we still need it for the rest of the cases. If we could force the
            // system to add all of the types we need during startup, we could
            // avoid this. Alternatively, this might be a good candidate for a spin
            // lock, instead of a mutex
        Lock();
        auto existing = GetSetForTypeCode(typeid(Type).hash_code());
        if (existing) {
            Unlock();
            return *static_cast<DefaultAssetHeap<Type>*>(existing);
        }

		DefaultAssetHeap<Type>* result = nullptr;
        try 
        {
            auto newPtr = std::make_unique<DefaultAssetHeap<Type>>();
            result = newPtr.get();
            Add(typeid(Type).hash_code(), std::move(newPtr));
        } catch (...) {
            Unlock();
            throw;
        }
        Unlock();
        return *result;
    }

}

