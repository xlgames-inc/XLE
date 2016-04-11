// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Core/Prefix.h"
#include "../../RenderCore/Metal/Metal.h"

#if GFXAPI_ACTIVE == GFXAPI_DX11

    #include "../PlatformInterface.h"
    #include "../DataPacket.h"
    #include "../../RenderCore/DX11/IDeviceDX11.h"
    #include "../../RenderCore/DX11/Metal/DX11Utils.h"
    #include "../../RenderCore/DX11/Metal/IncludeDX11.h"
	#include "../../RenderCore/DX11/Metal/ObjectFactory.h"
	#include "../../RenderCore/DX11/Metal/Format.h"
    #include "../../RenderCore/RenderUtils.h"
	#include "../../RenderCore/Format.h"
    #include "../../Utility/HeapUtils.h"
    #include <functional>

    namespace BufferUploads { namespace PlatformInterface
    {
		using namespace RenderCore;

        static bool IsDXTCompressed(NativeFormat format) { return GetCompressionType(format) == FormatCompressionType::BlockCompression; }
        static ID3D::Resource*        ResPtr(UnderlyingResource& resource) { return Metal::AsID3DResource(&resource); }

        void UnderlyingDeviceContext::PushToResource(   UnderlyingResource& resource, const BufferDesc& desc,
                                                        unsigned resourceOffsetValue, const void* data, size_t dataSize,
                                                        TexturePitches rowAndSlicePitch, 
                                                        const Box2D& box, unsigned lodLevel, unsigned arrayIndex)
        {
			auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
            switch (desc._type) {
            case BufferDesc::Type::Texture:
                {
                    const bool isFullUpdate = box == Box2D();

                    #if defined(XL_DEBUG)
                        if (isFullUpdate) {
                            intrusive_ptr<ID3D::Texture2D> texture = Metal::QueryInterfaceCast<ID3D::Texture2D>(ResPtr(resource));
                            if (texture) {
                                D3D11_TEXTURE2D_DESC desc;
                                texture->GetDesc(&desc);
                                assert(desc.Usage == D3D11_USAGE_DEFAULT);
                            }
                        }
                    #endif

                    uint32 subResource = D3D11CalcSubresource(lodLevel, arrayIndex, desc._textureDesc._mipCount);
                    if (isFullUpdate) {

						metalContext->GetUnderlying()->UpdateSubresource(ResPtr(resource), subResource, NULL, data, rowAndSlicePitch._rowPitch, rowAndSlicePitch._slicePitch);

                    } else {

                        D3D11_BOX d3dBox = {box._left, box._top, 0, box._right, box._bottom, 1};
                        const void* pAdjustedSrcData = data;

                        #if DX_VERSION >= DX_VERSION_11_1
                                //  Attempt to use "ID3D11DeviceContext1", if we can get it. This version solves
                                //  a bug in the earlier version of D3D11
                            ID3D11DeviceContext1 * devContext1Temp = nullptr;
                            auto hresult = metalContext->GetUnderlying()->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&devContext1Temp);
                            intrusive_ptr<ID3D11DeviceContext1> devContext1(devContext1Temp, false);
                            if (SUCCEEDED(hresult) && devContext1) {

                                assert(!IsBadReadPtr(data, rowAndSlicePitch._slicePitch));
                                unsigned copyFlags = 0; // (can be no_overwrite / discard on Win8)
                                devContext1->UpdateSubresource1(ResPtr(resource), subResource, &d3dBox, data, rowAndSlicePitch._rowPitch, rowAndSlicePitch._slicePitch, copyFlags);
                            
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
                                 unsigned srcBitsPerElement = BitsPerPixel(desc._textureDesc._format);

                                     // convert from pixels to blocks
                                 if (IsDXTCompressed(desc._textureDesc._format)) {
                                      alignedBox.left     /= 4;
                                      alignedBox.right    /= 4;
                                      alignedBox.top      /= 4;
                                      alignedBox.bottom   /= 4;
                                      srcBitsPerElement   *= 16;
                                 }
                             
                                 pAdjustedSrcData = ((const BYTE*)data) - (alignedBox.front * rowAndSlicePitch._slicePitch) - (alignedBox.top * rowAndSlicePitch._rowPitch) - (alignedBox.left * (srcBitsPerElement/8));
                            }

                            Metal::TextureDesc2D destinationDesc(ResPtr(resource));

                            // {
                            //     char buffer[4196];
                            //     _snprintf_s(buffer, _TRUNCATE, "Do UpdateSubresource: {%i,%i,%i,%i,%i,%i} 0x%x08 %0x08x (%i,%i)\n",
                            //         d3dBox.left, d3dBox.top, d3dBox.front, d3dBox.right, d3dBox.bottom, d3dBox.back,
                            //         pAdjustedSrcData, data, rowAndSlicePitch.first, rowAndSlicePitch.second);
                            //     OutputDebugString(buffer);
                            // }

                            assert(pAdjustedSrcData != nullptr);
                            assert(!IsBadReadPtr(data, rowAndSlicePitch._slicePitch));
							metalContext->GetUnderlying()->UpdateSubresource(ResPtr(resource), subResource, &d3dBox, pAdjustedSrcData, rowAndSlicePitch._rowPitch, rowAndSlicePitch._slicePitch);

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
                                intrusive_ptr<ID3D::Buffer> buffer = Metal::QueryInterfaceCast<ID3D::Buffer>(ResPtr(resource));
                                if (buffer) {
                                    D3D11_BUFFER_DESC desc;
                                    buffer->GetDesc(&desc);
                                    assert(desc.Usage == D3D11_USAGE_DEFAULT);
                                }
                            }
                        #endif
							metalContext->GetUnderlying()->UpdateSubresource(ResPtr(resource), 0, &box, data, 0, 0);
                    } else {
                        D3D11_MAPPED_SUBRESOURCE mappedSubresource;
                        const bool canDoNoOverwrite = _renderCoreContext->IsImmediate();
                        assert(canDoNoOverwrite || resourceOffsetValue==0);     //  this code could be dangerous when the resource offset value != 0. Map() will map the entire
                                                                                //  resource. But when using a batched index buffer, we only want to map and write to a small part of the buffer.
                        HRESULT hresult = metalContext->GetUnderlying()->Map(ResPtr(resource), 0, canDoNoOverwrite?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD, 0/*D3D11_MAP_FLAG_DO_NOT_WAIT*/, &mappedSubresource);
                        assert(SUCCEEDED(hresult));
                        if (SUCCEEDED(hresult)) {
                            assert(mappedSubresource.RowPitch >= dataSize);
                            XlCopyMemoryAlign16(PtrAdd(mappedSubresource.pData, resourceOffsetValue), data, dataSize);
							metalContext->GetUnderlying()->Unmap(ResPtr(resource), 0);
                        }
                    }
                }
                break;
            }
        }

        void UnderlyingDeviceContext::PushToStagingResource(
            UnderlyingResource& resource, const BufferDesc&desc, 
            const ResourceInitializer& data)
        {
            // In D3D, we must map each subresource separately
            switch (desc._type) {
            case BufferDesc::Type::Texture:
                {
                    auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
                    for (unsigned m=0; m<desc._textureDesc._mipLevels; ++m)
                        for (unsigned a=0; a<desc._textureDesc._arrayCount; ++a) {
                            auto subResData = data(m, a);
                            if (!subResData._data || !subResData._size) continue;

                            uint32 subResource = D3D11CalcSubresource(lodLevel, arrayIndex, desc._textureDesc._mipCount);
                            D3D11_MAPPED_SUBRESOURCE mappedSubresource;
                            XlZeroMemory(mappedSubresource);
                            HRESULT hresult = metalContext->GetUnderlying()->Map(
                                ResPtr(resource), subResource, 
                                D3D11_MAP_WRITE, 0, &mappedSubresource);
                            assert(SUCCEEDED(hresult));

                            if (SUCCEEDED(hresult) && mappedSubresource.pData) {
                                CopyMipLevel(
                                    mappedSubresource.pData, mappedSubresource.DepthPitch, 
                                    subResData._data, subResData._size, 
                                    CalculateMipMapDesc(desc._textureDesc, lodLevel), mappedSubresource.RowPitch);
                            }
                        }
                }
                break;
            }
        }

        void UnderlyingDeviceContext::UpdateFinalResourceFromStaging(UnderlyingResource& finalResource, UnderlyingResource& staging, const BufferDesc& destinationDesc, unsigned lodLevelMin, unsigned lodLevelMax, unsigned stagingLODOffset)
        {
			auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
            using namespace RenderCore;
            if ((lodLevelMin == ~unsigned(0x0) || lodLevelMax == ~unsigned(0x0)) && destinationDesc._type == BufferDesc::Type::Texture && !stagingLODOffset) {
                Metal::Copy(*metalContext, ResPtr(finalResource), ResPtr(staging));
            } else {
                for (unsigned a=0; a<destinationDesc._textureDesc._arrayCount; ++a) {
                    for (unsigned c=lodLevelMin; c<=lodLevelMax; ++c) {
                        Metal::CopyPartial(
                            *metalContext,
                            Metal::CopyPartial_Dest(ResPtr(finalResource), D3D11CalcSubresource(c, a, destinationDesc._textureDesc._mipCount)),
                            Metal::CopyPartial_Src(ResPtr(staging), D3D11CalcSubresource(c-stagingLODOffset, a, destinationDesc._textureDesc._mipCount)));
                    }
                }
            }
        }

        #pragma warning(disable:4127)       // conditional expression is constant

        void UnderlyingDeviceContext::ResourceCopy_DefragSteps(UnderlyingResource& destination, UnderlyingResource& source, const std::vector<DefragStep>& steps)
        {
            if (!UseMapBasedDefrag) {
                    //
                    //      For each adjustment, we perform one CopySubresourceRegion...
                    //
				auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext); 
				for (std::vector<DefragStep>::const_iterator i=steps.begin(); i!=steps.end(); ++i) {
                    assert(i->_sourceEnd > i->_sourceStart);
                    using namespace RenderCore;
                    Metal::CopyPartial(
                        *metalContext,
                        Metal::CopyPartial_Dest(ResPtr(destination), 0, i->_destination),
                        Metal::CopyPartial_Src(ResPtr(source), 0, i->_sourceStart, Metal::PixelCoord(i->_sourceEnd, 1, 1)));
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

        void UnderlyingDeviceContext::ResourceCopy(UnderlyingResource& destination, UnderlyingResource& source)
        {
			auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
            RenderCore::Metal::Copy(*metalContext, ResPtr(destination), ResPtr(source));
        }

        intrusive_ptr<RenderCore::Metal::CommandList> UnderlyingDeviceContext::ResolveCommandList()
        {
			auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
            return metalContext->ResolveCommandList();
        }

        void                        UnderlyingDeviceContext::BeginCommandList()
        {
			auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
			metalContext->BeginCommandList();
        }

        UnderlyingDeviceContext::MappedBuffer UnderlyingDeviceContext::Map(UnderlyingResource& resource, MapType::Enum mapType, unsigned subResource)
        {
            // uint32 subResource = 0;
            // intrusive_ptr<ID3D::Texture2D> tex2D = QueryInterfaceCast<ID3D::Texture2D>(ResPtr(resource));
            // if (tex2D && arrayIndex > 0) {
            //     D3D11_TEXTURE2D_DESC desc;
            //     tex2D->GetDesc(&desc);
            //     subResource = D3D11CalcSubresource(lodLevel, arrayIndex, desc.MipLevels);
            // } else {
            //     subResource = D3D11CalcSubresource(lodLevel, 0, 0);
            // }
            D3D11_MAPPED_SUBRESOURCE result;
            D3D11_MAP platformMap;
            switch (mapType) {
            case MapType::NoOverwrite:  platformMap = D3D11_MAP_WRITE_NO_OVERWRITE; break;
            case MapType::Write:        platformMap = D3D11_MAP_WRITE;              break;
            case MapType::Discard:      platformMap = D3D11_MAP_WRITE_DISCARD;      break;
            case MapType::ReadOnly:     platformMap = D3D11_MAP_READ;               break;
            default:                    platformMap = D3D11_MAP_WRITE;              break;
            }
			auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
            HRESULT hresult = metalContext->GetUnderlying()->Map(ResPtr(resource), subResource, platformMap, 0/*D3D11_MAP_FLAG_DO_NOT_WAIT*/, &result);
            if (SUCCEEDED(hresult)) {
                return MappedBuffer(*this, resource, subResource, result.pData, TexturePitches(result.RowPitch, result.DepthPitch));
            }
            return MappedBuffer();
        }

        UnderlyingDeviceContext::MappedBuffer UnderlyingDeviceContext::MapPartial(UnderlyingResource& resource, MapType::Enum mapType, unsigned offset, unsigned size, unsigned subResource)
        {
            assert(0);  // can't do partial maps in D3D11
            return MappedBuffer();
        }

        void UnderlyingDeviceContext::Unmap(UnderlyingResource& resource, unsigned subResourceIndex)
        {
			auto metalContext = Metal::DeviceContext::Get(*_renderCoreContext);
			metalContext->GetUnderlying()->Unmap(ResPtr(resource), subResourceIndex);
        }

		std::shared_ptr<RenderCore::IDevice> UnderlyingDeviceContext::GetObjectFactory()
		{
			return _renderCoreContext->GetDevice();
		}

        UnderlyingDeviceContext::UnderlyingDeviceContext(RenderCore::IThreadContext& renderCoreContext) 
        : _renderCoreContext(&renderCoreContext)
        {
            _useUpdateSubresourceWorkaround = false;

            if (!renderCoreContext.IsImmediate()) {
				auto dev = renderCoreContext.GetDevice();
				IDeviceDX11* devDX11 = (IDeviceDX11*)dev->QueryInterface(__uuidof(IDeviceDX11));
				if (devDX11) {
						//
						//  See D3D documentation for "ID3D11DeviceContext::UpdateSubresource'
						//  There's a bug in D3D related to using a deferred context when the driver
						//  doesn't support driver command lists. Let's check if we need to use
						//  the workaround for this bug...
						//
					D3D11_FEATURE_DATA_THREADING threadingCaps = { FALSE, FALSE };
					HRESULT hr = devDX11->GetUnderlyingDevice()->CheckFeatureSupport(
						D3D11_FEATURE_THREADING, &threadingCaps, sizeof(threadingCaps));
					_useUpdateSubresourceWorkaround = SUCCEEDED(hr) && !threadingCaps.DriverCommandLists;
				}
            }
        }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class RawDataPacket_ReadBack : public DataPacket
    {
    public:
        void*           GetData(SubResourceId subRes);
        size_t          GetDataSize(SubResourceId subRes) const;
        TexturePitches  GetPitches(SubResourceId subRes) const;

        std::shared_ptr<Marker> BeginBackgroundLoad() { return nullptr; }

        RawDataPacket_ReadBack(
            const ResourceLocator& locator, 
            PlatformInterface::UnderlyingDeviceContext& context);
        ~RawDataPacket_ReadBack();

    protected:
        unsigned _dataOffset;
        std::vector<PlatformInterface::UnderlyingDeviceContext::MappedBuffer> _mappedBuffer;
        unsigned _mipCount;
        unsigned _arrayCount;
    };

    void*     RawDataPacket_ReadBack::GetData(SubResourceId subRes)
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        unsigned subResIndex = mip + arrayIndex * _mipCount;
        assert(subResIndex < _mappedBuffer.size());
        return PtrAdd(_mappedBuffer[subResIndex].GetData(), _dataOffset);
    }

    size_t          RawDataPacket_ReadBack::GetDataSize(SubResourceId subRes) const
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        unsigned subResIndex = mip + arrayIndex * _mipCount;
        assert(subResIndex < _mappedBuffer.size());
        return _mappedBuffer[subResIndex].GetPitches()._slicePitch - _dataOffset;
    }

    TexturePitches RawDataPacket_ReadBack::GetPitches(SubResourceId subRes) const
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        unsigned subResIndex = mip + arrayIndex * _mipCount;
        assert(subResIndex < _mappedBuffer.size());
        return _mappedBuffer[subResIndex].GetPitches();
    }

    RawDataPacket_ReadBack::RawDataPacket_ReadBack(
		const ResourceLocator& locator, 
		PlatformInterface::UnderlyingDeviceContext& context)
    : _dataOffset(0)
    {
        assert(!locator.IsEmpty());
        auto resource = locator.GetUnderlying();
        UnderlyingResourcePtr stagingResource;

        auto desc = PlatformInterface::ExtractDesc(*resource);
        auto subResCount = 1u;
        _mipCount = _arrayCount = 1u;
        if (desc._type == BufferDesc::Type::Texture) {
            subResCount = 
                  std::max(1u, unsigned(desc._textureDesc._mipCount))
                * std::max(1u, unsigned(desc._textureDesc._arrayCount));
            _mipCount = desc._textureDesc._mipCount;
            _arrayCount = desc._textureDesc._arrayCount;
        }

            //
            //      If we have to read back through a staging resource, then let's create
            //      a temporary resource and initialise it...
            //
        using namespace PlatformInterface;
        if (RequiresStagingResourceReadBack && !(desc._cpuAccess & CPUAccess::Read)) {
            BufferDesc stagingDesc = AsStagingDesc(desc);
            stagingResource = CreateResource(*context.GetObjectFactory(), stagingDesc);
            if (stagingResource.get()) {
                context.ResourceCopy(*stagingResource.get(), *resource);
                resource = stagingResource.get();
            }
        }

        _mappedBuffer.reserve(subResCount);
        
        if (CanDoPartialMaps) {
            for (unsigned c=0; c<subResCount; ++c)
                _mappedBuffer.push_back(context.MapPartial(
                    *resource, UnderlyingDeviceContext::MapType::ReadOnly, 
                    locator.Offset(), locator.Size(), c));
        } else {
            for (unsigned c=0; c<subResCount; ++c)
                _mappedBuffer.push_back(
                    context.Map(*resource, UnderlyingDeviceContext::MapType::ReadOnly, c));
            _dataOffset = (locator.Offset() != ~unsigned(0x0))?locator.Offset():0;
        }
    }

    RawDataPacket_ReadBack::~RawDataPacket_ReadBack()
    {
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

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

        intrusive_ptr<ID3D::Query> Query_CreateEvent(Metal::ObjectFactory& objFactory)
        {
            D3D11_QUERY_DESC queryDesc;
            queryDesc.Query      = D3D11_QUERY_EVENT;
            queryDesc.MiscFlags  = 0;
            return objFactory.CreateQuery(&queryDesc);
        }

        ResourcePtr CreateResource(RenderCore::IDevice& device, const BufferDesc& desc, DataPacket* initialisationData)
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
			return Metal::ExtractDesc(&resource);
		}

        void AttachObject(ID3D::Resource* resource, const GUID& guid, IUnknown* attachableObject)
        {
            HRESULT hresult = resource->SetPrivateDataInterface(guid, attachableObject);
            assert(SUCCEEDED(hresult)); (void)hresult;
        }

    }}

#endif
