// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Math/Vector.h"
#include "../Core/Types.h"
#include <memory>
#include <functional>
#include <assert.h>

namespace Utility { class MemoryMappedFile; }

namespace SceneEngine
{
    template <typename Type> class TerrainUberSurface;
    class LightingParserContext;

    typedef std::pair<uint16, uint16> ShadowSample;
    typedef TerrainUberSurface<float> TerrainUberHeightsSurface;
    typedef TerrainUberSurface<ShadowSample> TerrainUberShadowingSurface;

    class ITerrainFormat;
    class TerrainConfig;
    class TerrainCoordinateSystem;

    bool BuildUberSurfaceFile(
        const char filename[], const TerrainConfig& config, 
        ITerrainFormat* ioFormat,
        unsigned xStart, unsigned yStart, unsigned xDims, unsigned yDims);

    /// <summary>Represents a single "uber" field of terrain data</summary>
    /// Normally the terrain is separated into many cells, each with limited
    /// dimensions. But while editing, it's useful to see the terrain as
    /// just a single large field of information (particularly because there
    /// is some overlap between adjacent cells).
    /// This object allows us to see terrain data in that format, by mapping
    /// a large file into memory.
    ///
    /// Note that this is a little restrictive at the moment, because we need
    /// to know the format of the data at compile time. It might be handy to
    /// make this interface more generic, so that the format of the element
    /// is defined by a NativeFormat::Enum type, and allow queries with
    /// dynamic casting.
    /// Clients of this type often want to re-sample the data. So they need to
    /// be able to manipulate it in full precision, whatever true format it is.
    /// That means we can't just cast all data to a Float4... we probably need
    /// something a little nicer than that.
    template <typename Type> class TerrainUberSurface
    {
    public:
        Type GetValue(unsigned x, unsigned y) const;
        void SetValue(unsigned x, unsigned y, Type newValue);
        Type GetValueFast(unsigned x, unsigned y) const;
        unsigned GetWidth() const { return _width; }
        unsigned GetHeight() const { return _height; }

        TerrainUberSurface(const char filename[]);
        ~TerrainUberSurface();
        
        TerrainUberSurface();
        TerrainUberSurface(TerrainUberSurface&& moveFrom);
        TerrainUberSurface& operator=(TerrainUberSurface&& moveFrom);
    private:
        std::unique_ptr<Utility::MemoryMappedFile> _mappedFile;

        unsigned _width, _height;
        Type* _dataStart;

        friend class TerrainUberSurfaceInterface;
    };

    class ShortCircuitUpdate
    {
    public:
        std::unique_ptr<RenderCore::Metal::ShaderResourceView> _srv;
        UInt2 _updateAreaMins, _updateAreaMaxs;
        UInt2 _resourceMins, _resourceMaxs;
        RenderCore::Metal::DeviceContext* _context;
    };

    class TerrainCoordinateSystem;

    class TerrainUberSurfaceInterface
    {
    public:
        void    AdjustHeights(Float2 center, float radius, float adjustment, float powerValue);
        void    Smooth(Float2 center, float radius, unsigned filterRadius, float standardDeviation, float strength, unsigned flags);
        void    AddNoise(Float2 center, float radius, float adjustment);
        void    CopyHeight(Float2 center, Float2 source, float radius, float adjustment, float powerValue, unsigned flags);
        void    Rotate(Float2 center, float radius, Float3 rotationAxis, float rotationAngle);

        void    FillWithNoise(Float2 mins, Float2 maxs, float baseHeight, float noiseHeight, float roughness, float fractalDetail);

        struct ErosionParameters
        {
            float _rainQuantityPerFrame, _changeToSoftConstant;
            float _softFlowConstant, _softChangeBackConstant;

            ErosionParameters(float rainQuantityPerFrame, float changeToSoftConstant, float softFlowConstant, float softChangeBackConstant)
            : _rainQuantityPerFrame(rainQuantityPerFrame), _changeToSoftConstant(changeToSoftConstant)
            , _softFlowConstant(softFlowConstant), _softChangeBackConstant(softChangeBackConstant) {}
        };

        void    Erosion_Begin(Float2 mins, Float2 maxs);
        void    Erosion_Tick(const ErosionParameters& params);
        void    Erosion_End();
        bool    Erosion_IsPrepared() const;
        void    Erosion_RenderDebugging(RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, const TerrainCoordinateSystem& coords);

        void    RegisterCell(
                    const char destinationFile[], UInt2 mins, UInt2 maxs, unsigned overlap,
                    std::function<void(const ShortCircuitUpdate&)> shortCircuitUpdate);
        void    RenderDebugging(RenderCore::Metal::DeviceContext* devContext, SceneEngine::LightingParserContext& context);
        void    BuildShadowingSurface(const char destinationFile[], Int2 interestingMins, Int2 interestingMaxs, Float2 sunDirectionOfMovement, float xyScale);

        TerrainUberHeightsSurface* GetUberSurface();

        TerrainUberSurfaceInterface(
            TerrainUberHeightsSurface& uberSurface,
            std::shared_ptr<ITerrainFormat> ioFormat);
        ~TerrainUberSurfaceInterface();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        void    FlushGPUCache();
        void    BuildGPUCache(UInt2 mins, UInt2 maxs);
        void    PrepareCache(UInt2 adjMin, UInt2 adjMax);
        void    ApplyTool(  UInt2 adjMins, UInt2 adjMaxs, const char shaderName[],
                            Float2 center, float radius, float adjustment, 
                            std::tuple<uint64, void*, size_t> extraPackets[], unsigned extraPacketCount);
        void    DoShortCircuitUpdate(RenderCore::Metal::DeviceContext* context, UInt2 adjMins, UInt2 adjMaxs);

        float   CalculateShadowingAngle(Float2 samplePt, float sampleHeight, Float2 sunDirectionOfMovement, float xyScale);
    };

        ///////////////   I N L I N E   I M P L E M E N T A T I O N S   ///////////////

    namespace Internal
    {
        template <typename Type> inline Type DummyValue() { return Type(0); }
        template <> inline ShadowSample DummyValue() { return ShadowSample(0, 0); }
    }

    template <typename Type>
        inline Type TerrainUberSurface<Type>::GetValue(unsigned x, unsigned y) const
    {
        if (y >= _height || x >= _width)
            return Internal::DummyValue<Type>();
        return _dataStart[y*_width+x];
    }

    template <typename Type>
        inline void TerrainUberSurface<Type>::SetValue(unsigned x, unsigned y, Type newValue)
    {
        if (y < _height && x < _width) {
            _dataStart[y*_width+x] = newValue;
        }
    }

    template <typename Type>
        inline Type TerrainUberSurface<Type>::GetValueFast(unsigned x, unsigned y) const
    {
        assert(y < _height && x < _width);
        return _dataStart[y*_width+x];
    }
}
