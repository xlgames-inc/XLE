// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IntersectionTest.h"
#include "RayVsModel.h"
#include "LightingParser.h"
#include "LightingParserContext.h"
#include "Terrain.h"
#include "PlacementsManager.h"
#include "SceneParser.h"

#include "../BufferUploads/DataPacket.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/DX11/Metal/IncludeDX11.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h"
#include "../RenderCore/RenderUtils.h"

#include "../Math/Transformations.h"
#include "../Math/Vector.h"
#include "../Math/ProjectionMath.h"

#include "../Core/WinAPI/IncludeWindows.h"      // *hack* just needed for getting client rect coords!


namespace SceneEngine
{
    

////////////////////////////////////////////////////////////////////////////////////////////////////////

    static std::shared_ptr<RenderCore::Metal::DeviceContext> GetImmediateContext()
    {
        using namespace RenderCore::Metal;
        ID3D::DeviceContext* immContextTemp = nullptr;
        ObjectFactory().GetUnderlying()->GetImmediateContext(&immContextTemp);
        return std::make_shared<DeviceContext>(
            intrusive_ptr<ID3D::DeviceContext>(moveptr(immContextTemp)));
    }

    static std::pair<Float3, bool> FindTerrainIntersection(
        RenderCore::Metal::DeviceContext* devContext,
        LightingParserContext& parserContext,
        TerrainManager& terrainManager,
        std::pair<Float3, Float3> worldSpaceRay)
    {
        TRY {
            TerrainManager::IntersectionResult intersections[8];
            unsigned intersectionCount = terrainManager.CalculateIntersections(
                intersections, dimof(intersections), worldSpaceRay, devContext, parserContext);

            if (intersectionCount > 0) {
                return std::make_pair(intersections[0]._intersectionPoint, true);
            }
        } CATCH (...) {
        } CATCH_END

        return std::make_pair(Float3(0,0,0), false);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////

    static std::pair<Float3, bool> FindTerrainIntersection(
        RenderCore::Metal::DeviceContext* devContext,
        const IntersectionTestContext& context,
        TerrainManager& terrainManager,
        std::pair<Float3, Float3> worldSpaceRay)
    {
            //  create a new device context and lighting parser context, and use
            //  this to find an accurate terrain collision.
        auto viewportDims = context.GetViewportSize();
        RenderCore::Metal::ViewportDesc newViewport(
            0.f, 0.f, float(viewportDims[0]), float(viewportDims[1]), 0.f, 1.f);
        devContext->Bind(newViewport);

        LightingParserContext parserContext(context.GetTechniqueContext());
        RenderingQualitySettings qualitySettings(viewportDims, 1, 0);
        LightingParser_SetupScene(
            devContext, parserContext, context.GetSceneParser(),
            context.GetCameraDesc(), qualitySettings);

        return FindTerrainIntersection(devContext, parserContext, terrainManager, worldSpaceRay);
    }

    static std::pair<unsigned, float> RayVsPlacements(
        RenderCore::Metal::DeviceContext* devContext,
        RayVsModelStateContext& stateContext,
        SceneEngine::PlacementsEditor& placementsEditor, SceneEngine::PlacementGUID object)
    {
            // Using the GPU, look for intersections between the ray
            // and the given model. Since we're using the GPU, we need to
            // get a device context. 
            //
            // We'll have to use the immediate context
            // because we want to get the result get right. But that means the
            // immediate context can't be doing anything else in another thread.
            //
            // This will require more complex threading support in the future!
        assert(devContext->GetUnderlying()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);
        auto fnResult = std::make_pair(0, FLT_MAX);

            //  We need to invoke the render for the given object
            //  now. Afterwards we can query the buffers for the result
        const unsigned techniqueIndex = 6;
        placementsEditor.RenderFiltered(
            devContext, stateContext.GetParserContext(), techniqueIndex,
            &object, &object+1);

        auto results = stateContext.GetResults();
        fnResult.first = results.size();
        for (auto i=results.cbegin(); i!=results.cend(); ++i) {
            if (i->_intersectionDepth < fnResult.second) {
                fnResult.second = i->_intersectionDepth;
            }
        }

        return fnResult;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////

    auto IntersectionTestScene::FirstRayIntersection(
        std::shared_ptr<RenderCore::Metal::DeviceContext> devContext,
        const IntersectionTestContext& context,
        std::pair<Float3, Float3> worldSpaceRay,
        Type::BitField filter) const -> Result
    {
        Result result;

        if ((filter & Type::Terrain) && _terrainManager) {
            auto intersection = FindTerrainIntersection(
                devContext.get(), context, *_terrainManager.get(), worldSpaceRay);
            if (intersection.second) {
                float distance = Magnitude(intersection.first - worldSpaceRay.first);
                if (distance < result._distance) {
                    result = Result();
                    result._type = Type::Terrain;
                    result._worldSpaceCollision = intersection.first;
                    result._distance = distance;
                }
            }
        }

        if ((filter & Type::Placement) && _placements) {
            auto roughIntersection = 
                _placements->Find_RayIntersection(worldSpaceRay.first, worldSpaceRay.second);

                // we can improve the intersection by doing ray-vs-triangle tests
                // on the roughIntersection geometry

                //  we need to create a temporary transaction to get
                //  at the information for these objects.
            auto trans = _placements->Transaction_Begin(
                AsPointer(roughIntersection.cbegin()), AsPointer(roughIntersection.cend()));

            TRY
            {
                float rayLength = Magnitude(worldSpaceRay.second - worldSpaceRay.first);

                auto cam = context.GetCameraDesc();
                RayVsModelStateContext stateContext(devContext, context.GetTechniqueContext(), &cam);
                stateContext.SetRay(worldSpaceRay);

                // note --  we could do this all in a single render call, except that there
                //          is no way to associate a low level intersection result with a specific
                //          draw call.
                auto count = trans->GetObjectCount();
                for (unsigned c=0; c<count; ++c) {
                    auto guid = trans->GetGuid(c);
                    auto r = RayVsPlacements(
                        devContext.get(), stateContext,
                        *_placements, guid);

                    if (r.first && r.second < result._distance) {
                        result = Result();
                        result._type = Type::Placement;
                        result._worldSpaceCollision = 
                            LinearInterpolate(worldSpaceRay.first, worldSpaceRay.second, r.second / rayLength);
                        result._distance = r.second;
                        result._objectGuid = guid;
                    }
                }
            } CATCH(...) {
            } CATCH_END

            trans->Cancel();
        }

        return result;
    }

    auto IntersectionTestScene::UnderCursor(
        const IntersectionTestContext& context,
        Int2 cursorPosition, Type::BitField filter) const -> Result
    {
        return FirstRayIntersection(
            context.GetImmediateContext(),
            context, context.CalculateWorldSpaceRay(cursorPosition), filter);
    }

    IntersectionTestScene::IntersectionTestScene(
        std::shared_ptr<TerrainManager> terrainManager,
        std::shared_ptr<PlacementsEditor> placements)
    : _terrainManager(std::move(terrainManager))
    , _placements(std::move(placements))
    {}

    IntersectionTestScene::~IntersectionTestScene()
    {}

////////////////////////////////////////////////////////////////////////////////////////////////////////

    static Int2 GetViewportDims()
    {
            // HACK -- currently there's no good way to get the viewport size
            // we have to do a hack via windows...!
        RECT clientRect; GetClientRect(GetActiveWindow(), &clientRect);
        return Int2(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
    }
    
    static Float4x4 CalculateWorldToProjection(const RenderCore::Techniques::CameraDesc& sceneCamera, float viewportAspect)
    {
        auto projectionMatrix = RenderCore::Techniques::PerspectiveProjection(sceneCamera, viewportAspect);
        return Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), projectionMatrix);
    }

    std::pair<Float3, Float3> IntersectionTestContext::CalculateWorldSpaceRay(Int2 screenCoord) const
    {
        auto viewport = GetViewportDims();
        auto sceneCamera = _sceneParser->GetCameraDesc();
        auto worldToProjection = CalculateWorldToProjection(sceneCamera, viewport[0] / float(viewport[1]));

        Float3 frustumCorners[8];
        CalculateAbsFrustumCorners(frustumCorners, worldToProjection);
        Float3 cameraPosition = ExtractTranslation(sceneCamera._cameraToWorld);

        return RenderCore::Techniques::BuildRayUnderCursor(
            screenCoord, frustumCorners, cameraPosition, 
            sceneCamera._nearClip, sceneCamera._farClip,
            std::make_pair(Float2(0.f, 0.f), Float2(float(viewport[0]), float(viewport[1]))));
    }

    Float2 IntersectionTestContext::ProjectToScreenSpace(const Float3& worldSpaceCoord) const
    {
        auto viewport = GetViewportDims();
        auto worldToProjection = CalculateWorldToProjection(_sceneParser->GetCameraDesc(), viewport[0] / float(viewport[1]));
        auto projCoords = worldToProjection * Expand(worldSpaceCoord, 1.f);

        return Float2(
            (projCoords[0] / projCoords[3] * 0.5f + 0.5f) * float(viewport[0]),
            (projCoords[1] / projCoords[3] * -0.5f + 0.5f) * float(viewport[1]));
    }

    Int2 IntersectionTestContext::GetViewportSize() const
    {
        return GetViewportDims();
    }

    std::shared_ptr<RenderCore::Metal::DeviceContext> IntersectionTestContext::GetImmediateContext() const
    {
        return SceneEngine::GetImmediateContext();
    }

    RenderCore::Techniques::CameraDesc IntersectionTestContext::GetCameraDesc() const 
    { 
        return _sceneParser->GetCameraDesc();
    }

    IntersectionTestContext::IntersectionTestContext(
        std::shared_ptr<SceneEngine::ISceneParser> sceneParser,
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext)
    : _sceneParser(std::move(sceneParser))
    , _techniqueContext(std::move(techniqueContext))
    {}

    IntersectionTestContext::~IntersectionTestContext() {}

}

