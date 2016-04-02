// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlatformInterface.h"
#include "../RenderCore/Metal/Format.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/StringFormat.h"
#include "../Utility/TimeUtils.h"
#include <assert.h>

#if GFXAPI_ACTIVE == GFXAPI_DX11
    #include "../../RenderCore/DX11/Metal/IncludeDX11.h"
#endif

// #pragma warning(disable:4127)
#pragma warning(disable:4505)       // unreferenced local function has been removed

namespace BufferUploads 
{
    bool operator==(const Box2D& lhs, const Box2D& rhs) 
    {
        return      lhs._left == rhs._left
                &&  lhs._top == rhs._top
                &&  lhs._right == rhs._right
                &&  lhs._bottom == rhs._bottom
                ;
    }
}

namespace BufferUploads { namespace PlatformInterface
{
    static const unsigned BlockCompDim = 4;
    static unsigned    RoundBCDim(unsigned input)
    {
        auto result = (input + 3) & ~3;
        assert(!(result%BlockCompDim));
        return result;
    }

    static bool IsDXTCompressed(unsigned format) { return GetCompressionType(NativeFormat::Enum(format)) == FormatCompressionType::BlockCompression; }

    unsigned TextureDataSize(unsigned nWidth, unsigned nHeight, unsigned nDepth, unsigned mipCount, NativeFormat::Enum format)
    {
        if (format == 0 /* invalid format most of the time */) {
            return 0;
        }

        const bool dxt = IsDXTCompressed(format);
        const auto bbp = BitsPerPixel(format);

        mipCount = std::max(mipCount, 1u);
        unsigned result = 0;
        for (unsigned mipIterator = 0; (mipIterator < mipCount) && (nWidth || nHeight || nDepth); ++mipIterator) {
            if (dxt) {
                auto blockWidth = std::max((nWidth + BlockCompDim - 1u) / BlockCompDim, 1u);
                auto blockHeight = std::max((nHeight + BlockCompDim - 1u) / BlockCompDim, 1u);
                result += blockWidth * blockHeight * std::max(nDepth, 1u) * (bbp * 16u / 8u);
            } else {
                result += std::max(nWidth, 1u) * std::max(nHeight, 1u) * std::max(nDepth, 1u) * (bbp / 8u);
            }

            nWidth >>= 1;
            nHeight >>= 1;
            nDepth >>= 1;
        }

        return result;
    }

    void CopyMipLevel(      void* destination, size_t destinationDataSize,
                            const void* sourceData, size_t sourceDataSize,
                            const TextureDesc& mipMapDesc, unsigned destinationBlockRowPitch)
    {
        int nD3DSize;
        bool isDXTCompressed = IsDXTCompressed((NativeFormat::Enum)mipMapDesc._nativePixelFormat);
        if (isDXTCompressed) {
            nD3DSize = TextureDataSize(mipMapDesc._width, mipMapDesc._height, 1, 1, (NativeFormat::Enum)mipMapDesc._nativePixelFormat);
        } else {
            nD3DSize = destinationBlockRowPitch * mipMapDesc._height;
        }

        auto originalDest = destination;
        (void)originalDest;

        if (size_t(nD3DSize) != sourceDataSize) {
            unsigned sourceRowPitch;
            unsigned rows;
            if (isDXTCompressed) {
                sourceRowPitch = TextureDataSize(RoundBCDim(mipMapDesc._width), BlockCompDim, 1, 1, (NativeFormat::Enum)mipMapDesc._nativePixelFormat);
                rows = (mipMapDesc._height + BlockCompDim - 1) / BlockCompDim;
            } else {
                sourceRowPitch = TextureDataSize(mipMapDesc._width, 1, 1, 1, (NativeFormat::Enum)mipMapDesc._nativePixelFormat);
                rows = mipMapDesc._height;
            }

            rows = std::min(rows, unsigned(sourceDataSize/sourceRowPitch));
            for (unsigned j = 0; j < rows; j++) {
                assert((size_t(destination) + sourceRowPitch - size_t(originalDest)) <= destinationDataSize);
                XlCopyMemoryAlign16(destination, sourceData, sourceRowPitch);
                sourceData = PtrAdd(sourceData, sourceRowPitch);
                destination = PtrAdd(destination, destinationBlockRowPitch);
            }

        } else {
                // Copy data to/from video texture
            int nPitch = TextureDataSize(mipMapDesc._width, 1, 1, 1, (NativeFormat::Enum)mipMapDesc._nativePixelFormat);
            assert(sourceDataSize % nPitch == 0); (void)nPitch;
            assert(size_t(destination) + sourceDataSize <= destinationDataSize);
            XlCopyMemoryAlign16((uint8*)destination, sourceData, sourceDataSize);
        }
    }

    TextureDesc  CalculateMipMapDesc(const TextureDesc& topMostMipDesc, unsigned mipMapIndex)
    {
        assert(mipMapIndex<topMostMipDesc._mipCount);
        TextureDesc result = topMostMipDesc;
        result._width    = std::max(result._width  >> mipMapIndex, 1u); 
        result._height   = std::max(result._height >> mipMapIndex, 1u);
        if (IsDXTCompressed((NativeFormat::Enum)topMostMipDesc._nativePixelFormat)) { 
            result._width = RoundBCDim(result._width);
            result._height = RoundBCDim(result._height);
        }
        //result._depth  = std::max(minDimension, result._depth>>mipMapIndex); 
        result._mipCount -= uint8(mipMapIndex);
        return result;
    }
    
    unsigned ByteCount(const TextureDesc& tDesc)
    {
        unsigned bitsPerPixel = BitsPerPixel((NativeFormat::Enum)tDesc._nativePixelFormat);
        unsigned result = 0;
        unsigned mipMin = IsDXTCompressed((NativeFormat::Enum)tDesc._nativePixelFormat)?4:1;
        for (unsigned mipIndex=0; mipIndex<tDesc._mipCount; ++mipIndex) {
            result  +=  std::max(mipMin, tDesc._width>>mipIndex)
                    *   std::max(mipMin, tDesc._height>>mipIndex)
                    *   std::max(uint16(1), tDesc._arrayCount)
                    *   bitsPerPixel
                    /   8
                    ;
        }
        return result;
    }

    unsigned ByteCount(const BufferDesc& desc)
    {
        if (desc._type == BufferDesc::Type::LinearBuffer) {
            return desc._linearBufferDesc._sizeInBytes;
        } else if (desc._type == BufferDesc::Type::Texture) {
            return ByteCount(desc._textureDesc);
        }
        return 0;
    }

    int64 QueryPerformanceCounter()
    {
        return Utility::GetPerformanceCounter();
    }

        //////////////////////////////////////////////////////////////////////////////////////////////

    UnderlyingDeviceContext::MappedBuffer::~MappedBuffer()
    {
        if (_sourceContext && _data) {
            _sourceContext->Unmap(*_resource.get(), _subResourceIndex);
        }
    }

    UnderlyingDeviceContext::MappedBuffer::MappedBuffer()
    {
        _sourceContext = 0;
        _subResourceIndex = 0;
        _data = 0;
    }

    UnderlyingDeviceContext::MappedBuffer::MappedBuffer(MappedBuffer&& moveFrom) never_throws
    {
        _sourceContext = std::move(moveFrom._sourceContext);
        _resource = std::move(moveFrom._resource);
        _subResourceIndex = std::move(moveFrom._subResourceIndex);
        _data = std::move(moveFrom._data);
        _pitches = moveFrom._pitches;
        moveFrom._data = nullptr;
        moveFrom._subResourceIndex = 0;
        moveFrom._sourceContext = nullptr;
        moveFrom._pitches = TexturePitches();
    }

    const UnderlyingDeviceContext::MappedBuffer& UnderlyingDeviceContext::MappedBuffer::operator=(UnderlyingDeviceContext::MappedBuffer&& moveFrom)
    {
        if (_sourceContext && _data) {
            _sourceContext->Unmap(*_resource.get(), _subResourceIndex);
        }

        _sourceContext = std::move(moveFrom._sourceContext);
        _resource = std::move(moveFrom._resource);
        _subResourceIndex = std::move(moveFrom._subResourceIndex);
        _data = std::move(moveFrom._data);
        _pitches = moveFrom._pitches;
        moveFrom._data = nullptr;
        moveFrom._subResourceIndex = 0;
        moveFrom._sourceContext = nullptr;
        moveFrom._pitches = TexturePitches();
        return *this;
    }

    UnderlyingDeviceContext::MappedBuffer::MappedBuffer(
        UnderlyingDeviceContext& context, const Underlying::Resource& resource, 
        unsigned subResourceIndex, void* data,
        TexturePitches pitches)
    {
        _sourceContext = &context;
        _resource.reset(const_cast<Underlying::Resource*>(&resource));
        _subResourceIndex = subResourceIndex;
        _data = data;
        _pitches = pitches;
    }

    UnderlyingDeviceContext::~UnderlyingDeviceContext() {}


        //////////////////////////////////////////////////////////////////////////////////////////////

    #if GFXAPI_ACTIVE == GFXAPI_DX11
        intrusive_ptr<ID3D::Query> Query_CreateEvent(ObjectFactory& factory);
        bool    Query_IsEventTriggered(ID3D::DeviceContext* context, ID3D::Query* query);
        void    Query_End(ID3D::DeviceContext* context, ID3D::Query* query);
    #endif

    static const GPUEventStack::EventID EventID_Temporary    = ~GPUEventStack::EventID(0x1);
    static const GPUEventStack::EventID EventID_Unallocated  = ~GPUEventStack::EventID(0x0);

    void  GPUEventStack::TriggerEvent(RenderCore::Metal::DeviceContext* context, EventID event)
    {
#if GFXAPI_ACTIVE == GFXAPI_DX11
            //
            //      Look for a query in the query stack that isn't being used...
            //      this will become our.
            //      Must be done in the render thread! (event queries only work on immediate context)
            //
        for (std::vector<Query>::iterator i=_queries.begin(); i!=_queries.end(); ++i) {
            const EventID previousValue = Interlocked::CompareExchange(&i->_assignedID, EventID_Temporary, EventID_Unallocated);
            if (previousValue == EventID_Unallocated) {
                QueryID thisQueryID = Interlocked::Increment(&_nextQueryID);

                    //
                    //      Trigger the event... But make sure we do it in the correct order... use the 
                    //      _nextIDToSchedule variable to make sure it happens correctly, even when multiple 
                    //      threads are scheduling events at the same time!
                    //
                    //      But -- note that this ordering only really works correctly if 
                    //
                QueryID nextToSchedule = thisQueryID+1;
                for (;;) {
                    const QueryID scheduleResult = Interlocked::CompareExchange(&_nextQueryIDToSchedule, nextToSchedule, thisQueryID);
                    if (scheduleResult == thisQueryID) {
                        if (!i->_query) { i->_query = Query_CreateEvent(_objFactory); }
                        if (i->_query) { Query_End(context->GetUnderlying(), i->_query.get()); }
                        break;
                    }
                    Threading::Pause();
                }
                i->_eventID = event;
                i->_assignedID = thisQueryID;
                return;
            }
        }
        LogWarning << "Ran out of free query objects in GPUEventStack";
        _lastCompletedID = std::max(_lastCompletedID, event);       // consider it immediately completed
#endif
    }

    void        GPUEventStack::Update(RenderCore::Metal::DeviceContext* context)
    {
#if GFXAPI_ACTIVE == GFXAPI_DX11
            //
            //      Look for completed queries, and update our current ID as they complete (also return the 
            //      query to the pool)
            //      Must be done in the render thread! (event queries only work on immediate context)
            //
        for (std::vector<Query>::iterator i=_queries.begin(); i!=_queries.end(); ++i) {
            if (i->_assignedID >= 0) {
                if (!i->_query || Query_IsEventTriggered(context->GetUnderlying(), i->_query.get())) {
                    _lastCompletedID = std::max(_lastCompletedID, i->_eventID);
                    Interlocked::Exchange(&i->_assignedID, EventID_Unallocated);
                }
            }
        }
#endif
    }

    void        GPUEventStack::OnLostDevice()
    {
#if GFXAPI_ACTIVE != GFXAPI_OPENGLES
            // On device lost, we must consider all queries triggered, and then un-allocate them
        for (std::vector<Query>::iterator i=_queries.begin(); i!=_queries.end(); ++i) {
            i->_query.reset();
            if (i->_assignedID >= 0) {
                _lastCompletedID = std::max(_lastCompletedID, i->_eventID);
                Interlocked::Exchange(&i->_assignedID, EventID_Unallocated);
            }
        }
#endif
    }

    void        GPUEventStack::OnDeviceReset()
    {
            // On device lost, we must consider all queries triggered, and then un-allocate them
        for (std::vector<Query>::iterator i=_queries.begin(); i!=_queries.end(); ++i) {
            *i = Query();
        }
    }

    GPUEventStack::GPUEventStack(RenderCore::IDevice* device) : _objFactory(device)
    {
            //
            //      What we really want is an "event" query that holds an integer value (like the events on the 
            //      PS3 hardware). This would allow us to track the GPU progress without needing multiple event
            //      objects.
            //
            //      But D3D only has the boolean events; so we need to use a separate event object for each marker
            //      we want to record...
            //
        const unsigned queryStackDepth = 32;
        _queries.resize(queryStackDepth);
        _nextQueryIDToSchedule = _nextQueryID = 1;
        _lastCompletedID = 0;
    }

    GPUEventStack::~GPUEventStack()
    {
    }

    GPUEventStack::Query::Query()
    {
#if GFXAPI_ACTIVE != GFXAPI_OPENGLES
        _query = nullptr;
#endif
        _assignedID = EventID_Unallocated;
    }

    GPUEventStack::Query::~Query()
    {}      // just a place for so the intrusive_ptr destructor code doesn't get compiled into multiple source files



    ////////////////////////////////////////////////////////////////////////////////

    static const char* AsString(TextureDesc::Dimensionality::Enum dimensionality)
    {
        switch (dimensionality) {
        case TextureDesc::Dimensionality::CubeMap:  return "Cube";
        case TextureDesc::Dimensionality::T1D:      return "T1D";
        case TextureDesc::Dimensionality::T2D:      return "T2D";
        case TextureDesc::Dimensionality::T3D:      return "T3D";
        default:                                    return "<<unknown>>";
        }
    }

    static std::string BuildDescription(const BufferDesc& desc)
    {
        using namespace BufferUploads;
        char buffer[2048];
        if (desc._type == BufferDesc::Type::Texture) {
            const TextureDesc& tDesc = desc._textureDesc;
            xl_snprintf(buffer, dimof(buffer), "[%s] Tex(%4s) (%4ix%4i) mips:(%2i)", 
                desc._name, (tDesc._dimensionality==TextureDesc::Dimensionality::T2D)?"  2D":"Cube",
                tDesc._width, tDesc._height, tDesc._mipCount);
        } else if (desc._type == BufferDesc::Type::LinearBuffer) {
            if (desc._bindFlags & BindFlag::VertexBuffer) {
                xl_snprintf(buffer, dimof(buffer), "[%s] VB (%6.1fkb)", 
                    desc._name, desc._linearBufferDesc._sizeInBytes/1024.f);
            } else if (desc._bindFlags & BindFlag::IndexBuffer) {
                xl_snprintf(buffer, dimof(buffer), "[%s] IB (%6.1fkb)", 
                    desc._name, desc._linearBufferDesc._sizeInBytes/1024.f);
            }
        } else {
            xl_snprintf(buffer, dimof(buffer), "Unknown");
        }
        return std::string(buffer);
    }

//    #if !defined(XL_RELEASE)
//        #define INTRUSIVE_D3D_PROFILING
//    #endif

    #if defined(INTRUSIVE_D3D_PROFILING)
        class ResourceTracker : public IUnknown
        {
        public:
            ResourceTracker(ID3D::Resource* resource, const char name[]);
            virtual ~ResourceTracker();

            ID3D::Resource* GetResource() const { return _resource; }
            const std::string & GetName() const      { return _name; }
            const BufferDesc& GetDesc() const   { return _desc; }

            virtual HRESULT STDMETHODCALLTYPE   QueryInterface(REFIID riid, __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject);
            virtual ULONG STDMETHODCALLTYPE     AddRef();
            virtual ULONG STDMETHODCALLTYPE     Release();
        private:
            Interlocked::Value  _referenceCount;
            ID3D::Resource* _resource;
            std::string _name;
            Interlocked::Value _allocatedMemory[BufferDesc::Type_Max];
            Interlocked::Value _volatileAllocatedMemory[BufferDesc::Type_Max];
            BufferDesc _desc;
        };

        // {7D2F715A-5C04-450A-8C2C-8931136581F9}
        EXTERN_C const GUID DECLSPEC_SELECTANY GUID_ResourceTracker = { 0x7d2f715a, 0x5c04, 0x450a, { 0x8c, 0x2c, 0x89, 0x31, 0x13, 0x65, 0x81, 0xf9 } };

        std::vector<ResourceTracker*>   g_Resources;
        CryCriticalSection              g_Resources_Lock;
        Interlocked::Value              g_AllocatedMemory[BufferDesc::Type_Max]          = { 0, 0, 0 };
        Interlocked::Value              g_VolatileAllocatedMemory[BufferDesc::Type_Max]  = { 0, 0, 0 };

        struct CompareResource
        {
            bool operator()( const ResourceTracker* lhs,    const ID3D::Resource* rhs  ) const  { return lhs->GetResource() < rhs; }
            bool operator()( const ID3D::Resource*  lhs,    const ResourceTracker* rhs ) const  { return lhs < rhs->GetResource(); }
            bool operator()( const ResourceTracker* lhs,    const ResourceTracker* rhs ) const  { return lhs < rhs; }
        };

        static unsigned CalculateVideoMemory(const BufferDesc& desc)
        {
            if (desc._allocationRules != AllocationRules::Staging && desc._gpuAccess) {
                return ByteCount(desc);
            }
            return 0;
        }

        ResourceTracker::ResourceTracker(ID3D::Resource* resource, const char name[]) : _name(name), _resource(resource), _referenceCount(0)
        {
            XlZeroMemory(_allocatedMemory);
            XlZeroMemory(_volatileAllocatedMemory);
            _desc = ExtractDesc(resource);
            if (_desc._allocationRules&AllocationRules::NonVolatile) {
                _allocatedMemory[_desc._type] = CalculateVideoMemory(_desc);
                Interlocked::Add(&g_AllocatedMemory[_desc._type], _allocatedMemory[_desc._type]);
            } else {
                _volatileAllocatedMemory[_desc._type] = CalculateVideoMemory(_desc);
                Interlocked::Add(&g_VolatileAllocatedMemory[_desc._type], _volatileAllocatedMemory[_desc._type]);
            }
        }

        ResourceTracker::~ResourceTracker()
        {
            for (unsigned c=0; c<BufferDesc::Type_Max; ++c) {
                if (_allocatedMemory[c]) {
                    Interlocked::Add(&g_AllocatedMemory[c], -_allocatedMemory[c]);
                }
                if (_volatileAllocatedMemory[c]) {
                    Interlocked::Add(&g_VolatileAllocatedMemory[c], -_volatileAllocatedMemory[c]);
                }
            }
            ScopedLock(g_Resources_Lock);
            std::vector<ResourceTracker*>::iterator i=std::lower_bound(g_Resources.begin(), g_Resources.end(), _resource, CompareResource());
            if (i!=g_Resources.end() && (*i) == this) {
                g_Resources.erase(i);
            }
        }

        HRESULT STDMETHODCALLTYPE ResourceTracker::QueryInterface(REFIID riid, __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject)
        {
            ppvObject = NULL;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE ResourceTracker::AddRef()
        {
            return Interlocked::Increment(&_referenceCount) + 1;
        }

        ULONG STDMETHODCALLTYPE ResourceTracker::Release()
        {
            Interlocked::Value newRefCount = Interlocked::Decrement(&_referenceCount) - 1;
            if (!newRefCount) {
                delete this;
            }
            return newRefCount;
        }

        #if defined(DIRECT3D9)
            void    Resource_Register(IDirect3DResource9* resource, const char name[])
        #else
            void    Resource_Register(ID3D11Resource* resource, const char name[])
        #endif
        {
            ResourceTracker* tracker = new ResourceTracker(resource, name);
            {
                ScopedLock(g_Resources_Lock);
                std::vector<ResourceTracker*>::iterator i=std::lower_bound(g_Resources.begin(), g_Resources.end(), resource, CompareResource());
                if (i!=g_Resources.end() && (*i)->GetResource()==resource) {
                    delete tracker;
                    return;
                }
                g_Resources.insert(i, tracker);
            }
            AttachObject(resource, GUID_ResourceTracker, tracker);
            Resource_SetName(resource, name);
        }

        static void LogString(const char value[])
        {
                //      After a device reset we can't see the log in the console, and the log text file might 
                //      not be updated yet... We need to use the debugger logging connection
            #if defined(WIN32)
                OutputDebugString(value);
            #endif
        }
        
        void    Resource_Report(bool justVolatiles)
        {
            LogString("D3D allocated resources report:\n");
            LogString(XlDynFormatString("Total for non-volatile texture objects: %8.6fMB\n", g_AllocatedMemory         [BufferDesc::Type::Texture     ] / (1024.f*1024.f)).c_str());
            LogString(XlDynFormatString("Total for non-volatile buffer objects : %8.6fMB\n", g_AllocatedMemory         [BufferDesc::Type::LinearBuffer] / (1024.f*1024.f)).c_str());
            LogString(XlDynFormatString("Total for volatile texture objects : %8.6fMB\n",    g_VolatileAllocatedMemory [BufferDesc::Type::Texture     ] / (1024.f*1024.f)).c_str());
            LogString(XlDynFormatString("Total for volatile buffer objects : %8.6fMB\n",     g_VolatileAllocatedMemory [BufferDesc::Type::LinearBuffer] / (1024.f*1024.f)).c_str());

            ScopedLock(g_Resources_Lock);
            for (std::vector<ResourceTracker*>::iterator i=g_Resources.begin(); i!=g_Resources.end(); ++i) {
                std::string name = (*i)->GetName();
                intrusive_ptr<ID3D::Resource> resource = QueryInterfaceCast<ID3D::Resource>((*i)->GetResource());

                const BufferDesc& desc = (*i)->GetDesc();
                if (!justVolatiles || !(desc._allocationRules&AllocationRules::NonVolatile)) {
                    char buffer[2048];
                    strcpy(buffer, BuildDescription(desc).c_str());
                    char nameBuffer[256];
                    Resource_GetName(resource, nameBuffer, dimof(nameBuffer));
                    if (nameBuffer[0]) {
                        strcat(buffer, "  Device name: ");
                        strcat(buffer, nameBuffer);
                    }
                    resource->AddRef();
                    DWORD refCount = resource->Release();
                    sprintf(&buffer[strlen(buffer)], "  Ref count: %i\n", refCount);
                    LogString(buffer);
                }
            }
        }

        static void CalculateExtraFields(BufferMetrics& metrics)
        {
            if (metrics._allocationRules != AllocationRules::Staging && metrics._gpuAccess) {
                metrics._videoMemorySize = ByteCount(metrics);
                metrics._systemMemorySize = 0;
                if (NonVolatileResourcesTakeSystemMemory && (metrics._allocationRules & AllocationRules::NonVolatile)) {
                    metrics._systemMemorySize = metrics._videoMemorySize;
                }
            } else {
                metrics._videoMemorySize = 0;
                metrics._systemMemorySize = ByteCount(metrics);
            }

            if (metrics._type == BufferDesc::Type::Texture) {
                ETEX_Format format = CTexture::TexFormatFromDeviceFormat((NativeFormat::Enum)metrics._textureDesc._nativePixelFormat);
                metrics._pixelFormatName = CTexture::NameForTextureFormat(format);
            } else {
                metrics._pixelFormatName = 0;
            }
        }

        static BufferMetrics ExtractMetrics(const BufferDesc& desc, const std::string& name)
        {
            BufferMetrics result;
            static_cast<BufferDesc&>(result) = desc;
            strncpy(result._name, name.c_str(), dimof(result._name)-1);
            result._name[dimof(result._name)-1] = '\0';
            CalculateExtraFields(result);
            return result;
        }
        
        static BufferMetrics ExtractMetrics(ID3D::Resource* resource)
        {
            BufferMetrics result;
            static_cast<BufferDesc&>(result) = ExtractDesc(resource);
            Resource_GetName(resource, result._name, dimof(result._name));
            CalculateExtraFields(result);
            return result;
        }

        size_t  Resource_GetAll(BufferUploads::BufferMetrics** bufferDescs)
        {
                // DavidJ --    Any D3D9 call with "g_Resources_Lock" locked can cause a deadlock... but I'm hoping AddRef() is fine!
                //              because we AddRef when constructing the smart pointer, we should be ok....
            ScopedLock(g_Resources_Lock);
            size_t count = g_Resources.size();
            BufferUploads::BufferMetrics* result = new BufferUploads::BufferMetrics[count];
            size_t c=0;
            for (std::vector<ResourceTracker*>::const_iterator i=g_Resources.begin(); i!=g_Resources.end(); ++i, ++c) {
                result[c] = ExtractMetrics((*i)->GetDesc(), (*i)->GetName());
            }
            
            (*bufferDescs) = result;
            return count;
        }

        void    Resource_SetName(ID3D::Resource* resource, const char name[])
        {
            if (name && name[0]) {
                #if defined(DIRECT3D9)
                    resource->SetPrivateData(WKPDID_D3DDebugObjectName, name, strlen(name), 0);
                #else
                    resource->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(name)-1, name);
                #endif
            }
        }

        void    Resource_GetName(ID3D::Resource* resource, char nameBuffer[], int nameBufferSize)
        {
            DWORD finalSize = nameBufferSize;
            #if defined(DIRECT3D9)
                HRESULT hresult = resource->GetPrivateData(WKPDID_D3DDebugObjectName, nameBuffer, &finalSize);
            #else
                HRESULT hresult = resource->GetPrivateData(WKPDID_D3DDebugObjectName, (uint32*)&finalSize, nameBuffer);
            #endif
            if (SUCCEEDED(hresult) && finalSize) {
                nameBuffer[std::min(size_t(finalSize),size_t(nameBufferSize-1))] = '\0';
            } else {
                nameBuffer[0] = '\0';
            }
        }

        static size_t   g_lastVideoMemoryHeadroom = 0;
        static bool     g_pendingVideoMemoryHeadroomCalculation = false;

        size_t  Resource_GetVideoMemoryHeadroom()
        {
            return g_lastVideoMemoryHeadroom;
        }

        void        Resource_ScheduleVideoMemoryHeadroomCalculation()
        {
            g_pendingVideoMemoryHeadroomCalculation = true;
        }

        void    Resource_RecalculateVideoMemoryHeadroom()
        {
            if (g_pendingVideoMemoryHeadroomCalculation) {
                    //
                    //      Calculate how much video memory we can allocate by making many
                    //      allocations until they fail.
                    //
                BufferDesc desc;
                desc._type = BufferDesc::Type::Texture;
                desc._bindFlags = BindFlag::ShaderResource;
                desc._cpuAccess = 0;
                desc._gpuAccess = GPUAccess::Read;
                desc._allocationRules = 0;
                desc._textureDesc._width = 1024;
                desc._textureDesc._height = 1024;
                desc._textureDesc._dimensionality = TextureDesc::Dimensionality::T2D;
                desc._textureDesc._nativePixelFormat = CTexture::DeviceFormatFromTexFormat(eTF_A8R8G8B8);
                desc._textureDesc._mipCount = 1;
                desc._textureDesc._arrayCount = 1;
                desc._textureDesc._samples = TextureSamples::Create();

                std::vector<intrusive_ptr<ID3D::Resource> > resources;
                for (;;) {
                    intrusive_ptr<ID3D::Resource> t = CreateResource(desc, NULL);
                    if (!t) {
                        break;
                    }
                    resources.push_back(t);
                }
                g_lastVideoMemoryHeadroom = ByteCount(desc) * resources.size();
                g_pendingVideoMemoryHeadroomCalculation = false;
            }
        }

    #else

        void    Resource_Register(const Underlying::Resource& resource, const char name[])
        {
        }

        void    Resource_Report(bool)
        {
        }

        void    Resource_SetName(const Underlying::Resource& resource, const char name[])
        {
        }

        void    Resource_GetName(const Underlying::Resource& resource, char buffer[], int bufferSize)
        {
        }

        size_t      Resource_GetAll(BufferUploads::BufferMetrics** bufferDescs)
        {
            *bufferDescs = NULL;
            return 0;
        }

        size_t  Resource_GetVideoMemoryHeadroom()
        {
            return 0;
        }

        void    Resource_RecalculateVideoMemoryHeadroom()
        {
        }

        void    Resource_ScheduleVideoMemoryHeadroomCalculation()
        {
        }

    #endif

}}

