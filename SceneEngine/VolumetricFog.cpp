// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "VolumetricFog.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "LightDesc.h"
#include "Noise.h"
#include "LightInternal.h"
#include "ShaderLightDesc.h"
#include "MetalStubs.h"

#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/DeferredShaderResource.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/QueryPool.h"
#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/IAnnotator.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../BufferUploads/DataPacket.h"

#include "../Assets/Assets.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/StringFormat.h"
#include "../Utility/BitUtils.h"
#include "../xleres/FileList.h"

#include <functional>       // for std::ref


namespace SceneEngine
{
    using namespace RenderCore;

    VolumetricFogMaterial VolumetricFogMaterial_Default();

        ///////////////////////////////////////////////////////////////////////////////////////////////

    enum class ShadowFilterMode { BoxFilter = 0, Seven = 1, Five = 2, None };

    class VolumetricFogShaders
    {
    public:
        class Desc
        {
        public:
            unsigned    _msaaSampleCount;
            bool        _useMsaaSamplers;
            bool        _flipDirection;
            bool        _esmShadowMaps;
            bool        _doNoiseOffset;
            unsigned    _shadowCascadeMode;
            unsigned    _blurredShadowCascadeCount;
            unsigned    _shadowCascadeSkip;
            unsigned    _depthSlices;
            unsigned    _shadowFilterMode;
            float       _filterStdDev;

            Desc(   unsigned msaaSampleCount, bool useMsaaSamplers, 
                    bool flipDirection, bool esmShadowMaps, 
                    bool doNoiseOffset, unsigned shadowCascadeMode,
                    unsigned blurredShadowCascadeCount,
                    unsigned shadowCascadeSkip,
                    unsigned depthSlices,
                    unsigned shadowFilterMode,
                    float filterStdDev)
            {
                XlZeroMemory(*this);
                _msaaSampleCount = msaaSampleCount;
                _useMsaaSamplers = useMsaaSamplers;
                _flipDirection = flipDirection;
                _esmShadowMaps = esmShadowMaps;
                _doNoiseOffset = doNoiseOffset;
                _shadowCascadeMode = shadowCascadeMode;
                _blurredShadowCascadeCount = blurredShadowCascadeCount;
                _shadowCascadeSkip = shadowCascadeSkip;
                _depthSlices = depthSlices;
                _shadowFilterMode = shadowFilterMode;
                _filterStdDev = filterStdDev;
            }
        };

        const Metal::ShaderProgram*     _buildExponentialShadowMap;
        Metal::BoundUniforms            _buildExponentialShadowMapBinding;
        const Metal::ShaderProgram*     _horizontalFilter;
        const Metal::ShaderProgram*     _verticalFilter;
        Metal::BoundUniforms            _horizontalFilterBinding, _verticalFilterBinding;

        const Metal::ComputeShader*     _injectLight;
        Metal::BoundUniforms            _injectLightBinding;
        const Metal::ComputeShader*     _propagateLight;
        Metal::BoundUniforms            _propagateLightBinding;
        const Metal::ShaderProgram*     _resolveLight;
        Metal::BoundUniforms            _resolveLightBinding;
        const Metal::ShaderProgram*     _resolveLightNoGrid;
        Metal::BoundUniforms            _resolveLightNoGridBinding;

        Metal::ConstantBuffer           _filterWeightsConstants;

        VolumetricFogShaders(const Desc& desc);
        ~VolumetricFogShaders();

        const ::Assets::DependencyValidation& GetDependencyValidation() const   { return _validationCallback; }
    private:
        ::Assets::DependencyValidation  _validationCallback
    };

    VolumetricFogShaders::VolumetricFogShaders(const Desc& desc)
    {
        char defines[256];
        _snprintf_s(
            defines, _TRUNCATE, 
            "ESM_SHADOW_MAPS=%i;DO_NOISE_OFFSET=%i;SHADOW_CASCADE_MODE=%i;BLURRED_SHADOW_CASCADE_COUNT=%i;DEPTH_SLICE_COUNT=%i;SHADOW_CASCADE_SKIP=%i", 
            int(desc._esmShadowMaps), int(desc._doNoiseOffset), 
            desc._shadowCascadeMode,
            desc._blurredShadowCascadeCount, desc._depthSlices,
            desc._shadowCascadeSkip);
        auto* buildExponentialShadowMap = &::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
            BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
            "xleres/VolumetricEffect/shadowsfilter.pixel.hlsl:BuildExponentialShadowMap:ps_*", 
            defines);

        ///////////////////////////////////////////////////////////////////////////////////////////
        const Metal::ShaderProgram *horizontalFilter = nullptr, *verticalFilter = nullptr;
        unsigned filterSize = 0;
        if (desc._shadowFilterMode == (unsigned)ShadowFilterMode::BoxFilter) {

            StringMeld<32> filterDefines; filterDefines << "ESM_SHADOW_MAPS=" << int(desc._esmShadowMaps);
            horizontalFilter = &::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
                "xleres/VolumetricEffect/shadowsfilter.pixel.hlsl:HorizontalBoxFilter11:ps_*",
                filterDefines.get());
            verticalFilter = &::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
                "xleres/VolumetricEffect/shadowsfilter.pixel.hlsl:VerticalBoxFilter11:ps_*",
                filterDefines.get());
            filterSize = 11;

        } else if (desc._shadowFilterMode == (unsigned)ShadowFilterMode::Seven) {

            horizontalFilter = &::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
                "xleres/VolumetricEffect/shadowsfilter.pixel.hlsl:HorizontalFilter7:ps_*");
            verticalFilter = &::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
                "xleres/VolumetricEffect/shadowsfilter.pixel.hlsl:VerticalFilter7:ps_*");
            filterSize = 7;

        } else if (desc._shadowFilterMode == (unsigned)ShadowFilterMode::Five) {

            horizontalFilter = &::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
                "xleres/VolumetricEffect/shadowsfilter.pixel.hlsl:HorizontalFilter5:ps_*");
            verticalFilter = &::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
                "xleres/VolumetricEffect/shadowsfilter.pixel.hlsl:VerticalFilter5:ps_*");
            filterSize = 5;

        }

        ///////////////////////////////////////////////////////////////////////////////////////////
        float filteringWeightsBuffer[12];
        XlZeroMemory(filteringWeightsBuffer);
        BuildGaussianFilteringWeights(filteringWeightsBuffer, desc._filterStdDev, filterSize);
        auto filterWeightsConstants = MakeMetalCB(filteringWeightsBuffer, sizeof(filteringWeightsBuffer));

        ///////////////////////////////////////////////////////////////////////////////////////////
		UniformsStreamInterface buildExponentialShadowMapBindingUsi;
		buildExponentialShadowMapBindingUsi.BindConstantBuffer(0, {Hash64("VolumetricFogConstants")});
		buildExponentialShadowMapBindingUsi.BindConstantBuffer(1, {Hash64("$Globals")});
		Metal::BoundUniforms buildExponentialShadowMapBinding(
			*buildExponentialShadowMap,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			buildExponentialShadowMapBindingUsi);


        Metal::BoundUniforms horizontalFilterBinding, verticalFilterBinding;
        if (horizontalFilter && verticalFilter) {
			UniformsStreamInterface usi;
			usi.BindConstantBuffer(0, {Hash64("$Globals")});
			usi.BindConstantBuffer(1, {Hash64("Filtering")});
            horizontalFilterBinding = Metal::BoundUniforms(
				std::ref(*horizontalFilter),
				Metal::PipelineLayoutConfig{},
				UniformsStreamInterface{},
				usi);

			verticalFilterBinding = Metal::BoundUniforms(
				std::ref(*verticalFilter),
				Metal::PipelineLayoutConfig{},
				UniformsStreamInterface{},
				usi);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////
        auto* injectLight = &::Assets::Legacy::GetAssetDep<Metal::ComputeShader>(
            "xleres/volumetriceffect/injectlight.compute.hlsl:InjectLighting:cs_*", defines);
        auto* propagateLight = &::Assets::Legacy::GetAssetDep<Metal::ComputeShader>(
            "xleres/volumetriceffect/injectlight.compute.hlsl:PropagateLighting:cs_*", defines);

		UniformsStreamInterface injectLightBindingUsi;
		injectLightBindingUsi.BindConstantBuffer(0, {Hash64("VolumetricFogConstants")});
        Metal::BoundUniforms injectLightBinding(
			*injectLight,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			injectLightBindingUsi);

		UniformsStreamInterface propagateLightBindingUsi;
		propagateLightBindingUsi.BindConstantBuffer(0, {Hash64("VolumetricFogConstants")});
        Metal::BoundUniforms propagateLightBinding(
			*propagateLight,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			propagateLightBindingUsi);

        ///////////////////////////////////////////////////////////////////////////////////////////
        const char* vertexShader = 
            desc._flipDirection
                ? BASIC2D_VERTEX_HLSL ":fullscreen_flip_viewfrustumvector:vs_*"
                : BASIC2D_VERTEX_HLSL ":fullscreen_viewfrustumvector:vs_*"
                ;
        char definesTable[256];
        Utility::XlFormatString(
            definesTable, dimof(definesTable), "USE_VOLUMETRIC_GRID=1;MSAA_SAMPLES=%i", 
            (desc._msaaSampleCount<=1)?0:desc._msaaSampleCount);
        if (desc._useMsaaSamplers)
            XlCatString(definesTable, dimof(definesTable), ";MSAA_SAMPLERS=1");

		UniformsStreamInterface resolveLightBindingUsi;
		resolveLightBindingUsi.BindConstantBuffer(0, {Hash64("VolumetricFogConstants")});
        resolveLightBindingUsi.BindShaderResource(0, Hash64("InscatterTexture"));
		resolveLightBindingUsi.BindShaderResource(1, Hash64("TransmissionTexture"));

        auto* resolveLight = &::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
            vertexShader, "xleres/VolumetricEffect/resolvefog.pixel.hlsl:ResolveFog:ps_*", definesTable);
        Metal::BoundUniforms resolveLightBinding(
			*resolveLight,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			resolveLightBindingUsi);

        auto* resolveLightNoGrid = &::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
            vertexShader, "xleres/VolumetricEffect/resolvefog.pixel.hlsl:ResolveFogNoGrid:ps_*", definesTable);
        Metal::BoundUniforms resolveLightNoGridBinding(
			*resolveLightNoGrid,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			resolveLightBindingUsi);

        ///////////////////////////////////////////////////////////////////////////////////////////
        auto validationCallback = ::Assets::GetDepValSys().Make();
        validationCallback.RegisterDependency(buildExponentialShadowMap->GetDependencyValidation());
        if (horizontalFilter) validationCallback.RegisterDependency(horizontalFilter->GetDependencyValidation());
        if (verticalFilter) validationCallback.RegisterDependency(verticalFilter->GetDependencyValidation());
        validationCallback.RegisterDependency(injectLight->GetDependencyValidation());
        validationCallback.RegisterDependency(propagateLight->GetDependencyValidation());
        validationCallback.RegisterDependency(resolveLight->GetDependencyValidation());

            //  Commit pointers
        _buildExponentialShadowMap = std::move(buildExponentialShadowMap);
        _horizontalFilter = std::move(horizontalFilter);
        _verticalFilter = std::move(verticalFilter);

        _buildExponentialShadowMapBinding = std::move(buildExponentialShadowMapBinding);
        _horizontalFilterBinding = std::move(horizontalFilterBinding);
        _verticalFilterBinding = std::move(verticalFilterBinding);

        _injectLight = injectLight;
        _propagateLight = propagateLight;
        _resolveLight = resolveLight;
        _resolveLightBinding = std::move(resolveLightBinding);
        _resolveLightNoGrid = resolveLightNoGrid;
        _resolveLightNoGridBinding = std::move(resolveLightNoGridBinding);
        _validationCallback = std::move(validationCallback);
        _injectLightBinding = std::move(injectLightBinding);
        _propagateLightBinding = std::move(propagateLightBinding);
        _filterWeightsConstants = std::move(filterWeightsConstants);
    }

    VolumetricFogShaders::~VolumetricFogShaders()
    {}

    ///////////////////////////////////////////////////////////////////////////////////////////////

    class VolumetricFogResources
    {
    public:
        class Desc
        {
        public:
            int         _frustumCount;
            bool        _esmShadowMaps;
            bool        _highPrecision;
            unsigned    _blurredShadowSize;
            UInt3       _gridDimensions;
            Desc(const VolumetricFogConfig::Renderer& cfg, unsigned frustumCount, bool esmShadowMaps, bool highPrecision) 
            {
                XlZeroMemory(*this);
                _frustumCount = std::min(cfg._maxShadowFrustums, frustumCount - cfg._skipShadowFrustums);
                _esmShadowMaps = esmShadowMaps;
                _blurredShadowSize = cfg._blurredShadowSize;
                _gridDimensions = cfg._gridDimensions;
                _highPrecision = highPrecision;
            }
        };

        typedef intrusive_ptr<BufferUploads::ResourceLocator> ResLocator;
        ResLocator                              _shadowMapTexture;
        std::vector<Metal::RenderTargetView>    _shadowMapRTVs;
        Metal::ShaderResourceView               _shadowMapSRV;

        ResLocator                              _shadowMapTextureTemp;
        std::vector<Metal::RenderTargetView>    _shadowMapTempRTVs;
        Metal::ShaderResourceView               _shadowMapTempSRV;

        ResLocator                      _densityValuesTexture;
        Metal::UnorderedAccessView      _densityValuesUAV;
        Metal::ShaderResourceView       _densityValuesSRV;

        ResLocator                      _inscatterShadowingValuesTexture;
        Metal::UnorderedAccessView      _inscatterShadowingValuesUAV;
        Metal::ShaderResourceView       _inscatterShadowingValuesSRV;

        ResLocator                      _inscatterPointLightsValuesTexture;
        Metal::UnorderedAccessView      _inscatterPointLightsValuesUAV;
        Metal::ShaderResourceView       _inscatterPointLightsValuesSRV;

        ResLocator                      _inscatterFinalsValuesTexture;
        Metal::UnorderedAccessView      _inscatterFinalsValuesUAV;
        Metal::ShaderResourceView       _inscatterFinalsValuesSRV;

        ResLocator                      _transmissionValuesTexture;
        Metal::UnorderedAccessView      _transmissionValuesUAV;
        Metal::ShaderResourceView       _transmissionValuesSRV;

        VolumetricFogResources(const Desc& desc);
        ~VolumetricFogResources();
    };

    static BufferUploads::BufferDesc SimGridDesc(RenderCore::Format format, UInt3 dims)
    {
        return BuildRenderTargetDesc(
            BufferUploads::BindFlag::UnorderedAccess|BufferUploads::BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain3D(
                dims[0], dims[1], dims[2],
                format), "VolFog");
    }

    VolumetricFogResources::VolumetricFogResources(const Desc& desc)
    {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        auto shadowMapFormat =  desc._highPrecision ? Format::R32_FLOAT : Format::R16_UNORM;
        auto renderTargetDesc = BuildRenderTargetDesc(
            BindFlag::RenderTarget|BindFlag::ShaderResource,
            TextureDesc::Plain2D(
                desc._blurredShadowSize, desc._blurredShadowSize, shadowMapFormat, 0, 
                uint8(desc._frustumCount)), "VolFog");
        auto& uploads = GetBufferUploads();

        auto shadowMapTexture = uploads.Transaction_Immediate(renderTargetDesc);
        auto shadowMapShaderResource = Metal::ShaderResourceView(shadowMapTexture->GetUnderlying());
        std::vector<Metal::RenderTargetView> shadowMapRenderTargets;
        for (unsigned c=0; c<unsigned(desc._frustumCount); ++c) {
			auto window = TextureViewDesc{
				shadowMapFormat,
				TextureViewDesc::SubResourceRange{c,1}};
            shadowMapRenderTargets.emplace_back(Metal::RenderTargetView(shadowMapTexture->GetUnderlying(), window));
        }

        auto shadowMapTextureTemp = uploads.Transaction_Immediate(renderTargetDesc);
        auto shadowMapShaderResourceTemp = Metal::ShaderResourceView(shadowMapTextureTemp->GetUnderlying());
        std::vector<Metal::RenderTargetView> shadowMapRenderTargetsTemp;
        for (unsigned c=0; c<unsigned(desc._frustumCount); ++c) {
			auto window = TextureViewDesc{
				shadowMapFormat,
				TextureViewDesc::SubResourceRange{ c,1 }};
            shadowMapRenderTargetsTemp.emplace_back(Metal::RenderTargetView(shadowMapTextureTemp->GetUnderlying(), window));
        }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        auto densityFormat = desc._highPrecision ? Format::R32_FLOAT : Format::R16_FLOAT;
        auto densityTexture = uploads.Transaction_Immediate(SimGridDesc(densityFormat, desc._gridDimensions));
        Metal::UnorderedAccessView densityUAV(densityTexture->GetUnderlying());
        Metal::ShaderResourceView densitySRV(densityTexture->GetUnderlying());

        auto shadowingFormat = desc._highPrecision ? Format::R32_FLOAT : Format::R8_UNORM;
        auto inscatterShadowingTexture = uploads.Transaction_Immediate(SimGridDesc(shadowingFormat, desc._gridDimensions));
        Metal::UnorderedAccessView inscatterShadowingUAV(inscatterShadowingTexture->GetUnderlying());
        Metal::ShaderResourceView inscatterShadowingSRV(inscatterShadowingTexture->GetUnderlying());
    
        auto transmissionFormat = desc._highPrecision ? Format::R32_FLOAT : Format::R16_FLOAT;
        auto transmissionTexture = uploads.Transaction_Immediate(SimGridDesc(transmissionFormat, desc._gridDimensions));
        Metal::UnorderedAccessView transmissionUAV(transmissionTexture->GetUnderlying());
        Metal::ShaderResourceView transmissionSRV(transmissionTexture->GetUnderlying());

        auto inScatterFormat =  desc._highPrecision ? Format::R32_FLOAT : Format::R16_FLOAT;
        auto inscatterFinalsTexture = uploads.Transaction_Immediate(SimGridDesc(inScatterFormat, desc._gridDimensions));
        Metal::UnorderedAccessView inscatterFinalsUAV(inscatterFinalsTexture->GetUnderlying());
        Metal::ShaderResourceView inscatterFinalsSRV(inscatterFinalsTexture->GetUnderlying());

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // auto inscatterPointLightsTexture = uploads.Transaction_Immediate(scatteringTextureDesc)->AdoptUnderlying();
        // Metal::UnorderedAccessView inscatterPointLightsUnorderedAccess(inscatterPointLightsTexture.get(), Format::R32G32B32A32_FLOAT);
        // Metal::ShaderResourceView inscatterPointLightsShaderResource(inscatterPointLightsTexture.get(), Format::R32G32B32A32_FLOAT);

        _densityValuesTexture = std::move(densityTexture);
        _densityValuesUAV = std::move(densityUAV);
        _densityValuesSRV = std::move(densitySRV);

        _inscatterShadowingValuesTexture = std::move(inscatterShadowingTexture);
        _inscatterShadowingValuesUAV = std::move(inscatterShadowingUAV);
        _inscatterShadowingValuesSRV = std::move(inscatterShadowingSRV);

        // _inscatterPointLightsValuesTexture = std::move(inscatterPointLightsTexture);
        // _inscatterPointLightsValuesUAV = std::move(inscatterPointLightsUnorderedAccess);
        // _inscatterPointLightsValuesSRV = std::move(inscatterPointLightsShaderResource);

        _inscatterFinalsValuesTexture = std::move(inscatterFinalsTexture);
        _inscatterFinalsValuesUAV = std::move(inscatterFinalsUAV);
        _inscatterFinalsValuesSRV = std::move(inscatterFinalsSRV);

        _transmissionValuesTexture = std::move(transmissionTexture);
        _transmissionValuesUAV = std::move(transmissionUAV);
        _transmissionValuesSRV = std::move(transmissionSRV);

        _shadowMapTexture = std::move(_shadowMapTexture);
        _shadowMapRTVs = std::move(shadowMapRenderTargets);
        _shadowMapSRV = std::move(shadowMapShaderResource);
        _shadowMapTextureTemp = std::move(_shadowMapTextureTemp);
        _shadowMapTempRTVs = std::move(shadowMapRenderTargetsTemp);
        _shadowMapTempSRV = std::move(shadowMapShaderResourceTemp);
    }

    VolumetricFogResources::~VolumetricFogResources() {}

    class VolumetricFogDensityTable
    {
    public:
        class Desc
        {
        public:
            float _density;
            Desc(float density) : _density(density) {}
        };

        intrusive_ptr<BufferUploads::ResourceLocator> _table;
        Metal::ShaderResourceView   _tableSRV;
        Metal::ConstantBuffer       _tableConstants;

        VolumetricFogDensityTable(const Desc& desc);
        ~VolumetricFogDensityTable();
    };

    float CalculateInscatter(float distance, float density)
    {
        float result = 0.f;
	    const unsigned stepCount = 256;
	    const float stepDistance = distance / float(stepCount);
	    float t = XlExp(-density * stepDistance);
	    for (uint c=0; c<stepCount; ++c) {
		    result += t * stepDistance * density;
		    t *= XlExp(-density * stepDistance);
	    }
	    return result;
    }

    VolumetricFogDensityTable::VolumetricFogDensityTable(const Desc& desc)
    {
            /////////////////////////////////////////////////////////////////////////////
            // Lookup table for calculating fog density
            // X coordinate is a distance value. Generally we want to balance this so
            //      the maximum distance is the point where transmission is very low
            // Y coordinate is a value for scaling the 
        const unsigned width = 256, height = 256;
        const auto bufferDesc = CreateDesc(
            BindFlag::ShaderResource,
            0, GPUAccess::Read, 
            TextureDesc::Plain2D(width, height, Format::R16_UNORM),
            "VolumetricFogLookupTable");

        auto& bufferUploads = GetBufferUploads();
        auto pkt = BufferUploads::CreateEmptyPacket(bufferDesc);

        const float maxDistance = 1024.f;
        float values[width][height];
        float maxValue = 0.f;
        for (unsigned y=0; y<height; ++y)
            for (unsigned x=0; x<width; ++x) {
                values[x][y] = CalculateInscatter(
                    x * maxDistance / float(width-1), 
                    desc._density * (y+1) / float(height));
                maxValue = std::max(maxValue, values[x][y]);
            }

        unsigned short* data = (unsigned short*)pkt->GetData({});
        for (unsigned y=0; y<height; ++y)
            for (unsigned x=0; x<width; ++x)
                data[y*256+x] = (unsigned short)(values[x][y] * float(0xffff) / maxValue);

        _table = bufferUploads.Transaction_Immediate(bufferDesc, pkt.get());
        _tableSRV = Metal::ShaderResourceView(_table->GetUnderlying());

        float constants[4] = { maxValue, maxDistance, 0.f, 0.f };
        _tableConstants = MakeMetalCB(constants, sizeof(constants));
    }

    VolumetricFogDensityTable::~VolumetricFogDensityTable() {}

    static void VolumetricFog_DrawDebugging(RenderCore::Metal::DeviceContext& context, Techniques::ParsingContext& parserContext, VolumetricFogResources& res);
    static bool UseESMShadowMaps() { return Tweakable("VolFogESM", false); }
    static unsigned GetShadowCascadeMode(PreparedShadowFrustum& shadowFrustum)
    {
        return (shadowFrustum._mode == ShadowProjectionMode::Ortho) ? 2u : 1u;
    }
    static unsigned GetShadowFilterMode()   { return Tweakable("VolFogFilter", 0); }
    static float GetShadowFilterStdDev()    { return Tweakable("VolFogStdDev", 2.2f); }
    static bool GetHighPrecisionResources() { return Tweakable("VolFogHighPrecision", false); }

    static RenderCore::Metal::ConstantBufferPacket MakeVolFogConstants(
        const VolumetricFogConfig::FogVolume& volume,
        const VolumetricFogConfig::Renderer& rendererCfg)
    {
        const auto& mat = volume._material;
        const float shadowDepthScale = 1.0f / 2048.f;   // (should be based on the far clip in the shadow projection)
        struct Constants
        {
            Float3 ForwardInscatter; unsigned _pad1;
            Float3 AmbientInscatter; unsigned _pad2;
            Float3 ReciprocalGridDimensions;
            float WorldSpaceGridDepth;

            float ESM_C;
            float ShadowsBias;
            float ShadowDepthScale;
            float JitteringAmount;
            float Density;
            float NoiseDensityScale;
            float NoiseSpeed;
            float HeightStart;
            float HeightEnd;
            unsigned dummy[3];
        } constants = {
            mat._sunInscatter, 0,
            mat._ambientInscatter, 0,
            Float3(1.0f / float(rendererCfg._gridDimensions[0]), 1.0f / float(rendererCfg._gridDimensions[1]), 1.0f / float(rendererCfg._gridDimensions[2])),
            rendererCfg._worldSpaceGridDepth,
            mat._ESM_C, mat._shadowsBias, shadowDepthScale,
            mat._jitteringAmount, mat._opticalThickness, 
            mat._noiseThicknessScale, mat._noiseSpeed,
            volume._heightStart, volume._heightEnd,
            {0,0,0}
        };

        return RenderCore::MakeSharedPkt(constants);
    }

    void VolumetricFog_Build(   RenderCore::IThreadContext& threadContext, 
                                Techniques::ParsingContext& parsingContext,
                                bool useMsaaSamplers, 
                                PreparedDMShadowFrustum& shadowFrustum,
                                const VolumetricFogConfig::Renderer& rendererCfg,
                                const VolumetricFogConfig::FogVolume& cfg)
    {
        CATCH_ASSETS_BEGIN

			auto& context = *RenderCore::Metal::DeviceContext::Get(threadContext);

            ///////////////////////////////////////////////////////////////////////////////////////

            auto& fogRes = ConsoleRig::FindCachedBox2<VolumetricFogResources>(
                rendererCfg, shadowFrustum._frustumCount, 
                UseESMShadowMaps(), GetHighPrecisionResources());
            auto& fogShaders = ConsoleRig::FindCachedBoxDep2<VolumetricFogShaders>(
                1, useMsaaSamplers, false, UseESMShadowMaps(), 
                cfg._material._noiseThicknessScale > 0.f,
                GetShadowCascadeMode(shadowFrustum),
                unsigned(fogRes._shadowMapRTVs.size()),
                rendererCfg._skipShadowFrustums, rendererCfg._gridDimensions[2],
                GetShadowFilterMode(), GetShadowFilterStdDev());

            ConstantBufferView constantBufferPackets2[2];
            constantBufferPackets2[0] = MakeVolFogConstants(cfg, rendererCfg);

            const unsigned blurredShadowSize = rendererCfg._blurredShadowSize;
            const unsigned blurDownsample = rendererCfg._shadowDownsample;
            unsigned downsampleScaleFactor = 
                unsigned(shadowFrustum._resolveParameters._shadowTextureSize) / (blurredShadowSize*blurDownsample);
            const auto gridDims = rendererCfg._gridDimensions;

            SavedTargets savedTargets(context);
            ViewportDesc newViewport(0, 0, float(blurredShadowSize), float(blurredShadowSize), 0.f, 1.f);
            context.Bind(newViewport);
            context.Bind(Techniques::CommonResources()._blendOpaque);
            SetupVertexGeneratorShader(context);
            context.Bind(*fogShaders._buildExponentialShadowMap);

            auto* shadowSRV = &shadowFrustum.GetSRV();
            assert(shadowSRV);
            context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(2, *shadowSRV));

            ///////////////////////////////////////////////////////////////////////////////////////

            unsigned c=0; 
            for (auto i=fogRes._shadowMapRTVs.begin(); i!=fogRes._shadowMapRTVs.end(); ++i, ++c) {
                struct WorkingSlice { unsigned _workingSlice; unsigned downsampleFactor; unsigned dummy[2]; }
                    globalsBuffer = { c + rendererCfg._skipShadowFrustums, downsampleScaleFactor, { 0u,0u } };
                constantBufferPackets2[1] = MakeSharedPkt(globalsBuffer);

                fogShaders._buildExponentialShadowMapBinding.Apply(context, 0, parsingContext.GetGlobalUniformsStream());
				fogShaders._buildExponentialShadowMapBinding.Apply(context, 1, UniformsStream{MakeIteratorRange(constantBufferPackets2)});

                context.Bind(MakeResourceList(*i), nullptr);
                context.Draw(4);
            }

            if (fogShaders._horizontalFilter && fogShaders._verticalFilter) {
                c = 0;
                auto i2 = fogRes._shadowMapTempRTVs.begin();
                for (auto i=fogRes._shadowMapRTVs.begin(); i!=fogRes._shadowMapRTVs.end(); ++i, ++c, ++i2) {
                    struct WorkingSlice { unsigned _workingSlice; unsigned dummy[3]; } globalsBuffer = { c, {0u,0u,0u} };
                    ConstantBufferView constantBufferPackets[2];
                    constantBufferPackets[0] = MakeSharedPkt(globalsBuffer);
                    constantBufferPackets[1] = &fogShaders._filterWeightsConstants;
                    fogShaders._horizontalFilterBinding.Apply(context, 0, parsingContext.GetGlobalUniformsStream());
					fogShaders._horizontalFilterBinding.Apply(context, 1, UniformsStream{MakeIteratorRange(constantBufferPackets)});

                    MetalStubs::UnbindPS<Metal::ShaderResourceView>(context, 0, 1);
                    context.Bind(MakeResourceList(*i2), nullptr);
                    context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(fogRes._shadowMapSRV));
                    context.Bind(*fogShaders._horizontalFilter);
                    context.Draw(4);

                    MetalStubs::UnbindPS<Metal::ShaderResourceView>(context, 0, 1);
                    context.Bind(MakeResourceList(*i), nullptr);
                    context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(fogRes._shadowMapTempSRV));
                    context.Bind(*fogShaders._verticalFilter);
                    context.Draw(4);
                }
            }

            ///////////////////////////////////////////////////////////////////////////////////////

            MetalStubs::UnbindPS<Metal::ShaderResourceView>(context, 0, 1);
            savedTargets.ResetToOldTargets(context);
            context.Bind(Techniques::CommonResources()._blendStraightAlpha);

            ///////////////////////////////////////////////////////////////////////////////////////

            auto& perlinNoiseRes = ConsoleRig::FindCachedBox<PerlinNoiseResources>(PerlinNoiseResources::Desc());
            context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));
            context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(9, 
                ::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>(
                    "xleres/DefaultResources/balanced_noise.dds:LT")->Actualize()->GetShaderResource()));
            context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(
                Techniques::CommonResources()._defaultSampler, 
                Techniques::CommonResources()._linearClampSampler));

                //  Inject the starting light into the volume texture (using compute shader)
            context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(
                fogRes._inscatterShadowingValuesUAV, fogRes._densityValuesUAV,
                fogRes._transmissionValuesUAV, fogRes._inscatterFinalsValuesUAV));
            context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(2, fogRes._shadowMapSRV));

            fogShaders._injectLightBinding.Apply(context, 0, parsingContext.GetGlobalUniformsStream());
			fogShaders._injectLightBinding.Apply(context, 1, UniformsStream{MakeIteratorRange(constantBufferPackets2)});
            context.Bind(*fogShaders._injectLight);
            context.Dispatch(gridDims[0]/10, gridDims[1]/10, gridDims[2]/8);

            MetalStubs::UnbindCS<Metal::UnorderedAccessView>(context, 0, 2);

                // do light propagation
            context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(3, fogRes._inscatterShadowingValuesSRV, fogRes._densityValuesSRV, fogRes._inscatterPointLightsValuesSRV));
            fogShaders._propagateLightBinding.Apply(context, 0, parsingContext.GetGlobalUniformsStream());
			fogShaders._propagateLightBinding.Apply(context, 1, UniformsStream{MakeIteratorRange(constantBufferPackets2)});
            context.Bind(*fogShaders._propagateLight);
            context.Dispatch(gridDims[0]/10, gridDims[1]/10, 1);

            MetalStubs::UnbindCS<Metal::UnorderedAccessView>(context, 0, 4);
            MetalStubs::UnbindCS<Metal::ShaderResourceView>(context, 2, 4);
            MetalStubs::UnbindCS<Metal::ShaderResourceView>(context, 13, 3);
            MetalStubs::UnbindComputeShader(context);

            ///////////////////////////////////////////////////////////////////////////////////////

            if (Tweakable("VolumetricFogDebugging", false)) {
                parsingContext._pendingOverlays.push_back(
                    std::bind(&VolumetricFog_DrawDebugging, std::placeholders::_1, std::placeholders::_2, std::ref(fogRes)));
            }
        CATCH_ASSETS_END(parsingContext)
    }

    void VolumetricFog_Resolve( RenderCore::IThreadContext& threadContext, 
                                Techniques::ParsingContext& parsingContext,
                                unsigned samplingCount, bool useMsaaSamplers, bool flipDirection,
                                PreparedShadowFrustum* shadowFrustum,
                                const VolumetricFogConfig::Renderer& rendererCfg,
                                const VolumetricFogConfig::FogVolume& cfg)
    {
        unsigned shadowCascadeMode = 2;
        unsigned shadowMapRTVs = 0;
        VolumetricFogResources* fogRes = nullptr;
        bool doShadows = shadowFrustum && (cfg._material._flags & unsigned(VolumetricFogMaterial::Flags::EnableShadows));
        if (doShadows) {
            fogRes = &ConsoleRig::FindCachedBox2<VolumetricFogResources>(
                rendererCfg, shadowFrustum->_frustumCount, 
                UseESMShadowMaps(), GetHighPrecisionResources());
            shadowCascadeMode = GetShadowCascadeMode(*shadowFrustum);
            shadowMapRTVs = unsigned(fogRes->_shadowMapRTVs.size());
        }

        auto& fogShaders = ConsoleRig::FindCachedBoxDep2<VolumetricFogShaders>(
            samplingCount, useMsaaSamplers, flipDirection, UseESMShadowMaps(), 
            cfg._material._noiseThicknessScale > 0.f,
            shadowCascadeMode,
            shadowMapRTVs,
            rendererCfg._skipShadowFrustums,
            rendererCfg._gridDimensions[2],
            GetShadowFilterMode(), GetShadowFilterStdDev());

        ConstantBufferView constantBufferPackets[2];
        constantBufferPackets[0] = MakeVolFogConstants(cfg, rendererCfg);

        // auto& fogTable = Techniques::FindCachedBox2<VolumetricFogDensityTable>(
        //     cfg._material._opticalThickness);

        const Metal::ShaderResourceView* srvs[] =
        {
            fogRes ? &fogRes->_inscatterFinalsValuesSRV : nullptr,
            fogRes ? &fogRes->_transmissionValuesSRV : nullptr
            //, &fogTable._tableSRV
        };
        // prebuiltConstants[1] = &fogTable._tableConstants;

		auto& metalContext = *RenderCore::Metal::DeviceContext::Get(threadContext);

        if (doShadows) {

            fogShaders._resolveLightBinding.Apply(metalContext, 0, parsingContext.GetGlobalUniformsStream());
			fogShaders._resolveLightBinding.Apply(metalContext, 1, 
                UniformsStream{
                    MakeIteratorRange(constantBufferPackets),
					UniformsStream::MakeResources(MakeIteratorRange(srvs))});
            metalContext.Bind(*fogShaders._resolveLight);

        } else {

            fogShaders._resolveLightNoGridBinding.Apply(metalContext, 0, parsingContext.GetGlobalUniformsStream());
			fogShaders._resolveLightNoGridBinding.Apply(metalContext, 1, 
                UniformsStream{
					MakeIteratorRange(constantBufferPackets),
					UniformsStream::MakeResources(MakeIteratorRange(srvs))});
            metalContext.Bind(*fogShaders._resolveLightNoGrid);

        }

        metalContext.Draw(4);
    }

        ///////////////////////////////////////////////////////////////////////////////////////////////

    VolumetricFogMaterial VolumetricFogMaterial_Default()
    {
        VolumetricFogMaterial result;
        result._opticalThickness = 1.f / 500.f;
        result._noiseThicknessScale = 0.f;
        result._noiseSpeed = 1.f;
        result._sunInscatter = 17.f * Float3(.7f, .6f, 1.f);
        result._ambientInscatter = 17.f * Float3(0.5f, 0.5f, .65f);
        result._ESM_C = .25f * 80.f;
        result._shadowsBias = 0.00000125f;
        result._jitteringAmount = 0.5f;
        result._flags |= (unsigned)VolumetricFogMaterial::Flags::EnableShadows;
        return result;
    }

    VolumetricFogConfig::FogVolume::FogVolume()
    : _material(VolumetricFogMaterial_Default())
    {
        _heightStart = 100.f;
        _heightEnd = 0.f;
        _center = Zero<Float3>();
        _radius = 100.f;
    }

    #define ParamName(x) static auto x = ParameterBox::MakeParameterNameHash(#x);

    VolumetricFogConfig::FogVolume::FogVolume(const ParameterBox& params)
    {
        ParamName(ReciprocalThickness);
        ParamName(NoiseThicknessScale);
        ParamName(NoiseSpeed);
        ParamName(SunInscatter);
        ParamName(SunInscatterScale);
        ParamName(AmbientInscatter);
        ParamName(AmbientInscatterScale);
        ParamName(ESM_C);
        ParamName(ShadowBias);
        ParamName(JitteringAmount);

        ParamName(HeightStart);
        ParamName(HeightEnd);
        ParamName(EnableShadows);

        _material._opticalThickness = 1.f / std::max(1e-5f, params.GetParameter(ReciprocalThickness, 1.f));
        _material._noiseThicknessScale = params.GetParameter(NoiseThicknessScale, _material._noiseThicknessScale);
        _material._noiseSpeed = params.GetParameter(NoiseSpeed, _material._noiseSpeed);

        _material._sunInscatter = 
            params.GetParameter(SunInscatterScale, 1.f)
            * AsFloat3Color(params.GetParameter(SunInscatter, int(-1)));
        _material._ambientInscatter = 
            params.GetParameter(AmbientInscatterScale, 1.f)
            * AsFloat3Color(params.GetParameter(AmbientInscatter, int(-1)));

        _material._ESM_C = params.GetParameter(ESM_C, _material._ESM_C);
        _material._shadowsBias = params.GetParameter(ShadowBias, _material._shadowsBias);
        _material._jitteringAmount = params.GetParameter(JitteringAmount, _material._jitteringAmount);
        _material._flags = 
            unsigned(VolumetricFogMaterial::Flags::EnableShadows) * params.GetParameter(EnableShadows, true);

        _heightStart = params.GetParameter(HeightStart, _heightStart);
        _heightEnd = params.GetParameter(HeightEnd, _heightEnd);
    }

    VolumetricFogConfig::Renderer::Renderer()
    {
        _blurredShadowSize = 256;
        _shadowDownsample = 4;
        _skipShadowFrustums = 1;
        _maxShadowFrustums = 3;
        _gridDimensions = UInt3(160, 90, 128);
        _worldSpaceGridDepth = 150.f;
        _enable = true;
    }

    VolumetricFogConfig::Renderer::Renderer(const ParameterBox& params)
    {
        ParamName(BlurredShadowSize);
        ParamName(ShadowDownsample);
        ParamName(SkipShadowFrustums);
        ParamName(MaxShadowFrustums);
        ParamName(GridDimensions);
        ParamName(WorldSpaceGridDepth);
        ParamName(Enable);

        _blurredShadowSize = params.GetParameter(BlurredShadowSize, _blurredShadowSize);
        _shadowDownsample = params.GetParameter(ShadowDownsample, _shadowDownsample);
        _skipShadowFrustums = params.GetParameter(SkipShadowFrustums, _skipShadowFrustums);
        _maxShadowFrustums = params.GetParameter(MaxShadowFrustums, _maxShadowFrustums);
        _gridDimensions = params.GetParameter(GridDimensions, _gridDimensions);
        _worldSpaceGridDepth = params.GetParameter(WorldSpaceGridDepth, _worldSpaceGridDepth);
        _enable = params.GetParameter(Enable, _enable);

        // grid dimensions has special rules. X & Y must be multiplies of 10. Z must be a multiple of 8
        _gridDimensions[0] = CeilToMultiple(_gridDimensions[0], 10);
        _gridDimensions[1] = CeilToMultiple(_gridDimensions[1], 10);
        _gridDimensions[2] = CeilToMultiple(_gridDimensions[2], 8);
    }

        ///////////////////////////////////////////////////////////////////////////////////////////////

    class VolumetricFogManager::Pimpl
    {
    public:
        std::shared_ptr<ILightingParserPlugin> _parserPlugin;
        VolumetricFogConfig _cfg;
    };

        ///////////////////////////////////////////////////////////////////////////////////////////////

    class VolumetricFogPlugin : public ILightingParserPlugin
    {
    public:
        virtual void OnPreScenePrepare(
            IThreadContext&, Techniques::ParsingContext&, LightingParserContext&) const;

        virtual void OnLightingResolvePrepare(
            IThreadContext&, Techniques::ParsingContext&, LightingParserContext&, 
			LightingResolveContext&) const;

        virtual void OnPostSceneRender(
            IThreadContext&, Techniques::ParsingContext&, LightingParserContext&, 
            Techniques::BatchFilter, unsigned techniqueIndex) const;

        virtual void InitBasicLightEnvironment(
            IThreadContext&, Techniques::ParsingContext&, LightingParserContext&, 
			ShaderLightDesc::BasicEnvironment& env) const;

        VolumetricFogPlugin(VolumetricFogManager::Pimpl& pimpl);
        ~VolumetricFogPlugin();

    protected:
        VolumetricFogManager::Pimpl* _pimpl;    // (unprotected to avoid a cyclic dependency... But could just be a pointer to the VolumetricFogConfig)
    };

    static void DoVolumetricFogResolve(
        IThreadContext& threadContext, Techniques::ParsingContext& parserContext, 
		LightingParserContext& lightingParserContext,
        LightingResolveContext& resolveContext, unsigned resolvePass,
        const VolumetricFogConfig::Renderer& rendererCfg,
        const VolumetricFogConfig::FogVolume& volume)
    {
        GPUAnnotation anno(threadContext, "VolFog");

        const auto useMsaaSamplers = resolveContext.UseMsaaSamplers();
        const auto samplingCount = resolveContext.GetSamplingCount();
        VolumetricFog_Resolve(
            threadContext, parserContext, 
            (resolvePass==0)?samplingCount:1, useMsaaSamplers, resolvePass==1,
            &lightingParserContext._preparedDMShadows[0].second, rendererCfg, volume);
    }

    static void DoVolumetricFogResolveNoGrid(
        IThreadContext& threadContext, Techniques::ParsingContext& parserContext, 
        LightingResolveContext& resolveContext, unsigned resolvePass,
        const VolumetricFogConfig::Renderer& rendererCfg,
        const VolumetricFogConfig::FogVolume& volume)
    {
        GPUAnnotation anno(threadContext, "VolFog");

        const auto useMsaaSamplers = resolveContext.UseMsaaSamplers();
        const auto samplingCount = resolveContext.GetSamplingCount();
        VolumetricFog_Resolve(
            threadContext, parserContext, 
            (resolvePass==0)?samplingCount:1, useMsaaSamplers, resolvePass==1,
            nullptr, rendererCfg, volume);
    }

    void VolumetricFogPlugin::OnLightingResolvePrepare(
        IThreadContext& threadContext, 
		Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext, 
        LightingResolveContext& resolveContext) const 
    {
        if (_pimpl->_cfg._volumes.empty() || _pimpl->_cfg._renderer._enable == false) return;

        const bool doVolumetricFog = Tweakable("DoVolumetricFog", true);
        if (!doVolumetricFog) return;

        ///////////////////////////////////////////////////////////////////////////////////////////
            // todo --  We should check which volumes are in range, and within
            //          the right portals / culling / etc.

            // Note that our prepare must occur after the shadows prepare
            //      (we're going to access the list of prepared shadows in 
            //      the parser context)
        ///////////////////////////////////////////////////////////////////////////////////////////

        using namespace std::placeholders;
        auto& cfg = _pimpl->_cfg._volumes[0];
        bool doShadows = 
                !lightingParserContext._preparedDMShadows.empty() && lightingParserContext._preparedDMShadows[0].second.IsReady() 
            && (cfg._material._flags & unsigned(VolumetricFogMaterial::Flags::EnableShadows));
        if (doShadows) {

            const auto useMsaaSamplers = resolveContext.UseMsaaSamplers();
            VolumetricFog_Build(
                threadContext, parsingContext, 
                useMsaaSamplers, lightingParserContext._preparedDMShadows[0].second,
                _pimpl->_cfg._renderer, cfg);

            resolveContext.AppendResolve(
                std::bind(DoVolumetricFogResolve, _1, _2, _3, _4, _5, 
                    std::ref(_pimpl->_cfg._renderer),
                    std::ref(cfg)));
        } else {

            resolveContext.AppendResolve(
                std::bind(DoVolumetricFogResolveNoGrid, _1, _2, _4, _5, 
                    std::ref(_pimpl->_cfg._renderer),
                    std::ref(cfg)));

        }
    }

    void VolumetricFogPlugin::OnPreScenePrepare(
        RenderCore::IThreadContext&, Techniques::ParsingContext&, LightingParserContext&) const {}
    
    void VolumetricFogPlugin::OnPostSceneRender(
        RenderCore::IThreadContext&, Techniques::ParsingContext&, LightingParserContext&, 
        Techniques::BatchFilter, unsigned techniqueIndex) const {}

    void VolumetricFogPlugin::InitBasicLightEnvironment(
        RenderCore::IThreadContext& metalContext,
        Techniques::ParsingContext&, LightingParserContext&, ShaderLightDesc::BasicEnvironment& env) const
    {
        if (_pimpl->_cfg._volumes.empty() || _pimpl->_cfg._renderer._enable == false) return;

        const bool doVolumetricFog = Tweakable("DoVolumetricFog", true);
        if (!doVolumetricFog) return;

        // Init the shader constants that will be used for forward light resolve shaders...
        const auto& vol = _pimpl->_cfg._volumes[0];
        env._volumeFog = ShaderLightDesc::VolumeFog {
            vol._material._opticalThickness, vol._heightStart, vol._heightEnd, 1u,
            vol._material._sunInscatter, 0u, 
            vol._material._ambientInscatter, 0u };

        #if 0
            // Bind the shader resources that will be required by forward lit resources
            auto& fogTable = Techniques::FindCachedBox2<VolumetricFogDensityTable>(
                vol._material._opticalThickness);
            metalContext.BindVS(MakeResourceList(13, fogTable._tableConstants));
            metalContext.BindVS(MakeResourceList(23, fogTable._tableSRV));
        #endif
    }

    VolumetricFogPlugin::VolumetricFogPlugin(VolumetricFogManager::Pimpl& pimpl) : _pimpl(&pimpl) {}
    VolumetricFogPlugin::~VolumetricFogPlugin() {}

        ///////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<ILightingParserPlugin> VolumetricFogManager::GetParserPlugin()  { return _pimpl->_parserPlugin; }
    void VolumetricFogManager::Load(const VolumetricFogConfig& cfg)                 { _pimpl->_cfg = cfg; }

    void VolumetricFogManager::AddVolume(const VolumetricFogConfig::FogVolume& volume)
    {
        _pimpl->_cfg._volumes.push_back(volume);
    }

    VolumetricFogManager::VolumetricFogManager()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_parserPlugin = std::make_shared<VolumetricFogPlugin>(*_pimpl.get());
    }

    VolumetricFogManager::~VolumetricFogManager() {}

        ///////////////////////////////////////////////////////////////////////////////////////////////

    void VolumetricFog_DrawDebugging(RenderCore::Metal::DeviceContext& context, Techniques::ParsingContext& parserContext, VolumetricFogResources& res)
    {
            // draw debugging for blurred shadows texture
        using namespace RenderCore;
        auto& debuggingShader = ::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
            BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
            "xleres/volumetriceffect/debugging.pixel.hlsl:VolumeShadows:ps_*",
            "");
        context.Bind(debuggingShader);
        context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(0, res._shadowMapSRV, res._inscatterFinalsValuesSRV, res._transmissionValuesSRV, res._inscatterShadowingValuesSRV, res._densityValuesSRV));
        context.Bind(Techniques::CommonResources()._blendStraightAlpha);
        SetupVertexGeneratorShader(context);
        context.Draw(4);
        MetalStubs::UnbindPS<RenderCore::Metal::ShaderResourceView>(context, 0, 1);
    }

        ///////////////////////////////////////////////////////////////////////////////////////////////

#if 0
    class AirLightResources
    {
    public:
        class Desc {};

        AirLightResources(const Desc& desc);
        ~AirLightResources();

        intrusive_ptr<ID3D::Resource>                          _lookupTexture;
        RenderCore::Metal::ShaderResourceView               _lookupShaderResource;
    };

    static float AirLightF(float u, float v)
    {
            //
            // see A Practical Analytic Single Scattering Model for Real Time Rendering
            //      paper from Columbia university
            //      equation 10.

        float A = 1.f; // XlExp(-u * XlTan(0.f));
        const unsigned steps = 256;
        const float stepWidth = v/float(steps);

        float result = 0.f;
        for (unsigned c=0; c<steps; ++c) {
            float e = (c+1)*stepWidth;
            float B = XlExp(-u * XlTan(e));
            result += (A + B) * .5f * stepWidth;
        }
        return result;
    }

    AirLightResources::AirLightResources(const Desc& desc)
    {
        const unsigned lookupTableDimensions = 256;
        float lookupTableData[lookupTableDimensions*lookupTableDimensions];
        const float uMax = 10.f;
        const float vMax = gPI * 0.5f;
        for (unsigned y=0; y<lookupTableDimensions; ++y) {
            for (unsigned x=0; x<lookupTableDimensions; ++x) {
                const float u = x*uMax/float(lookupTableDimensions-1);
                const float v = y*vMax/float(lookupTableDimensions-1);
            }
        }

        auto& uploads = *GetBufferUploads();
        auto lookupDesc = BuildRenderTargetDesc(BufferUploads::BindFlag::ShaderResource, BufferUploads::TextureDesc::Plain2D(lookupTableDimensions, lookupTableDimensions, NativeFormat::R32_TYPELESS));
        intrusive_ptr<ID3D::Resource> lookupTexture((ID3D::Resource*)uploads.Transaction_Immediate(
            lookupDesc, lookupTableData, lookupTableDimensions*lookupTableDimensions*sizeof(float), 
            std::make_pair(lookupTableDimensions*sizeof(float), lookupTableDimensions*lookupTableDimensions*sizeof(float)))._resource, false);
        ShaderResourceView lookupShaderResource(lookupTexture.get(), NativeFormat::R32_FLOAT);

        _lookupTexture = std::move(lookupTexture);
        _lookupShaderResource = std::move(lookupShaderResource);
    }

    AirLightResources::~AirLightResources()
    {}
#endif

}

