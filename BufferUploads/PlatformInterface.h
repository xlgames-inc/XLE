// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"
#include "DataPacket.h"     // (actually just for TexturePitches)
#include "../RenderCore/IDevice_Forward.h"
#include "../Utility/IntrusivePtr.h"
#include "../RenderCore/Metal/Forward.h"

namespace Utility { class DefragStep; }

namespace BufferUploads { namespace PlatformInterface
{
        /////////////////////////////////////////////////////////////////////

	using UnderlyingResource = RenderCore::IResource;
	using UnderlyingResourcePtr = RenderCore::IResourcePtr;

	UnderlyingResourcePtr CreateResource(RenderCore::IDevice& device, const ResourceDesc& desc, DataPacket* initialisationData = nullptr);
    ResourceDesc      ExtractDesc(RenderCore::IResource& resource);

    int64_t           QueryPerformanceCounter();

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
            UnderlyingResource& resource, const ResourceDesc& desc, unsigned offset,
            const void* data, size_t dataSize);
        
        using ResourceInitializer = std::function<RenderCore::SubResourceInitData(RenderCore::SubResourceId)>;
        
        unsigned PushToTexture(
            UnderlyingResource& resource, const ResourceDesc& desc,
            const RenderCore::Box2D& box, 
            const ResourceInitializer& data);

        unsigned PushToStagingTexture(
            UnderlyingResource& resource, const ResourceDesc& desc,
            const RenderCore::Box2D& box, 
            const ResourceInitializer& data);

        void UpdateFinalResourceFromStaging(
            UnderlyingResource& finalResource, UnderlyingResource& staging,
            const ResourceDesc& destinationDesc, unsigned lodLevelMin=~unsigned(0x0), unsigned lodLevelMax=~unsigned(0x0), 
            unsigned stagingLODOffset=0,
            VectorPattern<unsigned, 2> destXYOffset = {0,0},
            const RenderCore::Box2D& srcBox = RenderCore::Box2D());

            ////////   R E S O U R C E   C O P Y   ////////
        void ResourceCopy_DefragSteps(const UnderlyingResourcePtr& destination, const UnderlyingResourcePtr& source, const std::vector<Utility::DefragStep>& steps);
        void ResourceCopy(UnderlyingResource& destination, UnderlyingResource& source);

            ////////   C O M M A N D   L I S T S   ////////
        std::shared_ptr<RenderCore::Metal::CommandList> ResolveCommandList();
        void                                            BeginCommandList();

            ////////   R E A D   B A C K   ////////
        intrusive_ptr<DataPacket> Readback(const ResourceLocator& locator);

            ////////   C O N S T R U C T I O N   ////////
        UnderlyingDeviceContext(RenderCore::IThreadContext& renderCoreContext);
        ~UnderlyingDeviceContext();

		std::shared_ptr<RenderCore::IDevice> GetObjectFactory();
        RenderCore::IThreadContext& GetUnderlying() { return *_renderCoreContext; }
        // RenderCore::Metal::DeviceContext& GetUnderlying() { return *_devContext.get(); }

        #if GFXAPI_TARGET == GFXAPI_DX11
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
#if 0
    class GPUEventStack
    {
    public:
        typedef unsigned    EventID;

        void        TriggerEvent(RenderCore::Metal::DeviceContext* context, EventID event);
        void        Update(RenderCore::Metal::DeviceContext* context);
        EventID     GetLastCompletedEvent() const       { return _lastCompletedID; }

        void        OnLostDevice();
        void        OnDeviceReset();

        GPUEventStack(RenderCore::IDevice& device);
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
#else
	class GPUEventStack
	{
	public:
		typedef unsigned    EventID;

		void        TriggerEvent(RenderCore::Metal::DeviceContext* context, EventID event);
		void        Update(RenderCore::Metal::DeviceContext* context);
		EventID     GetLastCompletedEvent() const;

		void        OnLostDevice();
		void        OnDeviceReset();

		GPUEventStack(RenderCore::IDevice& device);
		~GPUEventStack();
	};
#endif

        ///////////////////////////////////////////////////////////////////

            ////////   F U N C T I O N A L I T Y   F L A G S   ////////

        //          Use these to customise behaviour for platforms
        //          without lots of #if defined(...) type code
    #if GFXAPI_TARGET == GFXAPI_DX11
		static const bool SupportsResourceInitialisation_Texture = true;
		static const bool SupportsResourceInitialisation_Buffer = true;
        static const bool RequiresStagingTextureUpload = false;
        static const bool RequiresStagingResourceReadBack = true;
        static const bool CanDoNooverwriteMapInBackground = false;
        static const bool UseMapBasedDefrag = false;
        static const bool ContextBasedMultithreading = true;
        static const bool CanDoPartialMaps = false;
        static const bool NonVolatileResourcesTakeSystemMemory = false;
    #elif GFXAPI_TARGET == GFXAPI_DX9
		static const bool SupportsResourceInitialisation_Texture = false;
		static const bool SupportsResourceInitialisation_Buffer = false;
        static const bool RequiresStagingTextureUpload = true;
        static const bool RequiresStagingResourceReadBack = false;
        static const bool CanDoNooverwriteMapInBackground = true;
        static const bool UseMapBasedDefrag = true;
        static const bool ContextBasedMultithreading = false;
        static const bool CanDoPartialMaps = true;
        static const bool NonVolatileResourcesTakeSystemMemory = true;
    #elif GFXAPI_TARGET == GFXAPI_OPENGLES
        static const bool SupportsResourceInitialisation_Texture = true;
		static const bool SupportsResourceInitialisation_Buffer = true;
        static const bool RequiresStagingTextureUpload = false;
        static const bool RequiresStagingResourceReadBack = true;
        static const bool CanDoNooverwriteMapInBackground = false;
        static const bool UseMapBasedDefrag = false;
        static const bool ContextBasedMultithreading = true;
        static const bool CanDoPartialMaps = false;
        static const bool NonVolatileResourcesTakeSystemMemory = false;
	#elif GFXAPI_TARGET == GFXAPI_VULKAN
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
