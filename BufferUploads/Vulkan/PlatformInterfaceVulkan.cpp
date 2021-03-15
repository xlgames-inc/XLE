// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../RenderCore/Metal/Metal.h"

#if GFXAPI_TARGET == GFXAPI_VULKAN

    #include "../PlatformInterface.h"
	#include "../ResourceLocator.h"
    #include "../DataPacket.h"
	#include "../../RenderCore/IThreadContext.h"
    #include "../../RenderCore/Metal/Resource.h"
    #include "../../RenderCore/Metal/DeviceContext.h"
    #include "../../RenderCore/ResourceUtils.h"

    namespace BufferUploads { namespace PlatformInterface
    {
        using namespace RenderCore;

        unsigned UnderlyingDeviceContext::PushToTexture(
            IResource& resource, const ResourceDesc& desc,
            const Box2D& box, 
            const ResourceInitializer& data)
        {
			auto* metalResource = (Metal::Resource*)resource.QueryInterface(typeid(Metal::Resource).hash_code());
			if (!metalResource)
				Throw(::Exceptions::BasicLabel("Incorrect resource type passed to buffer uploads platform layer"));

            // In Vulkan, the only way we have to send data to a resource is by using
            // a memory map and CPU assisted copy. 
            assert(desc._type == ResourceDesc::Type::Texture);
            if (box == Box2D())
                return Metal::Internal::CopyViaMemoryMap(*_renderCoreContext->GetDevice(), *metalResource, data);

			auto& metalContext = *Metal::DeviceContext::Get(*_renderCoreContext);

            // When we have a box, we support writing to only a single subresource
            // We will iterate through the subresources an mip a single one
            auto dev = _renderCoreContext->GetDevice();
            auto copiedBytes = 0u;
            for (unsigned mip=0; mip<std::max(1u, unsigned(desc._textureDesc._mipCount)); ++mip)
                for (unsigned arrayLayer=0; arrayLayer<std::max(1u, unsigned(desc._textureDesc._arrayCount)); ++arrayLayer) {
                    auto srd = data({mip, arrayLayer});
                    if (!srd._data.size()) continue;

                    Metal::ResourceMap map(metalContext, *metalResource, Metal::ResourceMap::Mode::WriteDiscardPrevious, SubResourceId{mip, arrayLayer});
                    copiedBytes += CopyMipLevel(
                        map.GetData().begin(), map.GetData().size(), map.GetPitches(), 
                        desc._textureDesc,
                        box, srd);
                }

            return copiedBytes;
        }

        unsigned UnderlyingDeviceContext::PushToStagingTexture(
			UnderlyingResource& resource, const ResourceDesc&desc, 
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
			IResource& finalResource, IResource& stagingResource, 
			const ResourceDesc& destinationDesc, 
            unsigned lodLevelMin, unsigned lodLevelMax, unsigned stagingLODOffset,
            VectorPattern<unsigned, 2> stagingXYOffset,
            const Box2D& srcBox)
        {
            auto& metalContext = *Metal::DeviceContext::Get(*_renderCoreContext);
            auto allLods = 
                (lodLevelMin == ~unsigned(0x0) || lodLevelMin == 0u)
                && (lodLevelMax == ~unsigned(0x0) || lodLevelMax == (std::max(1u, (unsigned)destinationDesc._textureDesc._mipCount)-1));

			// During the transfer, the images must be in either TransferSrcOptimal, TransferDstOptimal or General.
			// So, we must change the layout immediate before and after the transfer.
            Metal::Internal::CaptureForBind(metalContext, finalResource, BindFlag::TransferDst);
            Metal::Internal::CaptureForBind(metalContext, stagingResource, BindFlag::TransferSrc);
            auto blitEncoder = metalContext.BeginBlitEncoder();

            if (allLods && destinationDesc._type == ResourceDesc::Type::Texture && !stagingLODOffset && !stagingXYOffset[0] && !stagingXYOffset[1]) {
                blitEncoder.Copy(finalResource, stagingResource);
            } else {
                for (unsigned a=0; a<std::max(1u, (unsigned)destinationDesc._textureDesc._arrayCount); ++a) {
                    for (unsigned c=lodLevelMin; c<=lodLevelMax; ++c) {
                        blitEncoder.Copy(
                            Metal::BlitEncoder::CopyPartial_Dest{
                                &finalResource, 
                                SubResourceId{c, a}, {stagingXYOffset[0], stagingXYOffset[1], 0}},
                            Metal::BlitEncoder::CopyPartial_Src{
                                &stagingResource, 
                                SubResourceId{c-stagingLODOffset, a},
                                {(unsigned)srcBox._left, (unsigned)srcBox._top, 0u},
                                {(unsigned)srcBox._right, (unsigned)srcBox._bottom, 1u}});
                    }
                }
            }
        }

        unsigned UnderlyingDeviceContext::PushToBuffer(
            IResource& resource, const ResourceDesc& desc, unsigned offset,
            const void* data, size_t dataSize)
        {
			auto* metalResource = (Metal::Resource*)resource.QueryInterface(typeid(Metal::Resource).hash_code());
			if (!metalResource)
				Throw(::Exceptions::BasicLabel("Incorrect resource type passed to buffer uploads platform layer"));

            // note -- this is a direct, immediate map... There must be no contention while we map.
            assert(desc._type == ResourceDesc::Type::LinearBuffer);
			auto& metalContext = *Metal::DeviceContext::Get(*_renderCoreContext);
            Metal::ResourceMap map(metalContext, *metalResource, Metal::ResourceMap::Mode::WriteDiscardPrevious, SubResourceId{0,0}, offset);
            auto copyAmount = std::min(map.GetData().size(), dataSize);
            if (copyAmount > 0)
                XlCopyMemory(map.GetData().begin(), data, copyAmount);
            return (unsigned)copyAmount;
        }

        void UnderlyingDeviceContext::ResourceCopy_DefragSteps(
			const UnderlyingResourcePtr& destination, const UnderlyingResourcePtr& source, 
			const std::vector<DefragStep>& steps)
        {
            assert(0);
        }

        void UnderlyingDeviceContext::ResourceCopy(UnderlyingResource& destination, UnderlyingResource& source)
        {
            assert(0);
        }

        std::shared_ptr<Metal::CommandList> UnderlyingDeviceContext::ResolveCommandList()
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
            if (!metalContext->HasActiveCommandList())
                metalContext->BeginCommandList();
        }

#if 0
		class RawDataPacket_ReadBack : public IDataPacket
		{
		public:
			void*           GetData(SubResourceId subRes);
			size_t          GetDataSize(SubResourceId subRes) const;
			TexturePitches  GetPitches(SubResourceId subRes) const;

			RawDataPacket_ReadBack(
				Metal::DeviceContext& context,
				Metal::Resource& resource, 
				SubResourceId subResource);
			~RawDataPacket_ReadBack();
		protected:
			Metal::ResourceMap _resourceMap;
			SubResourceId _mappedSubResource;
		};

		void*           RawDataPacket_ReadBack::GetData(SubResourceId subRes)
		{
			assert(_mappedSubResource._mip == subRes._mip && _mappedSubResource._arrayLayer == subRes._arrayLayer);
			return _resourceMap.GetData().begin();
		}

		size_t          RawDataPacket_ReadBack::GetDataSize(SubResourceId subRes) const
		{
			assert(_mappedSubResource._mip == subRes._mip && _mappedSubResource._arrayLayer == subRes._arrayLayer);
			return _resourceMap.GetData().size();
		}

		TexturePitches  RawDataPacket_ReadBack::GetPitches(SubResourceId subRes) const
		{
			assert(_mappedSubResource._mip == subRes._mip && _mappedSubResource._arrayLayer == subRes._arrayLayer);
			return TexturePitches { (unsigned)_resourceMap.GetData().size(), (unsigned)_resourceMap.GetData().size(), (unsigned)_resourceMap.GetData().size() };
		}

		RawDataPacket_ReadBack::RawDataPacket_ReadBack(
			Metal::DeviceContext& context, Metal::Resource& resource, SubResourceId subResource)
		: _resourceMap(context, resource, Metal::ResourceMap::Mode::Read, subResource)
		, _mappedSubResource(subResource)
		{
		}

		RawDataPacket_ReadBack::~RawDataPacket_ReadBack()
		{}

        intrusive_ptr<DataPacket> UnderlyingDeviceContext::Readback(const ResourceLocator& locator)
        {
			auto* res = (Metal::Resource*)locator.GetUnderlying()->QueryInterface(typeid(Metal::Resource).hash_code());
			assert(res);
			auto& metalContext = *Metal::DeviceContext::Get(*_renderCoreContext);
			return make_intrusive<RawDataPacket_ReadBack>(std::ref(metalContext), std::ref(*res), SubResourceId{0,0});
        }
#endif

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

#if 0
        IResourcePtr CreateResource(IDevice& device, const ResourceDesc& desc, DataPacket* initialisationData)
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

		ResourceDesc ExtractDesc(RenderCore::IResource& resource)
        {
            return resource.GetDesc();
        }
#endif

    }}

#endif
