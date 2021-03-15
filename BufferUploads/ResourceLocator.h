// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>
#include <vector>

namespace Utility { class DefragStep; }
namespace RenderCore { class IResource; }

namespace BufferUploads
{
    using IResource = RenderCore::IResource;

    class IResourcePool
    {
    public:
        virtual void AddRef(
            uint64_t resourceMarker, IResource& resource, 
            unsigned offset, unsigned size) = 0;
        virtual void ReturnToPool(
            uint64_t resourceMarker, std::shared_ptr<IResource>&& resource, 
            unsigned offset, unsigned size) = 0;
        virtual ~IResourcePool() {}
    };

    class ResourceLocator
    {
    public:
        std::shared_ptr<IResource> _resource;
        unsigned _offset = ~0u, _size = ~0u;
        std::shared_ptr<IResourcePool> _pool;
        uint64_t _poolMarker = ~0ull;
    };

        /////////////////////////////////////////////////

    class Event_ResourceReposition
    {
    public:
        std::shared_ptr<IResource> _originalResource;
        std::shared_ptr<IResource> _newResource;
        std::shared_ptr<IResourcePool> _pool;
        uint64_t _poolMarker;
        std::vector<Utility::DefragStep> _defragSteps;
    };    
}
