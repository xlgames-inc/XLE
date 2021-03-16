// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../RenderCore/Metal/Metal.h"

#if GFXAPI_TARGET == GFXAPI_VULKAN

    #include "../PlatformInterface.h"
    #include "../DataPacket.h"
	#include "../../RenderCore/IThreadContext.h"
    #include "../../RenderCore/Metal/Resource.h"
    #include "../../RenderCore/Metal/DeviceContext.h"
    #include "../../RenderCore/ResourceUtils.h"

    namespace BufferUploads { namespace PlatformInterface
    {
        using namespace RenderCore;

        unsigned UnderlyingDeviceContext::WriteToTextureViaMap(
            const ResourceLocator& resource, const ResourceDesc& desc,
            const Box2D& box, 
            const ResourceInitializer& data)
        {
            assert(resource.IsWholeResource());
			auto* metalResource = resource.GetContainingResource().get();

            // In Vulkan, the only way we have to send data to a resource is by using
            // a memory map and CPU assisted copy. 
            assert(desc._type == ResourceDesc::Type::Texture);
            if (box == Box2D()) {
                auto* vulkanResource = (Metal::Resource*)metalResource->QueryInterface(typeid(Metal::Resource).hash_code());
                assert(vulkanResource);
                return Metal::Internal::CopyViaMemoryMap(*_renderCoreContext->GetDevice(), *vulkanResource, data);
            }

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

        void UnderlyingDeviceContext::UpdateFinalResourceFromStaging(
			const ResourceLocator& finalResource, const ResourceLocator& stagingResource, 
			const ResourceDesc& destinationDesc, 
            const StagingToFinalMapping& stagingToFinalMapping)
        {
            assert(finalResource.IsWholeResource());
            assert(stagingResource.IsWholeResource());

            assert(destinationDesc._type == ResourceDesc::Type::Texture);
            auto dstLodLevelMax = std::min(stagingToFinalMapping._dstLodLevelMax, (unsigned)destinationDesc._textureDesc._mipCount-1);
            auto dstArrayLayerMax = std::min(stagingToFinalMapping._dstArrayLayerMax, (unsigned)destinationDesc._textureDesc._arrayCount-1);
            auto allLods = stagingToFinalMapping._dstLodLevelMin == 0 && dstLodLevelMax == ((unsigned)destinationDesc._textureDesc._mipCount-1);
            auto allArrayLayers = stagingToFinalMapping._dstArrayLayerMin == 0 && dstArrayLayerMax == ((unsigned)destinationDesc._textureDesc._arrayCount-1);
            if (destinationDesc._textureDesc._arrayCount == 0) {
                dstArrayLayerMax = 0;
                allArrayLayers = true;
            }
            auto entire2DPlane = stagingToFinalMapping._stagingXYOffset[0] == 0 && stagingToFinalMapping._stagingXYOffset[1] == 0;

			// During the transfer, the images must be in either TransferSrcOptimal, TransferDstOptimal or General.
            auto& metalContext = *Metal::DeviceContext::Get(*_renderCoreContext);
            Metal::Internal::CaptureForBind(metalContext, *finalResource.GetContainingResource(), BindFlag::TransferDst);
            Metal::Internal::CaptureForBind(metalContext, *stagingResource.GetContainingResource(), BindFlag::TransferSrc);
            auto blitEncoder = metalContext.BeginBlitEncoder();

            if (allLods && allArrayLayers && entire2DPlane) {
                blitEncoder.Copy(*finalResource.GetContainingResource().get(), *stagingResource.GetContainingResource().get());
            } else {
                auto& dstBox = stagingToFinalMapping._dstBox;
                for (unsigned a=stagingToFinalMapping._dstArrayLayerMin; a<=dstArrayLayerMax; ++a) {
                    for (unsigned mip=stagingToFinalMapping._dstLodLevelMin; mip<=dstLodLevelMax; ++mip) {
                        blitEncoder.Copy(
                            Metal::BlitEncoder::CopyPartial_Dest{
                                finalResource.GetContainingResource().get(), 
                                SubResourceId{mip, a}, {(unsigned)dstBox._left, (unsigned)dstBox._top, 0}},
                            Metal::BlitEncoder::CopyPartial_Src{
                                stagingResource.GetContainingResource().get(), 
                                SubResourceId{mip-stagingToFinalMapping._stagingLODOffset, a-stagingToFinalMapping._stagingArrayOffset},
                                {(unsigned)dstBox._left - stagingToFinalMapping._stagingXYOffset[0], (unsigned)dstBox._top - stagingToFinalMapping._stagingXYOffset[1], 0u},
                                {(unsigned)dstBox._right - stagingToFinalMapping._stagingXYOffset[0], (unsigned)dstBox._bottom - stagingToFinalMapping._stagingXYOffset[1], 1u}});
                    }
                }
            }
        }

        unsigned UnderlyingDeviceContext::WriteToBufferViaMap(
            const ResourceLocator& resource, const ResourceDesc& desc, unsigned offset,
            IteratorRange<const void*> data)
        {
			assert(resource.IsWholeResource());
			auto* metalResource = resource.GetContainingResource().get();

            // note -- this is a direct, immediate map... There must be no contention while we map.
            assert(desc._type == ResourceDesc::Type::LinearBuffer);
			auto& metalContext = *Metal::DeviceContext::Get(*_renderCoreContext);
            Metal::ResourceMap map(metalContext, *metalResource, Metal::ResourceMap::Mode::WriteDiscardPrevious, SubResourceId{0,0}, offset);
            auto copyAmount = std::min(map.GetData().size(), data.size());
            if (copyAmount > 0)
                XlCopyMemory(map.GetData().begin(), data.begin(), copyAmount);
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
