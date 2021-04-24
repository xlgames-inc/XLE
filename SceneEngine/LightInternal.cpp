// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightInternal.h"
#include "LightDesc.h"
// #include "SceneEngineUtils.h"
#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/Streams/PathUtils.h"

namespace SceneEngine
{



    bool PreparedRTShadowFrustum::IsReady() const
    {
        assert(0);
        return false;
        // return _listHeadSRV.IsGood() && _linkedListsSRV.IsGood() && _trianglesSRV.IsGood();
    }

    PreparedRTShadowFrustum::PreparedRTShadowFrustum() {}

    PreparedRTShadowFrustum::PreparedRTShadowFrustum(PreparedRTShadowFrustum&& moveFrom) never_throws
    : PreparedShadowFrustum(std::move(moveFrom))
    , _listHeadSRV(std::move(moveFrom._listHeadSRV))
    , _linkedListsSRV(std::move(moveFrom._linkedListsSRV))
    , _trianglesSRV(std::move(moveFrom._trianglesSRV))
    {
    }

    PreparedRTShadowFrustum& PreparedRTShadowFrustum::operator=(PreparedRTShadowFrustum&& moveFrom) never_throws
    {
        PreparedShadowFrustum::operator=(std::move(moveFrom));
        _listHeadSRV = std::move(moveFrom._listHeadSRV);
        _linkedListsSRV = std::move(moveFrom._linkedListsSRV);
        _trianglesSRV = std::move(moveFrom._trianglesSRV);
        return *this;
    }

    GlobalLightingDesc::GlobalLightingDesc() 
    : _ambientLight(0.f, 0.f, 0.f), _skyReflectionScale(1.0f), _skyReflectionBlurriness(2.f), _skyBrightness(1.f)
    , _doAtmosphereBlur(false)
    , _rangeFogInscatter(0.f, 0.f, 0.f)
    , _rangeFogThickness(10000.f)
    , _doRangeFog(false)
    , _atmosBlurStdDev(1.3f), _atmosBlurStart(1000.f), _atmosBlurEnd(1500.f)
    , _skyTextureType(SkyTextureType::Equirectangular)
    {
        _skyTexture[0] = '\0';
        _diffuseIBL[0] = '\0';
        _specularIBL[0] = '\0';
    }

    static ParameterBox::ParameterNameHash ParamHash(const char name[])
    {
        return ParameterBox::MakeParameterNameHash(name);
    }

    static Float3 AsFloat3Color(unsigned packedColor)
    {
        return Float3(
            (float)((packedColor >> 16) & 0xff) / 255.f,
            (float)((packedColor >>  8) & 0xff) / 255.f,
            (float)(packedColor & 0xff) / 255.f);
    }

    GlobalLightingDesc::GlobalLightingDesc(const ParameterBox& props)
    : GlobalLightingDesc()
    {
        static const auto ambientHash = ParamHash("AmbientLight");
        static const auto ambientBrightnessHash = ParamHash("AmbientBrightness");

        static const auto skyTextureHash = ParamHash("SkyTexture");
        static const auto skyTextureTypeHash = ParamHash("SkyTextureType");
        static const auto skyReflectionScaleHash = ParamHash("SkyReflectionScale");
        static const auto skyReflectionBlurriness = ParamHash("SkyReflectionBlurriness");
        static const auto skyBrightness = ParamHash("SkyBrightness");
        static const auto diffuseIBLHash = ParamHash("DiffuseIBL");
        static const auto specularIBLHash = ParamHash("SpecularIBL");

        static const auto rangeFogInscatterHash = ParamHash("RangeFogInscatter");
        static const auto rangeFogInscatterReciprocalScaleHash = ParamHash("RangeFogInscatterReciprocalScale");
        static const auto rangeFogInscatterScaleHash = ParamHash("RangeFogInscatterScale");
        static const auto rangeFogThicknessReciprocalScaleHash = ParamHash("RangeFogThicknessReciprocalScale");

        static const auto atmosBlurStdDevHash = ParamHash("AtmosBlurStdDev");
        static const auto atmosBlurStartHash = ParamHash("AtmosBlurStart");
        static const auto atmosBlurEndHash = ParamHash("AtmosBlurEnd");

        static const auto flagsHash = ParamHash("Flags");

            ////////////////////////////////////////////////////////////

        _ambientLight = props.GetParameter(ambientBrightnessHash, 1.f) * AsFloat3Color(props.GetParameter(ambientHash, ~0x0u));
        _skyReflectionScale = props.GetParameter(skyReflectionScaleHash, _skyReflectionScale);
        _skyReflectionBlurriness = props.GetParameter(skyReflectionBlurriness, _skyReflectionBlurriness);
        _skyBrightness = props.GetParameter(skyBrightness, _skyBrightness);
        _skyTextureType = (SkyTextureType)props.GetParameter(skyTextureTypeHash, unsigned(_skyTextureType));

        float inscatterScaleScale = 1.f / std::max(1e-5f, props.GetParameter(rangeFogInscatterReciprocalScaleHash, 1.f));
        inscatterScaleScale = props.GetParameter(rangeFogInscatterScaleHash, inscatterScaleScale);
        _rangeFogInscatter = inscatterScaleScale * AsFloat3Color(props.GetParameter(rangeFogInscatterHash, 0));
        _rangeFogThickness = 1.f / std::max(1e-5f, props.GetParameter(rangeFogThicknessReciprocalScaleHash, 0.f));

        _atmosBlurStdDev = props.GetParameter(atmosBlurStdDevHash, _atmosBlurStdDev);
        _atmosBlurStart = props.GetParameter(atmosBlurStartHash, _atmosBlurStart);
        _atmosBlurEnd = std::max(_atmosBlurStart, props.GetParameter(atmosBlurEndHash, _atmosBlurEnd));

        auto flags = props.GetParameter<unsigned>(flagsHash);
        if (flags.has_value()) {
            _doAtmosphereBlur = !!(flags.value() & (1<<0));
            _doRangeFog = !!(flags.value() & (1<<1));
        }

        auto skyTexture = props.GetParameterAsString(skyTextureHash);
        if (skyTexture.has_value())
            XlCopyString(_skyTexture, skyTexture.value());
        auto diffuseIBL = props.GetParameterAsString(diffuseIBLHash);
        if (diffuseIBL.has_value())
            XlCopyString(_diffuseIBL, diffuseIBL.value());
        auto specularIBL = props.GetParameterAsString(specularIBLHash);
        if (specularIBL.has_value())
            XlCopyString(_specularIBL, specularIBL.value());

		// If we don't have a diffuse IBL texture, or specular IBL texture, then attempt to build
		// the filename from the sky texture
		if ((!_diffuseIBL[0] || !_specularIBL[0]) && _skyTexture[0]) {
			auto splitter = MakeFileNameSplitter(_skyTexture);

			if (!_diffuseIBL[0]) {
				XlCopyString(_diffuseIBL, MakeStringSection(splitter.DriveAndPath().begin(), splitter.File().end()));
				XlCatString(_diffuseIBL, "_diffuse");
				XlCatString(_diffuseIBL, splitter.ExtensionWithPeriod());
			}

			if (!_specularIBL[0]) {
				XlCopyString(_specularIBL, MakeStringSection(splitter.DriveAndPath().begin(), splitter.File().end()));
				XlCatString(_specularIBL, "_specular");
				XlCatString(_specularIBL, splitter.ExtensionWithPeriod());
			}
		}
    }

    LightDesc::LightDesc(const Utility::ParameterBox& props)
    : LightDesc()
    {
        static const auto diffuseHash = ParameterBox::MakeParameterNameHash("Diffuse");
        static const auto diffuseBrightnessHash = ParameterBox::MakeParameterNameHash("DiffuseBrightness");
        static const auto diffuseModel = ParameterBox::MakeParameterNameHash("DiffuseModel");
        static const auto diffuseWideningMin = ParameterBox::MakeParameterNameHash("DiffuseWideningMin");
        static const auto diffuseWideningMax = ParameterBox::MakeParameterNameHash("DiffuseWideningMax");
        static const auto specularHash = ParameterBox::MakeParameterNameHash("Specular");
        static const auto specularBrightnessHash = ParameterBox::MakeParameterNameHash("SpecularBrightness");
        static const auto shadowResolveModel = ParameterBox::MakeParameterNameHash("ShadowResolveModel");
        static const auto cutoffRange = ParameterBox::MakeParameterNameHash("CutoffRange");
        static const auto shape = ParameterBox::MakeParameterNameHash("Shape");

        _diffuseColor = props.GetParameter(diffuseBrightnessHash, 1.f) * AsFloat3Color(props.GetParameter(diffuseHash, ~0x0u));
        _specularColor = props.GetParameter(specularBrightnessHash, 1.f) * AsFloat3Color(props.GetParameter(specularHash, ~0x0u));

        _diffuseWideningMin = props.GetParameter(diffuseWideningMin, _diffuseWideningMin);
        _diffuseWideningMax = props.GetParameter(diffuseWideningMax, _diffuseWideningMax);
        _diffuseModel = props.GetParameter(diffuseModel, 1);
        _cutoffRange = props.GetParameter(cutoffRange, _cutoffRange);

        _shape = (Shape)props.GetParameter(shape, unsigned(_shape));

        _shadowResolveModel = props.GetParameter(shadowResolveModel, 0);
    }

    RenderCore::SharedPkt BuildScreenToShadowConstants(
        unsigned frustumCount,
        const CB_ArbitraryShadowProjection& arbitraryCB,
        const CB_OrthoShadowProjection& orthoCB,
        const Float4x4& cameraToWorld,
        const Float4x4& cameraToProjection)
    {
        struct Basis
        {
            Float4x4    _cameraToShadow[6];
            Float4x4    _orthoCameraToShadow;
            Float2      _xyScale;
            Float2      _xyTrans;
            Float4x4    _orthoNearCameraToShadow;
        } basis;
        XlZeroMemory(basis);    // unused array elements must be zeroed out

            // The input coordinates are texture coordinate ranging from 0->1 across the viewport.
            // So, we must take into account X/Y scale and translation factors in the projection matrix.
            // Typically, this is just aspect ratio.
            // But if we have an unusual projection matrix (for example, when rendering tiles), then
            // we can also have a translation component in the projection matrix.
            // We can't incorporate this viewport/projection matrix scaling stuff into the main
            // cameraToShadow matrix because of the wierd way we transform through with this matrix!
            // So we have separate scale and translation values that are applied to the XY coordinates
            // of the inputs before transform
        #if 0
            auto projInverse = Inverse(cameraToProjection);
            auto viewportCorrection = MakeFloat4x4(
                 2.f,  0.f, 0.f, -1.f,
                 0.f, -2.f, 0.f, +1.f,
                 0.f,  0.f, 1.f,  0.f,
                 0.f,  0.f, 0.f,  1.f);
            auto temp = Combine(viewportCorrection, projInverse);
            basis._xyScale = Float2(temp(0,0), temp(1,1));
            basis._xyTrans = Float2(temp(0,3), temp(1,3));
        #endif

        basis._xyScale[0] =  2.f / cameraToProjection(0,0);
        basis._xyTrans[0] = -1.f / cameraToProjection(0,0) + cameraToProjection(0,2) / cameraToProjection(0,0);

        if (RenderCore::Techniques::GetDefaultClipSpaceType() == ClipSpaceType::PositiveRightHanded) {
            basis._xyScale[1] =  2.f / cameraToProjection(1,1);
            basis._xyTrans[1] = -1.f / cameraToProjection(1,1) + cameraToProjection(1,2) / cameraToProjection(1,1);
        } else {
            basis._xyScale[1] = -2.f / cameraToProjection(1,1);
            basis._xyTrans[1] =  1.f / cameraToProjection(1,1) + cameraToProjection(1,2) / cameraToProjection(1,1);
        }

        for (unsigned c=0; c<unsigned(frustumCount); ++c) {
            auto& worldToShadowProj = arbitraryCB._worldToProj[c];
            auto cameraToShadowProj = Combine(cameraToWorld, worldToShadowProj);
            basis._cameraToShadow[c] = cameraToShadowProj;
        }
        auto& worldToShadowProj = orthoCB._worldToProj;
        basis._orthoCameraToShadow = Combine(cameraToWorld, worldToShadowProj);
        basis._orthoNearCameraToShadow = Combine(basis._orthoCameraToShadow, orthoCB._nearCascade);
        return RenderCore::MakeSharedPkt(basis);
    }

    RenderCore::SharedPkt BuildScreenToShadowConstants(
        const PreparedShadowFrustum& preparedFrustum, 
        const Float4x4& cameraToWorld, 
        const Float4x4& cameraToProjection)
    {
        return BuildScreenToShadowConstants(
            preparedFrustum._frustumCount, preparedFrustum._arbitraryCBSource, preparedFrustum._orthoCBSource,
            cameraToWorld, cameraToProjection);
    }

	template<int BitCount, typename Input>
		static uint64_t GetBits(Input i)
	{
		auto mask = (1ull<<uint64_t(BitCount))-1ull;
		assert((uint64_t(i) & ~mask) == 0);
		return uint64_t(i) & mask;
	}

	inline uint32_t FloatBits(float i) { return *(uint32_t*)&i; }

	uint64_t Hash(const ShadowGeneratorDesc& shadowGeneratorDesc)
	{
		uint64_t h0 = 
			  (GetBits<12>(shadowGeneratorDesc._width)			<< 0ull)
			| (GetBits<12>(shadowGeneratorDesc._height)			<< 12ull)
			| (GetBits<8>(shadowGeneratorDesc._format)			<< 24ull)
			| (GetBits<4>(shadowGeneratorDesc._arrayCount)		<< 32ull)
			| (GetBits<4>(shadowGeneratorDesc._projectionMode)	<< 36ull)
			| (GetBits<4>(shadowGeneratorDesc._cullMode)		<< 40ull)
			| (GetBits<4>(shadowGeneratorDesc._resolveType)		<< 44ull)
			| (GetBits<1>(shadowGeneratorDesc._enableNearCascade)  << 48ull)
			;

		uint64_t h1 = 
				uint64_t(FloatBits(shadowGeneratorDesc._slopeScaledBias))
			|  (uint64_t(FloatBits(shadowGeneratorDesc._depthBiasClamp)) << 32ull)
			;

		uint64_t h2 = 
				uint64_t(FloatBits(shadowGeneratorDesc._dsSlopeScaledBias))
			|  (uint64_t(FloatBits(shadowGeneratorDesc._dsDepthBiasClamp)) << 32ull)
			;

		uint64_t h3 = 
				uint64_t(shadowGeneratorDesc._rasterDepthBias)
			|  (uint64_t(shadowGeneratorDesc._dsRasterDepthBias) << 32ull)
			;

		return HashCombine(h0, HashCombine(h1, HashCombine(h2, h3)));
	}


}

