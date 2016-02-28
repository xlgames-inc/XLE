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
#include "../../Assets/AssetUtils.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/AssetServices.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/StreamTypes.h"
#include "../../Utility/Streams/Data.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Threading/ThreadingUtils.h"
#include "../../Utility/Conversion.h"
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
        property virtual PendingSaveList::Action Action
        {
			PendingSaveList::Action get()
            { 
                return _pendingSave ? _pendingSave->_action : PendingSaveList::Action::Ignore;
            }
            void set(PendingSaveList::Action action)
            {
                if (_pendingSave) {
                    _pendingSave->_action = action; 
                }
            }
        }

        AssetItem(const Assets::IAssetSet& set, uint64 id) : _set(&set), _id(id)
        {
            Label = clix::marshalString<clix::E_UTF8>(set.GetAssetName(id));
            _pendingSave = nullptr;
        }
    };

	static String^ GetAssetTypeName(uint64 typeCode)
	{
		using MatType = ::Assets::ConfigFileListContainer<RenderCore::Assets::RawMaterial>;
		if (typeCode == typeid(MatType).hash_code()) {
			return "Material";
		}
		return String::Empty;
	}

	private ref class ResourceImages
	{
	public:
		static System::Drawing::Image^ s_materialImage = nullptr;
	};

	static System::Drawing::Image^ GetAssetTypeImage(uint64 typeCode)
	{
		using MatType = ::Assets::ConfigFileListContainer<RenderCore::Assets::RawMaterial>;
		if (typeCode == typeid(MatType).hash_code()) {
			if (!ResourceImages::s_materialImage) {
				System::IO::Stream^ stream = System::Reflection::Assembly::GetExecutingAssembly()->GetManifestResourceStream("materialS.png");
				ResourceImages::s_materialImage = System::Drawing::Image::FromStream(stream);
			}
			return ResourceImages::s_materialImage;
		}
		return nullptr;
	}

    public ref class AssetTypeItem
    {
    public:
        property virtual System::String^ Label;
		property virtual System::Drawing::Image^ Icon;

        const Assets::IAssetSet* _set;
        List<AssetItem^>^ _children;

		PendingSaveList::Action _action;

        property virtual PendingSaveList::Action Action
        {
			PendingSaveList::Action get()
            {
                return _action;
            }

            void set(PendingSaveList::Action action)
            {
				_action = action;
                if (_children!=nullptr)
                {
                    for each(auto child in _children)
                        child->Action = action;
                }
            }
        }

        AssetTypeItem(const Assets::IAssetSet& set) : _set(&set)
        { 
            _children = nullptr;
			Label = GetAssetTypeName(set.GetTypeCode());
			if (String::IsNullOrEmpty(Label))
				Label = clix::marshalString<clix::E_UTF8>(set.GetTypeName());
			Icon = GetAssetTypeImage(set.GetTypeCode());
            _action = PendingSaveList::Action::Save;
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

    static array<Byte>^ LoadOriginalFileAsByteArray(const char filename[])
    {
        TRY {
            BasicFile file(filename, "rb");

            file.Seek(0, SEEK_END);
            size_t size = file.TellP();
            file.Seek(0, SEEK_SET);

            auto block = gcnew array<Byte>(int(size));
            {
                pin_ptr<unsigned char> pinned = &block[0];
                file.Read((uint8*)pinned, 1, size);
            }
            return block;
        } CATCH(const std::exception& e) {
			auto builder = gcnew System::Text::StringBuilder();
			builder->Append("Error while opening input file ");
			builder->Append(clix::marshalString<clix::E_UTF8>(filename));
			builder->AppendLine(". Exception message follows:");
			builder->AppendLine(clix::marshalString<clix::E_UTF8>(e.what()));
			return System::Text::Encoding::UTF8->GetBytes(builder->ToString());
        } CATCH(...) {
			auto builder = gcnew System::Text::StringBuilder();
			builder->Append("Error while opening input file ");
			builder->Append(clix::marshalString<clix::E_UTF8>(filename));
			builder->AppendLine(". Unknown exception type.");
			return System::Text::Encoding::UTF8->GetBytes(builder->ToString());
		} CATCH_END
    }

    array<Byte>^ AsByteArray(const uint8* begin, const uint8* end)
    {
        using System::Runtime::InteropServices::Marshal;

        array<Byte>^ result = gcnew array<Byte>(int(end-begin));
        Marshal::Copy(IntPtr(const_cast<uint8*>(begin)), result, 0, result->Length);
        return result;
    }

    static auto DeserializeAllMaterials(InputStreamFormatter<utf8>& formatter, const ::Assets::DirectorySearchRules& searchRules)
        -> std::vector<std::pair<::Assets::rstring, RenderCore::Assets::RawMaterial>>
    {
        std::vector<std::pair<::Assets::rstring, RenderCore::Assets::RawMaterial>> result;

        using Blob = InputStreamFormatter<utf8>::Blob;
        for (;;) {
            switch(formatter.PeekNext()) {
            case Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);
                    RenderCore::Assets::RawMaterial mat(formatter, searchRules);
                    result.emplace_back(
                        std::make_pair(
                            Conversion::Convert<::Assets::rstring>(eleName.AsString()), std::move(mat)));

                    if (!formatter.TryEndElement())
                        Throw(Utility::FormatException("Expecting end element", formatter.GetLocation()));
                }
                break;

            case Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                }
                break;

            default:
                return result;
            }
        }
    }

    static void SerializeAllMaterials(OutputStreamFormatter& formatter, std::vector<std::pair<std::string, RenderCore::Assets::RawMaterial>>& mats)
    {
        for (const auto&m:mats) {
            auto ele = formatter.BeginElement(m.first.c_str());
            m.second.Serialize(formatter);
            formatter.EndElement(ele);
        }
    }

    static void MergeAndSerialize(
        OutputStreamFormatter& output,
        array<Byte>^ originalFile,
        StringSection<::Assets::ResChar> filename,
        StringSection<::Assets::ResChar> section,
        const RenderCore::Assets::RawMaterial& mat)
    {
            //  Sometimes mutliple assets will write to different parts of the same
            //  file. It's a bit wierd, but we want the before and after parts to
            //  only show changes related to this particular asset
        using namespace RenderCore::Assets;

        std::vector<std::pair<::Assets::rstring, RenderCore::Assets::RawMaterial>> preMats;
        if (originalFile->Length) {
            auto searchRules = ::Assets::DefaultDirectorySearchRules(filename);
            pin_ptr<uint8> pinnedAddress(&originalFile[0]);
            InputStreamFormatter<utf8> formatter(
                MemoryMappedInputStream((const char*)pinnedAddress, PtrAdd((const char*)pinnedAddress, originalFile->Length)));
            preMats = DeserializeAllMaterials(formatter, searchRules);
        }

        auto i = std::find_if(
            preMats.begin(), preMats.end(),
            [section](const std::pair<std::string, RenderCore::Assets::RawMaterial>& e)
            { return XlEqString(MakeStringSection(e.first), section); });

        if (i != preMats.end()) {
            i->second = mat;
        } else {
            preMats.push_back(std::make_pair(section.AsString(), mat));
        }

        SerializeAllMaterials(output, preMats);
    }

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

                    // HACK -- special case for RawMaterial objects!
            using namespace RenderCore::Assets;
            auto materials = ::Assets::Internal::GetAssetSet<::Assets::ConfigFileListContainer<RawMaterial>>();
            for (const auto& a:materials->_divergentAssets) {
                auto asset = a.second;
                auto hash = a.first;
                if (!asset->HasChanges()) continue;
            
                auto targetFilename = asset->GetIdentifier()._targetFilename;
                auto splitName = MakeFileNameSplitter(targetFilename);
                auto originalFile = LoadOriginalFileAsByteArray(splitName.AllExceptParameters().AsString().c_str());

                MemoryOutputStream<utf8> strm;
                OutputStreamFormatter fmtter(strm);
                MergeAndSerialize(fmtter, originalFile, 
                    splitName.AllExceptParameters(), splitName.Parameters(), 
                    asset->GetAsset()._asset);
                auto newFile = AsByteArray(strm.GetBuffer().Begin(), strm.GetBuffer().End());

                result->Add(
                    *materials, hash,
                    gcnew PendingSaveList::Entry(
                        clix::marshalString<clix::E_UTF8>(splitName.AllExceptParameters()), 
                        originalFile, newFile));
            }

        #endif

        return result;
    }

    auto PendingSaveList::Commit() -> CommitResult^
    {
        #if defined(ASSETS_STORE_DIVERGENT)

			auto errorMessages = gcnew System::Text::StringBuilder;

            using namespace RenderCore::Assets;
            auto materials = ::Assets::Internal::GetAssetSet<::Assets::ConfigFileListContainer<RawMaterial>>();
			for (const auto& a:materials->_divergentAssets) {
                auto asset = a.second;
                auto hash = a.first;
				if (!asset->HasChanges()) continue;

                auto entry = GetEntry(*materials, hash);
                if (!entry || entry->_action == Action::Ignore) continue;

				if (entry->_action == Action::Abandon) {
					asset->AbandonChanges();
					continue;
				}

				assert(entry->_action == Action::Save);
            
                    //  Sometimes mutliple assets will write to different parts of the same
                    //  file. It's a bit wierd, but we want the before and after parts to
                    //  only show changes related to this particular asset
                auto targetFilename = asset->GetIdentifier()._targetFilename;
                auto splitName = MakeFileNameSplitter(targetFilename);
                auto originalFile = LoadOriginalFileAsByteArray(splitName.AllExceptParameters().AsString().c_str());
            
                MemoryOutputStream<utf8> strm;
                OutputStreamFormatter fmtter(strm);
                MergeAndSerialize(fmtter, originalFile, 
                    splitName.AllExceptParameters(), splitName.Parameters(), 
                    asset->GetAsset()._asset);

				auto dstFile = splitName.AllExceptParameters().AsString();
                TRY
                {
					{
						BasicFile outputFile(dstFile.c_str(), "wb");
						outputFile.Write(strm.GetBuffer().Begin(), 1, size_t(strm.GetBuffer().End()) - size_t(strm.GetBuffer().Begin()));
					}

					// abandon changes now to allow us to reload the asset from disk
					asset->AbandonChanges();
					continue;
                } CATCH(const std::exception& e) {
					errorMessages->Append("Error while writing to file: ");
					errorMessages->Append(clix::marshalString<clix::E_UTF8>(dstFile));
					errorMessages->AppendLine(". Exception message follows:");
					errorMessages->AppendLine(clix::marshalString<clix::E_UTF8>(e.what()));
				} CATCH (...) {
					errorMessages->Append("Error while writing to file: ");
					errorMessages->Append(clix::marshalString<clix::E_UTF8>(dstFile));
					errorMessages->AppendLine(". Unknown exception type.");
                } CATCH_END
            }

			auto result = gcnew CommitResult;
			result->ErrorMessages = errorMessages->ToString();
			return result;

		#else

			auto result = gcnew CommitResult;
			result->ErrorMessages = "Divergent asset behaviour is disabled in this build.";
			return result;

        #endif
    }

    bool PendingSaveList::HasModifiedAssets()
    {
        #if defined(ASSETS_STORE_DIVERGENT)

            auto materials = ::Assets::Internal::GetAssetSet<::Assets::ConfigFileListContainer<RenderCore::Assets::RawMaterial>>();
            for (const auto&a:materials->_divergentAssets)
                if (a.second->HasChanges()) return true;

        #endif

        return true;
    }
}

