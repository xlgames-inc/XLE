// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Math/Matrix.h"
#include <memory>

namespace RenderCore { namespace Assets { class SharedStateSet; class ModelRenderer; class ModelScaffold; }}
namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace SceneEngine
{
    /// <summary>Prepares imposter "sprites" for objects, and uses them as a stand-in</summary>
    /// Far-off objects can be approximated with sprites (though often a single object may need
    /// several sprites for different angles and lighting conditions).
    ///
    /// In this case, the sprites are dynamically created as required.
    class DynamicImposters
    {
    public:
        using SharedStateSet = RenderCore::Assets::SharedStateSet;
        using ModelRenderer = RenderCore::Assets::ModelRenderer;
        using ModelScaffold = RenderCore::Assets::ModelScaffold;

        //////////////////////////////////////////////////////////////////////////////
            //   Q U E U E   &   R E N D E R
        void Render(
            RenderCore::Metal::DeviceContext& context,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex);
        void Queue(
            const ModelRenderer& renderer, const ModelScaffold& scaffold, 
            const Float3x4& localToWorld, const Float3& cameraPosition);
        void Reset();

        //////////////////////////////////////////////////////////////////////////////
            //   C O N F I G
        class Config
        {
        public:
            float       _thresholdDistance;
            unsigned    _angleQuant;
            float       _calibrationDistance;
            float       _calibrationFov;
            unsigned    _calibrationPixels;
            UInt2       _minDims;
            UInt2       _maxDims;
            UInt3       _altasSize;
            unsigned    _maxSpriteCount;

            Config();
        };

        void Load(const Config& config);
        void Disable();

        float GetThresholdDistance() const;
        bool IsEnabled() const;

        //////////////////////////////////////////////////////////////////////////////
            //   M E T R I C S
        class Metrics;
        Metrics GetMetrics() const;
        RenderCore::Metal::ShaderResourceView GetAtlasResource(unsigned layer);

        DynamicImposters(SharedStateSet& sharedStateSet);
        ~DynamicImposters();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };


    class DynamicImposters::Metrics
    {
    public:
        unsigned    _spriteCount;

            // allocation stats
        unsigned    _pixelsAllocated;
        unsigned    _pixelsTotal;
        UInt2       _largestFreeBlockArea;
        UInt2       _largestFreeBlockSide;
        unsigned    _overflowCounter;
        unsigned    _pendingCounter;

            // atlas config
        unsigned    _bytesPerPixel;
        unsigned    _layerCount;

        Metrics();
    };
}

