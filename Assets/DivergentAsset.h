// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace Assets
{
	class UndoQueue;

///////////////////////////////////////////////////////////////////////////////////////////////////

	class ITransaction
	{
	public:

		ITransaction(std::shared_ptr<UndoQueue> undoQueue);
		virtual ~ITransaction();
	protected:
		std::shared_ptr<UndoQueue> _undoQueue;
	};

	class UndoQueue
	{
	public:
		void PushBack(std::shared_ptr<ITransaction> transaction);

		UndoQueue();
		~UndoQueue();
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	class DivergentAssetBase
	{
	public:
		DivergentAssetBase(std::weak_ptr<UndoQueue> undoQueue);
		virtual ~DivergentAssetBase();
	protected:
		std::weak_ptr<UndoQueue> _undoQueue;
	};

	template <typename Asset>
		class DivergentTransaction : public ITransaction
	{
	public:
		Asset& GetAsset();

		virtual void    Commit();
		virtual void    Cancel();

		DivergentTransaction(
			std::shared_ptr<Asset> workingCopy,
			std::shared_ptr<UndoQueue> undoQueue);
		virtual ~DivergentTransaction();

	protected:
		std::shared_ptr<Asset> _transactionCopy;
		std::shared_ptr<Asset> _workingCopy;
		std::shared_ptr<UndoQueue> _undoQueue;

		enum class State { NoAction, Modified, Committed };
		State _state;
	};

	template <typename Asset>
		class DivergentAsset : public DivergentAssetBase
	{
	public:
		const Asset& GetAsset() const;

		std::shared_ptr<DivergentTransaction<Asset>> Transaction_Begin();

		DivergentAsset(const Asset& pristineCopy, std::weak_ptr<UndoQueue> undoQueue);
		~DivergentAsset();
	protected:
		const Asset*				_pristineCopy;
		std::shared_ptr<Asset>		_workingCopy;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename Asset>
		Asset& DivergentTransaction<Asset>::GetAsset()
	{
			// we can't get the asset again after commit
		assert(_state != State::Committed);

		_state = (_state == State::NoAction) ? State::Modified : State::Committed;
		if (!_transactionCopy)
			_transactionCopy = std::make_shared<Asset>(*workingCopy.get()); 
		return *_transactionCopy.get();
	}

	template<typename Asset>
		void    DivergentTransaction<Asset>::Commit()
	{
		if (_transactionCopy) {
			Asset temp = std::move(*_workingCopy);
			*_workingCopy = std::move(*_transactionCopy);
			*_transactionCopy = std::move(temp);
			_state = State::Committed;
		}
	}
	
	template<typename Asset>
		void    DivergentTransaction<Asset>::Cancel()
	{
		if (state == State::Committed && _transactionCopy) {
			*_workingCopy = *_transactionCopy;
		}

		_transactionCopy.reset();
		_state = State::NoAction;
	}

	template<typename Asset>
		DivergentTransaction<Asset>::DivergentTransaction(
			std::shared_ptr<Asset> workingCopy,
			std::shared_ptr<UndoQueue> undoQueue)
		: _workingCopy(std::move(workingCopy))
		, _undoQueue(std::move(undoQueue))
		, _state(State::NoAction)
	{
	}

	template<typename Asset>
		DivergentTransaction<Asset>::~DivergentTransaction()
	{
		if (_state == State::Modified) { Commit(); }
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename Asset>
		const Asset& DivergentAsset<Asset>::GetAsset() const
	{
		if (_workingCopy) return *_workingCopy.get();
		return *_pristineCopy;
	}

	template<typename Asset>
		std::shared_ptr<DivergentTransaction<Asset>> DivergentAsset<Asset>::Transaction_Begin()
	{
		if (!_workingCopy) {
			_workingCopy = std::make_shared<Asset>(*_pristineCopy);
		}
		return std::make_shared<DivergentTransaction<Asset>>(_workingCopy);
	}

	template<typename Asset>
		DivergentAsset<Asset>::DivergentAsset(
			const Asset& pristineCopy, std::weak_ptr<UndoQueue> undoQueue)
	: DivergentAssetBase(std::move(undoQueue))
	{
		_pristineCopy = &pristineCopy;
	}

	template<typename Asset>
		DivergentAsset<Asset>::~DivergentAsset() {}
}

