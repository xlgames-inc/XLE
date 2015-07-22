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
#include "ShallowWater.h"
#include "Ocean.h"
#include "DeepOceanSim.h"
#include "SurfaceHeightsProvider.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"

#include "..\RenderCore\Resource.h"
#include "..\RenderCore\Metal\Format.h"
#include "..\RenderCore\Metal\Shader.h"
#include "..\RenderCore\Metal\State.h"
#include "..\RenderCore\Metal\InputLayout.h"
#include "..\RenderCore\RenderUtils.h"
#include "..\BufferUploads\IBufferUploads.h"
#include "..\BufferUploads\ResourceLocator.h"
#include "..\BufferUploads\DataPacket.h"

#include "..\Math\Geometry.h"
#include "..\ConsoleRig\IProgress.h"

#include "..\Utility\Streams\FileUtils.h"
#include "..\Utility\PtrUtils.h"
#include "..\Utility\BitUtils.h"
#include "..\Utility\IntrusivePtr.h"

#include "..\Core\WinAPI\IncludeWindows.h"
#include "..\Core\Exceptions.h"
#include <memory>
#include <stack>

#include "..\RenderCore\DX11\Metal\DX11.h"
#include "..\RenderCore\DX11\Metal\IncludeDX11.h"

namespace SceneEngine
{
    using namespace RenderCore;

    ////////////////////////////////////////////////////////////////////////////////////////////////

    class TerrainUberHeader
    {
    public:
        unsigned _magic;
        unsigned _width, _height;
        unsigned _dummy;

        static const unsigned Magic = 0xa3d3e3c3;
    };
   
    template <typename Type>
        void TerrainUberSurface<Type>::WriteCell(
            const ITerrainFormat& ioFormat,
            const char destinationFile[],
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements)
    {
        ioFormat.WriteCell(destinationFile, *this, cellMins, cellMaxs, treeDepth, overlapElements);
    }

    template <typename Type>
        void* TerrainUberSurface<Type>::GetData(UInt2 coord)
    {
        assert(_mappedFile && _dataStart);
        return &_dataStart[coord[1] * _width + coord[0]];
    }

    template <typename Type>
        unsigned TerrainUberSurface<Type>::GetStride() const { return _width*sizeof(Type); }

    template <typename Type>
        unsigned TerrainUberSurface<Type>::BitsPerPixel() const
    {
        return sizeof(Type)*8;
    }

    template<> unsigned TerrainUberSurface<float>::Format() const { return (unsigned)RenderCore::Metal::NativeFormat::R32_FLOAT; }
    template<> unsigned TerrainUberSurface<uint8>::Format() const { return (unsigned)RenderCore::Metal::NativeFormat::R8_UINT; }
    template<> unsigned TerrainUberSurface<ShadowSample>::Format() const { return (unsigned)RenderCore::Metal::NativeFormat::R16G16_UNORM; }

    template <typename Type>
        TerrainUberSurface<Type>::TerrainUberSurface(const char filename[])
    {
        _width = _height = 0;
        _dataStart = nullptr;

            //  Load the file as a Win32 "mapped file"
            //  the format is very simple.. it's just a basic header, and then
            //  a huge 2D array of height values
        auto mappedFile = std::make_unique<MemoryMappedFile>(filename, 0, MemoryMappedFile::Access::Read|MemoryMappedFile::Access::Write, BasicFile::ShareMode::Read);
        if (!mappedFile->IsValid())
            ThrowException(::Assets::Exceptions::InvalidResource(filename, "Failed while openning uber surface file"));
        
        auto& hdr = *(TerrainUberHeader*)mappedFile->GetData();
        if (hdr._magic != TerrainUberHeader::Magic)
            ThrowException(::Assets::Exceptions::InvalidResource(filename, "Uber surface file appears to be corrupt"));

        _width = hdr._width;
        _height = hdr._height;
        _dataStart = (Type*)PtrAdd(mappedFile->GetData(), sizeof(TerrainUberHeader));
        _mappedFile = std::move(mappedFile);
    }

    template <typename Type>
        TerrainUberSurface<Type>::TerrainUberSurface()
    {
        _width = _height = 0;
        _dataStart = nullptr;
    }

    template <typename Type>
        TerrainUberSurface<Type>::~TerrainUberSurface()
    {}

    template <typename Type>
        TerrainUberSurface<Type>::TerrainUberSurface(TerrainUberSurface<Type>&& moveFrom)
    : _mappedFile(std::move(moveFrom._mappedFile))
    , _dataStart(moveFrom._dataStart)
    , _width(moveFrom._width)
    , _height(moveFrom._height)
    {
        moveFrom._dataStart = nullptr;
        moveFrom._width = moveFrom._height = 0;
    }

    template <typename Type>
        TerrainUberSurface<Type>& TerrainUberSurface<Type>::operator=(TerrainUberSurface<Type>&& moveFrom)
    {
        _mappedFile = std::move(moveFrom._mappedFile);
        _width = moveFrom._width;
        _height = moveFrom._height;
        _dataStart = moveFrom._dataStart;
        moveFrom._dataStart = nullptr;
        moveFrom._width = moveFrom._height = 0;
        return *this;
    }

    ITerrainUberSurface::~ITerrainUberSurface() {}

    ////////////////////////////////////////////////////////////////////////////////////////////////

    namespace Internal { class SurfaceHeightsProvider; }

    class ErosionSimulation
    {
    public:
        intrusive_ptr<ID3D::Resource> _hardMaterials, _softMaterials, _softMaterialsCopy;

        Metal::UnorderedAccessView _hardMaterialsUAV;
        Metal::UnorderedAccessView _softMaterialsUAV;
        Metal::ShaderResourceView _softMaterialsCopySRV;
        Metal::ShaderResourceView _hardMaterialsSRV;
        Metal::ShaderResourceView _softMaterialsSRV;

        std::unique_ptr<ShallowWaterSim> _waterSim;
        std::unique_ptr<Internal::SurfaceHeightsProvider> _surfaceHeightsProvider;
        unsigned _bufferCount;

        UInt2 _gpuCacheOffset, _simSize;
        float _elementSpacing;
    };

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
        ITerrainUberSurface*            _uberSurface;

        UInt2                           _gpuCacheMins, _gpuCacheMaxs;
        intrusive_ptr<ID3D::Resource>   _gpucache[2];
        ErosionSimulation               _erosionSim;
        std::shared_ptr<ITerrainFormat> _ioFormat;

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

                auto readback = bufferUploads.Resource_ReadBack(
                    BufferUploads::ResourceLocator(_pimpl->_gpucache[0].get()));
                assert(readback.get());

                auto readbackStride = readback->GetPitches()._rowPitch;
                auto readbackData = readback->GetData();

                auto dstStart = _pimpl->_uberSurface->GetData(_pimpl->_gpuCacheMins);
                auto dstStride = _pimpl->_uberSurface->GetStride();
                auto bbp = _pimpl->_uberSurface->BitsPerPixel();

                UInt2 dims( _pimpl->_gpuCacheMaxs[0]-_pimpl->_gpuCacheMins[0]+1, 
                            _pimpl->_gpuCacheMaxs[1]-_pimpl->_gpuCacheMins[1]+1);
                for (unsigned y = 0; y<dims[1]; ++y)
                    XlCopyMemory(
                        PtrAdd(dstStart, y*dstStride),
                        PtrAdd(readbackData, y*readbackStride),
                        dims[0] * bbp / 8);
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
                        _pimpl->_uberSurface->WriteCell(
                            *_pimpl->_ioFormat,
                            i->_filename.c_str(),
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
        auto desc = Internal::BuildCacheDesc(dims, (RenderCore::Metal::NativeFormat::Enum)_pimpl->_uberSurface->Format());
        auto pkt = make_intrusive<Internal::UberSurfacePacket>(
            _pimpl->_uberSurface->GetData(mins), _pimpl->_uberSurface->GetStride(), dims);

            // create a texture on the GPU with some cached data from the uber surface.
            //      we need 2 copies of the gpu cache for update operations
        auto gpucache0 = bufferUploads.Transaction_Immediate(desc, pkt.get())->AdoptUnderlying();
        auto gpucache1 = bufferUploads.Transaction_Immediate(desc, pkt.get())->AdoptUnderlying();
        assert(gpucache0.get() && gpucache1.get());

        _pimpl->_gpucache[0] = std::move(gpucache0);
        _pimpl->_gpucache[1] = std::move(gpucache1);
        _pimpl->_gpuCacheMins = mins;
        _pimpl->_gpuCacheMaxs = maxs;
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

            UnorderedAccessView uav(_pimpl->_gpucache[0].get());
            context.BindCS(RenderCore::MakeResourceList(uav));

            auto& perlinNoiseRes = Techniques::FindCachedBox<PerlinNoiseResources>(PerlinNoiseResources::Desc());
            context.BindCS(RenderCore::MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));

            char fullShaderName[256];
            _snprintf_s(fullShaderName, dimof(fullShaderName), "%s:cs_*", shaderName);

            auto& cbBytecode = Assets::GetAssetDep<CompiledShaderByteCode>(fullShaderName);
            auto& cs = Assets::GetAssetDep<ComputeShader>(fullShaderName);
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
                context.GetUnderlying()->CopyResource(_pimpl->_gpucache[1].get(), _pimpl->_gpucache[0].get());
                cacheCopySRV = ShaderResourceView(_pimpl->_gpucache[1].get());
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
            upd._srv = std::make_unique<ShaderResourceView>(_pimpl->_gpucache[0].get());

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
        unsigned width, unsigned height, unsigned bitsPerElement)
    {
        BasicFile outputFile(destinationFile, "wb");

        TerrainUberHeader hdr;
        hdr._magic = TerrainUberHeader::Magic;
        hdr._width = width;
        hdr._height = height;
        hdr._dummy = 0;
        outputFile.Write(&hdr, sizeof(hdr), 1);

        unsigned lineSize = width*bitsPerElement/8;
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

        TRY {
            using namespace RenderCore;
            Metal::ShaderResourceView  gpuCacheSRV(_pimpl->_gpucache[0].get());
            context->BindPS(MakeResourceList(5, gpuCacheSRV));
            auto& debuggingShader = Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                "game/xleres/ui/terrainmodification.sh:GpuCacheDebugging:ps_*",
                "");
            context->Bind(debuggingShader);
            context->Bind(Techniques::CommonResources()._blendStraightAlpha);
            SetupVertexGeneratorShader(context);
            context->Draw(4);
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END

        context->UnbindPS<RenderCore::Metal::ShaderResourceView>(5, 1);
    }

    GenericUberSurfaceInterface::GenericUberSurfaceInterface(ITerrainUberSurface& uberSurface, std::shared_ptr<ITerrainFormat> ioFormat)
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

    static const unsigned ErosionWaterTileDimension = 256;
    static const unsigned ErosionWaterTileScale = 1;            // scale relative to the terrain surface resolution. Eg, 4 means each terrain grid becomes 4x4 grid elements in the water simulation

    void    HeightsUberSurfaceInterface::Erosion_Begin(
        RenderCore::IThreadContext* context,
        Float2 mins, Float2 maxs, const TerrainConfig& cfg)
    {
            //  We're going to do an erosion simulation over the given points
            //  First we need to allocate the buffers we need:
            //      * GPU heights cache
            //      * ShallowWaterSim object
            //      * ShallowWaterSim::ActiveElement elements

        unsigned tileSizeTerr = ErosionWaterTileDimension/ErosionWaterTileScale;
        unsigned gridsX = (unsigned)XlCeil((maxs[0] - mins[0])/tileSizeTerr);
        unsigned gridsY = (unsigned)XlCeil((maxs[1] - mins[1])/tileSizeTerr);

        Float2 center(XlFloor((mins[0] + maxs[0])/2.f), XlFloor((mins[1] + maxs[1])/2.f));
        UInt2 size(gridsX * tileSizeTerr, gridsY * tileSizeTerr);

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
        auto surfaceHeightsProvider = std::make_unique<Internal::SurfaceHeightsProvider>(
            (Float2(_pimpl->_gpuCacheMins) - Float2(finalCacheMin)) * terrainScale, (Float2(_pimpl->_gpuCacheMaxs + UInt2(1,1)) - Float2(finalCacheMin)) * terrainScale,
            _pimpl->_gpuCacheMaxs - _pimpl->_gpuCacheMins + UInt2(1,1), 
            _pimpl->_gpucache[0]);

            // Create the "box" that will contain resources required while performing the
            // erosion simulation
        static bool usePipeModel = true;
        auto newShallowWater = std::make_unique<ShallowWaterSim>(
            ShallowWaterSim::Desc(unsigned(ErosionWaterTileDimension), gridsX * gridsY, usePipeModel, 
            true, false, true));
        
        std::vector<Int2> newElements;
        for (unsigned y=0; y<gridsY; ++y)
            for (unsigned x=0; x<gridsX; ++x)
                newElements.push_back(Int2(x, y));

        auto metalContext = RenderCore::Metal::DeviceContext::Get(*context);
        ShallowWaterSim::SimulationContext simContext(
            *metalContext, DeepOceanSimSettings(),
            terrainScale * ErosionWaterTileDimension / ErosionWaterTileScale, Zero<Float2>(),
            surfaceHeightsProvider.get(), nullptr, ShallowWaterSim::BorderMode::Surface);

        newShallowWater->BeginElements(
            simContext,
            AsPointer(newElements.cbegin()), AsPointer(newElements.cend()));

        /////////////////////////////////////////////////////////////////////////////////////

        auto& bufferUploads = GetBufferUploads();
        auto desc = Internal::BuildCacheDesc(size, (RenderCore::Metal::NativeFormat::Enum)_pimpl->_uberSurface->Format());
        auto hardMaterials = bufferUploads.Transaction_Immediate(desc)->AdoptUnderlying();
        auto softMaterials = bufferUploads.Transaction_Immediate(desc)->AdoptUnderlying();
        auto softMaterialsCopy = bufferUploads.Transaction_Immediate(desc)->AdoptUnderlying();

        RenderCore::Metal::UnorderedAccessView hardMaterialsUAV(hardMaterials.get());
        RenderCore::Metal::UnorderedAccessView softMaterialsUAV(softMaterials.get());
        RenderCore::Metal::ShaderResourceView hardMaterialsSRV(hardMaterials.get());
        RenderCore::Metal::ShaderResourceView softMaterialsSRV(softMaterials.get());
        RenderCore::Metal::ShaderResourceView softMaterialsCopySRV(softMaterialsCopy.get());

            //  The simulating area may actually only a small part of the cached area.
            //  "hard materials" should be initialised with a copy of the heights texture. But "soft materials" should be
            //  cleared to zero;
        {
            float clearValues[4] = { 0.f, 0.f, 0.f, 0.f };
            metalContext->Clear(softMaterialsUAV, clearValues);
            // context.GetUnderlying()->CopyResource(hardMaterials.get(), _pimpl->_gpucache[0].get());

            D3D11_BOX srcBox;
            srcBox.left = gpuCacheOffset[0];
            srcBox.top = gpuCacheOffset[1];
            srcBox.front = 0;
            srcBox.right = unsigned(gpuCacheOffset[0] + size[0]);
            srcBox.bottom = unsigned(gpuCacheOffset[1] + size[1]);
            srcBox.back = 1;
            metalContext->GetUnderlying()->CopySubresourceRegion(
                hardMaterials.get(), 0, 0, 0, 0, _pimpl->_gpucache[0].get(), 0, &srcBox);
        }

        _pimpl->_erosionSim._surfaceHeightsProvider = std::move(surfaceHeightsProvider);
        _pimpl->_erosionSim._waterSim = std::move(newShallowWater);
        _pimpl->_erosionSim._bufferCount = 0;
        _pimpl->_erosionSim._hardMaterials = std::move(hardMaterials);
        _pimpl->_erosionSim._softMaterials = std::move(softMaterials);
        _pimpl->_erosionSim._softMaterialsCopy = std::move(softMaterialsCopy);
        _pimpl->_erosionSim._hardMaterialsUAV = std::move(hardMaterialsUAV);
        _pimpl->_erosionSim._softMaterialsUAV = std::move(softMaterialsUAV);
        _pimpl->_erosionSim._hardMaterialsSRV = std::move(hardMaterialsSRV);
        _pimpl->_erosionSim._softMaterialsSRV = std::move(softMaterialsSRV);
        _pimpl->_erosionSim._softMaterialsCopySRV = std::move(softMaterialsCopySRV);
        _pimpl->_erosionSim._gpuCacheOffset = gpuCacheOffset;
        _pimpl->_erosionSim._simSize = UInt2(unsigned(size[0]), unsigned(size[1]));
        _pimpl->_erosionSim._elementSpacing = terrainScale;
    }

    void    HeightsUberSurfaceInterface::Erosion_Tick(
        RenderCore::IThreadContext* context,
        const ErosionParameters& params)
    {
            //      Update the shallow water simulation
        if (!Erosion_IsPrepared()) {
            return;     // no active sim
        }

        float terrainScale = _pimpl->_erosionSim._elementSpacing;

        auto metalContext = RenderCore::Metal::DeviceContext::Get(*context);
        ShallowWaterSim::SimSettings settings;
        settings._rainQuantityPerFrame = params._rainQuantityPerFrame;
        settings._evaporationConstant = params._evaporationConstant;
        settings._pressureConstant = params._pressureConstant;

        ShallowWaterSim::SimulationContext simContext(
            *metalContext, DeepOceanSimSettings(),
            terrainScale * ErosionWaterTileDimension / ErosionWaterTileScale, Zero<Float2>(),
            _pimpl->_erosionSim._surfaceHeightsProvider.get(), nullptr,
            ShallowWaterSim::BorderMode::Surface);

        _pimpl->_erosionSim._waterSim->ExecuteInternalSimulation(
            simContext, settings, _pimpl->_erosionSim._bufferCount);

            //      We need to use the water movement information to change rock to dirt,
            //      and then move dirt along with the water movement
            //
            //      We really need to know velocity information for the water.
            //      We can try to calculate the velocity based on the change in height (but
            //      this won't give accurate results in all situations. Sometimes it might
            //      be a strong flow, but the height is staying the same).

        auto& erosionSim = _pimpl->_erosionSim;
        erosionSim._waterSim->BindForErosionSimulation(*metalContext, erosionSim._bufferCount);
        
        metalContext->GetUnderlying()->CopyResource(_pimpl->_gpucache[1].get(), _pimpl->_gpucache[0].get());
        Metal::ShaderResourceView terrainHeightsCopySRV(_pimpl->_gpucache[1].get());

        using namespace RenderCore::Metal;
        UnorderedAccessView uav(_pimpl->_gpucache[0].get());
        metalContext->BindCS(RenderCore::MakeResourceList(uav, erosionSim._hardMaterialsUAV, erosionSim._softMaterialsUAV));
        metalContext->BindCS(RenderCore::MakeResourceList(terrainHeightsCopySRV));

        struct TickErosionSimConstats
        { 
            Int2 gpuCacheOffset, simulationSize;
            
            float KConstant;			// = 2.f		(effectively, max sediment that can be moved in one second)
	        float ErosionRate;			// = 0.03f;		hard to soft rate
	        float SettlingRate;			// = 0.05f;		soft to hard rate (deposition / settling)
	        float MaxSediment;			// = 2.f;		max sediment per cell (ie, max value in the soft materials array)
	        float DepthMax;				// = 25.f		Shallow water erodes more quickly, up to this depth
	        float SedimentShiftScalar; 	// = 1.f;		Amount of sediment that moves per frame

	        float ElementSpacing;
	        float TanSlopeAngle;
	        float ThermalErosionRate;	// = 0.05;		Speed of material shifting due to thermal erosion
            unsigned dummy[3];

        } constants = {
            erosionSim._gpuCacheOffset, erosionSim._simSize,
            params._kConstant, 
            params._erosionRate,
            params._settlingRate,
            params._maxSediment,
            params._depthMax,
            params._sedimentShiftScalar,
            _pimpl->_erosionSim._elementSpacing,
            XlTan(params._thermalSlopeAngle * gPI / 180.f),
            params._thermalErosionRate
        };
        metalContext->BindCS(RenderCore::MakeResourceList(5, ConstantBuffer(&constants, sizeof(constants))));
        metalContext->BindCS(RenderCore::MakeResourceList(
            Techniques::CommonResources()._defaultSampler,
            Techniques::CommonResources()._linearClampSampler));

        char defines[256];
        _snprintf_s(defines, _TRUNCATE, 
            "SHALLOW_WATER_TILE_DIMENSION=%i;SURFACE_HEIGHTS_FLOAT=%i;USE_LOOKUP_TABLE=1", 
            erosionSim._waterSim->GetGridDimension(),
            _pimpl->_erosionSim._surfaceHeightsProvider->IsFloatFormat());

            // update sediment
        auto& updateShader = Assets::GetAssetDep<ComputeShader>("game/xleres/ocean/tickerosion.csh:UpdateSediment:cs_*", defines);
        metalContext->Bind(updateShader);
        metalContext->Dispatch(erosionSim._simSize[0]/16, erosionSim._simSize[1]/16, 1);

            // shift sediment
        metalContext->GetUnderlying()->CopyResource(erosionSim._softMaterialsCopy.get(), erosionSim._softMaterials.get());
        metalContext->BindCS(RenderCore::MakeResourceList(1, erosionSim._softMaterialsCopySRV));

        auto& shiftShader = Assets::GetAssetDep<ComputeShader>("game/xleres/ocean/tickerosion.csh:ShiftSediment:cs_*", defines);
        metalContext->Bind(shiftShader);
        metalContext->Dispatch(erosionSim._simSize[0]/16, erosionSim._simSize[1]/16, 1);

            // "thermal" erosion
        metalContext->GetUnderlying()->CopyResource(erosionSim._softMaterialsCopy.get(), erosionSim._hardMaterials.get());

        auto& thermalShader = Assets::GetAssetDep<ComputeShader>("game/xleres/ocean/tickerosion.csh:ThermalErosion:cs_*", defines);
        metalContext->Bind(thermalShader);
        metalContext->Dispatch(erosionSim._simSize[0]/16, erosionSim._simSize[1]/16, 1);
        
        metalContext->UnbindCS<UnorderedAccessView>(0, 8);

            //  Update the mesh with the changes
        DoShortCircuitUpdate(
            metalContext.get(),
            _pimpl->_gpuCacheMins + erosionSim._gpuCacheOffset, 
            _pimpl->_gpuCacheMins + erosionSim._gpuCacheOffset + erosionSim._simSize);
        ++_pimpl->_erosionSim._bufferCount;
    }

    void    HeightsUberSurfaceInterface::Erosion_End()
    {
            //      Finish the erosion sim, and delete all of the related objects

        _pimpl->_erosionSim._surfaceHeightsProvider.reset();
        _pimpl->_erosionSim._waterSim.reset();
        _pimpl->_erosionSim._bufferCount = 0;
        _pimpl->_erosionSim._hardMaterials.reset();
        _pimpl->_erosionSim._softMaterials.reset();
        _pimpl->_erosionSim._softMaterialsCopy.reset();
        _pimpl->_erosionSim._hardMaterialsUAV = RenderCore::Metal::UnorderedAccessView();
        _pimpl->_erosionSim._softMaterialsUAV = RenderCore::Metal::UnorderedAccessView();
        _pimpl->_erosionSim._hardMaterialsSRV = RenderCore::Metal::ShaderResourceView();
        _pimpl->_erosionSim._softMaterialsSRV = RenderCore::Metal::ShaderResourceView();
        _pimpl->_erosionSim._softMaterialsCopySRV = RenderCore::Metal::ShaderResourceView();
        _pimpl->_erosionSim._gpuCacheOffset = UInt2(0,0);
        _pimpl->_erosionSim._simSize = UInt2(0,0);
    }

    bool    HeightsUberSurfaceInterface::Erosion_IsPrepared() const
    {
        return _pimpl->_erosionSim._hardMaterials.get() != nullptr;
    }

    void    HeightsUberSurfaceInterface::Erosion_RenderDebugging(
        RenderCore::IThreadContext* context,
        LightingParserContext& parserContext,
        const TerrainCoordinateSystem& coords)
    {
        if (!Erosion_IsPrepared()) return;

        TRY {
            const float terrainScale = _pimpl->_erosionSim._elementSpacing;

                // we need to add an offset to transform from the water simulation coords
                // into true world space (though scales remain the same)
            auto worldSpaceOffset = 
                    terrainScale * Float2(_pimpl->_erosionSim._gpuCacheOffset + _pimpl->_gpuCacheMins)
                +   Truncate(coords.TerrainOffset());

            auto metalContext = RenderCore::Metal::DeviceContext::Get(*context);
            metalContext->BindPS(RenderCore::MakeResourceList(2, _pimpl->_erosionSim._hardMaterialsSRV, _pimpl->_erosionSim._softMaterialsSRV));
            _pimpl->_erosionSim._waterSim->RenderVelocities(
                *metalContext, parserContext,
                DeepOceanSimSettings(), terrainScale * ErosionWaterTileDimension / ErosionWaterTileScale, 
                worldSpaceOffset, _pimpl->_erosionSim._bufferCount-1, 
                ShallowWaterSim::BorderMode::Surface, true);
        } 
        CATCH (const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH (const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH_END
    }

    HeightsUberSurfaceInterface::ErosionParameters::ErosionParameters()
    {
        _rainQuantityPerFrame = 0.001f;
        _evaporationConstant = 0.99f;
        _pressureConstant = 200.f;
        _kConstant = 3.f;
        _erosionRate = 0.15f;
        _settlingRate = 0.25f;
        _maxSediment = 1.f;
        _depthMax = 25.f;
        _sedimentShiftScalar = 1.f;
        _thermalSlopeAngle = 40.f;
        _thermalErosionRate = 0.05f;
    }

    class ShadowingAngleOperator
    {
    public:
        void operator()(Float2 s0, Float2 s1, float edgeAlpha)
        {
                // for performance reasons, we don't do any boundary checking
                // on the input values. The input algorithm must guarantee that
                // we get reasonable results.
            Int2 is0 = Int2(int(s0[0]), int(s0[1])), is1 = Int2(int(s1[0]), int(s1[1]));
            assert(is0[0] >= 0 && is0[0] <= int(_surface->GetWidth()));
            assert(is0[1] >= 0 && is0[1] <= int(_surface->GetHeight()));
            assert(is1[0] >= 0 && is1[0] <= int(_surface->GetWidth()));
            assert(is1[1] >= 0 && is1[1] <= int(_surface->GetHeight()));

            // we need to find the height of the edge at the point we pass through it
            float h0 = _surface->GetValueFast(is0[0], is0[1]);
            float h1 = _surface->GetValueFast(is1[0], is1[1]);
            float finalHeight = LinearInterpolate(h0, h1, edgeAlpha);
            Float2 finalPos = LinearInterpolate(s0, s1, edgeAlpha);
            
                // this defines an angle. We can do "smaller than" comparisons on tanTheta to find the smallest theta
            float distance = Magnitude(finalPos - Truncate(_samplePt)) * _xyScale;
            assert(distance > 0.f);
            float grad = (finalHeight - _samplePt[2]) / distance;
            _bestResult = BranchlessMax(grad, _bestResult);
        }

        ShadowingAngleOperator(TerrainUberHeightsSurface* surface, Float3 samplePt, float xyScale) { _bestResult = -FLT_MAX; _surface = surface; _samplePt = samplePt; _xyScale = xyScale; }

        float _bestResult;
    protected:
        TerrainUberHeightsSurface* _surface;
        Float3 _samplePt;
        float _xyScale;
    };

    float GetInterpolatedValue(TerrainUberHeightsSurface& surface, Float2 pt)
    {
        Float2 floored(XlFloor(pt[0]), XlFloor(pt[1]));
        Float2 ceiled = floored + Float2(1.f, 1.f);
        Float2 alpha = pt - floored;
        float A = surface.GetValue((unsigned)floored[0], (unsigned)floored[1]);
        float B = surface.GetValue((unsigned) ceiled[0], (unsigned)floored[1]);
        float C = surface.GetValue((unsigned)floored[0], (unsigned) ceiled[1]);
        float D = surface.GetValue((unsigned) ceiled[0], (unsigned) ceiled[1]);
        return 
              A * (1.f - alpha[0]) * (1.f - alpha[1])
            + B * (      alpha[0]) * (1.f - alpha[1])
            + C * (1.f - alpha[0]) * (      alpha[1])
            + D * (      alpha[0]) * (      alpha[1])
            ;
    }

    float HeightsUberSurfaceInterface::CalculateShadowingAngle(Float2 samplePt, Float2 sunDirectionOfMovement, float xyScale)
    {
            //  Travel forward along the sunDirectionOfMovement and find the shadowing angle.
            //  It's important here that integer coordinates are on corners of the "pixels"
            //      -- ie, not the centers. This will keep the height map correctly aligned
            //  with the shadowing samples
        auto& surface = *_uberSurface;

        float sampleHeight = GetInterpolatedValue(*_uberSurface, samplePt);

            //  limit the maximum shadow distance (for efficiency while calculating the angles)
            //  As we get further away from the sample point, we're less likely to find the shadow
            //  caster... So just cut off at a given distance (helps performance greatly)
            //  It seems that around 1000 is needed for very long shadows. At 200, shadows get clipped off too soon.
        const unsigned maxShadowDistance = 1000;

            //  where will this iteration hit the edge of the valid area?
            //  start with a really long line, then clamp it to the valid region
        Float2 fe = samplePt + std::min(maxShadowDistance, surface._width + surface._height) * sunDirectionOfMovement;
        if (fe[0] < 0.f) {
            fe = LinearInterpolate(samplePt, fe, (-samplePt[0]) / (fe[0]-samplePt[0]));
            fe[0] = 0.f;
        } else if (fe[0] > float(surface._width-1)) {
            fe = LinearInterpolate(samplePt, fe, (float(surface._width-1) - samplePt[0]) / (fe[0]-samplePt[0]));
            fe[0] = float(surface._width-1);
        }
        
        if (fe[1] < 0.f) {
            fe = LinearInterpolate(samplePt, fe, (-samplePt[1]) / (fe[1]-samplePt[1]));
            fe[1] = 0.f;
        } else if (fe[1] > float(surface._height-1)) {
            fe = LinearInterpolate(samplePt, fe, (float(surface._height-1) - samplePt[1]) / (fe[1]-samplePt[1]));
            fe[1] = float(surface._height-1);
        }

        assert(samplePt[0] >= 0.f && samplePt[0] < surface._width);
        assert(samplePt[1] >= 0.f && samplePt[1] < surface._height);
        assert(fe[0] >= 0.f && fe[0] < surface._width);
        assert(fe[1] >= 0.f && fe[1] < surface._height);

        ShadowingAngleOperator opr(&surface, Expand(samplePt, sampleHeight), xyScale);
        GridEdgeIterator2(samplePt, fe, opr);
        float shadowingAngle = XlATan2(1.f, opr._bestResult);
        return shadowingAngle;
    }

    void    HeightsUberSurfaceInterface::BuildShadowingSurface(
        const char destinationFile[], Int2 interestingMins, Int2 interestingMaxs, 
        Float2 sunDirectionOfMovement, float xyScale, float shadowToHeightsScale,
        ConsoleRig::IProgress* progress)
    {
            //      There are some limitations on the way the sun can move.
            //      It must move in a perfect circle arc, and pass through a
            //      point directly above. It must be as if our terrain is near
            //      the equator of the planet. The results won't be exactly physically
            //      correct for areas away from the equator; but maybe that's difficult
            //      for people to notice.
            //
            //      While this limitation is fairly significant, for many situations
            //      it's ok. It's difficult for the player to notice that the sun
            //      follows the same path every time and we still have control over 
            //      the time of sunrise and sunset, etc.

            //  For each point, we want to move both ways along the "sunDirectionOfMovement"
            //  vector. We want to find the angle at which shadowing starts. We can then
            //  assume that the point will always be in shadow when the sun is below that
            //  angle (this applies for the other direction, also).
            //
            //  We'll align the shadowing information perfectly with the height information.
            //  That means the shadowing samples happen on the corners of the quads that
            //  are generated by the height map.
            //
            //  For iterating along the line, we can use a fixed point method like Bresenham's method. 
            //  We want to pass through every height map quad that the line passes through. We'll then
            //  find the two points on the edges of that quad that are pierced by the line.
            //  The simplest way to do this, is to consider the grid as a set of grid lines, and look
            //  for all of the cases where the test line intersects those grid lines.
            //  Note that it's possible that the first shadow could be cast by the diagonal edge on
            //  the triangles for the quad (not an outside edge). This method will miss those cases.
            //  But the difference should be subtle.
            //
            //  We want to write to a real big file... So here's what we'll do.
            //      Lets do 1 line at a time, and write each line to the file in one write operation
            //
            //  If we wanted to improve this, we could twiddle the data in the texture. This would
            //  mean that samples that are close to each other in physical space are more likely to
            //  be close in the file.

        auto width = unsigned(_pimpl->_uberSurface->GetWidth() / shadowToHeightsScale);
        auto height = unsigned(_pimpl->_uberSurface->GetHeight() / shadowToHeightsScale);

        BasicFile outputFile(destinationFile, "wb");

        TerrainUberHeader hdr;
        hdr._magic = TerrainUberHeader::Magic;
        hdr._width = width;
        hdr._height = height;
        hdr._dummy = 0;
        outputFile.Write(&hdr, sizeof(hdr), 1);

            // The "expansion constant" helps prevent shadows creaping up on peaks.
            // Peaks (especially sharp peaks) shouldn't receive shadows until the sun is >90 degrees, or <-90 degrees.
            // But if we clamp the direction at +-90, shadow will start to the creep
            // up on the peak when the sun gets near 90 degrees. We want to prevent the
            // shadow from behaving like this -- which we can do by clamping the angle
            // beyond 90.
        const float expansionConstant = 1.5f;
        const float conversionConstant = float(0xffff) / (.5f * expansionConstant * float(M_PI));

        auto step = progress ? progress->BeginStep("Generate Terrain Shadowing", height, true) : nullptr;

        const int border = int(1.f / shadowToHeightsScale);

        auto lineOfSamples = std::make_unique<ShadowSample[]>(width);
        std::fill(lineOfSamples.get(), &lineOfSamples[width], ShadowSample(0xffff, 0xffff));

        outputFile.Write(lineOfSamples.get(), sizeof(ShadowSample), width); // first line is a dummy

        int y=1;
        for (; y<int(height)-border; ++y) {
            if (y >= interestingMins[1] && y < interestingMaxs[1]) {
                for (int x=std::max(interestingMins[0], 1); x<std::min(interestingMaxs[0], int(width)-border); ++x) {

                        //  First values is in the opposite direction of the sun movement. This will be a negative number
                        //  (but we'll store it as a positive value to increase precision)
                    const float sampleOffset = 0.f;
                    float a0 = CalculateShadowingAngle(Float2(float(x) + sampleOffset, float(y) + sampleOffset) * shadowToHeightsScale, -sunDirectionOfMovement, xyScale);
                    float a1 = CalculateShadowingAngle(Float2(float(x) + sampleOffset, float(y) + sampleOffset) * shadowToHeightsScale,  sunDirectionOfMovement, xyScale);

                        // Both a0 and a1 should be positive. But we'll negate a0 before we use it for a comparison
                    assert(a0 > 0.f && a1 > 0.f);

                    lineOfSamples[x] = ShadowSample(
                        (int16)Clamp(a0 * conversionConstant, 0.f, float(0xffff)),
                        (int16)Clamp(a1 * conversionConstant, 0.f, float(0xffff)));
                }
            } else {
                std::fill(&lineOfSamples[interestingMins[0]], &lineOfSamples[std::min(interestingMaxs[0]+1, int(width))], ShadowSample(0xffff, 0xffff));
            }

            outputFile.Write(lineOfSamples.get(), sizeof(ShadowSample), width);

            if (step) {
                step->Advance();
                if (step->IsCancelled()) break;
            }
        }

            // if we get cancelled, fill the remainder in with blanks.
        for (; y<int(height); ++y) {
            std::fill(&lineOfSamples[interestingMins[0]], &lineOfSamples[std::min(interestingMaxs[0]+1, int(width))], ShadowSample(0xffff, 0xffff));
            outputFile.Write(lineOfSamples.get(), sizeof(ShadowSample), width);
        }
    }

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
        ITerrainUberSurface& uberSurface,
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


