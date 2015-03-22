// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DivergentAsset.h"

namespace Assets
{

    ITransaction::ITransaction(const char name[], std::shared_ptr<UndoQueue> undoQueue)
	: _undoQueue(std::move(undoQueue))
    , _name(name)
	{}

	ITransaction::~ITransaction()
	{}

	void UndoQueue::PushBack(std::shared_ptr<ITransaction> transaction) {}
    std::shared_ptr<ITransaction> UndoQueue::GetTop() { return nullptr; }
	UndoQueue::UndoQueue() {}
	UndoQueue::~UndoQueue() {}

	DivergentAssetBase::DivergentAssetBase(std::weak_ptr<UndoQueue> undoQueue)
	: _undoQueue(std::move(undoQueue))
	{}

	DivergentAssetBase::~DivergentAssetBase() {}
}

