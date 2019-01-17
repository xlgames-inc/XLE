#pragma once

#include "AssetHeap.h"
#include "../Utility/HeapUtils.h"

namespace Assets
{
    template<typename AssetType>
        class AssetHeapLRU : public IDefaultAssetHeap
    {
    public:
        template<typename... Params>
            FuturePtr<AssetType> Get(Params...);

        template<typename... Params>
            uint64_t SetShadowingAsset(AssetPtr<AssetType>&& newShadowingAsset, Params...);

        void            Clear();
        uint64_t        GetTypeCode() const;
        std::string     GetTypeName() const;
        void            OnFrameBarrier();

        std::vector<AssetHeapRecord>        LogRecords() const;

        AssetHeapLRU(unsigned cacheSize);
        ~AssetHeapLRU();
        AssetHeapLRU(const AssetHeapLRU&) = delete;
        AssetHeapLRU& operator=(const AssetHeapLRU&) = delete;
    private:
        mutable Threading::Mutex _lock;
        LRUCache<AssetFuture<AssetType>> _assets;
        std::vector<std::pair<uint64_t, FuturePtr<AssetType>>> _shadowingAssets;

        #if defined(_DEBUG)
            std::vector<std::pair<uint64_t, AssetHeapRecord>> _initializationRecords;
        #endif
    };

    template<typename AssetType>
        template<typename... Params>
            auto AssetHeapLRU<AssetType>::Get(Params... initialisers) -> FuturePtr<AssetType>
    {
        auto hash = Internal::BuildHash(initialisers...);

        FuturePtr<AssetType> newFuture;
        {
            ScopedLock(_lock);
            auto shadowing = LowerBound(_shadowingAssets, hash);
            if (shadowing != _shadowingAssets.end() && shadowing->first == hash)
                return shadowing->second;

            auto i = _assets.Get(hash);
            if (i && !IsInvalidated(*i))
                return i;

            auto stringInitializer = Internal::AsString(initialisers...);    // (used for tracking/debugging purposes)
            newFuture = std::make_shared<AssetFuture<AssetType>>(stringInitializer);
            _assets.Insert(hash, newFuture);

            #if defined(_DEBUG)
                auto record = LowerBound(_initializationRecords, hash);
                if (record!=_initializationRecords.end() && record->first == hash) {
                    ++record->second._initializationCount;
                } else {
                    _initializationRecords.insert(
                        record,
                        std::make_pair(
                            hash,
                            AssetHeapRecord{
                                stringInitializer,
                                AssetState::Invalid,
                                nullptr, nullptr, 0,
                                hash, 1
                            }));
                }
            #endif
        }

        // note -- call AutoConstructToFuture outside of the mutex lock, because this operation can be expensive
        // after the future has been constructed but before we complete AutoConstructToFuture, the asset is considered to be
        // in "pending" state, and Actualize() will through a PendingAsset exception, so this should be thread-safe, even if
        // another thread grabs the future before AutoConstructToFuture is done
        AutoConstructToFuture<AssetType>(*newFuture, std::forward<Params>(initialisers)...);
        return newFuture;
    }

    template<typename AssetType>
        template<typename... Params>
            uint64_t AssetHeapLRU<AssetType>::SetShadowingAsset(AssetPtr<AssetType>&& newShadowingAsset, Params... initialisers)
    {
        auto hash = Internal::BuildHash(initialisers...);

        ScopedLock(_lock);
        auto shadowing = LowerBound(_shadowingAssets, hash);
        if (shadowing != _shadowingAssets.end() && shadowing->first == hash) {
            shadowing->second->SimulateChange();
            if (newShadowingAsset) {
                shadowing->second->SetAsset(std::move(newShadowingAsset), nullptr);
            } else {
                _shadowingAssets.erase(shadowing);
            }
            return hash;
        }

        if (newShadowingAsset) {
            auto stringInitializer = Internal::AsString(initialisers...);    // (used for tracking/debugging purposes)
            auto newShadowingFuture = std::make_shared<AssetFuture<AssetType>>(stringInitializer);
            newShadowingFuture->SetAsset(std::move(newShadowingAsset), nullptr);
            _shadowingAssets.emplace(shadowing, std::make_pair(hash, std::move(newShadowingFuture)));
        }

        auto i = _assets.Get(hash);
        if (i)
            i->second->SimulateChange();

        return hash;
    }

    template<typename AssetType>
        void AssetHeapLRU<AssetType>::OnFrameBarrier()
    {
        ScopedLock(_lock);
        for (const auto&a:_assets.GetObjects())
            a->OnFrameBarrier();
        for (const auto&a: _shadowingAssets)
            a.second->OnFrameBarrier();
    }

    template<typename AssetType>
        AssetHeapLRU<AssetType>::AssetHeapLRU(unsigned cacheSize) : _assets(cacheSize) {}

    template<typename AssetType>
        AssetHeapLRU<AssetType>::~AssetHeapLRU() {}

    template<typename AssetType>
        void AssetHeapLRU<AssetType>::Clear()
    {
        ScopedLock(_lock);
        unsigned cacheSize = _assets.GetCacheSize();
        _assets = LRUCache<AssetFuture<AssetType>>{cacheSize};
        _shadowingAssets.clear();
    }

    template<typename AssetType>
        auto AssetHeapLRU<AssetType>::LogRecords() const -> std::vector<AssetHeapRecord>
    {
        ScopedLock(_lock);
        std::vector<AssetHeapRecord> result;
        result.reserve(_assets.GetCacheSize() + _shadowingAssets.size());
        auto typeCode = GetTypeCode();
        #if defined(_DEBUG)
            for (const auto&r : _initializationRecords) {
                auto record = r.second;
                auto item = const_cast<LRUCache<AssetFuture<AssetType>>&>(_assets).Get(r.first);
                if (item) {
                    record._state = item->GetAssetState();
                    record._depVal = item->GetDependencyValidation();
                    record._actualizationLog = item->GetActualizationLog();
                }
                result.push_back(record);
            }
        #else
            for (const auto&a : _assets.GetObjects())
                result.push_back({ a->Initializer(), a->GetAssetState(), a->GetDependencyValidation(), a->GetActualizationLog(), typeCode, 0x0 });
        #endif
        for (const auto&a : _shadowingAssets)
            result.push_back({ a.second->Initializer(), a.second->GetAssetState(), a.second->GetDependencyValidation(), a.second->GetActualizationLog(), typeCode, a.first });
        return result;
    }

    template<typename AssetType>
        uint64_t        AssetHeapLRU<AssetType>::GetTypeCode() const
        {
            return typeid(AssetType).hash_code();
        }

    template<typename AssetType>
        std::string        AssetHeapLRU<AssetType>::GetTypeName() const
        {
            return typeid(AssetType).name();
        }

}
