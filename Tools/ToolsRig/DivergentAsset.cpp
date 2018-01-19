// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DivergentAsset.h"

#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/AssetServices.h"
#include "../Utility/Threading/Mutex.h"

namespace ToolsRig
{
#if 0
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
		auto end = fn.cend();
        if (paramStart != std::basic_string<ResChar>::npos)
			end = fn.cbegin() + paramStart;

        Services::GetAsyncMan().GetIntermediateStore().ShadowFile(MakeStringSection(AsPointer(fn.cbegin()), AsPointer(end)));
    }
#endif


    ITransaction::ITransaction(const char transactionName[], const std::shared_ptr<UndoQueue>& undoQueue)
	: _undoQueue(std::move(undoQueue))
    , _transactionName(transactionName)
	{}

	ITransaction::~ITransaction()
	{}

	void UndoQueue::PushBack(const std::shared_ptr<ITransaction>& transaction) {}
    std::shared_ptr<ITransaction> UndoQueue::GetTop() { return nullptr; }
    unsigned UndoQueue::GetCount() { return 0; }
    ITransaction* UndoQueue::GetTransaction(unsigned) { return nullptr; }
	UndoQueue::UndoQueue() {}
	UndoQueue::~UndoQueue() {}

	DivergentAssetBase::DivergentAssetBase() {}
	DivergentAssetBase::~DivergentAssetBase() {}


	class DivergentAssetManager::Pimpl
	{
	public:
		struct Id { uint64_t _typeCode; uint64_t _idInAssetHeap; };
		struct Asset
		{
			::Assets::rstring _identifier;
			std::shared_ptr<DivergentAssetBase> _asset;
		};

		std::vector<std::pair<Id, Asset>> _assets;

		Threading::Mutex _lock;

		friend bool operator<(const Id& lhs, const Id& rhs)
		{
			if (lhs._typeCode < rhs._typeCode) return true;
			if (lhs._typeCode > rhs._typeCode) return false;
			return lhs._idInAssetHeap < rhs._idInAssetHeap;
		}

		friend bool operator==(const Id& lhs, const Id& rhs)
		{
			return lhs._typeCode == rhs._typeCode && lhs._idInAssetHeap == rhs._idInAssetHeap;
		}
	};

	auto DivergentAssetManager::GetAssets() const -> std::vector<Record>
	{
		ScopedLock(_pimpl->_lock);
		std::vector<Record> result;
		for (const auto&p : _pimpl->_assets) {
			result.push_back(Record{
				p.first._typeCode, p.first._idInAssetHeap,
				p.second._identifier, p.second._asset->HasChanges() });
		}
		return result;
	}

	std::shared_ptr<DivergentAssetBase> DivergentAssetManager::GetAsset(uint64_t typeCode, uint64_t id) const
	{
		ScopedLock(_pimpl->_lock);
		auto s = Pimpl::Id{ typeCode, id };
		auto i = LowerBound(_pimpl->_assets, s);
		if (i != _pimpl->_assets.end() && i->first == s)
			return i->second._asset;
		return nullptr;
	}

	void DivergentAssetManager::AddAsset(
		uint64_t typeCode, uint64_t id, const ::Assets::rstring& identifier,
		const std::shared_ptr<DivergentAssetBase>& asset)
	{
		ScopedLock(_pimpl->_lock);
		auto s = Pimpl::Id{ typeCode, id };
		auto i = LowerBound(_pimpl->_assets, s);
		if (i != _pimpl->_assets.end() && i->first == s) {
			i->second = { identifier, asset };
		} else {
			_pimpl->_assets.insert(i, {s, Pimpl::Asset{ identifier, asset }});
		}
	}

	DivergentAssetManager* DivergentAssetManager::s_instance = nullptr;

	DivergentAssetManager::DivergentAssetManager()
	{
		assert(!s_instance);
		s_instance = this;
		_pimpl = std::make_unique<Pimpl>();
	}

	DivergentAssetManager::~DivergentAssetManager() 
	{
		assert(s_instance == this);
		s_instance = nullptr;
	}

}



#include "../../RenderCore/Assets/RawMaterial.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/AssetSetManager.h"
#include "../../Assets/AssetHeap.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Utility/StringFormat.h"

namespace ToolsRig
{

	template<typename AssetType, typename... Params>
		std::shared_ptr<DivergentAsset<AssetType>> CreateDivergentAsset(Params... params)
	{
		auto& set = ::Assets::GetAssetSetManager().GetSetForType<AssetType>();

		auto originalFuture = set.Get(params...);
		originalFuture->StallWhilePending();

		auto divergentAsset = std::make_shared<DivergentAsset<AssetType>>(originalFuture->Actualize());
		auto workingAsset = divergentAsset->GetWorkingAsset();
		auto idInAssetHeap = set.SetShadowingAsset(std::move(workingAsset), params...);

		auto stringInitializer = ::Assets::Internal::AsString(params...);
		DivergentAssetManager::GetInstance().AddAsset(
			typeid(AssetType).hash_code(), idInAssetHeap, stringInitializer,
			divergentAsset);

		return divergentAsset;
	}

	template std::shared_ptr<DivergentAsset<RenderCore::Assets::RawMaterial>> CreateDivergentAsset(StringSection<char>);

}

