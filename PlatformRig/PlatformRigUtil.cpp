// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlatformRigUtil.h"
#include "../RenderCore/IDevice.h"
#include "../SceneEngine/LightDesc.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Format.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/IncludeLUA.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/BitUtils.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"

namespace PlatformRig
{

    void GlobalTechniqueContext::SetInteger(const char name[], uint32 value)
    {
        _globalEnvironmentState.SetParameter((const utf8*)name, value);
    }

    GlobalTechniqueContext::GlobalTechniqueContext()
    {
        using namespace luabridge;
        auto* luaState = ConsoleRig::Console::GetInstance().GetLuaState();
        getGlobalNamespace(luaState)
            .beginClass<GlobalTechniqueContext>("TechniqueContext")
                .addFunction("SetI", &GlobalTechniqueContext::SetInteger)
            .endClass();
            
        setGlobal(luaState, this, "TechContext");
    }

    GlobalTechniqueContext::~GlobalTechniqueContext()
    {
        auto* luaState = ConsoleRig::Console::GetInstance().GetLuaState();
        lua_pushnil(luaState);
        lua_setglobal(luaState, "TechContext");
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void ResizePresentationChain::OnResize(unsigned newWidth, unsigned newHeight)
    {
		auto chain = _presentationChain.lock();
        if (chain) {
                //  When we become an icon, we'll end up with zero width and height.
                //  We can't actually resize the presentation to zero. And we can't
                //  delete the presentation chain from here. So maybe just do nothing.
            if (newWidth && newHeight) {
				chain->Resize(newWidth, newHeight);
            }
        }
    }

    ResizePresentationChain::ResizePresentationChain(std::shared_ptr<RenderCore::IPresentationChain> presentationChain)
    : _presentationChain(presentationChain)
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    DefaultShadowFrustumSettings::DefaultShadowFrustumSettings()
    {
        const unsigned frustumCount = 5;
        const float maxDistanceFromCamera = 500.f;        // need really large distance because some models have a 100.f scale factor!
        const float frustumSizeFactor = 3.8f;
        const float focusDistance = 3.f;

        _frustumCount = frustumCount;
        _maxDistanceFromCamera = maxDistanceFromCamera;
        _frustumSizeFactor = frustumSizeFactor;
        _focusDistance = focusDistance;
        _flags = Flags::HighPrecisionDepths;
        _textureSize = 2048;

        _slopeScaledBias = Tweakable("ShadowSlopeScaledBias", 1.f);
        _depthBiasClamp = Tweakable("ShadowDepthBiasClamp", 0.f);
        _rasterDepthBias = Tweakable("ShadowRasterDepthBias", 600);

        _dsSlopeScaledBias = _slopeScaledBias;
        _dsDepthBiasClamp = _depthBiasClamp;
        _dsRasterDepthBias = _rasterDepthBias;

        _worldSpaceResolveBias = 0.f;   // (-.3f)
        _tanBlurAngle = 0.00436f;
        _minBlurSearch = 0.5f;
        _maxBlurSearch = 25.f;
    }

}

#include "../Utility/Meta/ClassAccessors.h"
#include "../Utility/Meta/ClassAccessorsImpl.h"

template<> const ClassAccessors& GetAccessors<PlatformRig::DefaultShadowFrustumSettings>()
{
    using Obj = PlatformRig::DefaultShadowFrustumSettings;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add(u("FrustumCount"), DefaultGet(Obj, _frustumCount),  
            [](Obj& obj, unsigned value) { obj._frustumCount = Clamp(value, 1u, SceneEngine::MaxShadowTexturesPerLight); });
        props.Add(u("MaxDistanceFromCamera"),  DefaultGet(Obj, _maxDistanceFromCamera),   DefaultSet(Obj, _maxDistanceFromCamera));
        props.Add(u("FrustumSizeFactor"),   DefaultGet(Obj, _frustumSizeFactor),    DefaultSet(Obj, _frustumSizeFactor));
        props.Add(u("FocusDistance"),   DefaultGet(Obj, _focusDistance),    DefaultSet(Obj, _focusDistance));
        props.Add(u("Flags"),   DefaultGet(Obj, _flags),    DefaultSet(Obj, _flags));
        props.Add(u("TextureSize"),   DefaultGet(Obj, _textureSize),    
            [](Obj& obj, unsigned value) { obj._textureSize = 1<<(IntegerLog2(value-1)+1); });  // ceil to a power of two
        props.Add(u("SingleSidedSlopeScaledBias"),   DefaultGet(Obj, _slopeScaledBias),    DefaultSet(Obj, _slopeScaledBias));
        props.Add(u("SingleSidedDepthBiasClamp"),   DefaultGet(Obj, _depthBiasClamp),    DefaultSet(Obj, _depthBiasClamp));
        props.Add(u("SingleSidedRasterDepthBias"),   DefaultGet(Obj, _rasterDepthBias),    DefaultSet(Obj, _rasterDepthBias));
        props.Add(u("DoubleSidedSlopeScaledBias"),   DefaultGet(Obj, _dsSlopeScaledBias),    DefaultSet(Obj, _dsSlopeScaledBias));
        props.Add(u("DoubleSidedDepthBiasClamp"),   DefaultGet(Obj, _dsDepthBiasClamp),    DefaultSet(Obj, _dsDepthBiasClamp));
        props.Add(u("DoubleSidedRasterDepthBias"),   DefaultGet(Obj, _dsRasterDepthBias),    DefaultSet(Obj, _dsRasterDepthBias));
        props.Add(u("WorldSpaceResolveBias"),   DefaultGet(Obj, _worldSpaceResolveBias),    DefaultSet(Obj, _worldSpaceResolveBias));
        props.Add(u("BlurAngleDegrees"),   
            [](const Obj& obj) { return Rad2Deg(XlATan(obj._tanBlurAngle)); },
            [](Obj& obj, float value) { obj._tanBlurAngle = XlTan(Deg2Rad(value)); } );
        props.Add(u("MinBlurSearch"),   DefaultGet(Obj, _minBlurSearch),    DefaultSet(Obj, _minBlurSearch));
        props.Add(u("MaxBlurSearch"),   DefaultGet(Obj, _maxBlurSearch),    DefaultSet(Obj, _maxBlurSearch));
        init = true;
    }
    return props;
}

namespace PlatformRig
{

    static Float4x4 MakeWorldToLight(
        const Float3& negativeLightDirection,
        const Float3& position)
    {
        return InvertOrthonormalTransform(
            MakeCameraToWorld(-negativeLightDirection, Float3(1.f, 0.f, 0.f), position));
    }

    static std::pair<SceneEngine::ShadowProjectionDesc::Projections, Float4x4> 
        BuildBasicShadowProjections(
            const SceneEngine::LightDesc& lightDesc,
            const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc,
            const DefaultShadowFrustumSettings& settings)
    {
        using namespace SceneEngine;
        ShadowProjectionDesc::Projections result;

        const float shadowNearPlane = 1.f;
        const float shadowFarPlane = settings._maxDistanceFromCamera;
        static float shadowWidthScale = 3.f;
        static float projectionSizePower = 3.75f;
        float shadowProjectionDist = shadowFarPlane - shadowNearPlane;

        auto negativeLightDirection = lightDesc._position;

        auto cameraPos = ExtractTranslation(mainSceneProjectionDesc._cameraToWorld);
        auto cameraForward = ExtractForward_Cam(mainSceneProjectionDesc._cameraToWorld);

            //  Calculate a simple set of shadow frustums.
            //  This method is non-ideal, but it's just a place holder for now
        result._normalProjCount = 5;
        result._mode = ShadowProjectionDesc::Projections::Mode::Arbitrary;
        for (unsigned c=0; c<result._normalProjCount; ++c) {
            const float projectionWidth = shadowWidthScale * std::pow(projectionSizePower, float(c));
            auto& p = result._fullProj[c];

            Float3 shiftDirection = cameraForward - negativeLightDirection * Dot(cameraForward, negativeLightDirection);

            Float3 focusPoint = cameraPos + (projectionWidth * 0.45f) * shiftDirection;
            auto lightViewMatrix = MakeWorldToLight(
                negativeLightDirection, focusPoint + (.5f * shadowProjectionDist) * negativeLightDirection);
            p._projectionMatrix = OrthogonalProjection(
                -.5f * projectionWidth, -.5f * projectionWidth,
                 .5f * projectionWidth,  .5f * projectionWidth,
                shadowNearPlane, shadowFarPlane,
                GeometricCoordinateSpace::RightHanded,
                RenderCore::Techniques::GetDefaultClipSpaceType());
            p._viewMatrix = lightViewMatrix;

            result._minimalProjection[c] = ExtractMinimalProjection(p._projectionMatrix);
        }
        
            //  Setup a single world-to-clip that contains all frustums within it. This will 
            //  be used for cull objects out of shadow casting.
        auto& lastProj = result._fullProj[result._normalProjCount-1];
        auto worldToClip = Combine(lastProj._viewMatrix, lastProj._projectionMatrix);

        return std::make_pair(result, worldToClip);
    }

    static void CalculateCameraFrustumCornersDirection(
        Float3 result[4],
        const RenderCore::Techniques::ProjectionDesc& projDesc,
        ClipSpaceType::Enum clipSpaceType)
    {
        // For the given camera, calculate 4 vectors that represent the
        // the direction from the camera position to the frustum corners
        // (there are 8 frustum corners, but the directions to the far plane corners
        // are the same as the near plane corners)
        Float4x4 projection = projDesc._cameraToProjection;
        Float4x4 noTransCameraToWorld = projDesc._cameraToWorld;
        SetTranslation(noTransCameraToWorld, Float3(0.f, 0.f, 0.f));
        auto trans = Combine(InvertOrthonormalTransform(noTransCameraToWorld), projection);
        Float3 corners[8];
        CalculateAbsFrustumCorners(corners, trans, clipSpaceType);
        for (unsigned c=0; c<4; ++c) {
            result[c] = Normalize(corners[4+c]);    // use the more distance corners, on the far clip plane
        }
    }

    static std::pair<Float4x4, Float4> BuildCameraAlignedOrthogonalShadowProjection(
        const SceneEngine::LightDesc& lightDesc,
        const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc,
        float depth, float maxDistanceFromCamera)
    {
            // Build a special "camera aligned" shadow projection.
            // This can be used to for especially high resolution shadows very close to the
            // near clip plane.
            // First, we build a rough projection-to-world based on the camera right direction...

        auto projRight = ExtractRight(mainSceneProjectionDesc._cameraToWorld);
        auto projForward = -lightDesc._position;
        auto projUp = Cross(projRight, projForward);
        auto adjRight = Cross(projForward, projUp);

        auto camPos = ExtractTranslation(mainSceneProjectionDesc._cameraToWorld);
        auto projToWorld = MakeCameraToWorld(projForward, Normalize(projUp), Normalize(adjRight), camPos);
        auto worldToLightProj = InvertOrthonormalTransform(projToWorld);

            // Now we just have to fit the finsl projection around the frustum corners

        auto clipSpaceType = RenderCore::Techniques::GetDefaultClipSpaceType();
        auto miniProj = PerspectiveProjection(
            mainSceneProjectionDesc._verticalFov, mainSceneProjectionDesc._aspectRatio,
            mainSceneProjectionDesc._nearClip, depth,
            GeometricCoordinateSpace::RightHanded, clipSpaceType);

        auto worldToMiniProj = Combine(
            InvertOrthonormalTransform(mainSceneProjectionDesc._cameraToWorld), miniProj);

        Float3 frustumCorners[8];
        CalculateAbsFrustumCorners(frustumCorners, worldToMiniProj, clipSpaceType);

        Float3 shadowViewSpace[8];
		Float3 shadowViewMins( FLT_MAX,  FLT_MAX,  FLT_MAX);
		Float3 shadowViewMaxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (unsigned c = 0; c < 8; c++) {
			shadowViewSpace[c] = TransformPoint(worldToLightProj, frustumCorners[c]);

				//	In our right handed coordinate space, the z coordinate in view space should
				//	be negative. But we always specify near & far in positive values. So
				//	we have to swap the sign of z here

			shadowViewSpace[c][2] = -shadowViewSpace[c][2];

			shadowViewMins[0] = std::min(shadowViewMins[0], shadowViewSpace[c][0]);
			shadowViewMins[1] = std::min(shadowViewMins[1], shadowViewSpace[c][1]);
			shadowViewMins[2] = std::min(shadowViewMins[2], shadowViewSpace[c][2]);
			shadowViewMaxs[0] = std::max(shadowViewMaxs[0], shadowViewSpace[c][0]);
			shadowViewMaxs[1] = std::max(shadowViewMaxs[1], shadowViewSpace[c][1]);
			shadowViewMaxs[2] = std::max(shadowViewMaxs[2], shadowViewSpace[c][2]);
		}

        const float shadowNearPlane = -maxDistanceFromCamera;
        const float shadowFarPlane  =  maxDistanceFromCamera;

        Float4x4 projMatrix = OrthogonalProjection(
            shadowViewMins[0], shadowViewMaxs[1], shadowViewMaxs[0], shadowViewMins[1], 
            shadowNearPlane, shadowFarPlane,
            GeometricCoordinateSpace::RightHanded, clipSpaceType);

        auto result = Combine(worldToLightProj, projMatrix);
        return std::make_pair(result, ExtractMinimalProjection(projMatrix));
    }

    static std::pair<SceneEngine::ShadowProjectionDesc::Projections, Float4x4>  
        BuildSimpleOrthogonalShadowProjections(
            const SceneEngine::LightDesc& lightDesc,
            const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc,
            const DefaultShadowFrustumSettings& settings)
    {
        // We're going to build some basic adaptive shadow frustums. These frustums
        // all fit within the same "definition" orthogonal space. This means that
        // cascades can't be rotated or skewed relative to each other. Usually this 
        // should be fine, (and perhaps might reduce some flickering around the 
        // cascade edges) but it means that the cascades might not be as tightly
        // bound as they might be.

        using namespace SceneEngine;
        using namespace RenderCore;

        ShadowProjectionDesc::Projections result;
        result._normalProjCount = settings._frustumCount;
        result._mode = ShadowProjectionDesc::Projections::Mode::Ortho;

        const float shadowNearPlane = -settings._maxDistanceFromCamera;
        const float shadowFarPlane = settings._maxDistanceFromCamera;
        auto clipSpaceType = Techniques::GetDefaultClipSpaceType();

        float t = 0;
        for (unsigned c=0; c<result._normalProjCount; ++c) { t += std::pow(settings._frustumSizeFactor, float(c)); }

        Float3 cameraPos = ExtractTranslation(mainSceneProjectionDesc._cameraToWorld);
        Float3 focusPoint = cameraPos + settings._focusDistance * ExtractForward(mainSceneProjectionDesc._cameraToWorld);
        result._definitionViewMatrix = MakeWorldToLight(lightDesc._position, focusPoint);
        assert(std::isfinite(result._definitionViewMatrix(0,3)) && !std::isnan(result._definitionViewMatrix(0,3)));
        Float4x4 worldToLightProj = result._definitionViewMatrix;

            //  Calculate 4 vectors for the directions of the frustum corners, 
            //  relative to the camera position.
        Float3 frustumCornerDir[4];
        CalculateCameraFrustumCornersDirection(frustumCornerDir, mainSceneProjectionDesc, clipSpaceType);

        Float3 allCascadesMins( FLT_MAX,  FLT_MAX,  FLT_MAX);
		Float3 allCascadesMaxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		float distanceFromCamera = 0.f;
		for (unsigned f=0; f<result._normalProjCount; ++f) {

			float camNearPlane = distanceFromCamera;
			distanceFromCamera += std::pow(settings._frustumSizeFactor, float(f)) * settings._maxDistanceFromCamera / t;
			float camFarPlane = distanceFromCamera;

                //  Find the frustum corners for this part of the camera frustum,
				//  and then build a shadow frustum that will contain those corners.
				//  Potentially not all of the camera frustum is full of geometry --
				//  if we knew which parts were full, and which were empty, we could
				//  optimise the shadow frustum further.

			Float3 absFrustumCorners[8];
			for (unsigned c = 0; c < 4; ++c) {
				absFrustumCorners[c] = cameraPos + camNearPlane * frustumCornerDir[c];
				absFrustumCorners[4 + c] = cameraPos + camFarPlane * frustumCornerDir[c];
            }

				//	Let's assume that we're not going to rotate the shadow frustum
				//	during this fitting. Then, this is easy... The shadow projection
				//	is orthogonal, so we just need to find the AABB in shadow-view space
				//	for these corners, and the projection parameters will match those very
				//	closely.
                //
                //  Note that we could potentially get a better result if we rotate the
                //  shadow frustum projection to better fit around the projected camera.
                //  It might make shadow texels creep and flicker as the projection changes,
                //  but perhaps a better implementation of this function could try that out.

			Float3 shadowViewSpace[8];
			Float3 shadowViewMins( FLT_MAX,  FLT_MAX,  FLT_MAX);
			Float3 shadowViewMaxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for (unsigned c = 0; c < 8; c++) {
				shadowViewSpace[c] = TransformPoint(worldToLightProj, absFrustumCorners[c]);

					//	In our right handed coordinate space, the z coordinate in view space should
					//	be negative. But we always specify near & far in positive values. So
					//	we have to swap the sign of z here

				shadowViewSpace[c][2] = -shadowViewSpace[c][2];

				shadowViewMins[0] = std::min(shadowViewMins[0], shadowViewSpace[c][0]);
				shadowViewMins[1] = std::min(shadowViewMins[1], shadowViewSpace[c][1]);
				shadowViewMins[2] = std::min(shadowViewMins[2], shadowViewSpace[c][2]);
				shadowViewMaxs[0] = std::max(shadowViewMaxs[0], shadowViewSpace[c][0]);
				shadowViewMaxs[1] = std::max(shadowViewMaxs[1], shadowViewSpace[c][1]);
				shadowViewMaxs[2] = std::max(shadowViewMaxs[2], shadowViewSpace[c][2]);
			}

                //	We have to pull the min depth distance back towards the light
				//	This is so we can capture geometry that is between the light
				//	and the frustum

            shadowViewMins[2] = shadowNearPlane;
            shadowViewMaxs[2] = shadowFarPlane;

            result._orthoSub[f]._projMins = shadowViewMins;
            result._orthoSub[f]._projMaxs = shadowViewMaxs;

            allCascadesMins[0] = std::min(allCascadesMins[0], shadowViewMins[0]);
            allCascadesMins[1] = std::min(allCascadesMins[1], shadowViewMins[1]);
            allCascadesMins[2] = std::min(allCascadesMins[2], shadowViewMins[2]);
            allCascadesMaxs[0] = std::max(allCascadesMaxs[0], shadowViewMaxs[0]);
            allCascadesMaxs[1] = std::max(allCascadesMaxs[1], shadowViewMaxs[1]);
            allCascadesMaxs[2] = std::max(allCascadesMaxs[2], shadowViewMaxs[2]);
        }

        for (unsigned f=0; f<result._normalProjCount; ++f) {
            result._fullProj[f]._viewMatrix = result._definitionViewMatrix;

                // Note that we're flipping y here in order to keep
                // the winding direction correct in the final projection
            const auto& mins = result._orthoSub[f]._projMins;
            const auto& maxs = result._orthoSub[f]._projMaxs;
            Float4x4 projMatrix = OrthogonalProjection(
                mins[0], maxs[1], maxs[0], mins[1], mins[2], maxs[2],
                GeometricCoordinateSpace::RightHanded, clipSpaceType);
            result._fullProj[f]._projectionMatrix = projMatrix;

            result._minimalProjection[f] = ExtractMinimalProjection(projMatrix);
        }

            //  When building the world to clip matrix, we want some to use some projection
            //  that projection that will contain all of the shadow frustums.
            //  We can use allCascadesMins and allCascadesMaxs to find the area of the 
            //  orthogonal space that is actually used. We just have to incorporate these
            //  mins and maxs into the projection matrix

        Float4x4 clippingProjMatrix = OrthogonalProjection(
            allCascadesMins[0], allCascadesMaxs[1], allCascadesMaxs[0], allCascadesMins[1], 
            shadowNearPlane, shadowFarPlane,
            GeometricCoordinateSpace::RightHanded, clipSpaceType);
        Float4x4 worldToClip = Combine(result._definitionViewMatrix, clippingProjMatrix);

        std::tie(result._specialNearProjection, result._specialNearMinimalProjection) = 
            BuildCameraAlignedOrthogonalShadowProjection(lightDesc, mainSceneProjectionDesc, 2.5, 30.f);
        result._useNearProj = true;

        return std::make_pair(result, worldToClip);
    }

    

    SceneEngine::ShadowProjectionDesc 
        CalculateDefaultShadowCascades(
            const SceneEngine::LightDesc& lightDesc,
            unsigned lightId,
            const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc,
            const DefaultShadowFrustumSettings& settings)
    {
            //  Build a default shadow frustum projection from the given inputs
            //  Note -- this is a very primitive implementation!
            //          But it actually works ok.
            //          Still, it's just a placeholder.

        using namespace SceneEngine;

        ShadowProjectionDesc result;
        result._width   = settings._textureSize;
        result._height  = settings._textureSize;
        if (settings._flags & DefaultShadowFrustumSettings::Flags::HighPrecisionDepths) {
            // note --  currently having problems in Vulkan with reading from the D24_UNORM_XX format
            //          might be better to move to 32 bit format now, anyway
            result._typelessFormat  = RenderCore::Format::R32_TYPELESS; //R24G8_TYPELESS;
            result._writeFormat     = RenderCore::Format::D32_FLOAT; //D24_UNORM_S8_UINT;
            result._readFormat      = RenderCore::Format::R32_FLOAT; //R24_UNORM_X8_TYPELESS;
        } else {
            result._typelessFormat  = RenderCore::Format::R16_TYPELESS;
            result._writeFormat     = RenderCore::Format::D16_UNORM;
            result._readFormat      = RenderCore::Format::R16_UNORM;
        }
        
        if (settings._flags & DefaultShadowFrustumSettings::Flags::ArbitraryCascades) {
            auto t = BuildBasicShadowProjections(lightDesc, mainSceneProjectionDesc, settings);
            result._projections = t.first;
            result._worldToClip = t.second;
        } else {
            auto t = BuildSimpleOrthogonalShadowProjections(lightDesc, mainSceneProjectionDesc, settings);
            result._projections = t.first;
            result._worldToClip = t.second;
        }

        if (settings._flags & DefaultShadowFrustumSettings::Flags::RayTraced) {
            result._resolveType = ShadowProjectionDesc::ResolveType::RayTraced;
        } else {
            result._resolveType = ShadowProjectionDesc::ResolveType::DepthTexture;
        }

        if (settings._flags & DefaultShadowFrustumSettings::Flags::CullFrontFaces) {
            result._windingCull = ShadowProjectionDesc::WindingCull::FrontFaces;
        } else {
            result._windingCull = ShadowProjectionDesc::WindingCull::BackFaces;
        }

        result._slopeScaledBias = settings._slopeScaledBias;
        result._depthBiasClamp = settings._depthBiasClamp;
        result._rasterDepthBias = settings._rasterDepthBias;
        result._dsSlopeScaledBias = settings._dsSlopeScaledBias;
        result._dsDepthBiasClamp = settings._dsDepthBiasClamp;
        result._dsRasterDepthBias = settings._dsRasterDepthBias;

        result._worldSpaceResolveBias = settings._worldSpaceResolveBias;
        result._tanBlurAngle = settings._tanBlurAngle;
        result._minBlurSearch = settings._minBlurSearch;
        result._maxBlurSearch = settings._maxBlurSearch;

        result._lightId = lightId;

        return result;
    }

}


