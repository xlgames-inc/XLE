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

    namespace BufferUploads { namespace PlatformInterface
    {
        void UnderlyingDeviceContext::PushToResource(
			UnderlyingResource& resource, const BufferDesc& desc, 
            unsigned resourceOffsetValue, const void* data, size_t dataSize,
            TexturePitches rowAndSlicePitch, 
            const Box2D& box, unsigned lodLevel, unsigned arrayIndex)
        {
        }

        void UnderlyingDeviceContext::PushToStagingResource(
			UnderlyingResource& resource, const BufferDesc&desc, 
            unsigned resourceOffsetValue, const void* data, size_t dataSize, 
            TexturePitches rowAndSlicePitch, 
            const Box2D& box, unsigned lodLevel, unsigned arrayIndex)
        {
        }

        void UnderlyingDeviceContext::UpdateFinalResourceFromStaging(
			UnderlyingResource& finalResource, UnderlyingResource& staging, 
			const BufferDesc& destinationDesc, unsigned lodLevelMin, unsigned lodLevelMax, unsigned stagingLODOffset)
        {
        }

        #pragma warning(disable:4127)       // conditional expression is constant

        void UnderlyingDeviceContext::ResourceCopy_DefragSteps(
			UnderlyingResource& destination, UnderlyingResource& source, 
			const std::vector<DefragStep>& steps)
        {
        }

        void UnderlyingDeviceContext::ResourceCopy(UnderlyingResource& destination, UnderlyingResource& source)
        {
            // RenderCore::Metal::Copy(*_devContext, ResPtr(destination), ResPtr(source));
        }

        RenderCore::Metal::CommandListPtr UnderlyingDeviceContext::ResolveCommandList()
        {
            return nullptr;
        }

        void                        UnderlyingDeviceContext::BeginCommandList()
        {
        }

        UnderlyingDeviceContext::MappedBuffer UnderlyingDeviceContext::Map(UnderlyingResource& resource, MapType::Enum mapType, unsigned subResource)
        {
            return MappedBuffer();
        }

        UnderlyingDeviceContext::MappedBuffer UnderlyingDeviceContext::MapPartial(UnderlyingResource& resource, MapType::Enum mapType, unsigned offset, unsigned size, unsigned subResource)
        {
            return MappedBuffer();
        }

        void UnderlyingDeviceContext::Unmap(UnderlyingResource& resource, unsigned subResourceIndex)
        {
        }

		std::shared_ptr<RenderCore::IDevice> UnderlyingDeviceContext::GetObjectFactory()
		{
			return _renderCoreContext->GetDevice();
		}

        UnderlyingDeviceContext::UnderlyingDeviceContext(RenderCore::IThreadContext& renderCoreContext) 
        : _renderCoreContext(&renderCoreContext)
        {
        }

        RenderCore::ResourcePtr CreateResource(RenderCore::IDevice& device, const BufferDesc& desc, DataPacket* initialisationData)
        {
			if (initialisationData) {
				return device.CreateResource(desc,
					[initialisationData](unsigned mipIndex, unsigned arrayIndex) -> RenderCore::SubResourceInitData
					{
						RenderCore::SubResourceInitData result;
						auto sr = DataPacket::TexSubRes(mipIndex, arrayIndex);
						result._data = initialisationData->GetData(sr);
						result._size = initialisationData->GetDataSize(sr);
						auto pitches = initialisationData->GetPitches(sr);
						result._rowPitch = pitches._rowPitch;
						result._slicePitch = pitches._slicePitch;
						return result;
					});
			} else {
				return device.CreateResource(desc);
			}
        }

		BufferDesc ExtractDesc(UnderlyingResource& resource)
        {
            BufferDesc desc;
            XlZeroMemory(desc);
            desc._type = BufferDesc::Type::Unknown;
            return desc;
        }

    }}

#endif
