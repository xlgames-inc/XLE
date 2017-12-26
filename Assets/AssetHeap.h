#pragma once

#include "AssetUtils.h"					// (for PendingOperationMarker)
#include "AssetSetManager.h"
#include "AssetsInternal.h"
#include "DeferredConstruction.h"
#include "../Utility/Threading/Mutex.h"
#include <memory>

namespace Assets
{
	template<typename AssetType> class IAssetHeap;

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename AssetType>
		using AssetPtr = std::shared_ptr<AssetType>;

	template<typename AssetType>
		class AssetFuture : public IAsyncMarker
	{
	public:
		const AssetPtr<AssetType>& Actualize() const;
		void			OnFrameBarrier();
		
		AssetState		GetAssetState() const;
		AssetState		StallWhilePending() const;

		const std::string&     Initializer() const { return _initializer; }

		explicit AssetFuture(const std::string& initializer);
		~AssetFuture();

		AssetFuture(AssetFuture&&) never_throws = default;
		AssetFuture& operator=(AssetFuture&&) never_throws = default;

		void SetAsset(std::unique_ptr<AssetType>&&, const std::string& msg, AssetState state);
	private:
		mutable Threading::Mutex		_lock;
		mutable Threading::Conditional	_conditional;

		AssetState			_state;
		AssetPtr<AssetType> _actualized;
		std::string			_actualizationMsg;

		AssetPtr<AssetType> _pending;
		AssetState			_pendingState;
		std::string			_pendingActualizationMsg;

		std::string			_initializer;	// stored for debugging purposes
	};

	template<typename AssetType>
		using AssetFuturePtr = std::shared_ptr<AssetFuture<AssetType>>;

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename AssetType>
		const AssetPtr<AssetType>& AssetFuture<AssetType>::Actualize() const
	{
		auto state = _state;
		if (state == AssetState::Ready)
			return _actualized;	// once the state is set to "Ready" neither it nor _actualized can change -- so we've safe to access it without a lock

		ScopedLock(_lock); 
		
		if (state == AssetState::Pending) {
			// Note that the asset have have completed loading here -- but it may still be in it's "pending" state,
			// waiting for a frame barrier. Let's include the pending state in the exception message to make it clearer. 
			const char* exceptionMsg = "Still pending in actualization request";
			switch (_pendingState) {
			case AssetState::Invalid: "Still pending in actualization request (but pending invalid)"; break;
			case AssetState::Ready: "Still pending in actualization request (but pending ready)"; break;
			}
			Throw(Exceptions::PendingAsset(MakeStringSection(_identifier), exceptionMsg));
		}
		
		assert(state == AssetState::Invalid);
		Throw(Exceptions::InvalidAsset(MakeStringSection(_identifier), _actualizationMsg.c_str()));
	}

	template<typename AssetType>
		void AssetFuture<AssetType>::OnFrameBarrier() 
	{
			// lock & swap the asset into the front buffer. We only do this during the "frame barrier" phase, to
			// prevent assets from changing in the middle of a single frame.
		ScopedLock(_lock);
		if (_state == AssetState::Pending && _pendingState != AssetState::Pending) {
			_actualized = std::move(_pending);
			_actualizationMsg = std::move(_pendingActualizationMsg);
			// Note that we must change "_state" last -- because another thread can access _actualized without a mutex lock
			// when _state is set to AssetState::Ready
			// we should also consider a cache flush here to ensure the CPU commits in the correct order
			_state = _pendingState;
		}
	}

	template<typename AssetType>
		AssetState		AssetFuture<AssetType>::GetAssetState() const
	{
		return _state;
	}

	template<typename AssetType>
		AssetState		AssetFuture<AssetType>::StallWhilePending() const
	{
		auto* that = const_cast<AssetFuture<AssetType>*>(this);	// hack to defeat the "const" on this method
		std::unique_lock<decltype(that->_lock)> lock(that->_lock);
		for (;;) {
			if (that->_state != AssetState::Pending) return that->_state;
			if (that->_pendingState != AssetState::Pending) {
				// Force the background version into the foreground (see OnFrameBarrier)
				// This is required because we can be woken up by SetAsset, which only set the
				// background asset. But the caller most likely needs the asset right now, so
				// we've got to swap it into the foreground.
				// There is a problem if the caller is using bothActualize() and StallWhilePending() on the
				// same asset in the same frame -- in this case, the order can have side effects.
				assert(that->_state == AssetState::Pending);
				that->_actualized = std::move(that->_pending);
				that->_actualizationMsg = std::move(that->_pendingActualizationMsg);
				that->_state = that->_pendingState;
				return that->_state;
			}
			that->_conditional.wait(lock);
		}
	}

	template<typename AssetType>
		void AssetFuture<AssetType>::SetAsset(std::unique_ptr<AssetType>&& newAsset, const std::string& msg, AssetState state)
	{
		{
			ScopedLock(_lock);
			_pending = std::move(newAsset);
			_pendingState = state;
			_pendingActualizationMsg = msg;
		}
		_conditional.notify_all();
	}

	template<typename AssetType>
		AssetFuture<AssetType>::AssetFuture(const std::string& initializer)
	: _initializer(initializer)
	{
		// Technically, we're not actually "pending" yet, because no background operation has begun.
		// If this future is not bound to a specific operation, we'll be stuck in pending state
		// forever.
		_state = AssetState::Pending;
		_pendingState = AssetState::Pending;
	}

	template<typename AssetType>
		AssetFuture<AssetType>::~AssetFuture() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static std::string AsBlob(const std::exception& e) { return e.what(); }

	template<typename AssetType, typename... Params>
		void AutoConstructToFuture(const std::shared_ptr<AssetFuture<AssetType>>& future, Params... params)
	{
		TRY {
			auto asset = AutoConstructAsset<AssetType>(params...);
			future->SetAsset(std::move(asset), {}, asset ? AssetState::Ready : AssetState::Invalid);
		} CATCH(const std::exception& e) {
			future->SetAsset(nullptr, AsBlob(e), AssetState::Invalid);
		} CATCH_END
	}

	template<typename AssetType>
		static void DefaultCompilerConstruction(
			std::shared_ptr<AssetFuture<AssetType>> future,
			const StringSection<ResChar> initializers[], unsigned initializerCount,
			uint64 compileTypeCode = GetCompileProcessType<AssetType>())
	{
		// Begin a compilation operation via the registered compilers for this type.
		// Our deferred constructor will wait for the completion of that compilation operation,
		// and then construct the final asset from the result

		auto marker = Internal::BeginCompileOperation(compileTypeCode, initializers, initializerCount);
		std::basic_string<ResChar> init0 = initializers[0].AsString();

		// Attempt to load the existing asset immediately. In some cases we should fall back to a recompile (such as, if the
		// version number is bad). We could attempt to push this into a background thread, also

		auto existingArtifact = marker->GetExistingAsset();
		if (existingArtifact->GetDependencyValidation() && existingArtifact->GetDependencyValidation()->GetValidationIndex()==0) {
			bool doRecompile = false;
			AutoConstructToFuture(future, *existingArtifact, MakeStringSection(init0))
			if (!doRecompile) return;
		}

		auto pendingCompile = marker->InvokeCompile();
		std::weak_ptr<AssetFuture<AssetType>> weakPtrToFuture = future;
		// We must poll the compile operation every frame, and construct the asset when it is ready. Note that we're
		// still going to end up constructing the asset in the main thread.
		OnFrameBarrier(
			[pendingCompile, weakPtrToFuture, init0]() -> bool {
				auto state = pendingCompile->GetAssetState();
				if (state == AssetState::Pending) return true;

				auto thatFuture = weakPtrToFuture.lock();
				if (!thatFuture) return false;
					
				if (state == AssetState::Invalid) {
					thatFuture->SetAsset(nullptr, {}, AssetState::Invalid);
					return false;
				}

				assert(state == AssetState::Ready);
				AutoConstructToFuture(future, *pendingCompile->GetArtifacts()[0].second, MakeStringSection(init0));
				return false;
			}
		);

		return result;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	/// <summary>Interface for a class that manages a collection of loaded assets<summary>
	template<typename AssetType>
		class IAssetHeap
	{
	public:
		virtual ~IAssetHeap();
	};

	template<typename AssetType>
		class DefaultAssetHeap : public IAssetHeap<AssetType>
	{
	public:
		template<typename... Params>
			AssetFuturePtr<AssetType> Get(Params...);

		DefaultAssetHeap();
		~DefaultAssetHeap();
		DefaultAssetHeap(const DefaultAssetHeap&) = delete;
		DefaultAssetHeap& operator=(const DefaultAssetHeap&) = delete;
	private:
		Threading::Mutex _lock;
		std::vector<std::pair<uint64_t, _AssetFuturePtr>> _assets;

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
		AutoConstructToFuture<AssetType>(future, std::forward<Params>(initialisers)...);
		return future;
	}

	template<typename AssetType>
		DefaultAssetHeap<AssetType>::DefaultAssetHeap() {}

	template<typename AssetType>
		DefaultAssetHeap<AssetType>::~DefaultAssetHeap() {}

	template<typename AssetType> IAssetHeap<AssetType>::~IAssetHeap() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename AssetType>
		DefaultAssetHeap<AssetType>& GetBasicAssetHeap()
	{
		static std::shared_ptr<DefaultAssetHeap<AssetType>> heap = std::make_shared<DefaultAssetHeap<AssetType>>();
		return *heap;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename AssetType, typename... Params>
		std::shared_ptr<AssetFuture<AssetType>> MakeAsset(Params... initialisers)
	{
		return GetBasicAssetHeap<AssetType>().Get(std::forward<Params>(initialisers)...);
	}

	template<typename AssetType, typename... Params>
		const AssetType& Actualize(Params... initialisers)
	{
		auto future = MakeAsset<AssetType>(initialisers...);
		future->StallWhilePending();
		return *future->Actualize();
	}

}
