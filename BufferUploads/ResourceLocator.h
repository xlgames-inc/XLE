// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"
#include "../RenderCore/Metal/Resource.h"           // (for Underlying::Resource)
#include "../Utility/Threading/ThreadingUtils.h"    // for RefCountedObject
#include "../Utility/IntrusivePtr.h"
#include <memory>
#include <vector>

namespace Utility { class DefragStep; }

namespace BufferUploads
{
    typedef RenderCore::Metal::Underlying::Resource UnderlyingResource;

    buffer_upload_dll_export BufferDesc ExtractDesc(const UnderlyingResource& resource);

    class IResourcePool
    {
    public:
        virtual void AddRef(
            uint64 resourceMarker, UnderlyingResource* resource, 
            unsigned offset, unsigned size) = 0;
        virtual void ReturnToPool(
            uint64 resourceMarker, intrusive_ptr<UnderlyingResource>&& resource, 
            unsigned offset, unsigned size) = 0;
        virtual ~IResourcePool() {}
    };

        /////////////////////////////////////////////////

    class Event_ResourceReposition
    {
    public:
        UnderlyingResource* _originalResource;
        UnderlyingResource* _newResource;
        std::shared_ptr<IResourcePool> _pool;
        uint64 _poolMarker;
        std::vector<Utility::DefragStep> _defragSteps;
    };

    class ResourceLocator : public RefCountedObject
    {
    public:
        intrusive_ptr<UnderlyingResource> AdoptUnderlying();

        bool IsEmpty() const { return !_resource; }
        unsigned Offset() const { return _offset; }
        unsigned Size() const { return _size; }
        UnderlyingResource* GetUnderlying() const { return _resource.get(); }
        std::shared_ptr<IResourcePool> Pool() { return _pool; }
        uint64 PoolMarker() { return _poolMarker; }

        ResourceLocator(
            intrusive_ptr<UnderlyingResource>&& moveFrom, unsigned offset=~unsigned(0x0), unsigned size=~unsigned(0x0), 
            std::shared_ptr<IResourcePool> pool = nullptr, uint64 poolMarker = 0);
        ResourceLocator(ResourceLocator&& moveFrom);
        ResourceLocator& operator=(ResourceLocator&& moveFrom);
        ResourceLocator();
        ~ResourceLocator();

        void swap(ResourceLocator& other);
    protected:
        intrusive_ptr<UnderlyingResource> _resource;
        unsigned _offset, _size;
        std::shared_ptr<IResourcePool> _pool;
        uint64 _poolMarker;

        ResourceLocator(const ResourceLocator& resource);
        ResourceLocator& operator=(const ResourceLocator& resource);
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////

    inline intrusive_ptr<RenderCore::Metal::Underlying::Resource> ResourceLocator::AdoptUnderlying()
    {
        _offset = _size = 0;
        _pool.reset();
        _poolMarker = 0;
        return std::move(_resource);
    }

    inline ResourceLocator::ResourceLocator(
        intrusive_ptr<RenderCore::Metal::Underlying::Resource>&& moveFrom, 
        unsigned offset, unsigned size,
        std::shared_ptr<IResourcePool> pool, uint64 poolMarker)
    : _resource(std::forward<intrusive_ptr<RenderCore::Metal::Underlying::Resource>>(moveFrom))
    , _offset(offset), _size(size), _pool(pool), _poolMarker(poolMarker)
    {
        if (_pool) {
            _pool->AddRef(_poolMarker, _resource.get(), _offset, _size);
        }
    }

    inline ResourceLocator::ResourceLocator(ResourceLocator&& moveFrom) 
        : _resource(std::move(moveFrom._resource))
        , _offset(moveFrom._offset), _size(moveFrom._size)
        , _pool(std::move(moveFrom._pool)), _poolMarker(moveFrom._poolMarker)
    {}

    inline ResourceLocator& ResourceLocator::operator=(ResourceLocator&& moveFrom)
    {
        ResourceLocator(moveFrom).swap(*this);
        return *this;
    }

    inline ResourceLocator::ResourceLocator() : _pool(nullptr), _poolMarker(0), _offset(0), _size(0) {}
    inline ResourceLocator::~ResourceLocator()
    {
            // attempt to return our resource to the pool
        if (_pool) {
            _pool->ReturnToPool(_poolMarker, std::move(_resource), _offset, _size);
        }
    }

    inline void ResourceLocator::swap(ResourceLocator& other)
    {
        std::swap(_resource, other._resource);
        std::swap(_size, other._size);
        std::swap(_offset, other._offset);
        std::swap(_pool, other._pool);
        std::swap(_poolMarker, other._poolMarker);
    }
}
