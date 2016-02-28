// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetSetInternal.h"
#include "../Utility/IteratorUtils.h"
#include "../Core/Types.h"
#include <memory>
#include <string>

namespace Assets
{
	class UndoQueue;
    typedef char ResChar;

///////////////////////////////////////////////////////////////////////////////////////////////////

	class ITransaction
	{
	public:
        const std::basic_string<ResChar>& GetName() const { return _name; }
        uint64 GetAssetId() const { return _assetId; }
        uint64 GetTypeCode() const { return _typeCode; }

		ITransaction(const ResChar name[], uint64 assetId, uint64 typeCode, std::shared_ptr<UndoQueue> undoQueue);
		virtual ~ITransaction();
	protected:
		std::shared_ptr<UndoQueue> _undoQueue;
        std::basic_string<ResChar> _name;
        uint64 _assetId, _typeCode;
	};

	class UndoQueue
	{
	public:
		void PushBack(std::shared_ptr<ITransaction> transaction);
        std::shared_ptr<ITransaction> GetTop();

        unsigned GetCount();
        ITransaction* GetTransaction(unsigned index);

		UndoQueue();
		~UndoQueue();
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	using TransactionId = uint32;

	class DivergentAssetBase
	{
	public:
        class AssetIdentifier
        {
        public:
            std::basic_string<ResChar> _descriptiveName;
            std::basic_string<ResChar> _targetFilename;
			TransactionId _transactionId;

            void OnChange();
			AssetIdentifier() : _transactionId(0) {}
        };

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

		virtual TransactionId   Commit();
		virtual void			Cancel();

		DivergentTransaction(
            const char name[],
            uint64 assetId, uint64 typeCode,
            const std::shared_ptr<DivergentAssetBase::AssetIdentifier>& identifer,
			std::shared_ptr<Asset> workingCopy,
			std::shared_ptr<UndoQueue> undoQueue);
		virtual ~DivergentTransaction();

	protected:
		std::shared_ptr<Asset> _transactionCopy;
		std::shared_ptr<Asset> _liveCopy;
        std::shared_ptr<Asset> _originalCopy;
        std::shared_ptr<DivergentAssetBase::AssetIdentifier> _identifer;

		enum class State { NoAction, Modified, Committed };
		State _state;
        uint64 _assetId, _typeCode;
	};

    template <typename Transaction>
        class TransactionPtr : public std::shared_ptr<Transaction>		/* note -- we risk a slice here. Could we make it private inheritance? */
    {
    public:
        // using std::shared_ptr::shared_ptr;       // (not compiling in C++/CLI?)

        TransactionPtr(std::shared_ptr<Transaction> ptr) : std::shared_ptr<Transaction>(std::move(ptr)) {}
        ~TransactionPtr() { if (get()) get()->Commit(); }
    };

	template <typename Asset>
		class DivergentAsset : public DivergentAssetBase
	{
	public:
		const Asset& GetAsset() const;

        bool HasChanges() const { return _workingCopy!=nullptr; }
		TransactionId AbandonChanges();
        const AssetIdentifier& GetIdentifier() const { return *_identifier; }

        TransactionPtr<DivergentTransaction<Asset>> Transaction_Begin(const char name[]);

		DivergentAsset(
            const Internal::AssetSet<Asset>& assetSet, 
            uint64 assetId, const AssetIdentifier& identifer);
		~DivergentAsset();
	protected:
		const Internal::AssetSet<Asset>*	_assetSet;
		std::shared_ptr<Asset>				_workingCopy;
        uint64								_assetId;
        std::shared_ptr<AssetIdentifier>	_identifier;

        std::shared_ptr<DivergentTransaction<Asset>> _lastTransaction;

		const Asset& GetPristineCopy() const;
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

			auto newId = ++_identifer->_transactionId;
            const_cast<::Assets::DependencyValidation*>(_liveCopy->GetDependencyValidation().get())->OnChange();
            _identifer->OnChange();
			return newId;
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
            const char name[],
            uint64 assetId, uint64 typeCode,
            const std::shared_ptr<DivergentAssetBase::AssetIdentifier>& identifer,
			std::shared_ptr<Asset> workingCopy,
			std::shared_ptr<UndoQueue> undoQueue)
        : ITransaction(name, assetId, typeCode, std::move(undoQueue))
        , _liveCopy(std::move(workingCopy))
		, _state(State::NoAction)
        , _identifer(identifer)
	{
	}

	template<typename Asset>
		DivergentTransaction<Asset>::~DivergentTransaction()
	{
        if (_transactionCopy) { Commit(); }
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename Asset>
		const Asset& DivergentAsset<Asset>::GetAsset() const
	{
		if (_workingCopy) return *_workingCopy.get();
		return GetPristineCopy();
	}

	template<typename Asset>
		const Asset& DivergentAsset<Asset>::GetPristineCopy() const
	{
		Internal::AssetSetPtr<Asset> lock(*const_cast<Internal::AssetSet<Asset>*>(_assetSet));
		auto i = LowerBound(lock->_assets, _assetId);
		if (i != lock->_assets.end() && i->first == _assetId)
			return *i->second._active;

		// If we get here, we are in trouble. For every divergent asset, there should be an asset
		// in the asset store with the same id. If we don't find it, we have no means to return a valid
		// asset.
		Throw(std::logic_error("Could not retrieve pristine version of divergent asset from the asset store"));
	}

	template<typename Asset>
        TransactionPtr<DivergentTransaction<Asset>> DivergentAsset<Asset>::Transaction_Begin(const char name[])
	{
		if (!_workingCopy) {
			_workingCopy = std::make_shared<Asset>(GetPristineCopy());
		}

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
        auto undoQueue = _undoQueue.lock();
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
                name, _assetId, _assetSet->GetTypeCode(), _identifier, _workingCopy, undoQueue);
        }
        return TransactionPtr<DivergentTransaction<Asset>>(_lastTransaction);
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
		DivergentAsset<Asset>::DivergentAsset(
			const Internal::AssetSet<Asset>& assetSet,
            uint64 assetId, const AssetIdentifier& identifer)
	: DivergentAssetBase(std::weak_ptr<UndoQueue>())
    , _assetId(assetId)
    , _identifier(std::make_shared<AssetIdentifier>(identifer))
	, _assetSet(&assetSet)
	{
	}

	template<typename Asset>
		DivergentAsset<Asset>::~DivergentAsset() {}
}

