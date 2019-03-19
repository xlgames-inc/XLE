// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"
#include "../RenderCore/Types.h"
#include "../Assets/GenericFuture.h"
#include "../Utility/Threading/ThreadingUtils.h"    // for RefCountedObject
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"                 // for StringSection

namespace BufferUploads
{
    using TexturePitches = RenderCore::TexturePitches;

    class DataPacket : public RefCountedObject
    {
    public:
            //  
            //      Note DataPacket will be initialised with a ref count of 1 
            //      after the constructor.
            //      so:
            //          DataPacket* p = new RawDataPacket_Type();
            //          p->AddRef();
            //          p->Release();
            //          p->Release();
            //
            //      will not destroy the object until the second Release();
            //
        using SubResourceId = RenderCore::SubResourceId;

        virtual void*           GetData         (SubResourceId subRes = {}) = 0;
        virtual size_t          GetDataSize     (SubResourceId subRes = {}) const = 0;
        virtual TexturePitches  GetPitches      (SubResourceId subRes = {}) const = 0;

		class Marker : public ::Assets::GenericFuture
        {
        public:
            BufferDesc  _desc;
        };

        virtual std::shared_ptr<Marker>     BeginBackgroundLoad() = 0;
    };

        /////////////////////////////////////////////////

    class BasicRawDataPacket : public DataPacket
    {
    public:
        virtual void* GetData(SubResourceId subRes = {});
        virtual size_t GetDataSize(SubResourceId subRes = {}) const;
        virtual TexturePitches GetPitches(SubResourceId subRes = {}) const;
        virtual std::shared_ptr<Marker> BeginBackgroundLoad();

        BasicRawDataPacket(size_t dataSize, const void* data = nullptr, TexturePitches pitches = TexturePitches());
        virtual ~BasicRawDataPacket();
    protected:
        std::unique_ptr<uint8, PODAlignedDeletor> _data; 
        size_t _dataSize;
        TexturePitches _pitches;

        BasicRawDataPacket(const BasicRawDataPacket&);
        BasicRawDataPacket& operator=(const BasicRawDataPacket&);
    };

    buffer_upload_dll_export intrusive_ptr<DataPacket> CreateBasicPacket(
        size_t dataSize, const void* data = nullptr, 
        TexturePitches pitches = TexturePitches());

    buffer_upload_dll_export intrusive_ptr<DataPacket> CreateEmptyPacket(
        const BufferDesc& desc);

    buffer_upload_dll_export intrusive_ptr<DataPacket> CreateFileDataSource(
        const void* fileHandle, size_t offset, size_t dataSize,
        TexturePitches pitches);

    namespace TextureLoadFlags { 
        enum Enum { GenerateMipmaps = 1<<0 };
        typedef unsigned BitField;
    }
    buffer_upload_dll_export intrusive_ptr<DataPacket> CreateStreamingTextureSource(
        StringSection<::Assets::ResChar> filename, TextureLoadFlags::BitField flags = 0);

    buffer_upload_dll_export TextureDesc LoadTextureFormat(StringSection<::Assets::ResChar> filename);

}

