// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "LightDesc.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../Core/Types.h"

namespace RenderCore { class SharedPkt; }
namespace RenderCore { namespace Techniques { class ParsingContext; }}

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
        Float3x4    _nearCascade;       // special projection for the area closest to the camera
        Float4      _nearMinimalProjection;
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

        CB          _arbitraryCB;
        CB          _orthoCB;
        unsigned    _frustumCount;
        bool        _enableNearCascade;

        ShadowProjectionDesc::Projections::Mode::Enum _mode;
        CB_ArbitraryShadowProjection    _arbitraryCBSource;
        CB_OrthoShadowProjection        _orthoCBSource;

        void InitialiseConstants(
            RenderCore::Metal::DeviceContext* devContext,
            const MultiProjection<MaxShadowTexturesPerLight>&);

        PreparedShadowFrustum();
        PreparedShadowFrustum(PreparedShadowFrustum&& moveFrom) never_throws;
        PreparedShadowFrustum& operator=(PreparedShadowFrustum&& moveFrom) never_throws;
    };

    /// <summary>Prepared "Depth Map" shadow frustum</summary>
    class PreparedDMShadowFrustum : public PreparedShadowFrustum
    {
    public:
        CB_ShadowResolveParameters  _resolveParameters;
        CB                          _resolveParametersCB;

		const SRV&	GetSRV() const;

        bool IsReady() const;

        PreparedDMShadowFrustum();
        PreparedDMShadowFrustum(PreparedDMShadowFrustum&& moveFrom) never_throws;
        PreparedDMShadowFrustum& operator=(PreparedDMShadowFrustum&& moveFrom) never_throws;
	private:
		SRV _srv;
    };

    /// <summary>Prepared "Ray Traced" shadow frustum</summary>
    class PreparedRTShadowFrustum : public PreparedShadowFrustum
    {
    public:
        SRV _listHeadSRV;
        SRV _linkedListsSRV;
        SRV _trianglesSRV;

        bool IsReady() const;

        PreparedRTShadowFrustum();
        PreparedRTShadowFrustum(PreparedRTShadowFrustum&& moveFrom) never_throws;
        PreparedRTShadowFrustum& operator=(PreparedRTShadowFrustum&& moveFrom) never_throws;
    };

    void BuildShadowConstantBuffers(
        CB_ArbitraryShadowProjection& arbitraryCBSource,
        CB_OrthoShadowProjection& orthoCBSource,
        const MultiProjection<MaxShadowTexturesPerLight>& desc);

    RenderCore::SharedPkt BuildScreenToShadowConstants(
        const PreparedShadowFrustum& preparedFrustum, 
        const Float4x4& cameraToWorld, 
        const Float4x4& cameraToProjection);
    RenderCore::SharedPkt BuildScreenToShadowConstants(
        unsigned frustumCount,
        const CB_ArbitraryShadowProjection& arbitraryCB,
        const CB_OrthoShadowProjection& orthoCB,
        const Float4x4& cameraToWorld,
        const Float4x4& cameraToProjection);

    void BindShadowsForForwardResolve(
        RenderCore::Metal::DeviceContext& metalContext,
        RenderCore::Techniques::ParsingContext& parsingContext,
        const PreparedDMShadowFrustum& dominantLight);

    void UnbindShadowsForForwardResolve(
        RenderCore::Metal::DeviceContext& metalContext,
        RenderCore::Techniques::ParsingContext& parsingContext);

}