// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Core/Prefix.h"
#include "../../RenderCore/Metal/Metal.h"

#if GFXAPI_ACTIVE == GFXAPI_DX11

    #include "../PlatformInterface.h"
    #include "../../RenderCore/DX11/IDeviceDX11.h"
    #include "../../RenderCore/DX11/Metal/DX11Utils.h"
    #include "../../RenderCore/DX11/Metal/IncludeDX11.h"
    #include "../../RenderCore/Metal/Format.h"
    #include "../../RenderCore/RenderUtils.h"
    #include "../../Utility/HeapUtils.h"
    #include <functional>

    namespace BufferUploads { namespace PlatformInterface
    {
        static bool IsDXTCompressed(unsigned format) { return GetCompressionType(NativeFormat::Enum(format)) == FormatCompressionType::BlockCompression; }
        static Underlying::Resource*        ResPtr(const Underlying::Resource& resource) { return const_cast<Underlying::Resource*>(&resource); }

        void UnderlyingDeviceContext::PushToResource(   const Underlying::Resource& resource, const BufferDesc& desc, 
                                                        unsigned resourceOffsetValue, const void* data, size_t dataSize,
                                                        std::pair<unsigned,unsigned> rowAndSlicePitch, 
                                                        const Box2D& box, unsigned lodLevel, unsigned arrayIndex)
        {
            switch (desc._type) {
            case BufferDesc::Type::Texture:
                {
                    const bool isFullUpdate = box == Box2D();

                    #if defined(XL_DEBUG)
                        if (isFullUpdate) {
                            intrusive_ptr<ID3D::Texture2D> texture = QueryInterfaceCast<ID3D::Texture2D>(ResPtr(resource));
                            if (texture) {
                                D3D11_TEXTURE2D_DESC desc;
                                texture->GetDesc(&desc);
                                assert(desc.Usage == D3D11_USAGE_DEFAULT);
                                assert(((desc.Height-1) * rowAndSlicePitch.first + desc.Width) <= dataSize);
                            }
                        }
                    #endif

                    uint32 subResource = D3D11CalcSubresource(lodLevel, arrayIndex, desc._textureDesc._mipCount);
                    if (isFullUpdate) {

                        _devContext->GetUnderlying()->UpdateSubresource(ResPtr(resource), subResource, NULL, data, rowAndSlicePitch.first, rowAndSlicePitch.second);

                    } else {

                        D3D11_BOX d3dBox = {box._left, box._top, 0, box._right, box._bottom, 1};
                        const void* pAdjustedSrcData = data;

                        #if DX_VERSION >= DX_VERSION_11_1
                                //  Attempt to use "ID3D11DeviceContext1", if we can get it. This version solves
                                //  a bug in the earlier version of D3D11
                            ID3D11DeviceContext1 * devContext1Temp = nullptr;
                            auto hresult = _devContext->GetUnderlying()->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&devContext1Temp);
                            intrusive_ptr<ID3D11DeviceContext1> devContext1(devContext1Temp, false);
                            if (SUCCEEDED(hresult) && devContext1) {

                                assert(!IsBadReadPtr(data, rowAndSlicePitch.second));
                                unsigned copyFlags = 0; // (can be no_overwrite / discard on Win8)
                                devContext1->UpdateSubresource1(ResPtr(resource), subResource, &d3dBox, data, rowAndSlicePitch.first, rowAndSlicePitch.second, copyFlags);
                            
                            } else 
                        #endif
                        {

                                //
                                //   note --    See documentation in ID3D11DeviceContext::UpdateSubresource for a bug when 
                                //              calling this in a deferred context with non-NULL D3D11_BOX
                                //              Here is the work around...
                                //                  (note we can also use ID3D11DeviceContext1::UpdateSubresource1 which
                                //                  has a fix for this)
                                //
                            if (_useUpdateSubresourceWorkaround) {

                                 D3D11_BOX alignedBox = d3dBox;
                                 unsigned srcBitsPerElement = BitsPerPixel(NativeFormat::Enum(desc._textureDesc._nativePixelFormat));

                                     // convert from pixels to blocks
                                 if (IsDXTCompressed(DXGI_FORMAT(desc._textureDesc._nativePixelFormat))) {
                                      alignedBox.left     /= 4;
                                      alignedBox.right    /= 4;
                                      alignedBox.top      /= 4;
                                      alignedBox.bottom   /= 4;
                                      srcBitsPerElement   *= 16;
                                 }
                             
                                 pAdjustedSrcData = ((const BYTE*)data) - (alignedBox.front * rowAndSlicePitch.second) - (alignedBox.top * rowAndSlicePitch.first) - (alignedBox.left * (srcBitsPerElement/8));
                            }

                            TextureDesc2D destinationDesc(ResPtr(resource));

                            // {
                            //     char buffer[4196];
                            //     _snprintf_s(buffer, _TRUNCATE, "Do UpdateSubresource: {%i,%i,%i,%i,%i,%i} 0x%x08 %0x08x (%i,%i)\n",
                            //         d3dBox.left, d3dBox.top, d3dBox.front, d3dBox.right, d3dBox.bottom, d3dBox.back,
                            //         pAdjustedSrcData, data, rowAndSlicePitch.first, rowAndSlicePitch.second);
                            //     OutputDebugString(buffer);
                            // }

                            assert(pAdjustedSrcData != nullptr);
                            assert(!IsBadReadPtr(data, rowAndSlicePitch.second));
                            _devContext->GetUnderlying()->UpdateSubresource(ResPtr(resource), subResource, &d3dBox, pAdjustedSrcData, rowAndSlicePitch.first, rowAndSlicePitch.second);

                        }
                    }
                }
                break;

            case BufferDesc::Type::LinearBuffer:
                {
                    assert(box == Box2D());
                        //
                        //      When CPU access is 0, we can only use UpdateSubresource.
                        //      But when we can't use UpdateSubresource when usage is D3D11_USAGE_DYNAMIC.
                        //  
                    if ((desc._cpuAccess&CPUAccess::WriteDynamic)!=CPUAccess::WriteDynamic) {
                        D3D11_BOX box;
                        box.top = box.front = 0;
                        box.bottom = box.back = 1;
                        box.left = resourceOffsetValue;
                        box.right = (UINT)(resourceOffsetValue+dataSize);
                        if (_useUpdateSubresourceWorkaround) {  // see documentation for ID3D11DeviceContext::UpdateSubresource for the description for this workaround
                            data = (const void*)(size_t(data)-resourceOffsetValue);
                        }
                        #if defined(XL_DEBUG)
                            {
                                intrusive_ptr<ID3D::Buffer> buffer = QueryInterfaceCast<ID3D::Buffer>(ResPtr(resource));
                                if (buffer) {
                                    D3D11_BUFFER_DESC desc;
                                    buffer->GetDesc(&desc);
                                    assert(desc.Usage == D3D11_USAGE_DEFAULT);
                                }
                            }
                        #endif
                        _devContext->GetUnderlying()->UpdateSubresource(ResPtr(resource), 0, &box, data, 0, 0);
                    } else {
                        D3D11_MAPPED_SUBRESOURCE mappedSubresource;
                        const bool canDoNoOverwrite = _renderCoreContext->IsImmediate();
                        assert(canDoNoOverwrite || resourceOffsetValue==0);     //  this code could be dangerous when the resource offset value != 0. Map() will map the entire
                                                                                //  resource. But when using a batched index buffer, we only want to map and write to a small part of the buffer.
                        HRESULT hresult = _devContext->GetUnderlying()->Map(ResPtr(resource), 0, canDoNoOverwrite?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD, 0/*D3D11_MAP_FLAG_DO_NOT_WAIT*/, &mappedSubresource);
                        assert(SUCCEEDED(hresult));
                        if (SUCCEEDED(hresult)) {
                            assert(mappedSubresource.RowPitch >= dataSize);
                            XlCopyMemoryAlign16(PtrAdd(mappedSubresource.pData, resourceOffsetValue), data, dataSize);
                            _devContext->GetUnderlying()->Unmap(ResPtr(resource), 0);
                        }
                    }
                }
                break;
            }
        }

        void UnderlyingDeviceContext::PushToStagingResource(    const Underlying::Resource& resource, const BufferDesc&desc, 
                                                                unsigned resourceOffsetValue, const void* data, size_t dataSize, 
                                                                std::pair<unsigned,unsigned> rowAndSlicePitch, 
                                                                const Box2D& box, unsigned lodLevel, unsigned arrayIndex)
        {
            assert(box == Box2D());
            switch (desc._type) {
            case BufferDesc::Type::Texture:
                {
                    uint32 subResource = D3D11CalcSubresource(lodLevel, arrayIndex, desc._textureDesc._mipCount);
                    D3D11_MAPPED_SUBRESOURCE mappedSubresource;
                    XlZeroMemory(mappedSubresource);
                    HRESULT hresult = _devContext->GetUnderlying()->Map(
                        ResPtr(resource), subResource, 
                        D3D11_MAP_WRITE, 0, &mappedSubresource);
                    assert(SUCCEEDED(hresult));

                    if (SUCCEEDED(hresult) && mappedSubresource.pData) {
                        CopyMipLevel(
                            mappedSubresource.pData, mappedSubresource.DepthPitch, data, dataSize, 
                            CalculateMipMapDesc(desc._textureDesc, lodLevel), mappedSubresource.RowPitch);
                    }
                }
                break;
            }
        }

        void UnderlyingDeviceContext::UpdateFinalResourceFromStaging(const Underlying::Resource& finalResource, const Underlying::Resource& staging, const BufferDesc& destinationDesc, unsigned lodLevelMin, unsigned lodLevelMax, unsigned stagingLODOffset)
        {
            if ((lodLevelMin == ~unsigned(0x0) || lodLevelMax == ~unsigned(0x0)) && destinationDesc._type == BufferDesc::Type::Texture && !stagingLODOffset) {
                _devContext->GetUnderlying()->CopyResource(ResPtr(finalResource), ResPtr(staging));
            } else {
                for (unsigned a=0; a<destinationDesc._textureDesc._arrayCount; ++a) {
                    for (unsigned c=lodLevelMin; c<=lodLevelMax; ++c) {
                        _devContext->GetUnderlying()->CopySubresourceRegion(
                            ResPtr(finalResource), 
                            D3D11CalcSubresource(c, a, destinationDesc._textureDesc._mipCount),
                            0, 0, 0,
                            ResPtr(staging), 
                            D3D11CalcSubresource(c-stagingLODOffset, a, destinationDesc._textureDesc._mipCount),
                            NULL );
                    }
                }
            }
        }

        #pragma warning(disable:4127)       // conditional expression is constant

        void UnderlyingDeviceContext::ResourceCopy_DefragSteps(const Underlying::Resource& destination, const Underlying::Resource& source, const std::vector<DefragStep>& steps)
        {
            if (!UseMapBasedDefrag) {
                    //
                    //      For each adjustment, we perform one CopySubresourceRegion...
                    //
                for (std::vector<DefragStep>::const_iterator i=steps.begin(); i!=steps.end(); ++i) {
                    assert(i->_sourceEnd > i->_sourceStart);
                    D3D11_BOX sourceBox;
                    sourceBox.left   = i->_sourceStart;
                    sourceBox.right  = i->_sourceEnd;
                    sourceBox.top    = sourceBox.front   = 0;
                    sourceBox.bottom = sourceBox.back    = 1;
                    _devContext->GetUnderlying()->CopySubresourceRegion(ResPtr(destination), 0, i->_destination, 0, 0, ResPtr(source), 0, &sourceBox);
                }
            } else {
                MappedBuffer sourceBuffer       = Map(source, MapType::ReadOnly);
                MappedBuffer destinationBuffer  = Map(destination, MapType::Discard);
                if (sourceBuffer.GetData() && destinationBuffer.GetData()) {
                    for (std::vector<DefragStep>::const_iterator i=steps.begin(); i!=steps.end(); ++i) {
                        const void* sourcePtr   = PtrAdd(sourceBuffer.GetData(), i->_sourceStart);
                        void* destinationPtr    = PtrAdd(destinationBuffer.GetData(), i->_destination);
                        XlCopyMemoryAlign16(destinationPtr, sourcePtr, int(i->_sourceEnd-i->_sourceStart));
                    }
                }
            }
        }

        void UnderlyingDeviceContext::ResourceCopy(const Underlying::Resource& destination, const Underlying::Resource& source)
        {
            _devContext->GetUnderlying()->CopyResource(ResPtr(destination), ResPtr(source));
        }

        intrusive_ptr<RenderCore::Metal::CommandList> UnderlyingDeviceContext::ResolveCommandList()
        {
            return _devContext->ResolveCommandList();
        }

        void                        UnderlyingDeviceContext::BeginCommandList()
        {
            _devContext->BeginCommandList();
        }

        UnderlyingDeviceContext::MappedBuffer UnderlyingDeviceContext::Map(const Underlying::Resource& resource, MapType::Enum mapType, unsigned lodLevel, unsigned arrayIndex)
        {
            uint32 subResource = 0;
            intrusive_ptr<ID3D::Texture2D> tex2D = QueryInterfaceCast<ID3D::Texture2D>(ResPtr(resource));
            if (tex2D && arrayIndex > 0) {
                D3D11_TEXTURE2D_DESC desc;
                tex2D->GetDesc(&desc);
                subResource = D3D11CalcSubresource(lodLevel, arrayIndex, desc.MipLevels);
            } else {
                subResource = D3D11CalcSubresource(lodLevel, 0, 0);
            }
            D3D11_MAPPED_SUBRESOURCE result;
            D3D11_MAP platformMap;
            switch (mapType) {
            case MapType::NoOverwrite:  platformMap = D3D11_MAP_WRITE_NO_OVERWRITE; break;
            case MapType::Write:        platformMap = D3D11_MAP_WRITE;              break;
            case MapType::Discard:      platformMap = D3D11_MAP_WRITE_DISCARD;      break;
            case MapType::ReadOnly:     platformMap = D3D11_MAP_READ;               break;
            default:                    platformMap = D3D11_MAP_WRITE;              break;
            }
            HRESULT hresult = _devContext->GetUnderlying()->Map(ResPtr(resource), subResource, platformMap, 0/*D3D11_MAP_FLAG_DO_NOT_WAIT*/, &result);
            if (SUCCEEDED(hresult)) {
                return MappedBuffer(*this, resource, subResource, result.pData, result.RowPitch, result.DepthPitch);
            }
            return MappedBuffer();
        }

        UnderlyingDeviceContext::MappedBuffer UnderlyingDeviceContext::MapPartial(const Underlying::Resource& resource, MapType::Enum mapType, unsigned offset, unsigned size, unsigned lodLevel, unsigned arrayIndex)
        {
            assert(0);  // can't do partial maps in D3D11
            return MappedBuffer();
        }

        void UnderlyingDeviceContext::Unmap(const Underlying::Resource& resource, unsigned subResourceIndex)
        {
            _devContext->GetUnderlying()->Unmap(ResPtr(resource), subResourceIndex);
        }

        UnderlyingDeviceContext::UnderlyingDeviceContext(RenderCore::IThreadContext& renderCoreContext) 
        : _renderCoreContext(&renderCoreContext)
        {
            _devContext = DeviceContext::Get(*_renderCoreContext);
            _useUpdateSubresourceWorkaround = false;

            if (!_devContext->IsImmediate()) {
                ID3D::Device* devicePtr = nullptr;
                _devContext->GetUnderlying()->GetDevice(&devicePtr);
                intrusive_ptr<ID3D::Device> device(devicePtr, false);

                    //
                    //  See D3D documentation for "ID3D11DeviceContext::UpdateSubresource'
                    //  There's a bug in D3D related to using a deferred context when the driver
                    //  doesn't support driver command lists. Let's check if we need to use
                    //  the workaround for this bug...
                    //
                D3D11_FEATURE_DATA_THREADING threadingCaps = { FALSE, FALSE };
                HRESULT hr = device->CheckFeatureSupport(D3D11_FEATURE_THREADING, &threadingCaps, sizeof(threadingCaps));
                _useUpdateSubresourceWorkaround = SUCCEEDED(hr) && !threadingCaps.DriverCommandLists;
            }
        }

        void    Query_End(ID3D::DeviceContext* context, ID3D::Query* query)
        {
            context->End(query);
        }

        bool    Query_IsEventTriggered(ID3D::DeviceContext* context, ID3D::Query* query)
        {
            uint32 queryValue = 0;
            HRESULT result = context->GetData(
                query, &queryValue, sizeof(queryValue), D3D11_ASYNC_GETDATA_DONOTFLUSH);
            return SUCCEEDED(result) && queryValue;
        }

        intrusive_ptr<ID3D::Query> Query_CreateEvent(ObjectFactory& objFactory)
        {
            D3D11_QUERY_DESC queryDesc;
            queryDesc.Query      = D3D11_QUERY_EVENT;
            queryDesc.MiscFlags  = 0;
            return objFactory.CreateQuery(&queryDesc);
        }

        static unsigned AsNativeCPUAccessFlag(CPUAccess::BitField bitField)
        {
            unsigned result = 0;
            if (bitField & CPUAccess::Read) {
                result |= D3D11_CPU_ACCESS_READ;
            } 
                // if cpu access is Write; we want a "D3D11_USAGE_DEFAULT" buffer with 0 write access flags
            if ((bitField & CPUAccess::WriteDynamic)==CPUAccess::WriteDynamic) {
                result |= D3D11_CPU_ACCESS_WRITE;
            }
            return result;
        }

        static CPUAccess::BitField AsGenericCPUAccess(unsigned d3dFlags)
        {
            CPUAccess::BitField result = 0;
            if (d3dFlags & D3D11_CPU_ACCESS_READ) {
                result |= CPUAccess::Read;
            }
            if (d3dFlags & D3D11_CPU_ACCESS_WRITE) {
                result |= CPUAccess::WriteDynamic;
            }
            return result;
        }

        static D3D11_USAGE UsageForDesc(const BufferDesc& desc, bool lateInitialisation)
        {
            if (desc._gpuAccess) {
                if ((desc._cpuAccess & CPUAccess::WriteDynamic)==CPUAccess::WriteDynamic) {   // Any resource with CPU write access must be marked as "Dynamic"! Don't set any CPU access flags for infrequent UpdateSubresource updates
                    return D3D11_USAGE_DYNAMIC;
                } else if ((desc._cpuAccess & CPUAccess::Write)||lateInitialisation) {
                    return D3D11_USAGE_DEFAULT;
                } else if (desc._gpuAccess & GPUAccess::Write) {
                    return D3D11_USAGE_DEFAULT;
                } else {
                    return D3D11_USAGE_IMMUTABLE;
                }
            } else {
                    //  No GPU access means this can only be useful as a staging buffer...
                return D3D11_USAGE_STAGING;
            }
        }

        static unsigned AsNativeBindFlags(BindFlag::BitField flags)
        {
            unsigned result = 0;
            if (flags & BindFlag::VertexBuffer)     { result |= D3D11_BIND_VERTEX_BUFFER; }
            if (flags & BindFlag::IndexBuffer)      { result |= D3D11_BIND_INDEX_BUFFER; }
            if (flags & BindFlag::ShaderResource)   { result |= D3D11_BIND_SHADER_RESOURCE; }
            if (flags & BindFlag::RenderTarget)     { result |= D3D11_BIND_RENDER_TARGET; }
            if (flags & BindFlag::DepthStencil)     { result |= D3D11_BIND_DEPTH_STENCIL; }
            if (flags & BindFlag::UnorderedAccess)  { result |= D3D11_BIND_UNORDERED_ACCESS; }
            if (flags & BindFlag::ConstantBuffer)   { result |= D3D11_BIND_CONSTANT_BUFFER; }
            if (flags & BindFlag::StreamOutput)     { result |= D3D11_BIND_STREAM_OUTPUT; }
            return result;
        }

        static BindFlag::BitField AsGenericBindFlags(unsigned d3dBindFlags)
        {
            BindFlag::BitField result = 0;
            if (d3dBindFlags & D3D11_BIND_VERTEX_BUFFER)    { result |= BindFlag::VertexBuffer; }
            if (d3dBindFlags & D3D11_BIND_INDEX_BUFFER)     { result |= BindFlag::IndexBuffer; }
            if (d3dBindFlags & D3D11_BIND_SHADER_RESOURCE)  { result |= BindFlag::ShaderResource; }
            if (d3dBindFlags & D3D11_BIND_RENDER_TARGET)    { result |= BindFlag::RenderTarget; }
            if (d3dBindFlags & D3D11_BIND_DEPTH_STENCIL)    { result |= BindFlag::DepthStencil; }
            if (d3dBindFlags & D3D11_BIND_UNORDERED_ACCESS) { result |= BindFlag::UnorderedAccess; }
            if (d3dBindFlags & D3D11_BIND_CONSTANT_BUFFER)  { result |= BindFlag::ConstantBuffer; }
            if (d3dBindFlags & D3D11_BIND_STREAM_OUTPUT)    { result |= BindFlag::StreamOutput; }
            return result;
        }

        intrusive_ptr<ID3D::Resource> CreateResource(ObjectFactory& device, const BufferDesc& desc, RawDataPacket* initialisationData)
        {
            D3D11_SUBRESOURCE_DATA subResources[128];
            TRY {
                switch (desc._type) {
                case BufferDesc::Type::Texture:
                    {
                        if (initialisationData) {
                            for (unsigned l=0; l<desc._textureDesc._mipCount; ++l) {
                                for (unsigned a=0; a<std::max(desc._textureDesc._arrayCount,uint8(1)); ++a) {
                                    uint32 subresourceIndex = D3D11CalcSubresource(l, a, desc._textureDesc._mipCount);
                                    subResources[subresourceIndex].pSysMem = initialisationData->GetData(l,a);
                                    std::pair<unsigned,unsigned> rowAndSlicePitch = initialisationData->GetRowAndSlicePitch(l,a);
                                    subResources[subresourceIndex].SysMemPitch = rowAndSlicePitch.first;
                                    subResources[subresourceIndex].SysMemSlicePitch = rowAndSlicePitch.second;
                                }
                            }
                        }

                        if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T1D) {

                            D3D11_TEXTURE1D_DESC textureDesc;
                            XlZeroMemory(textureDesc);
                            textureDesc.Width = desc._textureDesc._width;
                            textureDesc.MipLevels = desc._textureDesc._mipCount;
                            textureDesc.ArraySize = std::max(desc._textureDesc._arrayCount, uint8(1));
                            textureDesc.Format = AsDXGIFormat((NativeFormat::Enum)desc._textureDesc._nativePixelFormat);
                            const bool lateInitialisation = !initialisationData; /// it must be lateInitialisation, because we don't have any initialization data
                            textureDesc.Usage = UsageForDesc(desc, lateInitialisation);
                            textureDesc.BindFlags = AsNativeBindFlags(desc._bindFlags);
                            textureDesc.CPUAccessFlags = AsNativeCPUAccessFlag(desc._cpuAccess);
                            textureDesc.MiscFlags = 0;
                            return device.CreateTexture1D(&textureDesc, initialisationData?subResources:NULL, desc._name);

                        } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T2D) {

                            D3D11_TEXTURE2D_DESC textureDesc;
                            XlZeroMemory(textureDesc);
                            textureDesc.Width = desc._textureDesc._width;
                            textureDesc.Height = desc._textureDesc._height;
                            textureDesc.MipLevels = desc._textureDesc._mipCount;
                            textureDesc.ArraySize = std::max(desc._textureDesc._arrayCount, uint8(1));
                            textureDesc.Format = AsDXGIFormat((NativeFormat::Enum)desc._textureDesc._nativePixelFormat);
                            textureDesc.SampleDesc.Count = std::max(uint8(1),desc._textureDesc._samples._sampleCount);
                            textureDesc.SampleDesc.Quality = desc._textureDesc._samples._samplingQuality;
                            const bool lateInitialisation = !initialisationData; /// it must be lateInitialisation, because we don't have any initialization data
                            textureDesc.Usage = UsageForDesc(desc, lateInitialisation);
                            textureDesc.BindFlags = AsNativeBindFlags(desc._bindFlags);
                            textureDesc.CPUAccessFlags = AsNativeCPUAccessFlag(desc._cpuAccess);
                            textureDesc.MiscFlags = 0;
                            return device.CreateTexture2D(&textureDesc, initialisationData?subResources:NULL, desc._name);

                        } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T3D) {

                            D3D11_TEXTURE3D_DESC textureDesc;
                            XlZeroMemory(textureDesc);
                            textureDesc.Width = desc._textureDesc._width;
                            textureDesc.Height = desc._textureDesc._height;
                            textureDesc.Depth = desc._textureDesc._depth;
                            textureDesc.MipLevels = desc._textureDesc._mipCount;
                            textureDesc.Format = AsDXGIFormat((NativeFormat::Enum)desc._textureDesc._nativePixelFormat);
                            const bool lateInitialisation = !initialisationData; /// it must be lateInitialisation, because we don't have any initialization data
                            textureDesc.Usage = UsageForDesc(desc, lateInitialisation);
                            textureDesc.BindFlags = AsNativeBindFlags(desc._bindFlags);
                            textureDesc.CPUAccessFlags = AsNativeCPUAccessFlag(desc._cpuAccess);
                            textureDesc.MiscFlags = 0;
                            return device.CreateTexture3D(&textureDesc, initialisationData?subResources:NULL, desc._name);

                        } else {
                            assert(0);
                            return nullptr;
                        }
                    }
                    break;

                case BufferDesc::Type::LinearBuffer:
                    {
                        if (initialisationData) {
                            subResources[0].pSysMem = initialisationData->GetData(0,0);
                            subResources[0].SysMemPitch = 
                                subResources[0].SysMemSlicePitch = 
                                    (UINT)initialisationData->GetDataSize(0,0);
                        }

                        D3D11_BUFFER_DESC d3dDesc;
                        XlZeroMemory(d3dDesc);
                        d3dDesc.ByteWidth = desc._linearBufferDesc._sizeInBytes;
                        const bool lateInitialisation = !initialisationData;
                        d3dDesc.Usage = UsageForDesc(desc, lateInitialisation);
                        d3dDesc.BindFlags = AsNativeBindFlags(desc._bindFlags);
                        d3dDesc.CPUAccessFlags = AsNativeCPUAccessFlag(desc._cpuAccess);
                        d3dDesc.MiscFlags = 0;
                        d3dDesc.StructureByteStride = 0;
                        if (desc._bindFlags & BindFlag::StructuredBuffer) {
                            d3dDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                            d3dDesc.StructureByteStride = desc._linearBufferDesc._structureByteSize;
                        }
                        if (desc._bindFlags & BindFlag::DrawIndirectArgs) {
                            d3dDesc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
                        }
                        if (desc._bindFlags & BindFlag::RawViews) {
                            d3dDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
                        }
                        assert(desc._bindFlags!=BindFlag::IndexBuffer || d3dDesc.BindFlags == D3D11_USAGE_DYNAMIC);

                        return device.CreateBuffer(&d3dDesc, initialisationData?subResources:NULL, desc._name);
                    }

                default:
                    assert(0);
                }
            } CATCH (const RenderCore::Exceptions::GenericFailure&) {
                return intrusive_ptr<ID3D::Resource>();
            } CATCH_END

            return intrusive_ptr<ID3D::Resource>();
        }

        void AttachObject(ID3D::Resource* resource, const GUID& guid, IUnknown* attachableObject)
        {
            HRESULT hresult = resource->SetPrivateDataInterface(guid, attachableObject);
            assert(SUCCEEDED(hresult)); (void)hresult;
        }

        BufferDesc AsGenericDesc(const D3D11_BUFFER_DESC& d3dDesc)
        {
            BufferDesc desc;
            desc._type = BufferDesc::Type::LinearBuffer;
            desc._cpuAccess = AsGenericCPUAccess(d3dDesc.CPUAccessFlags);
            if (d3dDesc.Usage == D3D11_USAGE_DEFAULT) {
                desc._cpuAccess |= CPUAccess::Write;
            } else if (d3dDesc.Usage == D3D11_USAGE_DYNAMIC) {
                desc._cpuAccess |= CPUAccess::WriteDynamic;
            }
            desc._gpuAccess = GPUAccess::Read;
            desc._bindFlags = AsGenericBindFlags(d3dDesc.BindFlags);
            desc._linearBufferDesc._sizeInBytes = d3dDesc.ByteWidth;
            desc._name[0] = '\0';
            return desc;
        }

        BufferDesc AsGenericDesc(const D3D11_TEXTURE2D_DESC& d3dDesc)
        {
            BufferDesc desc;
            desc._type = BufferDesc::Type::Texture;
            desc._cpuAccess = AsGenericCPUAccess(d3dDesc.CPUAccessFlags);
            desc._gpuAccess = GPUAccess::Read;
            desc._bindFlags = AsGenericBindFlags(d3dDesc.BindFlags);
            desc._textureDesc._dimensionality = TextureDesc::Dimensionality::T2D;
            desc._textureDesc._width = d3dDesc.Width;
            desc._textureDesc._height = d3dDesc.Height;
            desc._textureDesc._mipCount = uint8(d3dDesc.MipLevels);
            desc._textureDesc._arrayCount = uint8(d3dDesc.ArraySize);
            desc._textureDesc._nativePixelFormat = d3dDesc.Format;
            desc._textureDesc._samples = BufferUploads::TextureSamples::Create();
            desc._name[0] = '\0';
            return desc;
        }

        BufferDesc AsGenericDesc(const D3D11_TEXTURE1D_DESC& d3dDesc)
        {
            BufferDesc desc;
            desc._type = BufferDesc::Type::Texture;
            desc._cpuAccess = AsGenericCPUAccess(d3dDesc.CPUAccessFlags);
            desc._gpuAccess = GPUAccess::Read;
            desc._bindFlags = AsGenericBindFlags(d3dDesc.BindFlags);
            desc._textureDesc._dimensionality = TextureDesc::Dimensionality::T1D;
            desc._textureDesc._width = d3dDesc.Width;
            desc._textureDesc._height = 1;
            desc._textureDesc._mipCount = uint8(d3dDesc.MipLevels);
            desc._textureDesc._arrayCount = uint8(d3dDesc.ArraySize);
            desc._textureDesc._nativePixelFormat = d3dDesc.Format;
            desc._textureDesc._samples = BufferUploads::TextureSamples::Create();
            desc._name[0] = '\0';
            return desc;
        }

        BufferDesc AsGenericDesc(const D3D11_TEXTURE3D_DESC& d3dDesc)
        {
            BufferDesc desc;
            desc._type = BufferDesc::Type::Texture;
            desc._cpuAccess = AsGenericCPUAccess(d3dDesc.CPUAccessFlags);
            desc._gpuAccess = GPUAccess::Read;
            desc._bindFlags = AsGenericBindFlags(d3dDesc.BindFlags);
            desc._textureDesc._dimensionality = TextureDesc::Dimensionality::T3D;
            desc._textureDesc._width = d3dDesc.Width;
            desc._textureDesc._height = d3dDesc.Height;
            desc._textureDesc._mipCount = uint8(d3dDesc.MipLevels);
            desc._textureDesc._arrayCount = 1;
            desc._textureDesc._nativePixelFormat = d3dDesc.Format;
            desc._textureDesc._samples = BufferUploads::TextureSamples::Create();
            desc._name[0] = '\0';
            return desc;
        }

        BufferDesc ExtractDesc(const Underlying::Resource& resource)
        {
            if (intrusive_ptr<ID3D::Buffer> buffer = QueryInterfaceCast<ID3D::Buffer>(ResPtr(resource))) {
                D3D11_BUFFER_DESC d3dDesc;
                buffer->GetDesc(&d3dDesc);
                return AsGenericDesc(d3dDesc);
            } else if (intrusive_ptr<ID3D::Texture1D> texture = QueryInterfaceCast<ID3D::Texture1D>(ResPtr(resource))) {
                D3D11_TEXTURE1D_DESC d3dDesc;
                texture->GetDesc(&d3dDesc);
                return AsGenericDesc(d3dDesc);
            } else if (intrusive_ptr<ID3D::Texture2D> texture = QueryInterfaceCast<ID3D::Texture2D>(ResPtr(resource))) {
                D3D11_TEXTURE2D_DESC d3dDesc;
                texture->GetDesc(&d3dDesc);
                return AsGenericDesc(d3dDesc);
            } else if (intrusive_ptr<ID3D::Texture3D> texture = QueryInterfaceCast<ID3D::Texture3D>(ResPtr(resource))) {
                D3D11_TEXTURE3D_DESC d3dDesc;
                texture->GetDesc(&d3dDesc);
                return AsGenericDesc(d3dDesc);
            }
            BufferDesc desc;
            XlZeroMemory(desc);
            desc._type = BufferDesc::Type::Unknown;
            return desc;
        }

    }}

#endif
