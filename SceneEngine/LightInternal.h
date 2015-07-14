// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "LightDesc.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../Core/Types.h"

namespace RenderCore { class SharedPkt; }

namespace SceneEngine
{
    template<int MaxProjections> class MultiProjection;

    class CB_ArbitraryShadowProjection
    {
    public:
        uint32      _projectionCount; 
        uint32      _dummy[3];
        Float4      _minimalProj[MaxShadowTexturesPerLight];
        Float4x4    _worldToProj[MaxShadowTexturesPerLight];
    };

    class CB_OrthoShadowProjection
    {
    public:
        Float3x4    _worldToProj;
        Float4      _minimalProjection;
        uint32      _projectionCount;
        uint32      _dummy[3];
        Float4      _cascadeScale[MaxShadowTexturesPerLight];
        Float4      _cascadeTrans[MaxShadowTexturesPerLight];
    };

    class CB_ShadowResolveParameters
    {
    public:
        float       _worldSpaceBias;
        float       _tanBlurAngle;
        float       _minBlurSearch, _maxBlurSearch;
        float       _shadowTextureSize;
        unsigned    _dummy[3];
        CB_ShadowResolveParameters();
    };

    /// <summary>Contains the result of a shadow prepare operation</summary>
    /// Typically shadows are prepared as one of the first steps of while rendering
    /// a frame. (though, I guess, the prepare step could happen at any time).
    /// We need to retain the shader constants and render target outputs
    /// from that preparation, to use later while resolving the lighting of the main
    /// scene.
    class PreparedShadowFrustum
    {
    public:
        typedef RenderCore::Metal::ShaderResourceView SRV;
        typedef RenderCore::Metal::ConstantBuffer CB;

        SRV         _shadowTextureSRV;
        CB          _arbitraryCB;
        CB          _orthoCB;
        unsigned    _frustumCount;

        ShadowProjectionDesc::Projections::Mode::Enum _mode;
        CB_ArbitraryShadowProjection    _arbitraryCBSource;
        CB_OrthoShadowProjection        _orthoCBSource;
        CB_ShadowResolveParameters      _resolveParameters;

        void InitialiseConstants(
            RenderCore::Metal::DeviceContext* devContext,
            const MultiProjection<MaxShadowTexturesPerLight>&);

        bool IsReady() const;

        PreparedShadowFrustum();
        PreparedShadowFrustum(PreparedShadowFrustum&& moveFrom);
        PreparedShadowFrustum& operator=(PreparedShadowFrustum&& moveFrom) never_throws;
    };

    void BuildShadowConstantBuffers(
        CB_ArbitraryShadowProjection& arbitraryCBSource,
        CB_OrthoShadowProjection& orthoCBSource,
        const MultiProjection<MaxShadowTexturesPerLight>& desc);

    RenderCore::SharedPkt BuildScreenToShadowConstants(
        unsigned frustumCount,
        const CB_ArbitraryShadowProjection& arbitraryCB,
        const CB_OrthoShadowProjection& orthoCB,
        const Float4x4& cameraToWorld);

}