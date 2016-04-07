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

    namespace BufferUploads { namespace PlatformInterface
    {
		static Underlying::Resource*        ResPtr(const Underlying::Resource& resource) { return const_cast<Underlying::Resource*>(&resource); }

        void UnderlyingDeviceContext::PushToResource(   const Underlying::Resource& resource, const BufferDesc& desc, 
                                                        unsigned resourceOffsetValue, const void* data, size_t dataSize,
                                                        TexturePitches rowAndSlicePitch, 
                                                        const Box2D& box, unsigned lodLevel, unsigned arrayIndex)
        {
        }

        void UnderlyingDeviceContext::PushToStagingResource(    const Underlying::Resource& resource, const BufferDesc&desc, 
                                                                unsigned resourceOffsetValue, const void* data, size_t dataSize, 
                                                                TexturePitches rowAndSlicePitch, 
                                                                const Box2D& box, unsigned lodLevel, unsigned arrayIndex)
        {
        }

        void UnderlyingDeviceContext::UpdateFinalResourceFromStaging(const Underlying::Resource& finalResource, const Underlying::Resource& staging, const BufferDesc& destinationDesc, unsigned lodLevelMin, unsigned lodLevelMax, unsigned stagingLODOffset)
        {
        }

        #pragma warning(disable:4127)       // conditional expression is constant

        void UnderlyingDeviceContext::ResourceCopy_DefragSteps(const Underlying::Resource& destination, const Underlying::Resource& source, const std::vector<DefragStep>& steps)
        {
        }

        void UnderlyingDeviceContext::ResourceCopy(const Underlying::Resource& destination, const Underlying::Resource& source)
        {
            RenderCore::Metal::Copy(*_devContext, ResPtr(destination), ResPtr(source));
        }

        RenderCore::Metal::CommandListPtr UnderlyingDeviceContext::ResolveCommandList()
        {
            return nullptr;
        }

        void                        UnderlyingDeviceContext::BeginCommandList()
        {
        }

        UnderlyingDeviceContext::MappedBuffer UnderlyingDeviceContext::Map(const Underlying::Resource& resource, MapType::Enum mapType, unsigned subResource)
        {
            return MappedBuffer();
        }

        UnderlyingDeviceContext::MappedBuffer UnderlyingDeviceContext::MapPartial(const Underlying::Resource& resource, MapType::Enum mapType, unsigned offset, unsigned size, unsigned subResource)
        {
            return MappedBuffer();
        }

        void UnderlyingDeviceContext::Unmap(const Underlying::Resource& resource, unsigned subResourceIndex)
        {
        }

        UnderlyingDeviceContext::UnderlyingDeviceContext(RenderCore::IThreadContext& renderCoreContext) 
        : _renderCoreContext(&renderCoreContext)
        {
        }

        intrusive_ptr<RenderCore::Metal::Underlying::Resource> CreateResource(ObjectFactory& device, const BufferDesc& desc, DataPacket* initialisationData)
        {
			return nullptr;
        }

		BufferDesc ExtractDesc(const Underlying::Resource& resource)
        {
            BufferDesc desc;
            XlZeroMemory(desc);
            desc._type = BufferDesc::Type::Unknown;
            return desc;
        }

    }}

#endif
