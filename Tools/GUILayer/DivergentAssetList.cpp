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
#include "../../Utility/Streams/StreamTypes.h"
#include "../../Utility/Streams/Data.h"
// #include "../../ConsoleRig/Console.h"
#include "MarshalString.h"

#include "../../RenderCore/Assets/Material.h"

using namespace System::Collections::Generic;

namespace GUILayer
{
    public ref class AssetItem
    {
    public:
        property virtual System::String^ Label;

        const Assets::IAssetSet* _set; 
        uint64 _id;

        PendingSaveList::Entry^ _pendingSave;
        property virtual System::Windows::Forms::CheckState SaveQueuedState
        {
            System::Windows::Forms::CheckState get() 
            { 
                bool queued = _pendingSave ? _pendingSave->_saveQueued : false;
                return queued ? System::Windows::Forms::CheckState::Checked : System::Windows::Forms::CheckState::Unchecked; 
            }
            void set(System::Windows::Forms::CheckState check) 
            {
                if (_pendingSave) {
                    _pendingSave->_saveQueued  = (check==System::Windows::Forms::CheckState::Checked); 
                }
            }
        }

        AssetItem(const Assets::IAssetSet& set, uint64 id) : _set(&set), _id(id)
        {
            Label = clix::marshalString<clix::E_UTF8>(set.GetAssetName(id));
            _pendingSave = nullptr;
        }
    };

    public ref class AssetTypeItem
    {
    public:
        property virtual System::String^ Label;

        const Assets::IAssetSet* _set;
        List<AssetItem^>^ _children;

        System::Windows::Forms::CheckState _checkState;

        property virtual System::Windows::Forms::CheckState SaveQueuedState
        {
            System::Windows::Forms::CheckState get() 
            { 
                return _checkState;
            }

            void set(System::Windows::Forms::CheckState check) 
            {
                _checkState = check;
                if (_children!=nullptr)
                {
                    for each(auto child in _children)
                        child->SaveQueuedState = check;
                }
            }
        }

        AssetTypeItem(const Assets::IAssetSet& set) : _set(&set)
        { 
            _children = nullptr;
            Label = clix::marshalString<clix::E_UTF8>(set.GetTypeName());
            _checkState = System::Windows::Forms::CheckState::Checked;
        }
    };

    public ref class TransactionItem
    {
    public:
        property virtual System::String^ Label;

        TransactionItem(const Assets::ITransaction& transaction)
        {
            Label = clix::marshalString<clix::E_UTF8>(transaction.GetName());
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

            auto result = gcnew List<AssetTypeItem^>();

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
                    if (!item->_children) {
                            // expecting the list of divergent assets of this type
                        item->_children = gcnew List<AssetItem^>();
                        auto divCount = item->_set->GetDivergentCount();
                        for (unsigned d = 0; d < divCount; ++d) {
                            if (!item->_set->DivergentHasChanges(d)) continue;

                            auto assetId = item->_set->GetDivergentId(d);
                            auto assetItem = gcnew AssetItem(*item->_set, assetId);

                                // if we have a "pending save" for this item
                                // then we have to hook it up
                            assetItem->_pendingSave = _saveList->GetEntry(*item->_set, assetId);

                            item->_children->Add(assetItem);
                        }
                    }


                    return item->_children;
                }
            }

            if (_undoQueue) {
                auto item = dynamic_cast<AssetItem^>(lastItem);
                if (item) {
                        // expecting the list of committed transactions on this asset
                        // we need to search through the undo queue for all of the transactions
                        // that match this asset type code and id. We'll add the most recent
                        // one first.
                    auto result = gcnew List<TransactionItem^>();
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
        _assetSets = &engine->GetNative().GetAssetServices()->GetAssetSets();
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

            auto block = gcnew array<Byte>(int(size+1));
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

        array<Byte>^ result = gcnew array<Byte>(int(end-begin));
        Marshal::Copy(IntPtr(const_cast<uint8*>(begin)), result, 0, result->Length);
        return result;
    }

#if 0
    static void SerializeInto(::Utility::Data& originalFile, const RenderCore::Assets::RawMaterial& material)
    {
        auto asData = material.SerializeAsData();
                    
        auto* replacedEntry = originalFile.ChildWithValue(asData->StrValue());
        if (replacedEntry) { 
            if (replacedEntry->prev)
                replacedEntry->prev->next = asData.get();
            else
                replacedEntry->parent->child = asData.get();

            if (replacedEntry->next)
                replacedEntry->next->prev = asData.get();
                        
            asData->prev = replacedEntry->prev;
            asData->next = replacedEntry->next;
            asData->parent = replacedEntry->parent;

            replacedEntry->next = nullptr;
            replacedEntry->prev = nullptr;
            replacedEntry->parent = nullptr;
            delete replacedEntry;
            asData.release();
        } else {
            originalFile.Add(asData.release());
        }
    }
#endif

    PendingSaveList^ PendingSaveList::Create()
    {
        auto result = gcnew PendingSaveList();

            //  Find all divergent assets that can be saved, and 
            //  add in an entry for them here. Each entry should
            //  contain the destination filename to write to, the
            //  previous contents of the file and the new contents.
            //  Typically our divergent assets should be small, and
            //  should serialise quickly... But if we have many large
            //  assets, this could get expensive quickly!

        #if defined(ASSETS_STORE_DIVERGENT)

            using namespace RenderCore::Assets;
            auto& materials = ::Assets::Internal::GetAssetSet<RawMaterial>();
            for (auto a = materials._divergentAssets.cbegin(); a!=materials._divergentAssets.cend(); ++a) {
                if (!a->second->HasChanges()) continue;
            
                    //  Sometimes mutliple assets will write to different parts of the same
                    //  file. It's a bit wierd, but we want the before and after parts to
                    //  only show changes related to this particular asset
                auto filename = a->second->GetIdentifier()._targetFilename;
                auto originalFile = LoadFileAsByteArray(filename.c_str());
            
                array<Byte>^ newFile;
                {
                    ::Utility::Data fullData;
                    if (originalFile->Length) {
                        pin_ptr<uint8> pinnedAddress(&originalFile[0]);
                        fullData.Load((const char*)pinnedAddress, originalFile->Length);
                    }

                    // SerializeInto(fullData, a->second->GetAsset());

                    MemoryOutputStream<utf8> strm;
                    fullData.SaveToOutputStream(strm);
                    newFile = AsByteArray(strm.GetBuffer().Begin(), strm.GetBuffer().End());
                }

                result->Add(
                    materials, a->first,
                    gcnew PendingSaveList::Entry(
                        clix::marshalString<clix::E_UTF8>(filename), 
                        originalFile, newFile));
            }

        #endif

        return result;
    }

    void PendingSaveList::Commit()
    {
        #if defined(ASSETS_STORE_DIVERGENT)

            using namespace RenderCore::Assets;
            auto& materials = ::Assets::Internal::GetAssetSet<RawMaterial>();
            for (auto a = materials._divergentAssets.cbegin(); a!=materials._divergentAssets.cend(); ++a) {
                if (!a->second->HasChanges()) continue;

                auto entry = GetEntry(materials, a->first);
                if (!entry || !entry->_saveQueued) continue;
            
                    //  Sometimes mutliple assets will write to different parts of the same
                    //  file. It's a bit wierd, but we want the before and after parts to
                    //  only show changes related to this particular asset
                auto filename = a->second->GetIdentifier()._targetFilename;
                auto originalFile = LoadFileAsByteArray(filename.c_str());
            
                {
                    ::Utility::Data fullData;
                    if (originalFile->Length) {
                        pin_ptr<uint8> pinnedAddress(&originalFile[0]);
                        fullData.Load((const char*)pinnedAddress, originalFile->Length);
                    }

                    // SerializeInto(fullData, a->second->GetAsset());

                    MemoryOutputStream<utf8> strm;
                    fullData.SaveToOutputStream(strm);

                    TRY
                    {
                        BasicFile outputFile(filename.c_str(), "wb");
                        outputFile.Write(strm.GetBuffer().Begin(), 1, size_t(strm.GetBuffer().End()) - size_t(strm.GetBuffer().Begin()));
                    } CATCH (...) {
                        // LogAlwaysError << "Problem when writing out to file: " << filename;
                    } CATCH_END
                }
            }

                // reset all divergent assets... Underneath, the real file should have changed
            for (auto a = materials._divergentAssets.cbegin(); a!=materials._divergentAssets.cend();) {
                auto entry = GetEntry(materials, a->first);
                if (!entry || !entry->_saveQueued) { 
                    ++a; 
                } else {
                    a = materials._divergentAssets.erase(a);
                }
            }

        #endif
    }

    bool PendingSaveList::HasModifiedAssets()
    {
        #if defined(ASSETS_STORE_DIVERGENT)

            auto& materials = ::Assets::Internal::GetAssetSet<RenderCore::Assets::RawMaterial>();
            (void)materials; // compiler incorrectly thinking that this is unreferenced
            for (auto a = materials._divergentAssets.cbegin(); a!=materials._divergentAssets.cend(); ++a)
                if (a->second->HasChanges()) return true;

        #endif

        return true;
    }
}

