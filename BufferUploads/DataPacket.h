// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"
#include "../Utility/MemoryUtils.h"

namespace BufferUploads
{
    class BasicRawDataPacket : public RawDataPacket
    {
    public:
        virtual void* GetData(unsigned mipIndex=0, unsigned arrayIndex=0);
        virtual std::pair<unsigned,unsigned> GetRowAndSlicePitch(unsigned mipIndex=0, unsigned arrayIndex=0) const;
        virtual size_t GetDataSize(unsigned mipIndex=0, unsigned arrayIndex=0) const;

        BasicRawDataPacket(size_t dataSize, const void* data = nullptr, std::pair<unsigned,unsigned> rowAndSlicePitch = std::make_pair(0,0));
        virtual ~BasicRawDataPacket();
    protected:
        std::unique_ptr<uint8, PODAlignedDeletor> _data; 
        size_t _dataSize;
        std::pair<unsigned,unsigned> _rowAndSlicePitch;

        BasicRawDataPacket(const BasicRawDataPacket&);
        BasicRawDataPacket& operator=(const BasicRawDataPacket&);
    };

    buffer_upload_dll_export intrusive_ptr<BasicRawDataPacket> CreateBasicPacket(
        size_t dataSize, const void* data = nullptr, 
        std::pair<unsigned,unsigned> rowAndSlicePitch = std::make_pair(0,0));

    buffer_upload_dll_export intrusive_ptr<BasicRawDataPacket> CreateEmptyPacket(
        const BufferDesc& desc);

    buffer_upload_dll_export intrusive_ptr<RawDataPacket> CreateFileDataSource(
        const void* fileHandle, size_t offset, size_t dataSize);

}