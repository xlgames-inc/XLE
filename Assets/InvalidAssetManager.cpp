// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InvalidAssetManager.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/IteratorUtils.h"
#include <vector>

namespace Assets
{
    class InvalidAssetManager::Pimpl
    {
    public:
        Threading::Mutex _assetsLock;
        std::vector<std::pair<uint64, AssetRef>> _assets;
        std::vector<std::pair<unsigned, InvalidAssetManager::OnChangeEvent>> _onChangeEvents;
        bool _active;
        unsigned _nextChangeEventId;

        void FireChangeEvents() { for (auto i:_onChangeEvents) i.second(); }
        Pimpl() { _active = false; _nextChangeEventId = 1; }
    };

    void InvalidAssetManager::MarkInvalid(StringSection<ResChar> name, StringSection<ResChar> errorString)
    {
        bool fireChangedEvents = false;
        if (_pimpl->_active) {
            ScopedLock(_pimpl->_assetsLock);
            auto hashName = Hash64(name.begin(), name.end());
            auto i = LowerBound(_pimpl->_assets, hashName);
            if (i != _pimpl->_assets.end() && i->first == hashName) {
                assert(XlEqString(name, i->second._name));
                i->second._errorString = errorString.AsString();
            } else {
                _pimpl->_assets.insert(i, std::make_pair(hashName, AssetRef { name.AsString(), errorString.AsString() }));
            }
            fireChangedEvents = true;
        }
        if (fireChangedEvents)
            _pimpl->FireChangeEvents();
    }

    void InvalidAssetManager::MarkValid(StringSection<ResChar> name)
    {
        bool fireChangedEvents = false;
        if (_pimpl->_active) {
            ScopedLock(_pimpl->_assetsLock);
            auto hashName = Hash64(name.begin(), name.end());
            auto i = LowerBound(_pimpl->_assets, hashName);
            if (i != _pimpl->_assets.end() && i->first == hashName) {
                _pimpl->_assets.erase(i);
                fireChangedEvents = true;
            }
        }
        if (fireChangedEvents)
            _pimpl->FireChangeEvents();
    }

    auto InvalidAssetManager::AddOnChangeEvent(OnChangeEvent evnt) -> ChangeEventId
    {
        auto id = _pimpl->_nextChangeEventId++;
        _pimpl->_onChangeEvents.push_back(std::make_pair(id, evnt));
        return id;
    }

    void InvalidAssetManager::RemoveOnChangeEvent(ChangeEventId id)
    {
        for (auto i=_pimpl->_onChangeEvents.begin(); i!=_pimpl->_onChangeEvents.end(); ++i)
            if (i->first == id)
                _pimpl->_onChangeEvents.erase(i);
    }

    auto InvalidAssetManager::GetAssets() const -> std::vector<AssetRef>
    {
        std::vector<AssetRef> result;
        if (_pimpl->_active) {
            ScopedLock(_pimpl->_assetsLock);
            result.reserve(_pimpl->_assets.size());
            for (const auto& i:_pimpl->_assets)
                result.push_back(i.second);
        }
        return std::move(result);
    }

    bool InvalidAssetManager::HasInvalidAssets() const
    {
        if (_pimpl->_active) {
            ScopedLock(_pimpl->_assetsLock);
            return !_pimpl->_assets.empty();
        }
        return false;
    }

    InvalidAssetManager::InvalidAssetManager(bool active)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_active = active;
    }

    InvalidAssetManager::~InvalidAssetManager() {}
}

