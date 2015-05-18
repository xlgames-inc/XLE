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
        bool _active;
    };

    void InvalidAssetManager::MarkInvalid(const rstring& name, const rstring& errorString)
    {
        if (_pimpl->_active) {
            ScopedLock(_pimpl->_assetsLock);
            auto hashName = Hash64(name);
            auto i = LowerBound(_pimpl->_assets, hashName);
            if (i != _pimpl->_assets.end() && i->first == hashName) {
                assert(i->second._name == name);
                i->second._errorString = errorString;
            } else {
                _pimpl->_assets.insert(
                    i, std::make_pair(hashName, AssetRef { name, errorString }));
            }
        }
    }

    void InvalidAssetManager::MarkValid(const ResChar name[])
    {
        if (_pimpl->_active) {
            ScopedLock(_pimpl->_assetsLock);
            auto hashName = Hash64(name);
            auto i = LowerBound(_pimpl->_assets, hashName);
            if (i != _pimpl->_assets.end() && i->first == hashName) {
                _pimpl->_assets.erase(i);
            }
        }
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

    InvalidAssetManager::InvalidAssetManager(bool active)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_active = active;
    }

    InvalidAssetManager::~InvalidAssetManager() {}
}

