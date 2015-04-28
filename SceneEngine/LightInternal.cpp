#include "LightInternal.h"
#include "LightDesc.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
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
                const auto& mins = desc._orthoSub[c]._projMins;
                const auto& maxs = desc._orthoSub[c]._projMaxs;
                using namespace RenderCore::Techniques;
                Float4x4 projMatrix = OrthogonalProjection(
                    mins[0], mins[1], maxs[0], maxs[1], mins[2], maxs[2],
                    GeometricCoordinateSpace::RightHanded, GetDefaultClipSpaceType());

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

    bool PreparedShadowFrustum::IsReady() const
    {
        return _shadowTextureResource.GetUnderlying() && (_arbitraryCB.GetUnderlying() || _orthoCB.GetUnderlying());
    }

    PreparedShadowFrustum::PreparedShadowFrustum()
    : _frustumCount(0) 
    , _mode(ShadowProjectionDesc::Projections::Mode::Arbitrary)
    {}

    PreparedShadowFrustum::PreparedShadowFrustum(PreparedShadowFrustum&& moveFrom)
    : _shadowTextureResource(std::move(moveFrom._shadowTextureResource))
    , _arbitraryCBSource(std::move(moveFrom._arbitraryCBSource))
    , _orthoCBSource(std::move(moveFrom._orthoCBSource))
    , _arbitraryCB(std::move(moveFrom._arbitraryCB))
    , _orthoCB(std::move(moveFrom._orthoCB))
    , _frustumCount(moveFrom._frustumCount)
    , _mode(moveFrom._mode)
    , _resolveParameters(moveFrom._resolveParameters)
    {}

    PreparedShadowFrustum& PreparedShadowFrustum::operator=(PreparedShadowFrustum&& moveFrom)
    {
        _shadowTextureResource = std::move(moveFrom._shadowTextureResource);
        _arbitraryCBSource = std::move(moveFrom._arbitraryCBSource);
        _orthoCBSource = std::move(moveFrom._orthoCBSource);
        _arbitraryCB = std::move(moveFrom._arbitraryCB);
        _orthoCB = std::move(moveFrom._orthoCB);
        _frustumCount = moveFrom._frustumCount;
        _mode = moveFrom._mode;
        _resolveParameters = moveFrom._resolveParameters;
        return *this;
    }

    GlobalLightingDesc::GlobalLightingDesc() 
    : _ambientLight(0.f, 0.f, 0.f), _skyReflectionScale(1.0f)
    , _doAtmosphereBlur(false), _doOcean(false), _doVegetationSpawn(false) 
    {
        _skyTexture[0] = '\0';
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
        static const auto ambientHash = ParameterBox::MakeParameterNameHash("ambientlight");
        static const auto ambientBrightnessHash = ParameterBox::MakeParameterNameHash("ambientbrightness");
        static const auto skyTextureHash = ParameterBox::MakeParameterNameHash("skytexture");
        static const auto skyReflectionScaleHash = ParameterBox::MakeParameterNameHash("skyreflectionscale");

        _ambientLight = props.GetParameter(ambientBrightnessHash, 1.f) * AsFloat3Color(props.GetParameter(ambientHash, ~0x0u));
        _skyReflectionScale = props.GetParameter(skyReflectionScaleHash, 1.f);
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
    }

    LightDesc::LightDesc()
    {
        _type = Directional;
        _negativeLightDirection = Normalize(Float3(-.1f, 0.33f, 1.f));
        _radius = 10000.f;
        _shadowFrustumIndex = ~unsigned(0x0);
        _diffuseColor = Float3(1.f, 1.f, 1.f);
        _specularColor = Float3(1.f, 1.f, 1.f);
        _nonMetalSpecularBrightness = 1.f;
    }

    LightDesc::LightDesc(const Utility::ParameterBox& props)
    {
        static const auto diffuseHash = ParameterBox::MakeParameterNameHash("diffuse");
        static const auto diffuseBrightnessHash = ParameterBox::MakeParameterNameHash("diffusebrightness");
        static const auto specularHash = ParameterBox::MakeParameterNameHash("specular");
        static const auto specularBrightnessHash = ParameterBox::MakeParameterNameHash("specularbrightness");
        static const auto specularNonMetalBrightnessHash = ParameterBox::MakeParameterNameHash("specularnonmetalbrightness");

        _type = LightDesc::Directional;
        _diffuseColor = props.GetParameter(diffuseBrightnessHash, 1.f) * AsFloat3Color(props.GetParameter(diffuseHash, ~0x0u));
        _specularColor = props.GetParameter(specularBrightnessHash, 1.f) * AsFloat3Color(props.GetParameter(specularHash, ~0x0u));
        _nonMetalSpecularBrightness = props.GetParameter(specularNonMetalBrightnessHash, 1.f);
                
        _radius = 10000.f;
        _shadowFrustumIndex = ~unsigned(0x0);
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
    }

}

