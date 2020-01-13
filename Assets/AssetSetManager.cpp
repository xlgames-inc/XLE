// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetSetManager.h"
#include "AssetHeap.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/IteratorUtils.h"
#include <vector>
#include <memory>
#include <assert.h>

namespace Assets
{
	class AssetSetManager::Pimpl
	{
	public:
		std::vector<std::pair<size_t, std::unique_ptr<IDefaultAssetHeap>>> _sets;
		std::vector<std::pair<size_t, std::unique_ptr<IDefaultAssetHeap>>> _setsPendingIteration;
		Threading::RecursiveMutex _lock;

		bool _inIterationOperation = false;
	};

	IDefaultAssetHeap * AssetSetManager::GetSetForTypeCode(size_t typeCode)
	{
		auto i = LowerBound(_pimpl->_sets, typeCode);
		if (i != _pimpl->_sets.end() && i->first == typeCode)
			return i->second.get();

		i = LowerBound(_pimpl->_setsPendingIteration, typeCode);
		if (i != _pimpl->_setsPendingIteration.end() && i->first == typeCode)
			return i->second.get();

		return nullptr;
	}

	void AssetSetManager::Add(size_t typeCode, std::unique_ptr<IDefaultAssetHeap>&& set)
	{
		auto* target = &_pimpl->_sets;
		// We can't modify "_sets" while we're iterating through it.
		// As a simple solution, just keep the these new sets in a pending list
		if (_pimpl->_inIterationOperation)
			target = &_pimpl->_setsPendingIteration;

		auto i = LowerBound(*target, typeCode);
		assert(i == target->end() || i->first != typeCode);
		target->insert(i, std::make_pair(typeCode, std::move(set)));
	}

    void AssetSetManager::Clear()
    {
        ScopedLock(_pimpl->_lock);
        _pimpl->_sets.clear();
    }

	std::vector<AssetHeapRecord> AssetSetManager::LogRecords() const
    {
		ScopedLock(_pimpl->_lock);
		std::vector<AssetHeapRecord> result;
		for (auto&set : _pimpl->_sets) {
			auto records = set.second->LogRecords();
			result.insert(result.end(), records.begin(), records.end());
		}
		return result;
    }

    unsigned AssetSetManager::GetAssetSetCount()
    {
        return unsigned(_pimpl->_sets.size());
    }

    const IDefaultAssetHeap* AssetSetManager::GetAssetSet(unsigned index)
    {
		// -- note -- pending sets can't be returned
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

	void AssetSetManager::OnFrameBarrier()
	{
		ScopedLock(_pimpl->_lock);
		_pimpl->_inIterationOperation = true;
		for (auto&set:_pimpl->_sets)
			set.second->OnFrameBarrier();

		// If we queued up any new sets to add to the main list, handle them now
		for (auto&set:_pimpl->_setsPendingIteration)
			_pimpl->_sets.insert(LowerBound(_pimpl->_sets, set.first), std::move(set));
		_pimpl->_setsPendingIteration.clear();
		_pimpl->_inIterationOperation = false;
	}

    AssetSetManager::AssetSetManager()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    AssetSetManager::~AssetSetManager()
    {}



	IDefaultAssetHeap::~IDefaultAssetHeap() {}

}
