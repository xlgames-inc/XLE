// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetSetManager.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include <vector>
#include <memory>

namespace Assets
{

    IAssetSet::~IAssetSet() {}

    class AssetSetManager::Pimpl
    {
    public:
        std::vector<std::unique_ptr<IAssetSet>> _sets;
        unsigned _boundThreadId;
    };

    void AssetSetManager::Add(std::unique_ptr<IAssetSet>&& set)
    {
        _pimpl->_sets.push_back(std::forward<std::unique_ptr<IAssetSet>>(set));
    }

    void AssetSetManager::Clear()
    {
        for (auto i=_pimpl->_sets.begin(); i!=_pimpl->_sets.end(); ++i) {
            (*i)->Clear();
        }
    }

    void AssetSetManager::LogReport()
    {
        for (auto i=_pimpl->_sets.begin(); i!=_pimpl->_sets.end(); ++i) {
            (*i)->LogReport();
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
        return _pimpl->_sets[index].get();
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