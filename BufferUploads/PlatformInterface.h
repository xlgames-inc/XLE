// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"
#include "DataPacket.h"     // (actually just for TexturePitches)
#include "../RenderCore/IDevice.h"
#include "../Utility/IntrusivePtr.h"
#include "../RenderCore/Metal/Types.h"              // (for RenderCore::Metal::UnderlyingQuery)
#include "../RenderCore/Metal/DeviceContext.h"      // (for RenderCore::Metal::CommandListPtr)

namespace Utility { class DefragStep; }
namespace RenderCore { enum class Format; }

namespace BufferUploads { namespace PlatformInterface
{
        /////////////////////////////////////////////////////////////////////

	using UnderlyingQuery = RenderCore::Metal::UnderlyingQuery;
	using UnderlyingResource = RenderCore::Resource;
	using UnderlyingResourcePtr = RenderCore::ResourcePtr;
	using NativeFormat = RenderCore::Format;

	UnderlyingResourcePtr CreateResource(RenderCore::IDevice& device, const BufferDesc& desc, DataPacket* initialisationData = nullptr);
    BufferDesc      ExtractDesc(RenderCore::Resource& resource);

    int64           QueryPerformanceCounter();

    void            Resource_Register(UnderlyingResource& resource, const char name[]);
    void            Resource_Report(bool justVolatiles);
    void            Resource_SetName(UnderlyingResource& resource, const char name[]);
    void            Resource_GetName(UnderlyingResource& resource, char buffer[], int bufferSize);
    size_t          Resource_GetAll(BufferUploads::BufferMetrics** bufferDescs);

    size_t          Resource_GetVideoMemoryHeadroom();
    void            Resource_RecalculateVideoMemoryHeadroom();
    void            Resource_ScheduleVideoMemoryHeadroomCalculation();

        /////////////////////////////////////////////////////////////////////

    class UnderlyingDeviceContext
    {
    public:
            ////////   P U S H   T O   R E S O U R C E   ////////
        unsigned PushToBuffer(
            UnderlyingResource& resource, const BufferDesc& desc, unsigned offset,
            const void* data, size_t dataSize);
        
        using ResourceInitializer = std::function<RenderCore::SubResourceInitData(RenderCore::SubResourceId)>;
        
        unsigned PushToTexture(
            UnderlyingResource& resource, const BufferDesc& desc,
            const RenderCore::Box2D& box, 
            const ResourceInitializer& data);

        unsigned PushToStagingTexture(
            UnderlyingResource& resource, const BufferDesc& desc,
            const RenderCore::Box2D& box, 
            const ResourceInitializer& data);

        void UpdateFinalResourceFromStaging(
            UnderlyingResource& finalResource, UnderlyingResource& staging,
            const BufferDesc& destinationDesc, unsigned lodLevelMin=~unsigned(0x0), unsigned lodLevelMax=~unsigned(0x0), 
            unsigned stagingLODOffset=0,
            VectorPattern<unsigned, 2> destXYOffset = {0,0},
            const RenderCore::Box2D& srcBox = RenderCore::Box2D());

            ////////   R E S O U R C E   C O P Y   ////////
        void ResourceCopy_DefragSteps(UnderlyingResource& destination, UnderlyingResource& source, const std::vector<Utility::DefragStep>& steps);
        void ResourceCopy(UnderlyingResource& destination, UnderlyingResource& source);

            ////////   C O M M A N D   L I S T S   ////////
        RenderCore::Metal::CommandListPtr       ResolveCommandList();
        void                                    BeginCommandList();

            ////////   R E A D   B A C K   ////////
        intrusive_ptr<DataPacket> Readback(const ResourceLocator& locator);

            ////////   C O N S T R U C T I O N   ////////
        UnderlyingDeviceContext(RenderCore::IThreadContext& renderCoreContext);
        ~UnderlyingDeviceContext();

		std::shared_ptr<RenderCore::IDevice> GetObjectFactory();
        RenderCore::IThreadContext& GetUnderlying() { return *_renderCoreContext; }
        // RenderCore::Metal::DeviceContext& GetUnderlying() { return *_devContext.get(); }

        #if GFXAPI_ACTIVE == GFXAPI_DX11
            private: 
                bool _useUpdateSubresourceWorkaround;
        #endif

    private:
        // void Unmap(UnderlyingResource&, unsigned _subresourceIndex);
        // friend class MappedBuffer;
        RenderCore::IThreadContext*         _renderCoreContext;
        // std::shared_ptr<RenderCore::Metal::DeviceContext>      _devContext;
    };

    UnderlyingDeviceContext::ResourceInitializer AsResourceInitializer(DataPacket& pkt);

        /////////////////////////////////////////////////////////////////////
    class GPUEventStack
    {
    public:
        typedef unsigned    EventID;

        void        TriggerEvent(RenderCore::Metal::DeviceContext* context, EventID event);
        void        Update(RenderCore::Metal::DeviceContext* context);
        EventID     GetLastCompletedEvent() const       { return _lastCompletedID; }

        void        OnLostDevice();
        void        OnDeviceReset();

        GPUEventStack(RenderCore::IDevice* device);
        ~GPUEventStack();
    private:
        typedef Interlocked::Value    QueryID;
        struct Query
        {
            UnderlyingQuery _query;
            EventID _eventID;
            QueryID _assignedID;
            Query();
            ~Query();
        };
        std::vector<Query> _queries;
        QueryID _nextQueryID;
        QueryID _nextQueryIDToSchedule;

        EventID _lastCompletedID;

        RenderCore::Metal::ObjectFactory* _objFactory;
    };

        ///////////////////////////////////////////////////////////////////

            ////////   F U N C T I O N A L I T Y   F L A G S   ////////

        //          Use these to customise behaviour for platforms
        //          without lots of #if defined(...) type code
    #if GFXAPI_ACTIVE == GFXAPI_DX11
		static const bool SupportsResourceInitialisation_Texture = true;
		static const bool SupportsResourceInitialisation_Buffer = true;
        static const bool RequiresStagingTextureUpload = false;
        static const bool RequiresStagingResourceReadBack = true;
        static const bool CanDoNooverwriteMapInBackground = false;
        static const bool UseMapBasedDefrag = false;
        static const bool ContextBasedMultithreading = true;
        static const bool CanDoPartialMaps = false;
        static const bool NonVolatileResourcesTakeSystemMemory = false;
    #elif GFXAPI_ACTIVE == GFXAPI_DX9
		static const bool SupportsResourceInitialisation_Texture = false;
		static const bool SupportsResourceInitialisation_Buffer = false;
        static const bool RequiresStagingTextureUpload = true;
        static const bool RequiresStagingResourceReadBack = false;
        static const bool CanDoNooverwriteMapInBackground = true;
        static const bool UseMapBasedDefrag = true;
        static const bool ContextBasedMultithreading = false;
        static const bool CanDoPartialMaps = true;
        static const bool NonVolatileResourcesTakeSystemMemory = true;
    #elif GFXAPI_ACTIVE == GFXAPI_OPENGLES
        static const bool SupportsResourceInitialisation_Texture = true;
		static const bool SupportsResourceInitialisation_Buffer = true;
        static const bool RequiresStagingTextureUpload = false;
        static const bool RequiresStagingResourceReadBack = true;
        static const bool CanDoNooverwriteMapInBackground = false;
        static const bool UseMapBasedDefrag = false;
        static const bool ContextBasedMultithreading = true;
        static const bool CanDoPartialMaps = false;
        static const bool NonVolatileResourcesTakeSystemMemory = false;
	#elif GFXAPI_ACTIVE == GFXAPI_VULKAN
		// Vulkan capabilities haven't been tested!
		static const bool SupportsResourceInitialisation_Texture = false;
		static const bool SupportsResourceInitialisation_Buffer = true;
		static const bool RequiresStagingTextureUpload = true;
		static const bool RequiresStagingResourceReadBack = true;
		static const bool CanDoNooverwriteMapInBackground = true;
		static const bool UseMapBasedDefrag = false;
		static const bool ContextBasedMultithreading = true;
		static const bool CanDoPartialMaps = true;
		static const bool NonVolatileResourcesTakeSystemMemory = false;
	#else
        #error Unsupported platform!
    #endif

        /////////////////////////////////////////////////////////////////////

}}
