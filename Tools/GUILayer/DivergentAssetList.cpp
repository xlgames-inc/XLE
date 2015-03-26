// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NativeEngineDevice.h"
#include "DivergentAssetList.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/DivergentAsset.h"
#include "MarshalString.h"

namespace GUILayer
{
    private ref class AssetTypeItem
    {
    public:
        property System::String^ Label;

        const Assets::IAssetSet* _set;

        AssetTypeItem(const Assets::IAssetSet& set) : _set(&set)
        { 
            Label = clix::marshalString<clix::E_UTF8>(set.GetTypeName());
        }
    };

    private ref class AssetItem
    {
    public:
        property System::String^ Label;

        const Assets::IAssetSet* _set; 
        uint64 _id;

        AssetItem(const Assets::IAssetSet& set, uint64 id) : _set(&set), _id(id)
        {
            Label = clix::marshalString<clix::E_UTF8>(set.GetAssetName(id));
        }
    };

    private ref class TransactionItem
    {
    public:
        property System::String^ Description;

        TransactionItem(const Assets::ITransaction& transaction)
        {
            Description = clix::marshalString<clix::E_UTF8>(transaction.GetName());
        }
    };

    System::Collections::IEnumerable^ DivergentAssetList::GetChildren(TreePath^ treePath)
    {
        // we will build a tree like this:
        //      <asset type>
        //          <asset name>
        //              <transaction>
        //              <transaction>
        //          <asset name>
        //              <transaction>
        //              <transaction>
        //  ..etc..

        if (treePath->IsEmpty()) {

            auto result = gcnew System::Collections::Generic::List<AssetTypeItem^>();

                // root node should be the list of asset types that have divergent assets
            auto count = _assetSets->GetAssetSetCount();
            for (unsigned c = 0; c < count; ++c) {
                const auto* set = _assetSets->GetAssetSet(c);
                if (!set) continue;

                auto divCount = set->GetDivergentCount();
                if (divCount > 0) {
                    result->Add(gcnew AssetTypeItem(*set));
                }
            }

            return result;

        } else {

            auto lastItem = treePath->LastNode;
            {
                auto item = dynamic_cast<AssetTypeItem^>(lastItem);
                if (item) {
                        // expecting the list of divergent assets of this type
                    auto result = gcnew System::Collections::Generic::List<AssetItem^>();
                    auto divCount = item->_set->GetDivergentCount();
                    for (unsigned d = 0; d < divCount; ++d) {
                        auto assetId = item->_set->GetDivergentId(d);
                        result->Add(gcnew AssetItem(*item->_set, assetId));
                    }
                    return result;
                }
            }

            if (_undoQueue) {
                auto item = dynamic_cast<AssetItem^>(lastItem);
                if (item) {
                        // expecting the list of committed transactions on this asset
                        // we need to search through the undo queue for all of the transactions
                        // that match this asset type code and id. We'll add the most recent
                        // one first.
                    auto result = gcnew System::Collections::Generic::List<TransactionItem^>();
                    auto id = item->_id;
                    auto typeCode = item->_set->GetTypeCode();
                    auto undoQueueCount = _undoQueue->GetCount();
                    for (unsigned c = 0; c < undoQueueCount; ++c) {
                        auto* trans = _undoQueue->GetTransaction(undoQueueCount - 1 - c);
                        if (!trans) continue;
                        if (trans->GetAssetId() == id && trans->GetTypeCode() == typeCode) {
                            result->Add(gcnew TransactionItem(*trans));
                        }
                    }

                    return result;
                }
            }

        }

        return nullptr;
    }

    bool DivergentAssetList::IsLeaf(TreePath^ treePath)
    {
        auto item = dynamic_cast<AssetItem^>(treePath->LastNode);
        return item != nullptr;
    }


    DivergentAssetList::DivergentAssetList(EngineDevice^ engine)
    {
        _assetSets = &engine->GetNative().GetASyncManager()->GetAssetSets();
        _undoQueue = nullptr;
    }
 

}

