// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ResourceDesc.h"       // actually only needed for TexturePitches
#include "Types_Forward.h"
#include "../Utility/MemoryUtils.h"
#include <memory>
#include <iostream>

namespace RenderCore
{
    class ResourceDesc;
    class TextureDesc;
    class TexturePitches;
    class SubResourceInitData;
    class Box2D;
	class IResource;

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      C O P Y I N G       //
///////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned CopyMipLevel(
        void* destination, size_t destinationDataSize, TexturePitches dstPitches,
        const TextureDesc& dstDesc,
        const SubResourceInitData& srcData);

    unsigned CopyMipLevel(
        void* destination, size_t destinationDataSize, TexturePitches dstPitches,
        const TextureDesc& dstDesc,
        const Box2D& dst2D,
        const SubResourceInitData& srcData);

    TextureDesc CalculateMipMapDesc(const TextureDesc& topMostMipDesc, unsigned mipMapIndex);

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      R E S O U R C E   S I Z E S       //
///////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned ByteCount(
        unsigned nWidth, unsigned nHeight, unsigned nDepth, 
        unsigned mipCount, Format format);
    unsigned ByteCount(const TextureDesc& tDesc);
    unsigned ByteCount(const ResourceDesc& desc);

    class SubResourceOffset { public: size_t _offset; size_t _size; TexturePitches _pitches; };
    SubResourceOffset GetSubResourceOffset(
        const TextureDesc& tDesc, unsigned mipIndex, unsigned arrayLayer);

    TexturePitches MakeTexturePitches(const TextureDesc& desc);

    unsigned CalculatePrimitiveCount(Topology topology, unsigned vertexCount, unsigned drawCallCount = 1);

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      V I E W    P O O L       //
///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename ViewType, typename ViewDescType=TextureViewDesc>
		class ViewPool
	{
	public:
		ViewType* GetView(const std::shared_ptr<IResource>& resource, const ViewDescType& view);
		void Erase(IResource& res);
        void Reset();

        struct Metrics
        {
            unsigned _viewCount;
        };
        Metrics GetMetrics() const;

	private:
		struct Entry { std::shared_ptr<IResource> _resource; std::unique_ptr<ViewType> _view; };
		std::vector<std::pair<uint64, Entry>> _views;
	};

	inline uint64_t CalculateHash(const TextureViewDesc& viewDesc)
	{
		return Hash64(&viewDesc, PtrAdd(&viewDesc, sizeof(TextureViewDesc)));
	}

	template<typename ViewType, typename ViewDescType>
		ViewType* ViewPool<ViewType, ViewDescType>::GetView(const std::shared_ptr<IResource>& resource, const ViewDescType& view)
	{
		uint64_t hash = HashCombine((size_t)resource.get(), CalculateHash(view));
		auto i = LowerBound(_views, hash);
		if (i != _views.end() && i->first == hash)
			return i->second._view.get();

        auto newView = std::make_unique<ViewType>(resource, view);
		i = _views.emplace(i, std::make_pair(hash, Entry{ resource, std::move(newView) }));
		return i->second._view.get();
	}

	template<typename ViewType, typename ViewDescType>
		void ViewPool<ViewType, ViewDescType>::Erase(IResource& res)
	{
		IResource* rawRes = &res;
		_views.erase(
			std::remove_if(
				_views.begin(), _views.end(),
				[rawRes](const std::pair<uint64, Entry>& p) { return p.second._resource.get() == rawRes; }),
			_views.end());
	}

    template<typename ViewType, typename ViewDescType>
        void ViewPool<ViewType, ViewDescType>::Reset()
    {
        _views.clear();
    }

    template<typename ViewType, typename ViewDescType>
        auto ViewPool<ViewType, ViewDescType>::GetMetrics() const -> Metrics
    {
        return Metrics { (unsigned)_views.size() };
    }

    std::ostream& SerializationOperator(std::ostream& strm, const ResourceDesc&);
}
