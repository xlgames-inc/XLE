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
        const std::basic_string<ResChar>& GetName() const { return _name; }
        uint64_t GetAssetId() const { return _assetId; }
        uint64_t GetTypeCode() const { return _typeCode; }

		ITransaction(StringSection<ResChar> name, uint64_t assetId, uint64_t typeCode, std::shared_ptr<UndoQueue> undoQueue);
		virtual ~ITransaction();
	protected:
		std::shared_ptr<UndoQueue> _undoQueue;
        std::basic_string<ResChar> _name;
        uint64_t _assetId, _typeCode;
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

	using TransactionId = uint32_t;

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

		DivergentAssetBase();
		virtual ~DivergentAssetBase();
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
            uint64_t assetId, uint64_t typeCode,
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
		uint64_t _assetId, _typeCode;
	};

	template <typename Asset>
		class DivergentAsset : public DivergentAssetBase
	{
	public:
		const Asset& GetAsset() const;

        bool HasChanges() const { return _workingCopy!=nullptr; }
		TransactionId AbandonChanges();
        const AssetIdentifier& GetIdentifier() const { return *_identifier; }

        std::shared_ptr<DivergentTransaction<Asset>> Transaction_Begin(const char name[], const std::shared_ptr<UndoQueue>& undoQueue = nullptr);

		DivergentAsset(
			const std::shared_ptr<Asset>& pristineCopy,
			uint64_t assetId, uint64_t typeCode,
			const AssetIdentifier& identifer);
		~DivergentAsset();
	protected:
		std::shared_ptr<Asset>				_pristineCopy;
		std::shared_ptr<Asset>				_workingCopy;
		uint64_t							_assetId, _typeCode;
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
			uint64_t assetId, uint64_t typeCode,
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
		return *_pristineCopy;
	}

	template<typename Asset>
		std::shared_ptr<DivergentTransaction<Asset>> DivergentAsset<Asset>::Transaction_Begin(const char name[], const std::shared_ptr<UndoQueue>& undoQueue)
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
                name, _assetId, _typeCode, _identifier, _workingCopy, undoQueue);
        }
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
		DivergentAsset<Asset>::DivergentAsset(
			const std::shared_ptr<Asset>& pristineCopy,
			uint64_t assetId, uint64_t typeCode,
            const AssetIdentifier& identifer)
	: DivergentAssetBase(std::weak_ptr<UndoQueue>())
	, _pristineCopy(pristineCopy)
	, _assetId(assetId)
	, _typeCode(typeCode)
    , _identifier(std::make_shared<AssetIdentifier>(identifer))
	{
	}

	template<typename Asset>
		DivergentAsset<Asset>::~DivergentAsset() {}

	template<typename Asset, typename... Params>
		std::shared_ptr<DivergentAsset<Asset>> GetDivergentAsset(Params...);
}

