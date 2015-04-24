// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NativeEngineDevice.h"
#include "DivergentAssetList.h"
#include "ExportedNativeTypes.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/DivergentAsset.h"
#include "../../Assets/Assets.h"
#include "../../Utility/Streams/FileUtils.h"
#include "MarshalString.h"

#include "../../RenderCore/Assets/Material.h"

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

        PendingSaveList::Entry^ _pendingSave;
        property bool SaveQueued;

        AssetItem(const Assets::IAssetSet& set, uint64 id) : _set(&set), _id(id)
        {
            Label = clix::marshalString<clix::E_UTF8>(set.GetAssetName(id));
            _pendingSave = nullptr;
            SaveQueued = false;
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
                        auto assetItem = gcnew AssetItem(*item->_set, assetId);

                            // if we have a "pending save" for this item
                            // then we have to hook it up
                        assetItem->_pendingSave = _saveList->GetEntry(*item->_set, assetId);
                        assetItem->SaveQueued = (assetItem->_pendingSave != nullptr);

                        result->Add(assetItem);
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


    DivergentAssetList::DivergentAssetList(EngineDevice^ engine, PendingSaveList^ saveList)
    {
        _assetSets = &engine->GetNative().GetASyncManager()->GetAssetSets();
        _undoQueue = nullptr;
        _saveList = saveList;
    }

    DivergentAssetList::~DivergentAssetList() {}
 


    void PendingSaveList::Add(const Assets::IAssetSet& set, uint64 id, Entry^ entry)
    {
        for each(auto e in _entries)
            if (e._assetSet == &set && e._id == id) {
                assert(0);
                return;
            }
        
        auto e = gcnew E();
        e->_assetSet = &set;
        e->_id = id;
        e->_entry = entry;
        _entries->Add(*e);
    }

    auto PendingSaveList::GetEntry(const Assets::IAssetSet& set, uint64 id) -> Entry^
    {
        for each(auto e in _entries)
            if (e._assetSet == &set && e._id == id) {
                return e._entry;
            }

        return nullptr;
    }

    PendingSaveList::PendingSaveList()
    {
        _entries = gcnew List<E>();
    }

    PendingSaveList::~PendingSaveList()
    {}

    array<Byte>^ LoadFileAsByteArray(const char filename[])
    {
        TRY {
            BasicFile file(filename, "rb");

            file.Seek(0, SEEK_END);
            size_t size = file.TellP();
            file.Seek(0, SEEK_SET);

            auto block = gcnew array<Byte>(size+1);
            {
                pin_ptr<unsigned char> pinned = &block[0];
                file.Read((uint8*)pinned, 1, size);
                pinned[size] = '\0';
            }
            return block;
        } CATCH(const std::exception& ) {
            return gcnew array<Byte>(0);
        } CATCH_END
    }

    array<Byte>^ AsByteArray(const uint8* begin, const uint8* end)
    {
        using System::Runtime::InteropServices::Marshal;

        array<Byte>^ result = gcnew array<Byte>(end-begin);
        Marshal::Copy(result, 0, IntPtr(const_cast<uint8*>(begin)), result->Length);
        return result;
    }

    PendingSaveList^ BuildPendingSaveList()
    {
        // auto result = gcnew PendingSaveList();

            //  Find all divergent assets that can be saved, and 
            //  add in an entry for them here. Each entry should
            //  contain the destination filename to write to, the
            //  previous contents of the file and the new contents.
            //  Typically our divergent assets should be small, and
            //  should serialise quickly... But if we have many large
            //  assets, this could get expensive quickly!

        #if defined(ASSETS_STORE_DIVERGENT)

            // using namespace RenderCore::Assets;
            // auto& materials = ::Assets::Internal::GetAssetSet<RawMaterial>();
            // for (auto a = materials._divergentAssets.cbegin(); a!=materials._divergentAssets.cend(); ++a) {
            //     if (!a->second->HasChanges()) continue;
            // 
            //     auto filename = a->second->GetIdentifier()._targetFilename;
            //     auto originalFile = LoadFileAsByteArray(filename.c_str());
            // 
            //     
            // }

        #endif

        return nullptr;
    }
}

