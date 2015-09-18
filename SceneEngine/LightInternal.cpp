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

namespace SceneEngine
{
    void BuildShadowConstantBuffers(
        CB_ArbitraryShadowProjection& arbitraryCBSource,
        CB_OrthoShadowProjection& orthoCBSource,
        const MultiProjection<MaxShadowTexturesPerLight>& desc)
    {
        auto frustumCount = desc._count;

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
                // const auto& mins = desc._orthoSub[c]._projMins;
                // const auto& maxs = desc._orthoSub[c]._projMaxs;
                // using namespace RenderCore::Techniques;
                // Float4x4 projMatrix = OrthogonalProjection(
                //     mins[0], mins[1], maxs[0], maxs[1], mins[2], maxs[2],
                //     GeometricCoordinateSpace::RightHanded, GetDefaultClipSpaceType());
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
        }
    }

    void PreparedShadowFrustum::InitialiseConstants(
        RenderCore::Metal::DeviceContext* devContext,
        const MultiProjection<MaxShadowTexturesPerLight>& desc)
    {
        _frustumCount = desc._count;
        _mode = desc._mode;

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

    PreparedDMShadowFrustum::PreparedDMShadowFrustum()
    {
        
    }

    PreparedDMShadowFrustum::PreparedDMShadowFrustum(PreparedDMShadowFrustum&& moveFrom) never_throws
    : PreparedShadowFrustum(std::move(moveFrom))
    , _shadowTextureSRV(std::move(moveFrom._shadowTextureSRV))
    , _resolveParameters(moveFrom._resolveParameters)
    {}

    PreparedDMShadowFrustum& PreparedDMShadowFrustum::operator=(PreparedDMShadowFrustum&& moveFrom) never_throws
    {
        PreparedShadowFrustum::operator=(std::move(moveFrom));
        _shadowTextureSRV = std::move(moveFrom._shadowTextureSRV);
        _resolveParameters = moveFrom._resolveParameters;
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
    {
        _skyTexture[0] = '\0';
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
        static const auto skyReflectionScaleHash = ParamHash("SkyReflectionScale");
        static const auto skyReflectionBlurriness = ParamHash("SkyReflectionBlurriness");
        static const auto skyBrightness = ParamHash("SkyBrightness");

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
    }

    ShadowProjectionDesc::ShadowProjectionDesc()
    {
        _width = _height = 0;
        _typelessFormat = _writeFormat = _readFormat = RenderCore::Metal::NativeFormat::Unknown;
        _worldToClip = Identity<Float4x4>();
        _shadowSlopeScaledBias = 0.f;
        _shadowDepthBiasClamp = 0.f;
        _shadowRasterDepthBias = 0;
        _worldSpaceResolveBias = 0.f;
        _tanBlurAngle = 0.f;
        _minBlurSearch = _maxBlurSearch = 0.f;
        _resolveType = ResolveType::DepthTexture;
        _lightId = ~0u;
    }

    LightDesc::LightDesc()
    {
        _type = Directional;
        _negativeLightDirection = Normalize(Float3(-.1f, 0.33f, 1.f));
        _radius = 10000.f;
        _diffuseColor = Float3(1.f, 1.f, 1.f);
        _specularColor = Float3(1.f, 1.f, 1.f);
        _nonMetalSpecularBrightness = 1.f;

        _diffuseWideningMin = 0.33f * 0.5f;
        _diffuseWideningMax = 0.33f * 2.5f;
        _diffuseModel = 1;

        _shadowResolveModel = 0;
    }

    LightDesc::LightDesc(const Utility::ParameterBox& props)
    {
        static const auto diffuseHash = ParameterBox::MakeParameterNameHash("Diffuse");
        static const auto diffuseBrightnessHash = ParameterBox::MakeParameterNameHash("DiffuseBrightness");
        static const auto diffuseModel = ParameterBox::MakeParameterNameHash("DiffuseModel");
        static const auto diffuseWideningMin = ParameterBox::MakeParameterNameHash("DiffuseWideningMin");
        static const auto diffuseWideningMax = ParameterBox::MakeParameterNameHash("DiffuseWideningMax");
        static const auto specularHash = ParameterBox::MakeParameterNameHash("Specular");
        static const auto specularBrightnessHash = ParameterBox::MakeParameterNameHash("SpecularBrightness");
        static const auto specularNonMetalBrightnessHash = ParameterBox::MakeParameterNameHash("SpecularNonMetalBrightness");
        static const auto shadowResolveModel = ParameterBox::MakeParameterNameHash("ShadowResolveModel");

        _type = LightDesc::Directional;
        _diffuseColor = props.GetParameter(diffuseBrightnessHash, 1.f) * AsFloat3Color(props.GetParameter(diffuseHash, ~0x0u));
        _specularColor = props.GetParameter(specularBrightnessHash, 1.f) * AsFloat3Color(props.GetParameter(specularHash, ~0x0u));
        _nonMetalSpecularBrightness = props.GetParameter(specularNonMetalBrightnessHash, 1.f);

        _diffuseWideningMin = props.GetParameter(diffuseWideningMin, 0.33f * 0.5f);
        _diffuseWideningMax = props.GetParameter(diffuseWideningMax, 0.33f * 2.5f);
        _diffuseModel = props.GetParameter(diffuseModel, 1);
                
        _shadowResolveModel = props.GetParameter(shadowResolveModel, 0);

        _radius = 10000.f;
    }

    RenderCore::SharedPkt BuildScreenToShadowConstants(
        unsigned frustumCount,
        const CB_ArbitraryShadowProjection& arbitraryCB,
        const CB_OrthoShadowProjection& orthoCB,
        const Float4x4& cameraToWorld)
    {
        struct Basis
        {
            Float4x4    _cameraToShadow[6];
            Float4x4    _orthoCameraToShadow;
        } basis;
        XlZeroMemory(basis);    // unused array elements must be zeroed out

        for (unsigned c=0; c<unsigned(frustumCount); ++c) {
            auto& worldToShadowProj = arbitraryCB._worldToProj[c];
            auto cameraToShadowProj = Combine(cameraToWorld, worldToShadowProj);
            basis._cameraToShadow[c] = cameraToShadowProj;
        }
        auto& worldToShadowProj = orthoCB._worldToProj;
        basis._orthoCameraToShadow = Combine(cameraToWorld, worldToShadowProj);
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
        const PreparedShadowFrustum& preparedFrustum, const Float4x4& cameraToWorld)
    {
        return BuildScreenToShadowConstants(
            preparedFrustum._frustumCount, preparedFrustum._arbitraryCBSource, preparedFrustum._orthoCBSource,
            cameraToWorld);
    }


}

