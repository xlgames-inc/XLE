// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TextureTileSet.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../RenderCore/Metal/Format.h"

namespace SceneEngine
{
    using namespace RenderCore;

    TextureTile::TextureTile()
    {
        _transaction = ~BufferUploads::TransactionID(0x0);
        _x = _y = _arrayIndex = ~unsigned(0x0);
        _width = _height = ~unsigned(0x0);
        _uploadId = ~unsigned(0x0);
    }

    TextureTile::~TextureTile()
    {
            // make sure our transactions have been cancelled,
            //  if you get this assert, it means we haven't called BufferUploads::IManager::Transaction_End
            //  on this transaction before this destructor.
        assert(_transaction == ~BufferUploads::TransactionID(0x0));
    }

    TextureTile::TextureTile(TextureTile&& moveFrom)
    {
        _transaction = std::move(moveFrom._transaction);
        moveFrom._transaction = ~BufferUploads::TransactionID(0x0);
        _x = std::move(moveFrom._x);
        _y = std::move(moveFrom._y);
        _arrayIndex = std::move(moveFrom._arrayIndex);
        _width = std::move(moveFrom._width);
        _height = std::move(moveFrom._height);
        _uploadId = std::move(moveFrom._uploadId);
        moveFrom._transaction = ~BufferUploads::TransactionID(0x0);
        moveFrom._x = moveFrom._y = moveFrom._arrayIndex = ~unsigned(0x0);
        moveFrom._width = moveFrom._height = ~unsigned(0x0);
        moveFrom._uploadId = ~unsigned(0x0);
    }

    TextureTile& TextureTile::operator=(TextureTile&& moveFrom)
    {
            //  can't reassign while a transaction is going, because
            //  we don't have a pointer to buffer uploads to end the
            //  previous transaction
        assert(_transaction == ~BufferUploads::TransactionID(0x0));
        TextureTile(moveFrom).swap(*this);
        return *this;
    }

    void TextureTile::swap(TextureTile& other)
    {
        std::swap(_transaction, other._transaction);
        std::swap(_x, other._x);
        std::swap(_y, other._y);
        std::swap(_arrayIndex, other._arrayIndex);
        std::swap(_width, other._width);
        std::swap(_height, other._height);
        std::swap(_uploadId, other._uploadId);
    }

    //////////////////////////////////////////////////////////////////////////////////////////


    static Int3 LinearToCoords(unsigned linearAddress, Int2 elesPerSlice)
    {
        unsigned slice = linearAddress / (elesPerSlice[0]*elesPerSlice[1]);
        unsigned t = (linearAddress - slice * (elesPerSlice[0]*elesPerSlice[1]));
        return Int3(t % elesPerSlice[0], t / elesPerSlice[1], slice);
    }

    static unsigned CoordsToLinear(Int3 coords, Int2 elesPerSlice)
    {
        return coords[0] + coords[1] * elesPerSlice[0] + coords[2] * (elesPerSlice[0] * elesPerSlice[1]);
    }

    void    TextureTileSet::Transaction_Begin(
                TextureTile& tile,
                const void* fileHandle, size_t offset, size_t dataSize)
    {
        CompleteCreation();
        if (!_resource || _resource->IsEmpty()) {
            return; // cannot begin transactions until the resource is allocated
        }

            //  Begin a streaming operation, loading from the provided file ptr
            //  This is useful if we have a single file with many texture within.
            //  Often we want to keep that file open, and stream in textures from
            //  it as required.
            //
            //  First, find an avoid element in our tile array. 
            //      Note that if we don't find something available, we need to
            //      evict a texture... That adds complexity, because we need to
            //      implement a LRU scheme, or some other scheme to pick the texture
            //      to evict.
            //
            //  Start a buffer uploads transaction to first load from the file
            //  into a staging texture, and then copy from there into the main
            //  tile set.
        Int3 address;
        bool foundFreeSpace = false;
        for (auto i=_slices.begin(); i!=_slices.end(); ++i) {
            if (i->_unallocatedCount) {
                auto heapIndex = i->_allocationFlags.Allocate();
                address[0] = heapIndex % _elementsPerArraySlice[0];
                address[1] = heapIndex / _elementsPerArraySlice[0];
                address[2] = (int)std::distance(_slices.begin(), i);
                --i->_unallocatedCount;
                foundFreeSpace = true;
                break;
            }
        }
        
        if (!foundFreeSpace) {
                //  look for the oldest block... We keep a queue of old blocks
                //  ... todo; we need to lock tiles that are pending, or have 
                //  been previously used this frame.
            auto oldestBlock = _lruQueue.GetOldestValue();
            if (oldestBlock == ~unsigned(0x0)) {
                assert(0);
                return; // cannot complete the upload
            }

            address = LinearToCoords(oldestBlock, _elementsPerArraySlice);
        }

        unsigned linearId = CoordsToLinear(address, _elementsPerArraySlice);
        _lruQueue.BringToFront(linearId);
        unsigned uploadId = ++_uploadIds[linearId];

            //  begin a transaction, and pass the id back to the client
            //  buffer uploads should read from the file at the given
            //  address, and upload the data into the given location
        if (tile._transaction == ~BufferUploads::TransactionID(0x0)) {
                //  if there's already a transaction on this tile, we might
                //  be chasing the same upload again
            tile._transaction = _bufferUploads->Transaction_Begin(_resource);
        }
        assert(tile._transaction != ~BufferUploads::TransactionID(0x0));

        BufferUploads::Box2D destinationBox;
        destinationBox._left    = address[0] * _elementSize[0];
        destinationBox._top     = address[1] * _elementSize[1];
        destinationBox._right   = (address[0] + 1) * _elementSize[0];
        destinationBox._bottom  = (address[1] + 1) * _elementSize[1];

        tile._x = address[0] * _elementSize[0];
        tile._y = address[1] * _elementSize[1];
        tile._arrayIndex = address[2];
        tile._width = _elementSize[0];
        tile._height = _elementSize[1];
        tile._uploadId = uploadId;
        assert(tile._width != ~unsigned(0x0) && tile._height != ~unsigned(0x0));

            // rowPitch calculation here won't work correctly for DXT compressed formats
        assert(Metal::GetCompressionType(_format) == Metal::FormatCompressionType::None);
        const unsigned rowPitch = Metal::BitsPerPixel(_format) * _elementSize[0] / 8;
        const unsigned slicePitch = rowPitch * _elementSize[1];

        auto dataPacket = BufferUploads::CreateFileDataSource(
            fileHandle, offset, dataSize, BufferUploads::TexturePitches(rowPitch, slicePitch));
        _bufferUploads->UpdateData(
            tile._transaction, dataPacket.get(),
            BufferUploads::PartialResource(destinationBox, 0, 0, address[2], address[2]));
    }

    bool    TextureTileSet::IsValid(const TextureTile& tile) const
    {
        if (tile._width == ~unsigned(0x0) || tile._height == ~unsigned(0x0))
            return false;
        
            //  check to see if this tile has been invalidated in an overwrite 
            //  operation.
        unsigned linearAddress = CoordsToLinear(Int3(   tile._x / _elementSize[0], 
                                                        tile._y / _elementSize[1], 
                                                        tile._arrayIndex), _elementsPerArraySlice);
        if (_uploadIds[linearAddress] != tile._uploadId)
            return false;   // another tile has been uploaded into this same spot

        _lruQueue.BringToFront(linearAddress);
        return true;
    }

    void    TextureTileSet::CompleteCreation()
    {
        if (_creationTransaction != ~BufferUploads::TransactionID(0x0)) {
            if (_bufferUploads->IsCompleted(_creationTransaction)) {
                auto uploadsResource = _bufferUploads->GetResource(_creationTransaction);
                if (uploadsResource && !uploadsResource->IsEmpty()) {
                    Metal::ShaderResourceView shaderResource(uploadsResource->GetUnderlying());
                    Metal::UnorderedAccessView uav;
                    if (_allowModification) {
                        uav = Metal::UnorderedAccessView(uploadsResource->GetUnderlying());
                    }
                    _bufferUploads->Transaction_End(_creationTransaction);
                    _creationTransaction = ~BufferUploads::TransactionID(0x0);

                    _resource = std::move(uploadsResource);
                    _shaderResource = std::move(shaderResource);
                    _uav = std::move(uav);
                }
            }
        }
    }

    TextureTileSet::TextureTileSet( BufferUploads::IManager& bufferUploads,
                                    Int2 elementSize, unsigned elementCount,
                                    RenderCore::Metal::NativeFormat::Enum format,
                                    bool allowModification)
    {
        _creationTransaction = ~BufferUploads::TransactionID(0x0);
        _allowModification = allowModification;

            //  keep pages of around 1024x1024... just add enough array elements
            //  to have as many elements as requested. Normally powers-of-two
            //  are best for element size and element count.
        const unsigned defaultPageSize = 1024;
        const int elementsH = defaultPageSize / elementSize[0];
        const int elementsV = defaultPageSize / elementSize[1];
        const int elementsPerPage = elementsH * elementsV;
        const int pageCount = (elementCount+elementsPerPage-1) / elementsPerPage;
        
            // fill in the slices (everything unallocated initially //
        std::vector<ArraySlice> slices;
        slices.reserve(pageCount);
        for (int c=0; c<pageCount; ++c) {
            slices.push_back(ArraySlice(elementsPerPage));
        }

            // note -- there's currently a problem if the page count is 1
            // The system will create a non-array texture, which will cause
            // problems when attempting to bind this to shaders that are
            // expecting an array texture.
        assert(pageCount > 1);

        using namespace BufferUploads;
        BufferDesc desc;
        desc._type = BufferDesc::Type::Texture;
        desc._bindFlags = BindFlag::ShaderResource;
        desc._cpuAccess = 0;
        desc._gpuAccess = GPUAccess::Read;
        desc._allocationRules = 0;
        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            defaultPageSize, defaultPageSize, format, 1, uint8(pageCount));
        XlCopyString(desc._name, "TextureTileSet");

        if (allowModification) {
            desc._bindFlags |= BindFlag::UnorderedAccess;
            desc._gpuAccess |= GPUAccess::Write;
        }

        intrusive_ptr<BufferUploads::ResourceLocator> resource;
        Metal::ShaderResourceView shaderResource;
        Metal::UnorderedAccessView uav;
        
        #pragma warning(disable:4127)       // warning C4127: conditional expression is constant
        const bool immediateCreate = false;
        if (immediateCreate) {
            resource            = bufferUploads.Transaction_Immediate(desc);
            shaderResource      = Metal::ShaderResourceView(resource->GetUnderlying());
            uav                 = Metal::UnorderedAccessView(resource->GetUnderlying());
        } else {
            _creationTransaction = bufferUploads.Transaction_Begin(desc, (DataPacket*)nullptr, TransactionOptions::ForceCreate);
        }

        LRUQueue lruQueue(elementsPerPage * pageCount);
        std::vector<unsigned> uploadIds;
        uploadIds.resize(elementsPerPage * pageCount, ~unsigned(0x0));

        _bufferUploads = &bufferUploads;
        _resource = std::move(resource);
        _slices = std::move(slices);
        _elementsPerArraySlice = Int2(elementsH, elementsV);
        _elementSize = elementSize;
        _shaderResource = std::move(shaderResource);
        _uav = std::move(uav);
        _uploadIds = std::move(uploadIds);
        _lruQueue = std::move(lruQueue);
        _format = format;
    }

    TextureTileSet::~TextureTileSet() {}

    TextureTileSet::ArraySlice::ArraySlice(int count)
    {
        _allocationFlags.Reserve(count);
        _unallocatedCount = count;
    }

    TextureTileSet::ArraySlice::ArraySlice(ArraySlice&& moveFrom)
    {
        _allocationFlags = std::move(moveFrom._allocationFlags);
        _unallocatedCount = std::move(moveFrom._unallocatedCount);
        moveFrom._unallocatedCount = 0;
    }

    auto TextureTileSet::ArraySlice:: operator=(ArraySlice&& moveFrom) -> ArraySlice&
    {
        _allocationFlags = std::move(moveFrom._allocationFlags);
        _unallocatedCount = std::move(moveFrom._unallocatedCount);
        moveFrom._unallocatedCount = 0;
        return *this;
    }
}

