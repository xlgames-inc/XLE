// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DepVal.h"
#include "AssetsCore.h"
#include "../Utility/IteratorUtils.h"
#include <memory>
#include <string>

namespace Assets
{
    typedef char ResChar;

///////////////////////////////////////////////////////////////////////////////////////////////////
	class UndoQueue;

	class ITransaction
	{
	public:
		const std::string& GetName() const { return _transactionName; }
		ITransaction(const char transactionName[], const std::shared_ptr<UndoQueue>& undoQueue);
		virtual ~ITransaction();
	protected:
		std::weak_ptr<UndoQueue> _undoQueue;
		std::string _transactionName;
	};

	class UndoQueue
	{
	public:
		void PushBack(const std::shared_ptr<ITransaction>& transaction);
        std::shared_ptr<ITransaction> GetTop();

        unsigned GetCount();
        ITransaction* GetTransaction(unsigned index);

		UndoQueue();
		~UndoQueue();
	};

	using TransactionId = uint32_t;

	template <typename Asset>
		class DivergentTransaction : public ITransaction
	{
	public:
		Asset& GetAsset();

		virtual TransactionId   Commit();
		virtual void			Cancel();

		DivergentTransaction(
			const char transactionName[],
			const std::shared_ptr<Asset>& workingCopy,
			const std::shared_ptr<UndoQueue>& undoQueue);
		virtual ~DivergentTransaction();

	protected:
		std::shared_ptr<Asset> _transactionCopy;
		std::shared_ptr<Asset> _liveCopy;
        std::shared_ptr<Asset> _originalCopy;

		enum class State { NoAction, Modified, Committed };
		State _state;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	class DivergentAssetBase
	{
	public:
		virtual bool HasChanges() const = 0;
		DivergentAssetBase();
		virtual ~DivergentAssetBase();
	};

	template <typename Asset>
		class DivergentAsset : public DivergentAssetBase
	{
	public:
		const std::shared_ptr<Asset>& GetWorkingAsset() const { return _workingCopy; }
		const std::shared_ptr<Asset>& GetPristineCopy() const { return _pristineCopy; }

		TransactionId AbandonChanges();
		bool HasChanges() const { return _hasChanges; }

        std::shared_ptr<DivergentTransaction<Asset>> Transaction_Begin(const char name[], const std::shared_ptr<UndoQueue>& undoQueue = nullptr);

		DivergentAsset(const std::shared_ptr<Asset>& pristineCopy);
		~DivergentAsset();
	protected:
		std::shared_ptr<Asset>	_pristineCopy;
		std::shared_ptr<Asset>	_workingCopy;
		bool					_hasChanges;

        std::shared_ptr<DivergentTransaction<Asset>> _lastTransaction;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename Asset>
		Asset& DivergentTransaction<Asset>::GetAsset()
	{
		_state = State::Modified;
		if (!_transactionCopy)
			_transactionCopy = std::make_shared<Asset>(*_liveCopy.get());
		return *_transactionCopy.get();
	}

	template<typename Asset>
		TransactionId    DivergentTransaction<Asset>::Commit()
	{
		if (_transactionCopy) {
            Asset temp = std::move(*_liveCopy);
            *_liveCopy = std::move(*_transactionCopy);
            if (!_originalCopy)
                _originalCopy = std::move(_transactionCopy);
			_transactionCopy.reset();
            *_originalCopy = std::move(temp);
			_state = State::Committed;

			// auto newId = ++_identifer->_transactionId;
            const_cast<::Assets::DependencyValidation*>(_liveCopy->GetDependencyValidation().get())->OnChange();
            // _identifer->OnChange();
			return 0u; // newId
		}
		return ~0u;
	}
	
	template<typename Asset>
		void    DivergentTransaction<Asset>::Cancel()
	{
		if (_originalCopy) {
            *_liveCopy = std::move(*_originalCopy);
		}

        _transactionCopy.reset();
        _originalCopy.reset();
		_state = State::NoAction;
	}

	template<typename Asset>
		DivergentTransaction<Asset>::DivergentTransaction(
			const char transactionName[],
			const std::shared_ptr<Asset>& workingCopy,
			const std::shared_ptr<UndoQueue>& undoQueue)
        : ITransaction(transactionName, undoQueue)
        , _liveCopy(workingCopy)
		, _state(State::NoAction)
	{
	}

	template<typename Asset>
		DivergentTransaction<Asset>::~DivergentTransaction()
	{
        if (_transactionCopy) { Commit(); }
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename Asset>
		std::shared_ptr<DivergentTransaction<Asset>> DivergentAsset<Asset>::Transaction_Begin(const char name[], const std::shared_ptr<UndoQueue>& undoQueue)
	{
            // If we have a "_lastTransaction" and that transaction is on the top of the undoqueue
            // and the name of that transaction matches the name for this new transaction,
            // then we can combine them together and just reuse the same one.
            //
            // We might also want a time-out here. If the transaction hasn't been used for a while,
            // or if the transaction was first created some time ago, then we could restart a
            // new transaction. 
            // This kind of time-out is important for undo. Each transaction becomes a single undo 
            // step. So we should attempt to group together actions as transactions in order to make
            // the undo operation seem most natural.
            //
        if (undoQueue) {
            if (_lastTransaction != undoQueue->GetTop()) {
                _lastTransaction.reset();
            }
        }

        if (_lastTransaction && XlCompareString(_lastTransaction->GetName().c_str(), name) != 0) {
            _lastTransaction.reset();
        }

        if (!_lastTransaction) {
            _lastTransaction = std::make_shared<DivergentTransaction<Asset>>(
                name, _workingCopy, undoQueue);
        }
		_hasChanges = true;
        return _lastTransaction;
	}

	template<typename Asset>
		TransactionId DivergentAsset<Asset>::AbandonChanges()
	{
		_lastTransaction.reset();
		_workingCopy.reset();
		auto newId = ++_identifier->_transactionId;
		// kick the pristine version to encourage a reload from disk now
		const_cast<::Assets::DependencyValidation*>(GetPristineCopy().GetDependencyValidation().get())->OnChange();
		_identifier->OnChange();
		return newId;
	}

	template<typename Asset>
		DivergentAsset<Asset>::DivergentAsset(const std::shared_ptr<Asset>& pristineCopy)
	: _pristineCopy(pristineCopy)
	, _hasChanges(false)
	{
		_workingCopy = std::make_shared<Asset>(*_pristineCopy);
	}

	template<typename Asset>
		DivergentAsset<Asset>::~DivergentAsset() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	class DivergentAssetManager
	{
	public:
		struct Record
		{
			uint64_t	_typeCode;
			uint64_t	_idInAssetHeap;
			rstring		_identifier;
			bool		_hasChanges;
		};
		auto	GetAssets() const ->std::vector<Record>;
		auto	GetAsset(uint64_t typeCode, uint64_t id) const -> std::shared_ptr<DivergentAssetBase>;
		void	AddAsset(uint64_t typeCode, uint64_t id, const rstring& identifier, const std::shared_ptr<DivergentAssetBase>&);

		DivergentAssetManager(); 
		~DivergentAssetManager();
	private:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename AssetType, typename... Params>
		std::shared_ptr<DivergentAsset<AssetType>> CreateDivergentAsset(Params... params);

}

