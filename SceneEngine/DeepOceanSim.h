// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../BufferUploads/IBufferUploads_Forward.h"
#include <vector>

namespace BufferUploads { class ResourceLocator; }
namespace Utility { class ParameterBox; }

namespace SceneEngine
{
    class LightingParserContext;

    class DeepOceanSimSettings
    {
    public:
        bool        _enable;
        float       _windAngle[2];
        float       _windVelocity[2];
        float       _physicalDimensions;
        unsigned    _gridDimensions;
        float       _strengthConstantXY;
        float       _strengthConstantZ;
        float       _detailNormalsStrength;
        float       _spectrumFade;
        float       _scaleAgainstWind[2];
        float       _suppressionFactor[2];
        float       _gridShiftSpeed;
        float       _baseHeight;

        float       _foamThreshold, _foamIncreaseSpeed;
        float       _foamIncreaseClamp;
        unsigned    _foamDecrease;

        DeepOceanSimSettings();
        DeepOceanSimSettings(const Utility::ParameterBox& params);
    };

    class DeepOceanSim
    {
    public:
        class Desc
        {
        public:
            Desc(unsigned width, unsigned height, bool useDerivativesMapForNormals, bool buildFoam);
            unsigned _width, _height;
            bool _useDerivativesMapForNormals;
            bool _buildFoam;
        };
        
        using SRV = RenderCore::Metal::ShaderResourceView;
        using RTV = RenderCore::Metal::RenderTargetView;
        using UAV = RenderCore::Metal::UnorderedAccessView;
        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;
        using MetalContext = RenderCore::Metal::DeviceContext;

        void Update(
            MetalContext* context, LightingParserContext& parserContext, 
            const DeepOceanSimSettings& oceanSettings,
            unsigned bufferCounter);

        void DrawDebugging(   
            MetalContext& context, 
            LightingParserContext& parserContext,
            const DeepOceanSimSettings& oceanSettings);

        DeepOceanSim(const Desc& desc);
        ~DeepOceanSim();

    // protected:

        ResLocator  _workingTextureReal;
        UAV         _workingTextureRealUVA;
        RTV         _workingTextureRealRTV;
        SRV         _workingTextureRealSRV;

        ResLocator  _workingTextureImaginary;
        UAV         _workingTextureImaginaryUVA;
        RTV         _workingTextureImaginaryRTV;
        SRV         _workingTextureImaginarySRV;

        ResLocator  _workingTextureXReal;
        UAV         _workingTextureXRealUVA;
        SRV         _workingTextureXRealSRV;

        ResLocator  _workingTextureXImaginary;
        UAV         _workingTextureXImaginaryUVA;
        SRV         _workingTextureXImaginarySRV;

        ResLocator  _workingTextureYReal;
        UAV         _workingTextureYRealUVA;
        SRV         _workingTextureYRealSRV;

        ResLocator  _workingTextureYImaginary;
        UAV         _workingTextureYImaginaryUVA;
        SRV         _workingTextureYImaginarySRV;

        ResLocator          _normalsTexture;
        std::vector<UAV>    _normalsTextureUAV;
        std::vector<SRV>    _normalsSingleMipSRV;
        SRV                 _normalsTextureSRV;

        ResLocator  _foamQuantity[2];
        UAV         _foamQuantityUAV[2];
        SRV         _foamQuantitySRV[2];
        SRV         _foamQuantitySRV2[2];

        bool        _useDerivativesMapForNormals;
    };

    namespace Internal
    {
        class OceanMaterialConstants
        {
        public:
            float _physicalWidth, _physicalHeight;
            float _strengthConstantXY;
            float _strengthConstantZ;

            float _shallowGridPhysicalDimension;
            float _baseHeight;
            float _foamThreshold, _foamIncreaseSpeed;

            float _foamIncreaseClamp;
            unsigned _foamDecrease;
            float _dummy[2];
        };

        OceanMaterialConstants BuildOceanMaterialConstants(
            const DeepOceanSimSettings& oceanSettings, float shallowGridPhysicalDimension);
    }
}
