#pragma once

#include "AssetsCore.h"
#include "GenericFuture.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/Threading/ThreadingUtils.h"		// (for Threading::YieldTimeSlice() below)
#include <memory>
#include <string>
#include <assert.h>

namespace Assets
{

		////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename AssetType>
		using AssetPtr = std::shared_ptr<AssetType>;

	template<typename AssetType>
		class AssetFuture : public IAsyncMarker
	{
	public:
		const AssetPtr<AssetType>& Actualize() const;
		const AssetPtr<AssetType>& TryActualize() const;

		void			OnFrameBarrier();
		bool			IsOutOfDate() const;
		
		AssetState		GetAssetState() const;
		AssetState		StallWhilePending() const;

		const std::string&     Initializer() const { return _initializer; }

		explicit AssetFuture(const std::string& initializer);
		~AssetFuture();

		AssetFuture(AssetFuture&&) never_throws = default;
		AssetFuture& operator=(AssetFuture&&) never_throws = default;

		void SetAsset(std::unique_ptr<AssetType>&&, const Blob& log);
		void SetInvalidAsset(const DepValPtr& depVal, const Blob& log);
		void SetPollingFunction(std::function<bool(AssetFuture<AssetType>&)>&&);
		
	private:
		mutable Threading::Mutex		_lock;
		mutable Threading::Conditional	_conditional;

		volatile AssetState _state;
		AssetPtr<AssetType> _actualized;
		Blob				_actualizationLog;
		DepValPtr			_actualizedDepVal;

		AssetPtr<AssetType> _pending;
		AssetState			_pendingState;
		Blob				_pendingActualizationLog;
		DepValPtr			_pendingDepVal;

		std::function<bool(AssetFuture<AssetType>&)> _pollingFunction;

		std::string			_initializer;	// stored for debugging purposes
	};

	template<typename AssetType>
		using FuturePtr = std::shared_ptr<AssetFuture<AssetType>>;

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
			Throw(Exceptions::PendingAsset(MakeStringSection(_initializer)));
		}
		
		assert(state == AssetState::Invalid);
		Throw(Exceptions::InvalidAsset(MakeStringSection(_initializer), _actualizedDepVal, _actualizationLog));
	}

	template<typename AssetType>
		const AssetPtr<AssetType>& AssetFuture<AssetType>::TryActualize() const
	{
		auto state = _state;
		if (state == AssetState::Ready)
			return _actualized;
		static AssetPtr<AssetType> dummy;
		return dummy;
	}

	template<typename AssetType>
		void AssetFuture<AssetType>::OnFrameBarrier() 
	{
		auto state = _state;
		if (state != AssetState::Pending) return;

			// lock & swap the asset into the front buffer. We only do this during the "frame barrier" phase, to
			// prevent assets from changing in the middle of a single frame.
		std::unique_lock<decltype(_lock)> lock(_lock);
		if (_pollingFunction) {
			auto pollingFunction = std::move(_pollingFunction);
			lock = {};
			bool pollingResult = pollingFunction(*this);
			lock = std::unique_lock<decltype(_lock)>(_lock);
			assert(!_pollingFunction);
			if (pollingResult) _pollingFunction = std::move(pollingFunction);
		}
		if (_state == AssetState::Pending && _pendingState != AssetState::Pending) {
			_actualized = std::move(_pending);
			_actualizationLog = std::move(_pendingActualizationLog);
			_actualizedDepVal = std::move(_pendingDepVal);
			// Note that we must change "_state" last -- because another thread can access _actualized without a mutex lock
			// when _state is set to AssetState::Ready
			// we should also consider a cache flush here to ensure the CPU commits in the correct order
			_state = _pendingState;
		}
	}

	template<typename AssetType>
		bool			AssetFuture<AssetType>::IsOutOfDate() const
	{
		auto state = _state;
		if (state == AssetState::Pending) return false;
		return _actualizedDepVal && _actualizedDepVal->GetValidationIndex() > 0;
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

		// If we have polling function assigned, we have to poll waiting for
		// it to be completed. Threading is a little complicated here, because
		// the pollingFunction is expected to lock our mutex, and it is not
		// recursive.
		// We also don't particularly want the polling function to be called 
		// from multiple threads at the same time. So, let's take ownership of
		// the polling function, and unlock the future while the polling function
		// is working. This will often result in 3 locks on the same mutex in
		// quick succession from this same thread sometimes.
		if (that->_pollingFunction) {
			auto pollingFunction = std::move(that->_pollingFunction);
			lock = {};

			for (;;) {
				bool pollingResult = pollingFunction(*that);
				if (!pollingResult) break;
				Threading::YieldTimeSlice();
			}
			
			lock = std::unique_lock<decltype(that->_lock)>(that->_lock);
		}

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
				that->_actualizationLog = std::move(that->_pendingActualizationLog);
				that->_state = that->_pendingState;
				return that->_state;
			}
			that->_conditional.wait(lock);
		}
	}

	template<typename AssetType>
		void AssetFuture<AssetType>::SetAsset(std::unique_ptr<AssetType>&& newAsset, const Blob& log)
	{
		{
			ScopedLock(_lock);
			_pending = std::move(newAsset);
			_pendingState = AssetState::Ready;
			_pendingActualizationLog = log;
			_pendingDepVal = _pending ? _pending->GetDependencyValidation() : nullptr;
		}
		_conditional.notify_all();
	}

	template<typename AssetType>
		void AssetFuture<AssetType>::SetInvalidAsset(const DepValPtr& depVal, const Blob& log)
	{
		{
			ScopedLock(_lock);
			_pending = nullptr;
			_pendingState = AssetState::Invalid;
			_pendingActualizationLog = log;
			_pendingDepVal = depVal;
		}
		_conditional.notify_all();
	}

	template<typename AssetType>
		void AssetFuture<AssetType>::SetPollingFunction(std::function<bool(AssetFuture<AssetType>&)>&& newFunction)
	{
		ScopedLock(_lock);
		assert(!_pollingFunction);
		_pollingFunction = std::move(newFunction);
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
