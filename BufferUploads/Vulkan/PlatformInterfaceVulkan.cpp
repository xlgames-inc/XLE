// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Core/Prefix.h"
#include "../../RenderCore/Metal/Metal.h"

#if GFXAPI_ACTIVE == GFXAPI_VULKAN

    #include "../PlatformInterface.h"
    #include "../DataPacket.h"
	#include "../../RenderCore/IThreadContext.h"
    #include "../../RenderCore/Metal/Resource.h"
    #include "../../RenderCore/ResourceUtils.h"

    namespace BufferUploads { namespace PlatformInterface
    {
        using namespace RenderCore;

        unsigned UnderlyingDeviceContext::PushToTexture(
            IResource& resource, const BufferDesc& desc,
            const Box2D& box, 
            const ResourceInitializer& data)
        {
			auto* metalResource = (Metal::Resource*)resource.QueryInterface(typeid(Metal::Resource).hash_code());
			if (!metalResource)
				Throw(::Exceptions::BasicLabel("Incorrect resource type passed to buffer uploads platform layer"));

            // In Vulkan, the only way we have to send data to a resource is by using
            // a memory map and CPU assisted copy. 
            assert(desc._type == BufferDesc::Type::Texture);
            if (box == Box2D())
                return Metal::CopyViaMemoryMap(*_renderCoreContext->GetDevice(), *metalResource, data);

            // When we have a box, we support writing to only a single subresource
            // We will iterate through the subresources an mip a single one
            auto dev = _renderCoreContext->GetDevice();
            auto copiedBytes = 0u;
            for (unsigned mip=0; mip<std::max(1u, unsigned(desc._textureDesc._mipCount)); ++mip)
                for (unsigned arrayLayer=0; arrayLayer<std::max(1u, unsigned(desc._textureDesc._arrayCount)); ++arrayLayer) {
                    auto srd = data({mip, arrayLayer});
                    if (!srd._data.size()) continue;

                    Metal::ResourceMap map(*dev, *metalResource, SubResourceId{mip, arrayLayer});
                    copiedBytes += CopyMipLevel(
                        map.GetData(), map.GetDataSize(), map.GetPitches(), 
                        desc._textureDesc,
                        box, srd);
                }

            return copiedBytes;
        }

        unsigned UnderlyingDeviceContext::PushToStagingTexture(
			UnderlyingResource& resource, const BufferDesc&desc, 
            const Box2D& box,
            const ResourceInitializer& data)
        {
            // Because this is a "staging" resource, we can assume it's not being
            // used currently. So, it's fine to just map the member and use a CPU assisted copy
            // Note that in Vulkan, we can map the entire resource once and just copy each
            // subresource as we go through.
            // The process is the same as PushToTexture...
            return PushToTexture(resource, desc, box, data);
        }

        void UnderlyingDeviceContext::UpdateFinalResourceFromStaging(
			IResource& finalResource, IResource& staging, 
			const BufferDesc& destinationDesc, 
            unsigned lodLevelMin, unsigned lodLevelMax, unsigned stagingLODOffset,
            VectorPattern<unsigned, 2> stagingXYOffset,
            const Box2D& srcBox)
        {
            auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
            auto allLods = 
                (lodLevelMin == ~unsigned(0x0) || lodLevelMin == 0u)
                && (lodLevelMax == ~unsigned(0x0) || lodLevelMax == (std::max(1u, (unsigned)destinationDesc._textureDesc._mipCount)-1));

			auto* metalFinal = (Metal::Resource*)finalResource.QueryInterface(typeid(Metal::Resource).hash_code());
			auto* metalStaging = (Metal::Resource*)staging.QueryInterface(typeid(Metal::Resource).hash_code());
			if (!metalFinal || !metalStaging)
				Throw(::Exceptions::BasicLabel("Incorrect resource type passed to buffer uploads platform layer"));

            // We don't have a way to know for sure what the current layout is for the given image on the given context. 
			// Let's just assume the previous states are as they would be in the most common cases
			//		-- normally, this is called immediately after creation, when filling in an OPTIMAL texture with
			//			data from a staging texture. When that happens, the src will be in layout "Preinitialized" and
			//			the dst will be in layout "Undefined"
			// During the transfer, the images must be in either TransferSrcOptimal, TransferDstOptimal or General.
			// So, we must change the layout immediate before and after the transfer.
			{
				Metal::LayoutTransition layoutTransitions[] = {
					{metalStaging, Metal_Vulkan::ImageLayout::General, Metal_Vulkan::ImageLayout::TransferSrcOptimal},
					{metalFinal, Metal_Vulkan::ImageLayout::Undefined, Metal_Vulkan::ImageLayout::TransferDstOptimal}};
				Metal::SetImageLayouts(*metalContext, MakeIteratorRange(layoutTransitions));
			}

            if (allLods && destinationDesc._type == BufferDesc::Type::Texture && !stagingLODOffset && !stagingXYOffset[0] && !stagingXYOffset[1]) {
                Metal::Copy(
                    *metalContext, 
                    *metalFinal, *metalStaging,
                    Metal::ImageLayout::TransferDstOptimal, Metal::ImageLayout::TransferSrcOptimal);
            } else {
                for (unsigned a=0; a<std::max(1u, (unsigned)destinationDesc._textureDesc._arrayCount); ++a) {
                    for (unsigned c=lodLevelMin; c<=lodLevelMax; ++c) {
                        Metal::CopyPartial(
                            *metalContext,
                            Metal::CopyPartial_Dest(
                                *metalFinal, 
                                SubResourceId{c, a}, {stagingXYOffset[0], stagingXYOffset[1], 0}),
                            Metal::CopyPartial_Src(
                                *metalStaging, 
                                SubResourceId{c-stagingLODOffset, a},
                                {(unsigned)srcBox._left, (unsigned)srcBox._top, 0u},
                                {(unsigned)srcBox._right, (unsigned)srcBox._bottom, 1u}),
                            Metal::ImageLayout::TransferDstOptimal, Metal::ImageLayout::TransferSrcOptimal);
                    }
                }
            }

            // Switch the layout to the final layout. Here, we're assuming all of the transfers are finished, and the
			// image will soon be used by a shader.
			{
				Metal::LayoutTransition layoutTransitions[] = {
					{metalFinal, Metal_Vulkan::ImageLayout::TransferDstOptimal, Metal_Vulkan::ImageLayout::ShaderReadOnlyOptimal}};
				Metal::SetImageLayouts(*metalContext, MakeIteratorRange(layoutTransitions));
			}

            // Is it reasonable to go back to preinitialised? If we don't do this, the texture can be reused and the next time we attempt to
            // switch it to TransferSrcOptimal, we will get a warning.
            // Metal::SetImageLayouts(*metalContext, {{&staging, Metal_Vulkan::ImageLayout::TransferSrcOptimal, Metal_Vulkan::ImageLayout::General}});
        }

        unsigned UnderlyingDeviceContext::PushToBuffer(
            IResource& resource, const BufferDesc& desc, unsigned offset,
            const void* data, size_t dataSize)
        {
			auto* metalResource = (Metal::Resource*)resource.QueryInterface(typeid(Metal::Resource).hash_code());
			if (!metalResource)
				Throw(::Exceptions::BasicLabel("Incorrect resource type passed to buffer uploads platform layer"));

            // note -- this is a direct, immediate map... There must be no contention while we map.
            assert(desc._type == BufferDesc::Type::LinearBuffer);
            Metal::ResourceMap map(*_renderCoreContext->GetDevice(), *metalResource, SubResourceId{0,0}, offset);
            auto copyAmount = std::min(map.GetDataSize(), dataSize);
            if (copyAmount > 0)
                XlCopyMemory(map.GetData(), data, copyAmount);
            return (unsigned)copyAmount;
        }

        void UnderlyingDeviceContext::ResourceCopy_DefragSteps(
			const UnderlyingResourcePtr& destination, const UnderlyingResourcePtr& source, 
			const std::vector<DefragStep>& steps)
        {
        }

        void UnderlyingDeviceContext::ResourceCopy(UnderlyingResource& destination, UnderlyingResource& source)
        {
            auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
            assert(metalContext);
			auto* metalDestination = (Metal::Resource*)destination.QueryInterface(typeid(Metal::Resource).hash_code());
			auto* metalSource = (Metal::Resource*)source.QueryInterface(typeid(Metal::Resource).hash_code());
			if (!metalDestination || !metalSource)
				Throw(::Exceptions::BasicLabel("Incorrect resource type passed to buffer uploads platform layer"));
            Metal::Copy(*metalContext, *metalDestination, *metalSource);
        }

        Metal::CommandListPtr UnderlyingDeviceContext::ResolveCommandList()
        {
            auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
            assert(metalContext);
            auto result = metalContext->ResolveCommandList();
			metalContext->BeginCommandList();	// begin a new one immediately
			return result;
        }

        void                        UnderlyingDeviceContext::BeginCommandList()
        {
            auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
            assert(metalContext);
            metalContext->BeginCommandList();
        }

        intrusive_ptr<DataPacket> UnderlyingDeviceContext::Readback(const ResourceLocator& locator)
        {
            return nullptr;
        }

        // UnderlyingDeviceContext::MappedBuffer UnderlyingDeviceContext::Map(UnderlyingResource& resource, MapType::Enum mapType, unsigned subResource)
        // {
        //     return MappedBuffer();
        // }
        // 
        // UnderlyingDeviceContext::MappedBuffer UnderlyingDeviceContext::MapPartial(UnderlyingResource& resource, MapType::Enum mapType, unsigned offset, unsigned size, unsigned subResource)
        // {
        //     return MappedBuffer();
        // }

        // void UnderlyingDeviceContext::Unmap(UnderlyingResource& resource, unsigned subResourceIndex)
        // {
        // }

		std::shared_ptr<IDevice> UnderlyingDeviceContext::GetObjectFactory()
		{
			return _renderCoreContext->GetDevice();
		}

        UnderlyingDeviceContext::UnderlyingDeviceContext(IThreadContext& renderCoreContext) 
        : _renderCoreContext(&renderCoreContext)
        {
        }

        ResourcePtr CreateResource(IDevice& device, const BufferDesc& desc, DataPacket* initialisationData)
        {
			if (initialisationData) {
				return device.CreateResource(desc,
					[initialisationData](SubResourceId sr) -> SubResourceInitData
					{
						SubResourceInitData result;
						auto data = initialisationData->GetData(sr);
						auto size = initialisationData->GetDataSize(sr);
						result._data = MakeIteratorRange(data, PtrAdd(data, size));
						auto pitches = initialisationData->GetPitches(sr);
						result._pitches._rowPitch = pitches._rowPitch;
						result._pitches._slicePitch = pitches._slicePitch;
						return result;
					});
			} else {
				return device.CreateResource(desc);
			}
        }

		BufferDesc ExtractDesc(RenderCore::IResource& resource)
        {
            return resource.GetDesc();
        }

    }}

#endif
