// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightInternal.h"
#include "LightDesc.h"
#include "SceneEngineUtils.h"
#include "../RenderCore/RenderUtils.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/Streams/PathUtils.h"

namespace SceneEngine
{
    void BuildShadowConstantBuffers(
        CB_ArbitraryShadowProjection& arbitraryCBSource,
        CB_OrthoShadowProjection& orthoCBSource,
        const MultiProjection<MaxShadowTexturesPerLight>& desc)
    {
        auto frustumCount = desc._normalProjCount;

        XlZeroMemory(arbitraryCBSource);   // unused array elements must be zeroed out
        XlZeroMemory(orthoCBSource);       // unused array elements must be zeroed out

        typedef MultiProjection<MaxShadowTexturesPerLight>::Mode Mode;
        if (desc._mode == Mode::Arbitrary) {

            arbitraryCBSource._projectionCount = frustumCount;
            for (unsigned c=0; c<frustumCount; ++c) {
                arbitraryCBSource._worldToProj[c] = 
                    Combine(  
                        desc._fullProj[c]._viewMatrix, 
                        desc._fullProj[c]._projectionMatrix);
                arbitraryCBSource._minimalProj[c] = desc._minimalProjection[c];
            }

        } else if (desc._mode == Mode::Ortho) {

            using namespace RenderCore;

                //  We can still fill in the constant buffer
                //  for arbitrary projection. If we use the right
                //  shader, we shouldn't need this. But we can have it
                //  prepared for comparisons.
            arbitraryCBSource._projectionCount = frustumCount;
            auto baseWorldToProj = desc._definitionViewMatrix;

            float p22 = 1.f, p23 = 0.f;

            for (unsigned c=0; c<frustumCount; ++c) {

                    // we don't really need to rebuild the projection
                    // matrix here. It should already be calcated in 
                    // _fullProj._projectionMatrix
                const auto& projMatrix = desc._fullProj[c]._projectionMatrix;
                assert(IsOrthogonalProjection(projMatrix));

                arbitraryCBSource._worldToProj[c] = Combine(baseWorldToProj, projMatrix);
                arbitraryCBSource._minimalProj[c] = ExtractMinimalProjection(projMatrix);

                orthoCBSource._cascadeScale[c][0] = projMatrix(0,0);
                orthoCBSource._cascadeScale[c][1] = projMatrix(1,1);
                orthoCBSource._cascadeTrans[c][0] = projMatrix(0,3);
                orthoCBSource._cascadeTrans[c][1] = projMatrix(1,3);

                if (c==0) {
                    p22 = projMatrix(2,2);
                    p23 = projMatrix(2,3);
                }

                    // (unused parts)
                orthoCBSource._cascadeScale[c][2] = 1.f;
                orthoCBSource._cascadeTrans[c][2] = 0.f;
                orthoCBSource._cascadeScale[c][3] = 1.f;
                orthoCBSource._cascadeTrans[c][3] = 0.f;
            }

                // prevent matches on the remaining frustums...
            for (unsigned c=frustumCount; c<MaxShadowTexturesPerLight; ++c) {
                orthoCBSource._cascadeTrans[c][0] = FLT_MAX;
                orthoCBSource._cascadeTrans[c][1] = FLT_MAX;
            }

                //  Also fill in the constants for ortho projection mode
            orthoCBSource._projectionCount = frustumCount;
            orthoCBSource._minimalProjection = desc._minimalProjection[0];

                //  We merge in the transform for the z component
                //  Every cascade uses the same depth range, which means we only
                //  have to adjust the X and Y components for each cascade
            auto zComponentMerge = Identity<Float4x4>();
            zComponentMerge(2,2) = p22;
            zComponentMerge(2,3) = p23;
            orthoCBSource._worldToProj = AsFloat3x4(Combine(baseWorldToProj, zComponentMerge));

                // the special "near" cascade is reached via the main transform
            orthoCBSource._nearCascade = 
                AsFloat3x4(Combine(
                    Inverse(AsFloat4x4(orthoCBSource._worldToProj)), 
                    desc._specialNearProjection));
            orthoCBSource._nearMinimalProjection = desc._specialNearMinimalProjection;
        }
    }

    void PreparedShadowFrustum::InitialiseConstants(
        RenderCore::Metal::DeviceContext* devContext,
        const MultiProjection<MaxShadowTexturesPerLight>& desc)
    {
        _frustumCount = desc._normalProjCount;
        _mode = desc._mode;
        _enableNearCascade = desc._useNearProj;

        BuildShadowConstantBuffers(_arbitraryCBSource, _orthoCBSource, desc);

        using RenderCore::Metal::ConstantBuffer;
            // arbitrary constants
        if (!_arbitraryCB.GetUnderlying()) {
            _arbitraryCB = ConstantBuffer(&_arbitraryCBSource, sizeof(_arbitraryCBSource));
        } else {
            _arbitraryCB.Update(*devContext, &_arbitraryCBSource, sizeof(_arbitraryCBSource));
        }

            // ortho constants
        if (!_orthoCB.GetUnderlying()) {
            _orthoCB = ConstantBuffer(&_orthoCBSource, sizeof(_orthoCBSource));
        } else {
            _orthoCB.Update(*devContext, &_orthoCBSource, sizeof(_orthoCBSource));
        }
    }

    PreparedShadowFrustum::PreparedShadowFrustum()
    : _frustumCount(0) 
    , _mode(ShadowProjectionDesc::Projections::Mode::Arbitrary)
    {}

    PreparedShadowFrustum::PreparedShadowFrustum(PreparedShadowFrustum&& moveFrom) never_throws
    : _arbitraryCBSource(std::move(moveFrom._arbitraryCBSource))
    , _orthoCBSource(std::move(moveFrom._orthoCBSource))
    , _arbitraryCB(std::move(moveFrom._arbitraryCB))
    , _orthoCB(std::move(moveFrom._orthoCB))
    , _frustumCount(moveFrom._frustumCount)
    , _mode(moveFrom._mode)
    {}

    PreparedShadowFrustum& PreparedShadowFrustum::operator=(PreparedShadowFrustum&& moveFrom) never_throws
    {
        _arbitraryCBSource = std::move(moveFrom._arbitraryCBSource);
        _orthoCBSource = std::move(moveFrom._orthoCBSource);
        _arbitraryCB = std::move(moveFrom._arbitraryCB);
        _orthoCB = std::move(moveFrom._orthoCB);
        _frustumCount = moveFrom._frustumCount;
        _mode = moveFrom._mode;
        return *this;
    }

    bool PreparedDMShadowFrustum::IsReady() const
    {
        return _shadowTextureSRV.GetUnderlying() && (_arbitraryCB.GetUnderlying() || _orthoCB.GetUnderlying());
    }

    PreparedDMShadowFrustum::PreparedDMShadowFrustum() {}

    PreparedDMShadowFrustum::PreparedDMShadowFrustum(PreparedDMShadowFrustum&& moveFrom) never_throws
    : PreparedShadowFrustum(std::move(moveFrom))
    , _shadowTextureSRV(std::move(moveFrom._shadowTextureSRV))
    , _resolveParameters(moveFrom._resolveParameters)
    , _resolveParametersCB(std::move(moveFrom._resolveParametersCB))
    {}

    PreparedDMShadowFrustum& PreparedDMShadowFrustum::operator=(PreparedDMShadowFrustum&& moveFrom) never_throws
    {
        PreparedShadowFrustum::operator=(std::move(moveFrom));
        _shadowTextureSRV = std::move(moveFrom._shadowTextureSRV);
        _resolveParameters = moveFrom._resolveParameters;
        _resolveParametersCB = std::move(moveFrom._resolveParametersCB);
        return *this;
    }


    bool PreparedRTShadowFrustum::IsReady() const
    {
        return _listHeadSRV.GetUnderlying() && _linkedListsSRV.GetUnderlying() && _trianglesSRV.GetUnderlying();
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
    , _rangeFogThickness(0.f, 0.f, 0.f)
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
        static const auto rangeFogInscatterScaleHash = ParamHash("RangeFogInscatterScale");
        static const auto rangeFogThicknessHash = ParamHash("RangeFogThickness");
        static const auto rangeFogThicknessScaleHash = ParamHash("RangeFogThicknessScale");

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

        _rangeFogInscatter = 1.f / std::max(1.f, props.GetParameter(rangeFogInscatterScaleHash, 1.f)) * AsFloat3Color(props.GetParameter(rangeFogInscatterHash, 0));
        _rangeFogThickness = 1.f / std::max(1.f, props.GetParameter(rangeFogThicknessScaleHash, 1.f)) * AsFloat3Color(props.GetParameter(rangeFogThicknessHash, 0));

        _atmosBlurStdDev = props.GetParameter(atmosBlurStdDevHash, _atmosBlurStdDev);
        _atmosBlurStart = props.GetParameter(atmosBlurStartHash, _atmosBlurStart);
        _atmosBlurEnd = std::max(_atmosBlurStart, props.GetParameter(atmosBlurEndHash, _atmosBlurEnd));

        auto flags = props.GetParameter<unsigned>(flagsHash);
        if (flags.first) {
            _doAtmosphereBlur = !!(flags.second & (1<<0));
            _doRangeFog = !!(flags.second & (1<<1));
        }

        props.GetString(skyTextureHash, _skyTexture, dimof(_skyTexture));
        props.GetString(diffuseIBLHash, _diffuseIBL, dimof(_diffuseIBL));
        props.GetString(specularIBLHash, _specularIBL, dimof(_specularIBL));

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

    ShadowProjectionDesc::ShadowProjectionDesc()
    {
        _width = _height = 0;
        _typelessFormat = _writeFormat = _readFormat = RenderCore::Metal::NativeFormat::Unknown;
        _worldToClip = Identity<Float4x4>();
        _slopeScaledBias = 0.f;
        _depthBiasClamp = 0.f;
        _rasterDepthBias = 0;
        _dsSlopeScaledBias = 0.f;
        _dsDepthBiasClamp = 0.f;
        _dsRasterDepthBias = 0;
        _worldSpaceResolveBias = 0.f;
        _tanBlurAngle = 0.f;
        _minBlurSearch = _maxBlurSearch = 0.f;
        _resolveType = ResolveType::DepthTexture;
        _windingCull = WindingCull::BackFaces;
        _lightId = ~0u;
    }

    LightDesc::LightDesc()
    {
        _shape = Directional;
        _position = Normalize(Float3(-.1f, 0.33f, 1.f));
        _orientation = Identity<Float3x3>();
        _cutoffRange = 10000.f;
        _radii = Float2(1.f, 1.f);
        _diffuseColor = Float3(1.f, 1.f, 1.f);
        _specularColor = Float3(1.f, 1.f, 1.f);

        _diffuseWideningMin = 0.5f;
        _diffuseWideningMax = 2.5f;
        _diffuseModel = 1;

        _shadowResolveModel = 0;
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
        basis._xyScale[1] = -2.f / cameraToProjection(1,1);
        basis._xyTrans[0] = -1.f / cameraToProjection(0,0) + cameraToProjection(0,2) / cameraToProjection(0,0);
        basis._xyTrans[1] =  1.f / cameraToProjection(1,1) + cameraToProjection(1,2) / cameraToProjection(1,1);

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

    CB_ShadowResolveParameters::CB_ShadowResolveParameters()
    {
        _worldSpaceBias = -0.03f;
        _tanBlurAngle = 0.00436f;		// tan(.25 degrees)
        _minBlurSearch = 0.5f;
        _maxBlurSearch = 25.f;
        _shadowTextureSize = 1024.f;
        XlZeroMemory(_dummy);
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


}

