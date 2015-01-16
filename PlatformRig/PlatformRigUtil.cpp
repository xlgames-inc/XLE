// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlatformRigUtil.h"
#include "../RenderCore/IDevice.h"
#include "../SceneEngine/LightDesc.h"
#include "../RenderCore/RenderUtils.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/IncludeLUA.h"
#include "../Math/Transformations.h"

namespace PlatformRig
{

    void GlobalTechniqueContext::SetInteger(const char name[], uint32 value)
    {
        _globalEnvironmentState.SetParameter(name, value);
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
        if (_presentationChain) {
                //  When we become an icon, we'll end up with zero width and height.
                //  We can't actually resize the presentation to zero. And we can't
                //  delete the presentation chain from here. So maybe just do nothing.
            if (newWidth && newHeight) {
                _presentationChain->Resize(newWidth, newHeight);
            }
        }
    }

    ResizePresentationChain::ResizePresentationChain(std::shared_ptr<RenderCore::IPresentationChain> presentationChain)
    : _presentationChain(presentationChain)
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    SceneEngine::ShadowFrustumDesc CalculateDefaultShadowFrustums(
        const SceneEngine::LightDesc& lightDesc,
        const RenderCore::CameraDesc& cameraDesc)
    {
            // Build a default shadow frustum projection from the given inputs
            //  Note -- this is a very primitive implementation!
            //          But it actually works ok.
            //          Still, it's just a placeholder.
        using namespace SceneEngine;
        ShadowFrustumDesc result;

        result._width             = 1024;
        result._height            = 1024;
        const bool useHighPrecisionDepths = false;
        if (constant_expression<useHighPrecisionDepths>::result()) {
            result._typelessFormat = (RenderCore::Metal::NativeFormat::Enum)DXGI_FORMAT_R24G8_TYPELESS;
            result._writeFormat    = (RenderCore::Metal::NativeFormat::Enum)DXGI_FORMAT_D24_UNORM_S8_UINT;
            result._readFormat     = (RenderCore::Metal::NativeFormat::Enum)DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        } else {
            result._typelessFormat    = (RenderCore::Metal::NativeFormat::Enum)DXGI_FORMAT_R16_TYPELESS;
            result._writeFormat       = (RenderCore::Metal::NativeFormat::Enum)DXGI_FORMAT_D16_UNORM;
            result._readFormat        = (RenderCore::Metal::NativeFormat::Enum)DXGI_FORMAT_R16_UNORM;
        }

        static float shadowNearPlane     = 1.f;
        static float shadowFarPlane      = 1000.f;
        static float shadowWidthScale    = 3.f;
        static float projectionSizePower = 3.75f;
        float shadowProjectionDist = shadowFarPlane - shadowNearPlane;

        auto negativeLightDirection = lightDesc._negativeLightDirection;

        auto cameraPos = ExtractTranslation(cameraDesc._cameraToWorld);
        auto cameraForward = ExtractForward_Cam(cameraDesc._cameraToWorld);

            //  Calculate a simple set of shadow frustums.
            //  This method is non-ideal, but it's just a place holder for now
        result._projectionCount = 5;
        for (unsigned c=0; c<result._projectionCount; ++c) {
            const float projectionWidth = shadowWidthScale * std::pow(projectionSizePower, float(c));
            auto& p = result._projections[c];

            Float3 shiftDirection = cameraForward - negativeLightDirection * Dot(cameraForward, negativeLightDirection);

            Float3 focusPoint = cameraPos + (projectionWidth * 0.45f) * shiftDirection;
            auto lightViewMatrix = 
                InvertOrthonormalTransform(
                    MakeCameraToWorld(
                        -negativeLightDirection, Float3(1.f, 0.f, 0.f), 
                        focusPoint + (.5f * shadowProjectionDist) * negativeLightDirection));

                //  note that the "flip" on the projection matrix is important here
                //  we need to make sure the correct faces are back-faced culled. If
                //  the wrong faces are culled, the results will still look close to being
                //  correct in many places, but there will be light leakage
            p._projectionMatrix = RenderCore::OrthogonalProjection(
                -.5f * projectionWidth,  .5f * projectionWidth,
                 .5f * projectionWidth, -.5f * projectionWidth,
                shadowNearPlane, shadowFarPlane,
                RenderCore::GeometricCoordinateSpace::RightHanded, 
                RenderCore::ClipSpaceType::Positive);
            p._viewMatrix = lightViewMatrix;

            p._projectionDepthRatio[0] = 1.f / (shadowFarPlane - shadowNearPlane);
            p._projectionDepthRatio[1] = -shadowNearPlane / (shadowFarPlane - shadowNearPlane);
            p._projectionScale[0] = 2.f / projectionWidth;
            p._projectionScale[1] = 2.f / projectionWidth;
        }

            //  Setup a single world-to-clip that contains all frustums within it. This will 
            //  be used for cull objects out of shadow casting.
        auto& lastProj = result._projections[result._projectionCount-1];
        result._worldToClip = Combine(lastProj._viewMatrix, lastProj._projectionMatrix);
        return result;
    }

}


