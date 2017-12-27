#pragma once

#include "AssetFuture.h"
#include "DeferredConstruction.h"
#include "DepVal.h"
#include "../Utility/Threading/Mutex.h"
#include <memory>

namespace Assets
{
///////////////////////////////////////////////////////////////////////////////////////////////////

	class IDefaultAssetHeap
	{
	public:
		virtual void            Clear() = 0;
		virtual void            LogReport() const = 0;

		virtual unsigned        GetDivergentCount() const = 0;
		virtual uint64          GetDivergentId(unsigned index) const = 0;
		virtual bool            DivergentHasChanges(unsigned index) const = 0;

		virtual ~IDefaultAssetHeap();
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename AssetType>
		class DefaultAssetHeap : public IDefaultAssetHeap
	{
	public:
		template<typename... Params>
			AssetFuturePtr<AssetType> Get(Params...);

		void            Clear();
		void            LogReport() const;

		unsigned        GetDivergentCount() const;
		uint64          GetDivergentId(unsigned index) const;
		bool            DivergentHasChanges(unsigned index) const;

		DefaultAssetHeap();
		~DefaultAssetHeap();
		DefaultAssetHeap(const DefaultAssetHeap&) = delete;
		DefaultAssetHeap& operator=(const DefaultAssetHeap&) = delete;
	private:
		Threading::Mutex _lock;
		std::vector<std::pair<uint64_t, AssetFuturePtr<AssetType>>> _assets;

		template<typename... Params>
			AssetFuturePtr<AssetType> MakeFuture(Params...);
	};

	template<typename AssetType>
		static bool IsInvalidated(const AssetFuture<AssetType>& future)
	{
		if (future.GetAssetState() != AssetState::Ready) return false;
		return future.Actualize()->GetDependencyValidation()->GetValidationIndex() > 0;
	}

	template<typename AssetType>
		template<typename... Params>
			auto DefaultAssetHeap<AssetType>::Get(Params... initialisers) -> AssetFuturePtr<AssetType>
	{
		auto hash = Internal::BuildHash(initialisers...);

		ScopedLock(_lock);
		auto i = LowerBound(_assets, hash);
		if (i != _assets.end() && i->first == hash)
			if (!IsInvalidated(*i->second))
				return i->second;

		auto newFuture = MakeFuture(std::forward<Params>(initialisers)...);
		_assets.insert(i, {hash, newFuture});
		return newFuture;
	}

	template<typename AssetType>
		template<typename... Params>
			auto DefaultAssetHeap<AssetType>::MakeFuture(Params... initialisers) -> AssetFuturePtr<AssetType>
	{
		auto stringInitializer = Internal::AsString(initialisers...);	// (used for tracking/debugging purposes)
		auto future = std::make_shared<AssetFuture<AssetType>>(stringInitializer);
		AutoConstructToFuture<AssetType>(*future, std::forward<Params>(initialisers)...);
		return future;
	}

	template<typename AssetType>
		DefaultAssetHeap<AssetType>::DefaultAssetHeap() {}

	template<typename AssetType>
		DefaultAssetHeap<AssetType>::~DefaultAssetHeap() {}


	
	template<typename AssetType>
		void            DefaultAssetHeap<AssetType>::Clear() {}
	template<typename AssetType>
		void            DefaultAssetHeap<AssetType>::LogReport() const {}

	template<typename AssetType>
		unsigned        DefaultAssetHeap<AssetType>::GetDivergentCount() const { return 0; }
	template<typename AssetType>
		uint64          DefaultAssetHeap<AssetType>::GetDivergentId(unsigned index) const { return 0u; }
	template<typename AssetType>
		bool            DefaultAssetHeap<AssetType>::DivergentHasChanges(unsigned index) const { return false; }

}
