// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DataPacket.h"
#include "PlatformInterface.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/Threading/LockFree.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/StringUtils.h"
#include <queue>
#include <thread>

#include "../Foreign/DirectXTex/DirectXTex/DirectXTex.h"


namespace BufferUploads
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    auto DataPacket::TexSubRes(unsigned mipIndex, unsigned arrayIndex) -> SubResource
    {
        return (mipIndex & 0xffff) | (arrayIndex << 16);
    }

    BasicRawDataPacket::BasicRawDataPacket(size_t dataSize, const void* data, TexturePitches pitches)
    : _dataSize(dataSize), _pitches(pitches)    
    {
        _data.reset((uint8*)XlMemAlign(dataSize, 16));

            // note --  prefer sending aligned data as input! Just makes it
            //          more convenient for copying
        if (data) {
            if ((size_t(data) & 0xf)==0x0 && (_dataSize & 0xf)==0x0) {
                XlCopyMemoryAlign16(_data.get(), data, _dataSize);
            } else {
                XlCopyMemory(_data.get(), data, _dataSize);
            }
        }
    }

    BasicRawDataPacket::~BasicRawDataPacket()
    {}

    void* BasicRawDataPacket::GetData(SubResource subRes)
    {
        assert(subRes == 0);
        return _data.get(); 
    }
    
    size_t BasicRawDataPacket::GetDataSize(SubResource subRes) const
    {
        assert(subRes == 0);
        return _dataSize; 
    }

    TexturePitches BasicRawDataPacket::GetPitches(SubResource subRes) const
    {
        assert(subRes == 0);
        return _pitches; 
    }

    auto BasicRawDataPacket::BeginBackgroundLoad() -> std::shared_ptr<Marker> { return nullptr; }

    intrusive_ptr<DataPacket> CreateBasicPacket(
        size_t dataSize, const void* data, TexturePitches rowAndSlicePitch)
    {
        return make_intrusive<BasicRawDataPacket>(dataSize, data, rowAndSlicePitch);
    }

    static const unsigned BlockCompDim = 4;
    static unsigned RoundBCDim(unsigned input)
    {
        uint32 part = input%BlockCompDim;
        auto result = input + part?(BlockCompDim-part):0;
        assert(!(result%BlockCompDim));
        return result;
    }

    TexturePitches::TexturePitches(const TextureDesc& desc)
    {
        using namespace RenderCore::Metal;
        _slicePitch = PlatformInterface::TextureDataSize(
            desc._width, desc._height, 1, 1, 
            (NativeFormat::Enum)desc._nativePixelFormat);
            
            //  row pitch calculation is a little platform-specific here...
            //  (eg, DX9 and DX11 use different systems)
            //  Perhaps this could be moved into the platform interface layer
        bool isDXT = GetCompressionType(NativeFormat::Enum(desc._nativePixelFormat)) 
            == FormatCompressionType::BlockCompression;
        if (isDXT) {
            _rowPitch = PlatformInterface::TextureDataSize(
                RoundBCDim(desc._width), BlockCompDim, 1, 1, 
                (NativeFormat::Enum)desc._nativePixelFormat);
        } else {
            _rowPitch = PlatformInterface::TextureDataSize(
                desc._width, 1, 1, 1, 
                (NativeFormat::Enum)desc._nativePixelFormat);
        }
    }

    intrusive_ptr<DataPacket> CreateEmptyPacket(const BufferDesc& desc)
    {
            // Create an empty packet of the appropriate size for the given desc
            // Linear buffers are simple, but textures need a little more detail...
        if (desc._type == BufferDesc::Type::LinearBuffer) {
            auto size = PlatformInterface::ByteCount(desc);
            return make_intrusive<BasicRawDataPacket>(size, nullptr, TexturePitches(size, size));
        } else if (desc._type == BufferDesc::Type::Texture) {
                //  currently not supporting textures with multiple mip-maps
                //  or multiple array slices
            assert(desc._textureDesc._mipCount <= 1);
            assert(desc._textureDesc._arrayCount <= 1);

            TexturePitches pitches(desc._textureDesc);
            return make_intrusive<BasicRawDataPacket>(pitches._slicePitch, nullptr, pitches);
        }

        return nullptr;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
            //      S T R E A M I N G   P A C K E T

    class CompletionThreadPool
    {
    public:
        template<class Fn, class... Args>
            void Enqueue(Fn&& fn, Args&&... args);

        CompletionThreadPool(unsigned threadCount);
        ~CompletionThreadPool();

    private:
        std::vector<std::thread> _workerThreads;
        
        Threading::Mutex _pendingsTaskLock;
        typedef std::function<void()> PendingTask;
        LockFree::FixedSizeQueue<PendingTask, 256> _pendingTasks;

        XlHandle _events[2];
        bool _workerQuit;
    };

    template<class Fn, class... Args>
        void CompletionThreadPool::Enqueue(Fn&& fn, Args&&... args)
        {
            _pendingTasks.push_overflow(
                std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));

                // set event should wake one thread -- and that thread should
                // then take over and execute the task
            XlSetEvent(_events[0]);
        }

    CompletionThreadPool::CompletionThreadPool(unsigned threadCount)
    {
            // once event is an "auto-reset" event, which should wake a single thread
            // another event is a "manual-reset" event. This should 
        _events[0] = XlCreateEvent(false);
        _events[1] = XlCreateEvent(true);
        _workerQuit = false;

        for (unsigned i = 0; i<threadCount; ++i)
            _workerThreads.emplace_back(
                [this]
                {
                    while (!this->_workerQuit) {
                        bool gotTask = false;
                        std::function<void()> task;

                        {
                                // note that _pendingTasks is safe for multiple pushing threads,
                                // but not safe for multiple popping threads. So we have to
                                // lock to prevent more than one thread from attempt to pop
                                // from it at the same time.
                            ScopedLock(this->_pendingsTaskLock);

                            std::function<void()>*t = nullptr;
                            if (_pendingTasks.try_front(t)) {
                                task = std::move(*t);
                                _pendingTasks.pop();
                                gotTask = true;
                            }
                        }

                        if (gotTask) {
                                // if we got this far, we can execute the task....
                            TRY
                            {
                                task();
                            } CATCH(const std::exception& e) {
                                LogAlwaysError << "Suppressing exception in thread pool thread: " << e.what();
                            } CATCH(...) {
                                LogAlwaysError << "Suppressing unknown exception in thread pool thread.";
                            } CATCH_END

                                // That that when using completion routines, we want to attempt to
                                // distribute the tasks evenly between threads (so that the completion
                                // routines will also be distributed evenly between threads.). To achieve
                                // this, let's not attempt to search for another task immediately... Instead
                                // when after we complete a task, let's encourage this thread to go back into
                                // a stall (unless all of our threads are saturated)
                            Threading::YieldTimeSlice();
                            continue;
                        }

                            // Wait for the event with the "alertable" flag set true
                            // note -- this is why we can't use std::condition_variable
                            //      (because threads waiting on a condition variable won't
                            //      be woken to execute completion routines)
                        XlWaitForMultipleSyncObjects(
                            2, this->_events,
                            false, XL_INFINITE, true);
                    }
                }
            );
    }

    CompletionThreadPool::~CompletionThreadPool()
    {
        _workerQuit = true;
        XlSetEvent(this->_events[1]);   // should wake all threads
        for (auto&t : _workerThreads) t.join();
    }

    class FileDataSource : public DataPacket
    {
    public:
        virtual void*           GetData         (SubResource subRes);
        virtual size_t          GetDataSize     (SubResource subRes) const;
        virtual TexturePitches  GetPitches      (SubResource subRes) const;

        virtual std::shared_ptr<Marker>     BeginBackgroundLoad();

        FileDataSource(const void* fileHandle, size_t offset, size_t dataSize, TexturePitches pitches);
        virtual ~FileDataSource();

    protected:
        HANDLE      _fileHandle;
        size_t      _dataSize;
        size_t      _offset;

        struct SpecialOverlapped
        {
            OVERLAPPED                      _internal;
            intrusive_ptr<FileDataSource>   _returnPointer;
        };
        SpecialOverlapped _overlappedStatus;
        std::unique_ptr<byte[], PODAlignedDeletor> _pkt;
        std::shared_ptr<Marker> _marker;

        TexturePitches  _pitches;

        static void CALLBACK CompletionRoutine(
            DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,
            LPOVERLAPPED lpOverlapped );
    };

    void* FileDataSource::GetData(SubResource subRes)
    {
        // assert(subRes == 0);
        return _pkt.get();
    }

    size_t FileDataSource::GetDataSize(SubResource subRes) const           { /*assert(subRes == 0);*/ return _dataSize; }
    TexturePitches FileDataSource::GetPitches(SubResource subRes) const    { /*assert(subRes == 0);*/ return _pitches; }

    static CompletionThreadPool& GetThreadPool()
    {
        static CompletionThreadPool s_threadPool(2);
        return s_threadPool;
    }

    void CALLBACK FileDataSource::CompletionRoutine(
        DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,
        LPOVERLAPPED lpOverlapped )
    {
        auto* o = (SpecialOverlapped*)lpOverlapped;
        assert(o && o->_returnPointer && o->_returnPointer->_marker);
        assert(o->_returnPointer->_marker->GetState() == Assets::AssetState::Pending);

            // We don't have to do any extra processing right now. Just mark the asset as ready
            // or invalid, based on the result...
        o->_returnPointer->_marker->SetState(
            (dwErrorCode == ERROR_SUCCESS) ? Assets::AssetState::Ready : Assets::AssetState::Invalid);

            // we can reset the "_returnPointer", which will also decrease the reference
            // count on the FileDataSource object
        o->_returnPointer.reset();
    }

    auto FileDataSource::BeginBackgroundLoad() -> std::shared_ptr < Marker >
    {
        assert(!_marker);
        assert(_fileHandle && _fileHandle != INVALID_HANDLE_VALUE);

        // Queue read operation begin (it will happen asynchronously)...

        _marker = std::make_shared<Marker>();
        
        XlSetMemory(&_overlappedStatus._internal, 0, sizeof(_overlappedStatus._internal));
        _overlappedStatus._internal.Pointer = (void*)_offset;
        _overlappedStatus._returnPointer = this;

        auto* o = &_overlappedStatus;
        GetThreadPool().Enqueue(
            [o]()
            {
                auto* pkt = o->_returnPointer.get();
                assert(pkt);

                    // We allocate the buffer here, to remove malloc costs from the caller thread
                pkt->_pkt.reset((byte*)XlMemAlign(pkt->_dataSize, 16));

                auto result = ReadFileEx(
                    pkt->_fileHandle, pkt->_pkt.get(), (DWORD)pkt->_dataSize, 
                    &pkt->_overlappedStatus._internal, &CompletionRoutine);

                if (!result) {
                    auto lastError = GetLastError();
                    (void)lastError;

                    pkt->_marker->SetState(Assets::AssetState::Invalid);
                    o->_returnPointer.reset();
                }
            });
        
        return _marker;
    }

    FileDataSource::FileDataSource(const void* fileHandle, size_t offset, size_t dataSize, TexturePitches pitches)
    {
        assert(dataSize);
        assert(fileHandle != INVALID_HANDLE_VALUE);

            //  duplicate the file handle so we get our own reference count on this
            //  file object.
        ::DuplicateHandle(
            GetCurrentProcess(), (HANDLE)fileHandle, GetCurrentProcess(),
            &_fileHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);

        _dataSize = dataSize;
        _pitches = pitches;
        _offset = offset;
    }

    FileDataSource::~FileDataSource()
    {
        if (_fileHandle && _fileHandle!=INVALID_HANDLE_VALUE) {
            CloseHandle(_fileHandle);
        }
    }

    intrusive_ptr<DataPacket> CreateFileDataSource(const void* fileHandle, size_t offset, size_t dataSize, TexturePitches pitches)
    {
        return make_intrusive<FileDataSource>(fileHandle, offset, dataSize, pitches);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class StreamingTexture : public DataPacket
    {
    public:
        virtual void*           GetData         (SubResource subRes);
        virtual size_t          GetDataSize     (SubResource subRes) const;
        virtual TexturePitches  GetPitches      (SubResource subRes) const;

        virtual std::shared_ptr<Marker>     BeginBackgroundLoad();

        StreamingTexture(const ::Assets::ResChar filename[]);
        virtual ~StreamingTexture();

    protected:
        wchar_t _filename[MaxPath];
        
        DirectX::ScratchImage _image;
        DirectX::TexMetadata _texMetadata;

        intrusive_ptr<StreamingTexture> _returnPointer;
        std::shared_ptr<Marker> _marker;
    };

    void* StreamingTexture::GetData(SubResource subRes)
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        auto* image = _image.GetImage(mip, arrayIndex, 0);
        if (image) return image->pixels;
        return nullptr;
    }

    size_t StreamingTexture::GetDataSize(SubResource subRes) const
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        auto* image = _image.GetImage(mip, arrayIndex, 0);
        if (image) return image->slicePitch;
        return 0;
    }

    TexturePitches StreamingTexture::GetPitches(SubResource subRes) const
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        auto* image = _image.GetImage(mip, arrayIndex, 0);
        if (image) return TexturePitches(unsigned(image->rowPitch), unsigned(image->slicePitch));
        return TexturePitches();
    }

    static CompletionThreadPool& GetTextureProcessingPool()
    {
        static CompletionThreadPool s_threadPool(4);
        return s_threadPool;
    }

    auto StreamingTexture::BeginBackgroundLoad() -> std::shared_ptr < Marker >
    {
        assert(!_marker && !_returnPointer);

        _marker = std::make_shared<Marker>();
        _returnPointer = this;      // hold a reference while the background operation is occurring

        GetTextureProcessingPool().Enqueue(
            [this]()
            {
                using namespace DirectX;
                HRESULT hresult = -1;
                const auto* filename = this->_filename;
                auto* ext = XlExtension((const ucs2*)filename);
                assert(ext);
                if (ext && !XlCompareStringI(ext, (const ucs2*)L"dds")) {
                    hresult = LoadFromDDSFile(
                        filename, DDS_FLAGS_NONE,
                        &_texMetadata, _image);
                } else if (ext && !XlCompareStringI(ext, (const ucs2*)L"tga")) {
                    hresult = LoadFromTGAFile(
                        filename, &_texMetadata, _image);
                } else {
                    hresult = LoadFromWICFile(
                        filename, WIC_FLAGS_NONE,
                        &_texMetadata, _image);
                }

                if (SUCCEEDED(hresult)) {
                    auto& desc = this->_marker->_desc;
                    desc._type = BufferDesc::Type::Texture;
                    desc._textureDesc._width = uint32(this->_texMetadata.width);
                    desc._textureDesc._height = uint32(this->_texMetadata.height);
                    desc._textureDesc._depth = uint32(this->_texMetadata.depth);
                    desc._textureDesc._arrayCount = uint8(this->_texMetadata.arraySize);
                    desc._textureDesc._mipCount = uint8(this->_texMetadata.mipLevels);
                    desc._textureDesc._samples = TextureSamples();
                    desc._textureDesc._nativePixelFormat = (unsigned)this->_texMetadata.format;
                    switch (this->_texMetadata.dimension) {
                    case TEX_DIMENSION_TEXTURE1D: desc._textureDesc._dimensionality = TextureDesc::Dimensionality::T1D; break;
                    default:
                    case TEX_DIMENSION_TEXTURE2D: desc._textureDesc._dimensionality = TextureDesc::Dimensionality::T2D; break;
                    case TEX_DIMENSION_TEXTURE3D: desc._textureDesc._dimensionality = TextureDesc::Dimensionality::T3D; break;
                    }
                    if (this->_texMetadata.IsCubemap())
                        desc._textureDesc._dimensionality = TextureDesc::Dimensionality::CubeMap;

                    if (this->_texMetadata.mipLevels <= 1) {
                        DirectX::ScratchImage newImage;
                        auto mipmapHresult = GenerateMipMaps(*this->_image.GetImage(0,0,0), (DWORD)TEX_FILTER_DEFAULT, 0, newImage);
                        if (SUCCEEDED(mipmapHresult)) {
                            this->_image = std::move(newImage);
                            desc._textureDesc._mipCount = uint8(this->_image.GetMetadata().mipLevels);
                        } else {
                            LogWarning << "Failed while building mip-maps for texture: " << filename;
                        }
                    }

                    if (SUCCEEDED(hresult)) {
                        this->_marker->SetState(Assets::AssetState::Ready);
                        return;
                    }
                }

                this->_marker->SetState(Assets::AssetState::Invalid);
                this->_returnPointer.reset();
            });
        
        return _marker;
    }

    StreamingTexture::StreamingTexture(const ::Assets::ResChar filename[])
    {
        XlZeroMemory(_texMetadata);
        Conversion::Convert(_filename, dimof(_filename), filename, &filename[XlStringLen(filename)]);
    }

    StreamingTexture::~StreamingTexture()
    {}

    intrusive_ptr<DataPacket> CreateStreamingTextureSource(const ::Assets::ResChar filename[])
    {
        return make_intrusive<StreamingTexture>(filename);
    }

}
