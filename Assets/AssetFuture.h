#pragma once

#include "AssetsCore.h"
#include "../Utility/Threading/Mutex.h"
#include <memory>
#include <string>

namespace Assets
{
	    /// <summary>Records the status of asynchronous operation, very much like a std::promise<AssetState></summary>
	class IAsyncMarker
	{
	public:
		virtual AssetState		GetAssetState() const = 0;
		virtual AssetState		StallWhilePending() const =0;
		virtual ~IAsyncMarker();
	};

		////////////////////////////////////////////////////////////////////////////////////////////////

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

		////////////////////////////////////////////////////////////////////////////////////////////////

    class GenericFuture : public IAsyncMarker, public std::enable_shared_from_this<GenericFuture>
    {
    public:
        AssetState		GetAssetState() const { return _state; }
        AssetState		StallWhilePending() const;
        const char*     Initializer() const;  // "initializer" interface only provided in debug builds, and only intended for debugging

        GenericFuture(AssetState state = AssetState::Pending);
        ~GenericFuture();

		GenericFuture(GenericFuture&&) = delete;
		GenericFuture& operator=(GenericFuture&&) = delete;
		GenericFuture(const GenericFuture&) = delete;
		GenericFuture& operator=(const GenericFuture&) = delete;

		void	SetState(AssetState newState);
		void	SetInitializer(const ResChar initializer[]);

	private:
		AssetState _state;
		DEBUG_ONLY(ResChar _initializer[MaxPath];)
    };

		////////////////////////////////////////////////////////////////////////////////////////////////

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
			Throw(Exceptions::PendingAsset(MakeStringSection(_initializer), exceptionMsg));
		}
		
		assert(state == AssetState::Invalid);
		Throw(Exceptions::InvalidAsset(MakeStringSection(_initializer), _actualizationMsg.c_str()));
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

}
