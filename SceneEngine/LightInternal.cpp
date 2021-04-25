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

}

