// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainUberSurface.h"
#include "TerrainScaffold.h"
#include "Terrain.h"
#include "TerrainConfig.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "Noise.h"
#include "Erosion.h"
#include "SurfaceHeightsProvider.h"

#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../BufferUploads/DataPacket.h"

#include "../Utility/Streams/FileUtils.h"
#include "../Utility/PtrUtils.h"

namespace SceneEngine
{
    using namespace RenderCore;

///////////////////////////////////////////////////////////////////////////////////////////////////

    void* TerrainUberSurfaceGeneric::GetData(UInt2 coord)
    {
        assert(_mappedFile && _dataStart);
        if (coord[0] >= _width || coord[1] >= _height) return nullptr;
        auto stride = _width * _sampleBytes;
        return PtrAdd(_dataStart, coord[1] * stride + coord[0] * _sampleBytes);
    }

    unsigned TerrainUberSurfaceGeneric::GetStride() const 
    { 
        return _width * _sampleBytes;
    }

    ImpliedTyping::TypeDesc TerrainUberSurfaceGeneric::Format() const { return _format; }

    TerrainUberSurfaceGeneric::TerrainUberSurfaceGeneric(const ::Assets::ResChar filename[])
    {
        _width = _height = 0;
        _dataStart = nullptr;
        _sampleBytes = 0;

            //  Load the file as a Win32 "mapped file"
            //  the format is very simple.. it's just a basic header, and then
            //  a huge 2D array of height values
        auto mappedFile = std::make_unique<MemoryMappedFile>(filename, 0, MemoryMappedFile::Access::Read|MemoryMappedFile::Access::Write, BasicFile::ShareMode::Read);
        if (!mappedFile->IsValid())
            Throw(::Assets::Exceptions::InvalidAsset(
                filename, "Failed while openning uber surface file"));
        
        auto& hdr = *(TerrainUberHeader*)mappedFile->GetData();
        if (hdr._magic != TerrainUberHeader::Magic)
            Throw(::Assets::Exceptions::InvalidAsset(
                filename, "Uber surface file appears to be corrupt"));

        _width = hdr._width;
        _height = hdr._height;
        _dataStart = PtrAdd(mappedFile->GetData(), sizeof(TerrainUberHeader));
        _format = ImpliedTyping::TypeDesc(
            ImpliedTyping::TypeCat(hdr._typeCat), 
            (uint16)hdr._typeArrayCount);
        _sampleBytes = _format.GetSize();

        if (mappedFile->GetSize() < (sizeof(TerrainUberHeader) + hdr._width * hdr._height * _sampleBytes))
            Throw(::Assets::Exceptions::InvalidAsset(
                filename, "Uber surface file appears to be corrupt (it is smaller than it should be)"));

        _mappedFile = std::move(mappedFile);
    }

    TerrainUberSurfaceGeneric::TerrainUberSurfaceGeneric()
    {
        _width = _height = 0;
        _dataStart = nullptr;
    }

    TerrainUberSurfaceGeneric::~TerrainUberSurfaceGeneric()
    {}

    TerrainUberSurfaceGeneric::TerrainUberSurfaceGeneric(TerrainUberSurfaceGeneric&& moveFrom)
    : _mappedFile(std::move(moveFrom._mappedFile))
    , _dataStart(moveFrom._dataStart)
    , _width(moveFrom._width)
    , _height(moveFrom._height)
    {
        moveFrom._dataStart = nullptr;
        moveFrom._width = moveFrom._height = 0;
    }

    TerrainUberSurfaceGeneric& TerrainUberSurfaceGeneric::operator=(TerrainUberSurfaceGeneric&& moveFrom)
    {
        _mappedFile = std::move(moveFrom._mappedFile);
        _width = moveFrom._width;
        _height = moveFrom._height;
        _dataStart = moveFrom._dataStart;
        moveFrom._dataStart = nullptr;
        moveFrom._width = moveFrom._height = 0;
        return *this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename Type>
        TerrainUberSurface<Type>::TerrainUberSurface(const ::Assets::ResChar filename[])
            : TerrainUberSurfaceGeneric(filename)
        {
            if (!(_format == ImpliedTyping::TypeOf<Type>()))
                Throw(::Assets::Exceptions::InvalidAsset(
                    filename, "Uber surface format doesn't match expected type"));
        }

    template <typename Type>
        TerrainUberSurface<Type>::TerrainUberSurface() {}

    ////////////////////////////////////////////////////////////////////////////////////////////////

    namespace Internal { class SurfaceHeightsProvider; }

    class GenericUberSurfaceInterface::Pimpl
    {
    public:
        class RegisteredCell
        {
        public:
            UInt2 _mins, _maxs;
            unsigned _overlap;
            std::string _filename;
            std::function<void(const ShortCircuitUpdate&)> _shortCircuitUpdate;
        };

        std::vector<RegisteredCell>     _registeredCells;
        TerrainUberSurfaceGeneric*      _uberSurface;

        UInt2                           _gpuCacheMins, _gpuCacheMaxs;
        std::shared_ptr<ITerrainFormat> _ioFormat;

        intrusive_ptr<BufferUploads::ResourceLocator>  _gpucache[2];

        std::unique_ptr<ErosionSimulation>  _erosionSim;
        UInt2                               _erosionSimGPUCacheOffset;
        Float2                              _erosionWorldSpaceOffset;

        Pimpl() : _uberSurface(nullptr) {}
    };

    namespace Internal
    {
        static BufferUploads::BufferDesc BuildCacheDesc(UInt2 dims, RenderCore::Metal::NativeFormat::Enum format)
        {
            using namespace BufferUploads;
            BufferDesc desc;
            desc._type = BufferDesc::Type::Texture;
            desc._bindFlags = BindFlag::UnorderedAccess | BindFlag::ShaderResource;
            desc._cpuAccess = 0;
            desc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
            desc._allocationRules = 0;
            desc._textureDesc = TextureDesc::Plain2D(dims[0], dims[1], format);
            XlCopyString(desc._name, "TerrainWorkingCache");
            return desc;
        }

        class UberSurfacePacket : public BufferUploads::DataPacket
        {
        public:
            virtual void* GetData(SubResource subRes);
            virtual size_t GetDataSize(SubResource subRes) const;
            virtual BufferUploads::TexturePitches GetPitches(SubResource subRes) const;

            virtual std::shared_ptr<Marker> BeginBackgroundLoad();

            UberSurfacePacket(void* sourceData, unsigned stride, UInt2 dims);

        private:
            void* _sourceData;
            unsigned _stride;
            UInt2 _dims;
        };

        void* UberSurfacePacket::GetData(SubResource subRes)
        {
            assert(subRes==0);
            return _sourceData;
        }

        size_t UberSurfacePacket::GetDataSize(SubResource subRes) const
        {
            assert(subRes==0);
            return _stride*_dims[1];
        }

        BufferUploads::TexturePitches UberSurfacePacket::GetPitches(SubResource subRes) const
        {
            assert(subRes==0);
            return BufferUploads::TexturePitches(_stride, _stride*_dims[1]);
        }

        auto UberSurfacePacket::BeginBackgroundLoad() -> std::shared_ptr<Marker> { return nullptr; }

        UberSurfacePacket::UberSurfacePacket(void* sourceData, unsigned stride, UInt2 dims)
        {
            _sourceData = sourceData;
            _stride = stride;
            _dims = dims;
        }

        class SurfaceHeightsProvider : public ISurfaceHeightsProvider
        {
        public:
            virtual SRV         GetSRV();
            virtual Addressing  GetAddress(Float2 minCoordWorld, Float2 maxCoordWorld);
            virtual bool        IsFloatFormat() const;

            SurfaceHeightsProvider(Float2 worldSpaceCacheMin, Float2 worldSpaceCacheMax, UInt2 cacheDims, intrusive_ptr<ID3D::Resource> gpuCache);
            ~SurfaceHeightsProvider();
        private:
            Float2 _worldSpaceCacheMin, _worldSpaceCacheMax;
            UInt2 _cacheDims;
            intrusive_ptr<ID3D::Resource> _gpuCache;
            SRV _srv;
        };

        SurfaceHeightsProvider::SurfaceHeightsProvider(Float2 worldSpaceCacheMin, Float2 worldSpaceCacheMax, UInt2 cacheDims, intrusive_ptr<ID3D::Resource> gpuCache)
        : _worldSpaceCacheMin(worldSpaceCacheMin), _worldSpaceCacheMax(worldSpaceCacheMax)
        , _cacheDims(cacheDims), _gpuCache(std::move(gpuCache)), _srv(_gpuCache.get())
        {}

        SurfaceHeightsProvider::~SurfaceHeightsProvider() {}

        auto SurfaceHeightsProvider::GetSRV() -> SRV { return _srv; }
        auto SurfaceHeightsProvider::GetAddress(Float2 minCoordWorld, Float2 maxCoordWorld) -> Addressing
        {
            Addressing result;
            result._baseCoordinate = Int3(0,0,0);
            result._heightScale = 1.f;
            result._heightOffset = 0.f;

            if (    minCoordWorld[0] >= _worldSpaceCacheMin[0] &&  maxCoordWorld[0] <= _worldSpaceCacheMax[0]
                &&  minCoordWorld[1] >= _worldSpaceCacheMin[1] &&  maxCoordWorld[1] <= _worldSpaceCacheMax[1]) {

                result._minCoordOffset[0] = (int)XlFloor(float(_cacheDims[0]) * (minCoordWorld[0] - _worldSpaceCacheMin[0]) / (_worldSpaceCacheMax[0] - _worldSpaceCacheMin[0]));
                result._minCoordOffset[1] = (int)XlFloor(float(_cacheDims[1]) * (minCoordWorld[1] - _worldSpaceCacheMin[1]) / (_worldSpaceCacheMax[1] - _worldSpaceCacheMin[1]));

                result._maxCoordOffset[0] = (int)XlFloor(float(_cacheDims[0]) * (maxCoordWorld[0] - _worldSpaceCacheMin[0]) / (_worldSpaceCacheMax[0] - _worldSpaceCacheMin[0]));
                result._maxCoordOffset[1] = (int)XlFloor(float(_cacheDims[1]) * (maxCoordWorld[1] - _worldSpaceCacheMin[1]) / (_worldSpaceCacheMax[1] - _worldSpaceCacheMin[1]));

                result._valid = true;
            } else {
                result._minCoordOffset = result._maxCoordOffset = UInt2(0,0);
                result._valid = false;
            }

            return result;
        }

        bool SurfaceHeightsProvider::IsFloatFormat() const { return true; }
    }

    void    GenericUberSurfaceInterface::FlushGPUCache()
    {
        if (_pimpl->_gpucache[0]) {
                // readback data from the gpu asset (often requires a staging-style resource)
            {
                using namespace BufferUploads;
                auto& bufferUploads = GetBufferUploads();

                auto readback = bufferUploads.Resource_ReadBack(*_pimpl->_gpucache[0].get());
                assert(readback.get());

                auto readbackStride = readback->GetPitches()._rowPitch;
                auto readbackData = readback->GetData();

                auto dstStart = _pimpl->_uberSurface->GetData(_pimpl->_gpuCacheMins);
                auto dstStride = _pimpl->_uberSurface->GetStride();
                auto bytesPerSample = _pimpl->_uberSurface->Format().GetSize();

                UInt2 dims( _pimpl->_gpuCacheMaxs[0]-_pimpl->_gpuCacheMins[0]+1, 
                            _pimpl->_gpuCacheMaxs[1]-_pimpl->_gpuCacheMins[1]+1);
                for (unsigned y = 0; y<dims[1]; ++y)
                    XlCopyMemory(
                        PtrAdd(dstStart, y*dstStride),
                        PtrAdd(readbackData, y*readbackStride),
                        dims[0] * bytesPerSample);
            }

                //  Destroy the gpu cache
            _pimpl->_gpucache[0].reset();
            _pimpl->_gpucache[1].reset();

                //  look for all of the cells that intersect with the area we've changed.
                //  we have to rebuild the entire cell
            if (_pimpl->_ioFormat) {
                for (auto i=_pimpl->_registeredCells.cbegin(); i!=_pimpl->_registeredCells.cend(); ++i) {
                    if (_pimpl->_gpuCacheMaxs[0] < i->_mins[0] || _pimpl->_gpuCacheMaxs[1] < i->_mins[1]) continue;
                    if (_pimpl->_gpuCacheMins[0] > i->_maxs[0] || _pimpl->_gpuCacheMins[1] > i->_maxs[1]) continue;

                    TRY {
                        const auto treeDepth = 5u;
                        _pimpl->_ioFormat->WriteCell(
                            i->_filename.c_str(), *_pimpl->_uberSurface,
                            i->_mins, i->_maxs, treeDepth, i->_overlap);
                    } CATCH (...) {
                    } CATCH_END     // if the directory for the output file doesn't exist, we can get an exception here
                }
            }

            _pimpl->_gpuCacheMins = _pimpl->_gpuCacheMaxs = UInt2(0,0);
        }
    }

    void    GenericUberSurfaceInterface::BuildGPUCache(UInt2 mins, UInt2 maxs)
    {
            //  if there's an existing GPU cache, we need to flush it out, and copy
            //  the results back to system memory
        if (_pimpl->_gpucache[0]) {
            FlushGPUCache();
        }

        using namespace BufferUploads;
        auto& bufferUploads = GetBufferUploads();

        UInt2 dims(maxs[0]-mins[0]+1, maxs[1]-mins[1]+1);
        auto desc = Internal::BuildCacheDesc(dims, Metal::AsNativeFormat(_pimpl->_uberSurface->Format()));
        auto pkt = make_intrusive<Internal::UberSurfacePacket>(
            _pimpl->_uberSurface->GetData(mins), _pimpl->_uberSurface->GetStride(), dims);

            // create a texture on the GPU with some cached data from the uber surface.
            //      we need 2 copies of the gpu cache for update operations
        auto gpucache0 = bufferUploads.Transaction_Immediate(desc, pkt.get());
        auto gpucache1 = bufferUploads.Transaction_Immediate(desc, pkt.get());
        assert(gpucache0.get() && gpucache1.get());

        _pimpl->_gpucache[0] = std::move(gpucache0);
        _pimpl->_gpucache[1] = std::move(gpucache1);
        _pimpl->_gpuCacheMins = mins;
        _pimpl->_gpuCacheMaxs = maxs;
    }

    intrusive_ptr<BufferUploads::ResourceLocator> GenericUberSurfaceInterface::CopyToGPU(
        UInt2 topLeft, UInt2 bottomRight)
    {
        using namespace BufferUploads;
        auto& bufferUploads = GetBufferUploads();

        auto dims = bottomRight - topLeft;
        auto desc = Internal::BuildCacheDesc(dims, Metal::AsNativeFormat(_pimpl->_uberSurface->Format()));
        auto pkt = make_intrusive<Internal::UberSurfacePacket>(
            _pimpl->_uberSurface->GetData(topLeft), _pimpl->_uberSurface->GetStride(), dims);

        return bufferUploads.Transaction_Immediate(desc, pkt.get());
    }

    void    GenericUberSurfaceInterface::PrepareCache(UInt2 adjMins, UInt2 adjMaxs)
    {
        unsigned fieldWidth = _pimpl->_uberSurface->GetWidth();
        unsigned fieldHeight = _pimpl->_uberSurface->GetHeight();

            // see if this fits within the existing GPU cache
        if (_pimpl->_gpucache[0]) {
            bool within =   adjMins[0] >= _pimpl->_gpuCacheMins[0] && adjMins[1] >= _pimpl->_gpuCacheMins[1]
                        &&  adjMaxs[0] <= _pimpl->_gpuCacheMaxs[0] && adjMaxs[1] <= _pimpl->_gpuCacheMaxs[1];
            if (!within) {
                FlushGPUCache();    // flush out and build new gpu cache
            }
        }

        if (!_pimpl->_gpucache[0]) {
            UInt2 editCenter = (adjMins + adjMaxs) / 2;
            const UInt2 cacheSize(512, 512);    // take a big part of the surface and upload to the gpu

            UInt2 cacheMins(
                (unsigned)std::max(0, std::min(signed(adjMins[0]), signed(editCenter[0]) - signed(cacheSize[0]))),
                (unsigned)std::max(0, std::min(signed(adjMins[1]), signed(editCenter[1]) - signed(cacheSize[1]))));

            UInt2 cacheMaxs(
                std::min(fieldWidth-1, std::max(adjMaxs[0], editCenter[0] + cacheSize[0])),
                std::min(fieldHeight-1, std::max(adjMaxs[1], editCenter[1] + cacheSize[1])));

            BuildGPUCache(cacheMins, cacheMaxs);
        }
    }

    static RenderCore::Metal::DeviceContext GetImmediateContext()
    {
        ID3D::DeviceContext* immContextTemp = nullptr;
        RenderCore::Metal::ObjectFactory().GetUnderlying()->GetImmediateContext(&immContextTemp);
        intrusive_ptr<ID3D::DeviceContext> immContext = moveptr(immContextTemp);
        return RenderCore::Metal::DeviceContext(std::move(immContext));
    }

    void GenericUberSurfaceInterface::CancelActiveOperations()
    {}

    void    GenericUberSurfaceInterface::ApplyTool( 
        UInt2 adjMins, UInt2 adjMaxs, const char shaderName[],
        Float2 center, float radius, float adjustment, 
        std::tuple<uint64, void*, size_t> extraPackets[], unsigned extraPacketCount)
    {
        CancelActiveOperations();
        TRY 
        {
            using namespace RenderCore::Metal;

                // might be able to do this in a deferred context?
            auto context = GetImmediateContext();

            UnorderedAccessView uav(_pimpl->_gpucache[0]->GetUnderlying());
            context.BindCS(RenderCore::MakeResourceList(uav));

            auto& perlinNoiseRes = Techniques::FindCachedBox<PerlinNoiseResources>(PerlinNoiseResources::Desc());
            context.BindCS(RenderCore::MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));

            char fullShaderName[256];
            _snprintf_s(fullShaderName, _TRUNCATE, "%s:cs_*", shaderName);

            auto& cbBytecode = ::Assets::GetAssetDep<CompiledShaderByteCode>(fullShaderName);
            auto& cs = ::Assets::GetAssetDep<ComputeShader>(fullShaderName);
            struct Parameters
            {
                Float2 _center;
                float _radius;
                float _adjustment;
                UInt2 _cacheMins;
                UInt2 _cacheMaxs;
                UInt2 _adjMins;
                int _dummy[2];
            } parameters = { center, radius, adjustment, _pimpl->_gpuCacheMins, _pimpl->_gpuCacheMaxs, adjMins, 0, 0 };

            BoundUniforms uniforms(cbBytecode);
            uniforms.BindConstantBuffer(Hash64("Parameters"), 0, 1);

            const auto InputSurfaceHash = Hash64("InputSurface");
            ShaderResourceView cacheCopySRV;
            if (uniforms.BindShaderResource(InputSurfaceHash, 0, 1)) {
                context.GetUnderlying()->CopyResource(_pimpl->_gpucache[1]->GetUnderlying(), _pimpl->_gpucache[0]->GetUnderlying());
                cacheCopySRV = ShaderResourceView(_pimpl->_gpucache[1]->GetUnderlying());
            }

            const ShaderResourceView* resources[] = { &cacheCopySRV };
            std::vector<ConstantBufferPacket> pkts;
            pkts.push_back(RenderCore::MakeSharedPkt(parameters));
            for (unsigned c=0; c<extraPacketCount; ++c) {
                uniforms.BindConstantBuffer(std::get<0>(extraPackets[c]), 1+c, 1);
                auto start = std::get<1>(extraPackets[c]);
                auto size = std::get<2>(extraPackets[c]);
                pkts.push_back(RenderCore::MakeSharedPkt(start, PtrAdd(start, size)));
            }

            uniforms.Apply(context, UniformsStream(), UniformsStream(AsPointer(pkts.begin()), nullptr, pkts.size(), resources, dimof(resources)));   // no access to global uniforms here
            context.Bind(cs);
            const unsigned threadGroupDim = 16;
            context.Dispatch(
                (adjMaxs[0] - adjMins[0] + 1 + threadGroupDim - 1) / threadGroupDim,
                (adjMaxs[1] - adjMins[1] + 1 + threadGroupDim - 1) / threadGroupDim);
            context.UnbindCS<UnorderedAccessView>(0, 1);

            DoShortCircuitUpdate(&context, adjMins, adjMaxs);
        }
        CATCH (...) {}
        CATCH_END
    }

    void    GenericUberSurfaceInterface::DoShortCircuitUpdate(
        RenderCore::Metal::DeviceContext* context, 
        UInt2 adjMins, UInt2 adjMaxs)
    {
        TRY 
        {
            using namespace RenderCore::Metal;

            ShortCircuitUpdate upd;
            upd._context = context;
            upd._updateAreaMins = adjMins;
            upd._updateAreaMaxs = adjMaxs;
            upd._resourceMins = _pimpl->_gpuCacheMins;
            upd._resourceMaxs = _pimpl->_gpuCacheMaxs;
            upd._srv = std::make_unique<ShaderResourceView>(_pimpl->_gpucache[0]->GetUnderlying());

            for (auto i=_pimpl->_registeredCells.cbegin(); i!=_pimpl->_registeredCells.cend(); ++i) {
                if (adjMaxs[0] < i->_mins[0] || adjMaxs[1] < i->_mins[1]) continue;
                if (adjMins[0] > i->_maxs[0] || adjMins[1] > i->_maxs[1]) continue;

                    //  do a short-circuit update here... Copy from the cached gpu surface
                    //  into the relevant parts of these cells.
                    //  note -- this doesn't really work correct if there are pending uploads
                    //          for these cells!

                i->_shortCircuitUpdate(upd);
            }
        }
        CATCH (...) {}
        CATCH_END
    }

    void    GenericUberSurfaceInterface::BuildEmptyFile(
        const ::Assets::ResChar destinationFile[], 
        unsigned width, unsigned height, const ImpliedTyping::TypeDesc& type)
    {
        BasicFile outputFile(destinationFile, "wb");

        TerrainUberHeader hdr;
        hdr._magic = TerrainUberHeader::Magic;
        hdr._width = width;
        hdr._height = height;
        hdr._typeCat = (unsigned)type._type;
        hdr._typeArrayCount = type._arrayCount;
        hdr._dummy[0] = hdr._dummy[1] = hdr._dummy[2] = 0;
        outputFile.Write(&hdr, sizeof(hdr), 1);

        unsigned lineSize = width*type.GetSize();
        auto lineOfSamples = std::make_unique<uint8[]>(lineSize);
        std::fill(lineOfSamples.get(), &lineOfSamples[lineSize], 0);
        for (int y=0; y<int(height); ++y) {
            outputFile.Write(lineOfSamples.get(), 1, lineSize);
        }
    }

    void    GenericUberSurfaceInterface::RegisterCell(
                    const char destinationFile[], UInt2 mins, UInt2 maxs, unsigned overlap,
                    std::function<void(const ShortCircuitUpdate&)> shortCircuitUpdate)
    {
            //      Register the given cell... When there are any modifications, 
            //      we'll write over this cell's cached file

        Pimpl::RegisteredCell registeredCell;
        registeredCell._mins = mins;
        registeredCell._maxs = maxs;
        registeredCell._overlap = overlap;
        registeredCell._filename = destinationFile;
        registeredCell._shortCircuitUpdate = shortCircuitUpdate;
        _pimpl->_registeredCells.push_back(registeredCell);
    }

    void    GenericUberSurfaceInterface::RenderDebugging(RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext)
    {
        if (!_pimpl->_gpucache[0])
            return;

        CATCH_ASSETS_BEGIN
            using namespace RenderCore;
            Metal::ShaderResourceView  gpuCacheSRV(_pimpl->_gpucache[0]->GetUnderlying());
            context->BindPS(MakeResourceList(5, gpuCacheSRV));
            auto& debuggingShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                "game/xleres/ui/terrainmodification.sh:GpuCacheDebugging:ps_*",
                "");
            context->Bind(debuggingShader);
            context->Bind(Techniques::CommonResources()._blendStraightAlpha);
            SetupVertexGeneratorShader(*context);
            context->Draw(4);
        CATCH_ASSETS_END(parserContext)

        context->UnbindPS<RenderCore::Metal::ShaderResourceView>(5, 1);
    }

    GenericUberSurfaceInterface::GenericUberSurfaceInterface(TerrainUberSurfaceGeneric& uberSurface, std::shared_ptr<ITerrainFormat> ioFormat)
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_uberSurface = &uberSurface;     // no protection on this pointer (assuming it's coming from a resource)
        pimpl->_ioFormat = std::move(ioFormat);

        _pimpl = std::move(pimpl);
    }

    GenericUberSurfaceInterface::~GenericUberSurfaceInterface() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    #define baseShader "game/xleres/ui/terrainmodification.sh:"

    void    HeightsUberSurfaceInterface::AdjustHeights(Float2 center, float radius, float adjustment, float powerValue)
    {
        if (!_pimpl || !_pimpl->_uberSurface)
            return;

        UInt2 adjMins(  (unsigned)std::max(0.f, XlFloor(center[0] - radius)),
                        (unsigned)std::max(0.f, XlFloor(center[1] - radius)));
        UInt2 adjMaxs(  std::min(_pimpl->_uberSurface->GetWidth()-1, (unsigned)XlCeil(center[0] + radius)),
                        std::min(_pimpl->_uberSurface->GetHeight()-1, (unsigned)XlCeil(center[1] + radius)));

        PrepareCache(adjMins, adjMaxs);

            //  run a shader that will modify the gpu-cached part of the uber surface as we need
        struct RaiseLowerParameters
        {
            float _powerValue;
            float dummy[3];
        } raiseLowerParameters = { powerValue, 0.f, 0.f, 0.f };

        std::tuple<uint64, void*, size_t> extraPackets[] = 
        {
            std::make_tuple(Hash64("RaiseLowerParameters"), &raiseLowerParameters, sizeof(RaiseLowerParameters))
        };

        ApplyTool(adjMins, adjMaxs, baseShader "RaiseLower", center, radius, adjustment, extraPackets, dimof(extraPackets));
    }

    void    HeightsUberSurfaceInterface::AddNoise(Float2 center, float radius, float adjustment)
    {
        if (!_pimpl || !_pimpl->_uberSurface)
            return;

        UInt2 adjMins(  (unsigned)std::max(0.f, XlFloor(center[0] - radius)),
                        (unsigned)std::max(0.f, XlFloor(center[1] - radius)));
        UInt2 adjMaxs(  std::min(_pimpl->_uberSurface->GetWidth()-1, (unsigned)XlCeil(center[0] + radius)),
                        std::min(_pimpl->_uberSurface->GetHeight()-1, (unsigned)XlCeil(center[1] + radius)));

        PrepareCache(adjMins, adjMaxs);

            //  run a shader that will modify the gpu-cached part of the uber surface as we need

        ApplyTool(adjMins, adjMaxs, baseShader "AddNoise", center, radius, adjustment, nullptr, 0);
    }

    void    HeightsUberSurfaceInterface::CopyHeight(Float2 center, Float2 source, float radius, float adjustment, float powerValue, unsigned flags)
    {
        if (!_pimpl || !_pimpl->_uberSurface)
            return;

        UInt2 adjMins(  (unsigned)std::max(0.f, XlFloor(center[0] - radius)),
                        (unsigned)std::max(0.f, XlFloor(center[1] - radius)));
        UInt2 adjMaxs(  std::min(_pimpl->_uberSurface->GetWidth()-1, (unsigned)XlCeil(center[0] + radius)),
                        std::min(_pimpl->_uberSurface->GetHeight()-1, (unsigned)XlCeil(center[1] + radius)));

        PrepareCache(adjMins, adjMaxs);

            //  run a shader that will modify the gpu-cached part of the uber surface as we need
        struct CopyHeightParameters
        {
            Float2 _source;
            float _powerValue;
            unsigned _flags;
        } copyHeightParameters = { source, powerValue, flags };

        std::tuple<uint64, void*, size_t> extraPackets[] = 
        {
            std::make_tuple(Hash64("CopyHeightParameters"), &copyHeightParameters, sizeof(CopyHeightParameters))
        };

        ApplyTool(adjMins, adjMaxs, baseShader "CopyHeight", center, radius, adjustment, extraPackets, dimof(extraPackets));
    }

    void    HeightsUberSurfaceInterface::Rotate(Float2 center, float radius, Float3 rotationAxis, float rotationAngle)
    {
        if (!_pimpl || !_pimpl->_uberSurface)
            return;

        const float extend = 1.2f;
        UInt2 adjMins(  (unsigned)std::max(0.f, XlFloor(center[0] - extend * radius)),
                        (unsigned)std::max(0.f, XlFloor(center[1] - extend * radius)));
        UInt2 adjMaxs(  std::min(_pimpl->_uberSurface->GetWidth()-1, (unsigned)XlCeil(center[0] + extend * radius)),
                        std::min(_pimpl->_uberSurface->GetHeight()-1, (unsigned)XlCeil(center[1] + extend * radius)));

        PrepareCache(adjMins, adjMaxs);

            //  run a shader that will modify the gpu-cached part of the uber surface as we need
        assert(rotationAxis[2] == 0.f);
        struct RotateParamaters
        {
            Float2 _rotationAxis;
            float _angle;
            float _dummy;
        } rotateParameters = { Truncate(rotationAxis), rotationAngle, 0.f };

        std::tuple<uint64, void*, size_t> extraPackets[] = 
        {
            std::make_tuple(Hash64("RotateParameters"), &rotateParameters, sizeof(RotateParamaters))
        };

        ApplyTool(adjMins, adjMaxs, baseShader "Rotate", center, radius, 1.f, extraPackets, dimof(extraPackets));
    }

    void    HeightsUberSurfaceInterface::Smooth(Float2 center, float radius, unsigned filterRadius, float standardDeviation, float strength, unsigned flags)
    {
        if (!_pimpl || !_pimpl->_uberSurface)
            return;

        auto fieldWidth = _pimpl->_uberSurface->GetWidth()-1;
        auto fieldHeight = _pimpl->_uberSurface->GetHeight()-1;

        UInt2 adjMins(  (unsigned)std::max(0.f, XlFloor(center[0] - radius)),
                        (unsigned)std::max(0.f, XlFloor(center[1] - radius)));
        UInt2 adjMaxs(  std::min(fieldWidth, (unsigned)XlCeil(center[0] + radius)),
                        std::min(fieldHeight, (unsigned)XlCeil(center[1] + radius)));

        PrepareCache(
            UInt2(std::max(0u, adjMins[0]-filterRadius), std::max(0u, adjMins[1]-filterRadius)),
            UInt2(std::min(fieldWidth, adjMaxs[0]+filterRadius), std::min(fieldHeight, adjMins[1]+filterRadius)));

            //  run a shader that will modify the gpu-cached part of the uber surface as we need

        struct BlurParameters
        {
            Float4 _weights[33];
            unsigned _filterSize;
            unsigned _flags;
            unsigned _dummy[2];
        } blurParameters;
        unsigned filterSize = std::min(33u, 1 + filterRadius * 2);
        blurParameters._filterSize = filterSize;
        blurParameters._flags = flags;
        blurParameters._dummy[0] = blurParameters._dummy[1] = 0;
        float temp[dimof(((BlurParameters*)nullptr)->_weights)];
        BuildGaussianFilteringWeights(temp, standardDeviation, filterSize);
        for (unsigned c=0; c<filterSize; ++c)
            blurParameters._weights[c] = Float4(temp[c], temp[c], temp[c], temp[c]);
    
        std::tuple<uint64, void*, size_t> extraPackets[] = 
        {
            std::make_tuple(Hash64("GaussianParameters"), &blurParameters, sizeof(BlurParameters))
        };
            
        ApplyTool(adjMins, adjMaxs, baseShader "Smooth", center, radius, strength, extraPackets, dimof(extraPackets));
    }

    void    HeightsUberSurfaceInterface::FillWithNoise(Float2 mins, Float2 maxs, float baseHeight, float noiseHeight, float roughness, float fractalDetail)
    {
        if (!_pimpl || !_pimpl->_uberSurface)
            return;

        auto fieldWidth = _pimpl->_uberSurface->GetWidth()-1;
        auto fieldHeight = _pimpl->_uberSurface->GetHeight()-1;

        UInt2 adjMins((unsigned)std::max(0.f, mins[0]), (unsigned)std::max(0.f, mins[1]));
        UInt2 adjMaxs(std::min(fieldWidth, (unsigned)maxs[0]), std::min(fieldHeight, (unsigned)maxs[1]));
        PrepareCache(adjMins, adjMaxs);

        struct FillWithNoiseParameters
        {
            Float2 _mins, _maxs;
            float _baseHeight, _noiseHeight, _roughness, _fractalDetail;
        } fillWithNoiseParameters = { mins, maxs, baseHeight, noiseHeight, roughness, fractalDetail };
    
        std::tuple<uint64, void*, size_t> extraPackets[] = 
        {
            std::make_tuple(Hash64("FillWithNoiseParameters"), &fillWithNoiseParameters, sizeof(FillWithNoiseParameters))
        };
            
        ApplyTool(adjMins, adjMaxs, baseShader "FillWithNoise", LinearInterpolate(mins, maxs, 0.5f), 1.f, 1.f, extraPackets, dimof(extraPackets));
    }

    void    HeightsUberSurfaceInterface::Erosion_Begin(
        RenderCore::IThreadContext* context,
        Float2 mins, Float2 maxs, const TerrainConfig& cfg)
    {
        Erosion_End();

        auto tileSize = ErosionSimulation::DefaultTileSize();
        unsigned gridsX = (unsigned)XlCeil((maxs[0] - mins[0])/tileSize[0]);
        unsigned gridsY = (unsigned)XlCeil((maxs[1] - mins[1])/tileSize[1]);

        Float2 center(XlFloor((mins[0] + maxs[0])/2.f), XlFloor((mins[1] + maxs[1])/2.f));
        UInt2 size(gridsX * tileSize[0], gridsY * tileSize[1]);

            // adjust "mins" and "maxs"
            // (size should be even now, so this is safe)
        mins = center - 0.5f * Float2(size);
        maxs = center + 0.5f * Float2(size);
            // but we need to shift the grid if we end up straddling zero
        if (mins[0] < 0.f) { maxs[0] -= mins[0]; mins[0] = 0.f; }
        if (mins[1] < 0.f) { maxs[1] -= mins[1]; mins[1] = 0.f; }
        assert(mins[0] >= 0 && mins[1] >= 0);

            // prepare our new GPU cache
        UInt2 finalCacheMin = UInt2(unsigned(mins[0]), unsigned(mins[1]));
        UInt2 finalCacheMax = UInt2(unsigned(maxs[0]), unsigned(maxs[1]));
        PrepareCache(finalCacheMin, finalCacheMax);

        Int2 gpuCacheOffset = UInt2(unsigned(mins[0]), unsigned(mins[1])) - _pimpl->_gpuCacheMins;

            // Build a SurfaceHeightsProvider that can map from world space coordinates
            // into the coordinates of the GPU cache. In the coordinate system of the 
            // water simulation, the origin is the minimum point of the first tile we're
            // simulating. In other words, there is a offset between the water sim world
            // coords and the real world coords
        float terrainScale = cfg.ElementSpacing();
        // auto surfaceHeightsProvider = std::make_unique<Internal::SurfaceHeightsProvider>(
        //     (Float2(_pimpl->_gpuCacheMins) - Float2(finalCacheMin)) * terrainScale, (Float2(_pimpl->_gpuCacheMaxs + UInt2(1,1)) - Float2(finalCacheMin)) * terrainScale,
        //     _pimpl->_gpuCacheMaxs - _pimpl->_gpuCacheMins + UInt2(1,1), 
        //     _pimpl->_gpucache[0]);

        auto metalContext = RenderCore::Metal::DeviceContext::Get(*context);

        UInt2 erosionSimSize = finalCacheMax - finalCacheMin + UInt2(1,1);
        auto erosionSim = std::make_unique<ErosionSimulation>(erosionSimSize, terrainScale);

        Metal::ShaderResourceView gpuCacheSRV(_pimpl->_gpucache[0]->GetUnderlying());
        erosionSim->InitHeights(
            *metalContext, gpuCacheSRV,
            gpuCacheOffset, gpuCacheOffset + erosionSimSize);

        _pimpl->_erosionSim = std::move(erosionSim);
        _pimpl->_erosionSimGPUCacheOffset = gpuCacheOffset;
        _pimpl->_erosionWorldSpaceOffset = terrainScale * Float2(_pimpl->_erosionSimGPUCacheOffset + _pimpl->_gpuCacheMins);
    }

    void    HeightsUberSurfaceInterface::Erosion_Tick(
        RenderCore::IThreadContext* context,
        const ErosionSimulation::Settings& params)
    {
        if (!Erosion_IsPrepared()) {
            return;     // no active sim
        }

        auto metalContext = RenderCore::Metal::DeviceContext::Get(*context);
        _pimpl->_erosionSim->Tick(*metalContext, params);

        auto gpuCacheOffset = _pimpl->_erosionSimGPUCacheOffset;
        auto size = _pimpl->_erosionSim->GetDimensions();

        Metal::UnorderedAccessView gpuCacheUAV(_pimpl->_gpucache[0]->GetUnderlying());
        _pimpl->_erosionSim->GetHeights(
            *metalContext, gpuCacheUAV,
            gpuCacheOffset, gpuCacheOffset + size);

            //  Update the mesh with the changes
        DoShortCircuitUpdate(
            metalContext.get(),
            _pimpl->_gpuCacheMins + gpuCacheOffset, 
            _pimpl->_gpuCacheMins + gpuCacheOffset + size);
    }

    void    HeightsUberSurfaceInterface::Erosion_End()
    {
        _pimpl->_erosionSimGPUCacheOffset = UInt2(0,0);
        _pimpl->_erosionWorldSpaceOffset = Float2(0.f, 0.f);
        _pimpl->_erosionSim.reset();
    }

    bool    HeightsUberSurfaceInterface::Erosion_IsPrepared() const
    {
        return _pimpl->_erosionSim != nullptr;
    }

    void    HeightsUberSurfaceInterface::Erosion_RenderDebugging(
        RenderCore::IThreadContext* context,
        LightingParserContext& parserContext,
        const TerrainCoordinateSystem& coords)
    {
        if (!Erosion_IsPrepared()) return;

        auto metalContext = RenderCore::Metal::DeviceContext::Get(*context);
        _pimpl->_erosionSim->RenderDebugging(
            *metalContext, parserContext, 
            ErosionSimulation::RenderDebugMode::WaterVelocity3D,
            _pimpl->_erosionWorldSpaceOffset + Truncate(coords.TerrainOffset()));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void HeightsUberSurfaceInterface::CancelActiveOperations()
    {
            // (if we're currently running erosion, cancel it now...)
        Erosion_End();
    }

    TerrainUberHeightsSurface* HeightsUberSurfaceInterface::GetUberSurface() { return _uberSurface; }

    HeightsUberSurfaceInterface::HeightsUberSurfaceInterface(
        TerrainUberHeightsSurface& uberSurface, std::shared_ptr<ITerrainFormat> ioFormat)
    : GenericUberSurfaceInterface(uberSurface, std::move(ioFormat))
    {
        _uberSurface = &uberSurface;
    }

    HeightsUberSurfaceInterface::~HeightsUberSurfaceInterface()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void CoverageUberSurfaceInterface::Paint(Float2 centre, float radius, unsigned paintValue)
    {
        if (!_pimpl || !_pimpl->_uberSurface)
            return;

        UInt2 adjMins(  (unsigned)std::max(0.f, XlFloor(centre[0] - radius)),
                        (unsigned)std::max(0.f, XlFloor(centre[1] - radius)));
        UInt2 adjMaxs(  std::min(_pimpl->_uberSurface->GetWidth()-1, (unsigned)XlCeil(centre[0] + radius)),
                        std::min(_pimpl->_uberSurface->GetHeight()-1, (unsigned)XlCeil(centre[1] + radius)));

        PrepareCache(adjMins, adjMaxs);

            //  run a shader that will modify the gpu-cached part of the uber surface as we need
        struct PaintParameters
        {
            unsigned _newValue;
            float dummy[3];
        } params = { paintValue, 0.f, 0.f, 0.f };

        std::tuple<uint64, void*, size_t> extraPackets[] = 
        {
            std::make_tuple(Hash64("PaintParameters"), &params, sizeof(params))
        };

        ApplyTool(adjMins, adjMaxs, "game/xleres/ui/terrainmodification_int.sh:Paint", centre, radius, 0.f, extraPackets, dimof(extraPackets));
    }

    void CoverageUberSurfaceInterface::CancelActiveOperations()
    {
    }

    CoverageUberSurfaceInterface::CoverageUberSurfaceInterface(
        TerrainUberSurfaceGeneric& uberSurface,
        std::shared_ptr<ITerrainFormat> ioFormat)
        : GenericUberSurfaceInterface(uberSurface, std::move(ioFormat))
    {
    }
    CoverageUberSurfaceInterface::~CoverageUberSurfaceInterface() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    template TerrainUberHeightsSurface;
    template TerrainUberSurface<ShadowSample>;
    template TerrainUberSurface<uint8>;
}


namespace Utility { namespace ImpliedTyping
{
    template<>
        TypeDesc TypeOf<SceneEngine::ShadowSample>()
        {
            return TypeDesc(TypeCat::UInt16, 2);
        }
}}