#include "LightInternal.h"
#include "LightDesc.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"
#include "../Utility/MemoryUtils.h"

namespace SceneEngine
{
    void PreparedShadowFrustum::InitialiseConstants(
        RenderCore::Metal::DeviceContext* devContext,
        const MultiProjection<MaxShadowTexturesPerLight>& desc)
    {
        _frustumCount = desc._count;

        XlZeroMemory(_arbitraryCBSource);
        XlZeroMemory(_orthoCBSource);

        typedef MultiProjection<MaxShadowTexturesPerLight>::Mode Mode;
        if (desc._mode == Mode::Arbitrary) {

            _arbitraryCBSource._projectionCount = _frustumCount;
            for (unsigned c=0; c<_frustumCount; ++c) {
                _arbitraryCBSource._worldToProj[c] = 
                    Combine(  
                        desc._fullProj[c]._viewMatrix, 
                        desc._fullProj[c]._projectionMatrix);
                _arbitraryCBSource._minimalProj[c] = desc._minimalProjection[c];
            }

        } else if (desc._mode == Mode::Ortho) {

                //  We can still fill in the constant buffer
                //  for arbitrary projection. If we use the right
                //  shader, we shouldn't need this. But we can have it
                //  prepared for comparisons.
            _arbitraryCBSource._projectionCount = _frustumCount;
            auto baseWorldToProj = Combine(desc._definitionViewMatrix, desc._definitionProjMatrix);
            for (unsigned c=0; c<_frustumCount; ++c) {
                Float4x4 additionalPart = Identity<Float4x4>();
                for (unsigned q=0; q<3; ++q) {
                    additionalPart(q,q) =  2.f  / (desc._orthoSub[c]._projMaxs[q] - desc._orthoSub[c]._projMins[q]);
                    additionalPart(q,3) = -0.5f * (desc._orthoSub[c]._projMaxs[q] + desc._orthoSub[c]._projMins[q]);
                }

                _arbitraryCBSource._worldToProj[c] = Combine(baseWorldToProj, additionalPart);
                _arbitraryCBSource._minimalProj[c] = ExtractMinimalProjection(additionalPart);
            }

                //  Also fill in the constants for ortho projection mode
            _orthoCBSource._projectionCount = _frustumCount;
            _orthoCBSource._minimalProjection = desc._minimalProjection[0];
            _orthoCBSource._worldToProj = AsFloat3x4(baseWorldToProj);

            for (unsigned c=0; c<_frustumCount; ++c) {
                for (unsigned q=0; q<3; ++q) {
                    _orthoCBSource._cascadeScale[c][q] =  2.f  / (desc._orthoSub[c]._projMaxs[q] - desc._orthoSub[c]._projMins[q]);
                    _orthoCBSource._cascadeTrans[c][q] = -0.5f * (desc._orthoSub[c]._projMaxs[q] + desc._orthoSub[c]._projMins[q]);
                }
            }

        }

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
        return *this;
    }

    GlobalLightingDesc::GlobalLightingDesc() 
    : _ambientLight(0.f, 0.f, 0.f), _skyTexture(nullptr)
    , _doAtmosphereBlur(false), _doOcean(false), _doToneMap(false), _doVegetationSpawn(false) 
    {}

    ShadowProjectionDesc::ShadowProjectionDesc()
    {
        _width = _height = 0;
        _typelessFormat = _writeFormat = _readFormat = RenderCore::Metal::NativeFormat::Unknown;
        _worldToClip = Identity<Float4x4>();
    }

}

