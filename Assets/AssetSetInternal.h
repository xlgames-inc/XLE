// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetSetManager.h"
#include "AssetsCore.h"
#include "AssetTraits.h"
#include <vector>
#include <assert.h>

#define ASSETS_STORE_NAMES
#define ASSETS_STORE_DIVERGENT		// divergent assets are intended for tools (not in-game). But we can't selectively disable this feature
#define ASSETS_MULTITHREADED        // allow GetAsset, GetAssetComp (and variants) to be used from multiple threads

#if defined(ASSETS_MULTITHREADED)
	// note --  there is an unfortunate complication here.
	//          .net code can't include the <mutex> header...
	//          So we have to push all code that interacts with
	//          the mutex class into the cpp file
	namespace std { class mutex; extern template unique_ptr<mutex>::~unique_ptr(); class recursive_mutex; extern template unique_ptr<recursive_mutex>::~unique_ptr(); }
	namespace Utility { namespace Threading { using Mutex = std::mutex; using RecursiveMutex = std::recursive_mutex; } }
#endif

namespace Assets
{
	class ICompileMarker;
	class IntermediateAssetLocator;
	class PendingCompileMarker;
	template <typename Asset> class DivergentAsset;
}

namespace Assets { namespace Internal 
{
    AssetSetManager& GetAssetSetManager();

    template <typename AssetType>
        class AssetSet : public IAssetSet
    {
    public:
        void            Clear();
        void            LogReport() const;

        uint64          GetTypeCode() const;
        const char*     GetTypeName() const;
        unsigned        GetDivergentCount() const;
        uint64          GetDivergentId(unsigned index) const;
        bool            DivergentHasChanges(unsigned index) const;
        std::string     GetAssetName(uint64 id) const;

        class AssetContainer
        {
        public:
            std::unique_ptr<AssetType> _active;
            std::unique_ptr<AssetType> _pendingReplacement;

            AssetContainer() {}
            AssetContainer(std::unique_ptr<AssetType>&& active, std::unique_ptr<AssetType>&& pendingReplacement) : _active(std::move(active)), _pendingReplacement(std::move(pendingReplacement)) {}
            AssetContainer(AssetContainer&& moveFrom) never_throws : _active(std::move(moveFrom._active)), _pendingReplacement(std::move(moveFrom._pendingReplacement)) {}
            AssetContainer& operator=(AssetContainer&& moveFrom) never_throws { _active = std::move(moveFrom._active); _pendingReplacement = std::move(moveFrom._pendingReplacement); return *this; }
        };

        std::vector<std::pair<uint64, AssetContainer>> _assets;
		std::vector<std::pair<uint64, std::shared_ptr<DeferredConstruction>>> _deferredConstructions;

        AssetType* Add(uint64 hash, std::unique_ptr<AssetType>&& asset)
        {
            AssetType* result = asset.get();
            auto i = LowerBound(_assets, hash);
            auto t = AssetSet<AssetType>::AssetContainer(std::move(asset), std::unique_ptr<AssetType>());
            _assets.insert(i, std::make_pair(hash, std::move(t)));
            return result;
        }
			
		#if defined(ASSETS_STORE_DIVERGENT)
			using DivAsset = typename AssetTraits<AssetType>::DivAsset;
			std::vector<std::pair<uint64, std::shared_ptr<DivAsset>>> _divergentAssets;
		#endif

        #if defined(ASSETS_STORE_NAMES)
            std::vector<std::pair<uint64, std::string>> _assetNames;
        #endif

        #if defined(ASSETS_MULTITHREADED)
            std::unique_ptr<Utility::Threading::RecursiveMutex> _lock;
        #endif

        AssetSet();
        ~AssetSet();
        AssetSet(const AssetSet&) = delete;
        AssetSet& operator=(const AssetSet&) = delete;
    };

    #if defined(ASSETS_MULTITHREADED)
            // these functions exist in order to avoid the issue
            // including <mutex> into C++/CLR files
        std::unique_ptr<Utility::Threading::Mutex> CreateMutexPtr();
        void LockMutex(Utility::Threading::Mutex&);
        void UnlockMutex(Utility::Threading::Mutex&);

        std::unique_ptr<Utility::Threading::RecursiveMutex> CreateRecursiveMutexPtr();
        void LockMutex(Utility::Threading::RecursiveMutex&);
        void UnlockMutex(Utility::Threading::RecursiveMutex&);

        template <typename AssetType>
            class AssetSetPtr // : public std::unique_lock<Utility::Threading::Mutex>
        {
        public:
            AssetSet<AssetType>* operator->() const never_throws { return _assetSet; }
            AssetSet<AssetType>& operator*() const never_throws { return *_assetSet; }
            AssetSet<AssetType>* get() const never_throws { return _assetSet; }

            AssetSetPtr(AssetSet<AssetType>& assetSet)
                : _assetSet(&assetSet) 
            {
                LockMutex(*_assetSet->_lock);
            }
            ~AssetSetPtr() 
            {
                if (_assetSet)
                    UnlockMutex(*_assetSet->_lock);
            }

            AssetSetPtr(AssetSetPtr&& moveFrom) never_throws
                : _assetSet(moveFrom._assetSet) { moveFrom._assetSet = nullptr; }

            AssetSetPtr& operator=(AssetSetPtr&& moveFrom) never_throws
            {
                _assetSet = moveFrom._assetSet;
                moveFrom._assetSet = nullptr;
                return *this;
            }
        private:
            AssetSet<AssetType>*    _assetSet;
        };
    #else
        template<typename AssetType>
            using AssetSetPtr = AssetSet<AssetType>*;
    #endif

        // (utility functions pulled out-of-line)
    void LogHeader(unsigned count, const char typeName[]);
    void LogAssetName(unsigned index, const char name[]);
    void InsertAssetName(   
        std::vector<std::pair<uint64, std::string>>& assetNames, 
        uint64 hash, const std::string& name);
    void InsertAssetNameNoCollision(   
        std::vector<std::pair<uint64, std::string>>& assetNames, 
        uint64 hash, const std::string& name);

    template<typename AssetType>
        AssetSetPtr<AssetType> GetAssetSet() 
    {
        static AssetSet<AssetType>* set = nullptr;
        if (!set)
            set = GetAssetSetManager().GetSetForType<AssetType>();
            
        #if defined(ASSETS_STORE_NAMES)
                // These should agree. If there's a mismatch, there may be a threading problem
            assert(set->_assets.size() == set->_assetNames.size());
        #endif

        #if defined(ASSETS_MULTITHREADED)
            return AssetSetPtr<AssetType>(*set);
        #else
                //  When not multithreaded, check the thread ids for safety.
                //  We have to check the thread ids
            assert(GetAssetSetManager().IsBoundThread());  
            return *set;
        #endif
    }
}}
