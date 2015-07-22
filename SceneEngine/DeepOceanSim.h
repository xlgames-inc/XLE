// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include <vector>

namespace SceneEngine
{
    class FFTBufferBox
    {
    public:
        class Desc
        {
        public:
            Desc(unsigned width, unsigned height, bool useDerivativesMapForNormals);
            unsigned _width, _height;
            bool _useDerivativesMapForNormals;
        };

        FFTBufferBox(const Desc& desc);
        ~FFTBufferBox();

        intrusive_ptr<ID3D::Resource>               _workingTextureReal;
        RenderCore::Metal::UnorderedAccessView      _workingTextureRealUVA;
        RenderCore::Metal::RenderTargetView         _workingTextureRealTarget;
        RenderCore::Metal::ShaderResourceView       _workingTextureRealShaderResource;

        intrusive_ptr<ID3D::Resource>               _workingTextureImaginary;
        RenderCore::Metal::UnorderedAccessView      _workingTextureImaginaryUVA;
        RenderCore::Metal::RenderTargetView         _workingTextureImaginaryTarget;
        RenderCore::Metal::ShaderResourceView       _workingTextureImaginaryShaderResource;

        intrusive_ptr<ID3D::Resource>               _workingTextureXReal;
        RenderCore::Metal::UnorderedAccessView      _workingTextureXRealUVA;
        RenderCore::Metal::ShaderResourceView       _workingTextureXRealShaderResource;

        intrusive_ptr<ID3D::Resource>               _workingTextureXImaginary;
        RenderCore::Metal::UnorderedAccessView      _workingTextureXImaginaryUVA;
        RenderCore::Metal::ShaderResourceView       _workingTextureXImaginaryShaderResource;

        intrusive_ptr<ID3D::Resource>               _workingTextureYReal;
        RenderCore::Metal::UnorderedAccessView      _workingTextureYRealUVA;
        RenderCore::Metal::ShaderResourceView       _workingTextureYRealShaderResource;

        intrusive_ptr<ID3D::Resource>               _workingTextureYImaginary;
        RenderCore::Metal::UnorderedAccessView      _workingTextureYImaginaryUVA;
        RenderCore::Metal::ShaderResourceView       _workingTextureYImaginaryShaderResource;

        intrusive_ptr<ID3D::Resource>                       _normalsTexture;
        std::vector<RenderCore::Metal::UnorderedAccessView> _normalsTextureUAV;
        std::vector<RenderCore::Metal::ShaderResourceView>  _normalsSingleMipSRV;
        RenderCore::Metal::ShaderResourceView               _normalsTextureShaderResource;

        intrusive_ptr<ID3D::Resource>               _foamQuantity[2];
        RenderCore::Metal::UnorderedAccessView      _foamQuantityUAV[2];
        RenderCore::Metal::ShaderResourceView       _foamQuantitySRV[2];
        RenderCore::Metal::ShaderResourceView       _foamQuantitySRV2[2];

        bool _useDerivativesMapForNormals;
    };

    class OceanSettings
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

        float _foamThreshold, _foamIncreaseSpeed;
        float _foamIncreaseClamp;
        unsigned _foamDecrease;

        OceanSettings();
    };

    class LightingParserContext;

    void UpdateOceanSurface(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
        const OceanSettings& oceanSettings, FFTBufferBox& fftBuffer,
        unsigned bufferCounter);

    void OceanSurface_DrawDebugging(   
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext,
        const OceanSettings& oceanSettings,
        FFTBufferBox& fftBuffer);

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
            const OceanSettings& oceanSettings, float shallowGridPhysicalDimension);
    }
}
