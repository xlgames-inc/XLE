// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetSetManager.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/IteratorUtils.h"
#include <vector>
#include <memory>
#include <assert.h>

namespace Assets
{

    IAssetSet::~IAssetSet() {}

    class AssetSetManager::Pimpl
    {
    public:
        std::vector<std::pair<size_t, std::unique_ptr<IAssetSet>>> _sets;
        unsigned _boundThreadId;
        Threading::Mutex _lock;
    };

    IAssetSet* AssetSetManager::GetSetForTypeCode(size_t typeCode)
    {
        auto i = LowerBound(_pimpl->_sets, typeCode);
        if (i != _pimpl->_sets.end() && i->first == typeCode)
            return i->second.get();
        return nullptr;
    }

    void AssetSetManager::Add(size_t typeCode, std::unique_ptr<IAssetSet>&& set)
    {
        auto i = LowerBound(_pimpl->_sets, typeCode);
        assert(i == _pimpl->_sets.end() || i->first != typeCode);
        _pimpl->_sets.insert(
            i,
            std::make_pair(
                typeCode,
                std::forward<std::unique_ptr<IAssetSet>>(set)));
    }

    void AssetSetManager::Clear()
    {
        for (auto i=_pimpl->_sets.begin(); i!=_pimpl->_sets.end(); ++i) {
            i->second->Clear();
        }
    }

    void AssetSetManager::LogReport()
    {
        for (auto i=_pimpl->_sets.begin(); i!=_pimpl->_sets.end(); ++i) {
            i->second->LogReport();
        }
    }

	bool AssetSetManager::IsBoundThread() const
	{
		return _pimpl->_boundThreadId == Threading::CurrentThreadId();
	}

    unsigned AssetSetManager::GetAssetSetCount()
    {
        return unsigned(_pimpl->_sets.size());
    }

    const IAssetSet* AssetSetManager::GetAssetSet(unsigned index)
    {
        return _pimpl->_sets[index].second.get();
    }

    void AssetSetManager::Lock()
    {
        _pimpl->_lock.lock();
    }

    void AssetSetManager::Unlock()
    {
        _pimpl->_lock.unlock();
    }

    AssetSetManager::AssetSetManager()
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_boundThreadId = Threading::CurrentThreadId();
        _pimpl = std::move(pimpl);
    }

    AssetSetManager::~AssetSetManager()
    {}

}