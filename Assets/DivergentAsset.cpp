// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DivergentAsset.h"
#include "CompileAndAsyncManager.h"
#include "IntermediateAssets.h"
#include "AssetServices.h"

namespace Assets
{
    void DivergentAssetBase::AssetIdentifier::OnChange()
    {
        // We need to mark the target file invalidated.
        // this is a little strange, because the target file
        // hasn't actually changed. 
        // 
        // But this is required because some dependent assets
        // don't have a dependency on the asset itself (just
        // on the underlying file). Invalidating the file ensures
        // that we invoke a update on all assets that require it.

        if (_targetFilename.empty()) return;

        auto fn = _targetFilename;
        auto paramStart = fn.find_last_of(':');
        if (paramStart != std::basic_string<ResChar>::npos)
            fn.erase(fn.begin() + paramStart, fn.end());

        Services::GetAsyncMan().GetIntermediateStore().ShadowFile(fn.c_str());
    }


    ITransaction::ITransaction(const char name[], uint64 assetId, uint64 typeCode, std::shared_ptr<UndoQueue> undoQueue)
	: _undoQueue(std::move(undoQueue))
    , _name(name), _assetId(assetId), _typeCode(typeCode)
	{}

	ITransaction::~ITransaction()
	{}

	void UndoQueue::PushBack(std::shared_ptr<ITransaction> transaction) {}
    std::shared_ptr<ITransaction> UndoQueue::GetTop() { return nullptr; }
    unsigned UndoQueue::GetCount() { return 0; }
    ITransaction* UndoQueue::GetTransaction(unsigned) { return nullptr; }
	UndoQueue::UndoQueue() {}
	UndoQueue::~UndoQueue() {}

	DivergentAssetBase::DivergentAssetBase(std::weak_ptr<UndoQueue> undoQueue)
	: _undoQueue(std::move(undoQueue))
	{}

	DivergentAssetBase::~DivergentAssetBase() {}

}

