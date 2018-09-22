// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AsyncMarkerGroup.h"
#include <memory>

namespace Assets
{
    class AsyncMarkerGroup::Entry
    {
    public:
        std::shared_ptr<IAsyncMarker> _marker;
        std::string _name;
    };
    
    void AsyncMarkerGroup::Add(const std::shared_ptr<IAsyncMarker>& marker, const std::string& name)
    {
        _entries.push_back({marker, name});
    }
    
    void AsyncMarkerGroup::Remove(const std::shared_ptr<IAsyncMarker>& marker)
    {
        _entries.erase(std::remove_if(_entries.begin(), _entries.end(),
                                      [&marker](const Entry& e) { return e._marker == marker; }),
                       _entries.end());
    }
    
    AssetState AsyncMarkerGroup::GetAssetState() const
    {
        bool hasPending = false;
        bool hasInvalid = false;
        for (const auto&e:_entries) {
            auto state = e._marker->GetAssetState();
            hasPending |= state == AssetState::Pending;
            hasInvalid |= state == AssetState::Invalid;
        }
        if (hasInvalid) return AssetState::Invalid;
        if (hasPending) return AssetState::Pending;
        return AssetState::Ready;
    }
    
    AssetState AsyncMarkerGroup::StallWhilePending() const
    {
        for (const auto&e:_entries) {
            auto state = e._marker->StallWhilePending();
            if (state == AssetState::Invalid)
                return AssetState::Invalid;     // as soon as we hit an invalid; we will no longer stall for any remaining assets
        }
        return AssetState::Ready;
    }
    
    AsyncMarkerGroup::AsyncMarkerGroup()
    {
    }
    
    AsyncMarkerGroup::~AsyncMarkerGroup()
    {
    }
    
}
