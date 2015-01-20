// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlacementsManipulators.h"
#include "ManipulatorsUtil.h"
#include "TerrainManipulators.h"        // needed for hit tests

#include "../RenderOverlays/DebuggingDisplay.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderOverlays/OverlayContext.h"
#include "../RenderOverlays/Overlays/Browser.h"

#include "../SceneEngine/PlacementsManager.h"
#include "../SceneEngine/Terrain.h"
#include "../SceneEngine/SceneParser.h"
#include "../SceneEngine/Techniques.h"

#include "../Utility/TimeUtils.h"
#include "../Math/Transformations.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../SceneEngine/SceneEngineUtility.h"
#include "../SceneEngine/ResourceBox.h"
#include "../Math/ProjectionMath.h"

#include "../Core/WinAPI/IncludeWindows.h"      // *hack* just needed for getting client rect coords!

namespace Sample
{
    extern std::shared_ptr<SceneEngine::ITerrainFormat> MainTerrainFormat;
    extern SceneEngine::TerrainCoordinateSystem MainTerrainCoords;
    extern SceneEngine::TerrainConfig MainTerrainConfig;
}

namespace Tools
{
    using namespace RenderOverlays::DebuggingDisplay;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class SelectedModel
    {
    public:
        std::string _modelName;
    };

    class PlacementsWidgets : public IWidget
    {
    public:
        void    Render(         IOverlayContext* context, Layout& layout, 
                                Interactables& interactables, InterfaceState& interfaceState);
        void    RenderToScene(  RenderCore::Metal::DeviceContext* context, 
                                SceneEngine::LightingParserContext& parserContext);
        bool    ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input);

        PlacementsWidgets(  std::shared_ptr<SceneEngine::PlacementsEditor> editor, 
                            std::shared_ptr<HitTestResolver> hitTestResolver);
    private:
        typedef Overlays::ModelBrowser ModelBrowser;

        std::shared_ptr<ModelBrowser>       _browser;
        std::shared_ptr<HitTestResolver>    _hitTestResolver;
        std::shared_ptr<SceneEngine::PlacementsEditor> _editor;
        bool                                _browserActive;
        std::shared_ptr<SelectedModel>      _selectedModel;

        std::vector<std::unique_ptr<IManipulator>> _manipulators;
        unsigned _activeManipulatorIndex;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    auto HitTestResolver::DoHitTest(Int2 screenCoord) const -> Result
    {
            // currently there's no good way to get the viewport size
            //  we have to do a hack via windows...!
        RECT clientRect; GetClientRect(GetActiveWindow(), &clientRect);

        TerrainHitTestContext hitTestContext(
            *_terrainManager, *_sceneParser, *_techniqueContext,
            Int2(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top));
        auto intersection = Tools::FindTerrainIntersection(hitTestContext, screenCoord);

        Result result;
        if (intersection.second) {
            result._type = Result::Terrain;
            result._worldSpaceCollision = intersection.first;
        }

        return result;
    }

    std::pair<Float3, Float3> HitTestResolver::CalculateWorldSpaceRay(Int2 screenCoord) const
    {
        RECT clientRect; GetClientRect(GetActiveWindow(), &clientRect);

        auto sceneCamera = _sceneParser->GetCameraDesc();
        auto projectionMatrix = RenderCore::PerspectiveProjection(
            sceneCamera._verticalFieldOfView, (clientRect.right - clientRect.left) / float(clientRect.bottom - clientRect.top),
            sceneCamera._nearClip, sceneCamera._farClip, RenderCore::GeometricCoordinateSpace::RightHanded, 
            #if (GFXAPI_ACTIVE == GFXAPI_DX11) || (GFXAPI_ACTIVE == GFXAPI_DX9)
                RenderCore::ClipSpaceType::Positive);
            #else
                RenderCore::ClipSpaceType::StraddlingZero);
            #endif
        auto worldToProjection = Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), projectionMatrix);

        Float3 frustumCorners[8];
        CalculateAbsFrustumCorners(frustumCorners, worldToProjection);
        Float3 cameraPosition = ExtractTranslation(sceneCamera._cameraToWorld);

        return RenderCore::BuildRayUnderCursor(
            screenCoord, frustumCorners, cameraPosition, 
            sceneCamera._nearClip, sceneCamera._farClip,
            std::make_pair(Float2(0.f, 0.f), Float2(float(clientRect.right - clientRect.left), float(clientRect.bottom - clientRect.top))));
    }

    HitTestResolver::HitTestResolver(
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<SceneEngine::ISceneParser> sceneParser,
        std::shared_ptr<SceneEngine::TechniqueContext> techniqueContext)
    : _terrainManager(std::move(terrainManager))
    , _sceneParser(std::move(sceneParser))
    , _techniqueContext(std::move(techniqueContext))
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PlaceSingle : public IManipulator
    {
    public:
        bool OnInputEvent(
            const InputSnapshot& evnt,
            const HitTestResolver& hitTestContext);
        void Render(
            RenderCore::Metal::DeviceContext* context,
            SceneEngine::LightingParserContext& parserContext);

        const char* GetName() const;
        std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        std::pair<BoolParameter*, size_t>   GetBoolParameters() const;

        PlaceSingle(
            std::shared_ptr<SelectedModel> selectedModel,
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~PlaceSingle();

    protected:
        Millisecond                     _spawnTimer;
        std::shared_ptr<SelectedModel>  _selectedModel;
        std::shared_ptr<SceneEngine::PlacementsEditor> _editor;
    };

    bool PlaceSingle::OnInputEvent(
        const InputSnapshot& evnt,
        const HitTestResolver& hitTestContext)
    {
        //  If we get a click on the terrain, then we should perform 
            //  whatever placement operation is required (eg, creating new placements)
        const Millisecond spawnTimeOut = 200;
        auto now = Millisecond_Now();
        if (evnt.IsHeld_LButton() && now >= (_spawnTimer + spawnTimeOut) && !_selectedModel->_modelName.empty()) {
            auto test = hitTestContext.DoHitTest(evnt._mousePosition);
            if (test._type == HitTestResolver::Result::Terrain) {

                    //  This is a spawn event. We should add a new item of the selected model
                    //  at the point clicked.
                TRY {
                    _editor->AddPlacement(
                        AsFloat3x4(test._worldSpaceCollision), 
                        _selectedModel->_modelName.c_str(), "");
                } CATCH (...) {
                } CATCH_END

                _spawnTimer = now;
                return true;
            }
        }

        return false;
    }

    void PlaceSingle::Render(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext)
    {
    }

    const char* PlaceSingle::GetName() const                                            { return "Place single"; }
    auto PlaceSingle::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>  { return std::make_pair(nullptr, 0); }
    auto PlaceSingle::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>    { return std::make_pair(nullptr, 0); }

    PlaceSingle::PlaceSingle(
        std::shared_ptr<SelectedModel> selectedModel,
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    {
        _spawnTimer = 0;
        _selectedModel = std::move(selectedModel);
        _editor = std::move(editor);
    }
    
    PlaceSingle::~PlaceSingle() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class SelectAndEdit : public IManipulator
    {
    public:
        bool OnInputEvent(
            const InputSnapshot& evnt,
            const HitTestResolver& hitTestContext);
        void Render(
            RenderCore::Metal::DeviceContext* context,
            SceneEngine::LightingParserContext& parserContext);

        const char* GetName() const;
        std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        std::pair<BoolParameter*, size_t>   GetBoolParameters() const;

        SelectAndEdit(
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~SelectAndEdit();

    protected:
        std::shared_ptr<SceneEngine::PlacementsEditor> _editor;
        std::vector<std::pair<uint64, uint64>> _activeSelection;
    };

    bool SelectAndEdit::OnInputEvent(
        const InputSnapshot& evnt,
        const HitTestResolver& hitTestContext)
    {
            //  On lbutton click, attempt to do hit detection
            //  select everything that intersects with the given ray
        if (evnt.IsRelease_LButton()) {
            auto worldSpaceRay = hitTestContext.CalculateWorldSpaceRay(evnt._mousePosition);
            auto selected = _editor->Find_RayIntersection(worldSpaceRay.first, worldSpaceRay.second);

                // replace the currently active selection
            _activeSelection = selected;
            return true;
        }

        return false;
    }

    class CommonOffscreenTarget
    {
    public:
        class Desc
        {
        public:
            unsigned _width, _height;
            RenderCore::Metal::NativeFormat::Enum _format;
            Desc(unsigned width, unsigned height, RenderCore::Metal::NativeFormat::Enum format)
                : _width(width), _height(height), _format(format) {}
        };

        RenderCore::Metal::RenderTargetView _rtv;
        RenderCore::Metal::ShaderResourceView _srv;
        intrusive_ptr<BufferUploads::ResourceLocator> _resource;

        CommonOffscreenTarget(const Desc& desc);
        ~CommonOffscreenTarget();
    };

    CommonOffscreenTarget::CommonOffscreenTarget(const Desc& desc)
    {
            //  Still some work involved to just create a texture
            //  
        using namespace BufferUploads;
        auto bufferDesc = CreateDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget, 0, GPUAccess::Write,
            TextureDesc::Plain2D(desc._width, desc._height, desc._format),
            "CommonOffscreen");

        auto resource = SceneEngine::GetBufferUploads()->Transaction_Immediate(bufferDesc, nullptr);

        RenderCore::Metal::RenderTargetView rtv(resource->GetUnderlying());
        RenderCore::Metal::ShaderResourceView srv(resource->GetUnderlying());

        _rtv = std::move(rtv);
        _srv = std::move(srv);
        _resource = std::move(resource);
    }

    CommonOffscreenTarget::~CommonOffscreenTarget() {}

    void SelectAndEdit::Render(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext)
    {
        if (!_activeSelection.empty()) {
                //  If we have some selection, we need to render it
                //  to an offscreen buffer, and we can perform some
                //  operation to highlight the objects in that buffer.
                //
                //  Note that we could get different results by doing
                //  this one placement at a time -- but we will most
                //  likely get the most efficient results by rendering
                //  all of objects that require highlights in one go.

            using namespace SceneEngine;
            using namespace RenderCore;
            SavedTargets savedTargets(context);
            const auto& viewport = savedTargets.GetViewports()[0];

            auto& offscreen = FindCachedBox<CommonOffscreenTarget>(
                CommonOffscreenTarget::Desc(unsigned(viewport.Width), unsigned(viewport.Height), 
                Metal::NativeFormat::R8G8B8A8_UNORM));

            context->Bind(MakeResourceList(offscreen._rtv), nullptr);
            context->Clear(offscreen._rtv, Float4(0.f, 0.f, 0.f, 0.f));
            _editor->RenderFiltered(
                context, parserContext, 0, 
                AsPointer(_activeSelection.cbegin()), AsPointer(_activeSelection.cend()));

            savedTargets.ResetToOldTargets(context);

                //  now we can render these objects over the main image, 
                //  using some filtering
        }
    }

    const char* SelectAndEdit::GetName() const                                            { return "Select And Edit"; }
    auto SelectAndEdit::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>  { return std::make_pair(nullptr, 0); }
    auto SelectAndEdit::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>    { return std::make_pair(nullptr, 0); }

    SelectAndEdit::SelectAndEdit(
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    {
        _editor = editor;
    }

    SelectAndEdit::~SelectAndEdit()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ScatterPlacements : public IManipulator
    {
    public:
        bool OnInputEvent(
            const InputSnapshot& evnt,
            const HitTestResolver& hitTestContext);
        void Render(
            RenderCore::Metal::DeviceContext* context,
            SceneEngine::LightingParserContext& parserContext);

        const char* GetName() const;
        std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        std::pair<BoolParameter*, size_t>   GetBoolParameters() const;

        ScatterPlacements(
            std::shared_ptr<SelectedModel> selectedModel,
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~ScatterPlacements();

    protected:
        Millisecond                     _spawnTimer;
        std::shared_ptr<SelectedModel>  _selectedModel;
        std::shared_ptr<SceneEngine::PlacementsEditor> _editor;

        float _radius;
        float _density;

        Float3 _hoverPoint;
        bool _hasHoverPoint;

        void PerformScatter(const Float3& centre, const char modelName[], const char materialName[]);
    };

    bool ScatterPlacements::OnInputEvent(
        const InputSnapshot& evnt,
        const HitTestResolver& hitTestContext)
    {
            //  If we get a click on the terrain, then we should perform 
            //  whatever placement operation is required (eg, creating new placements)
            //
            //  However, we need to do terrain collisions every time (because we want to
            //  move the highlight/preview position

        auto test = hitTestContext.DoHitTest(evnt._mousePosition);
        _hoverPoint = test._worldSpaceCollision;
        _hasHoverPoint = test._type == HitTestResolver::Result::Terrain;

        const Millisecond spawnTimeOut = 200;
        auto now = Millisecond_Now();
        if (evnt.IsHeld_LButton()) {
            if (test._type == HitTestResolver::Result::Terrain) {
                if (now >= (_spawnTimer + spawnTimeOut) && !_selectedModel->_modelName.empty()) {
                    PerformScatter(test._worldSpaceCollision, _selectedModel->_modelName.c_str(), "");
                    _spawnTimer = now;
                }
                return true;
            }
        } else { _spawnTimer = 0; }

        if (evnt._wheelDelta) {
            _radius = std::max(1.f, _radius + 3.f * evnt._wheelDelta / 120.f);
        }

        return false;
    }

    static float TriangleSignedArea(
        const Float2& pt1, const Float2& pt2, const Float2& pt3)
    {
        // reference:
        //  http://mathworld.wolfram.com/TriangleArea.html
        return .5f * (
            -pt2[0] * pt1[1] + pt3[0] * pt1[1] + pt1[0] * pt2[1]
            -pt3[0] * pt2[1] - pt1[0] * pt3[1] + pt2[0] * pt3[1]);
    }

    static bool PtInConvexPolygon(
        const unsigned* ptIndicesStart, const unsigned* ptIndicesEnd,
        const Float2 pts[], const Float2& pt)
    {
        unsigned posCount = 0, negCount = 0;
        for (auto* p=ptIndicesStart; (p+1)<ptIndicesEnd; ++p) {
            auto area = TriangleSignedArea(pts[*p], pts[*(p+1)], pt);
            if (area < 0.f) { ++negCount; } else { ++posCount; }
        }

        auto area = TriangleSignedArea(pts[*(ptIndicesEnd-1)], pts[*ptIndicesStart], pt);
        if (area < 0.f) { ++negCount; } else { ++posCount; }

            //  we're not making assumption about winding order. So 
            //  return true if every result is positive, or every result
            //  is negative; (in other words, one of the following must be zero)
        return posCount*negCount == 0;
    }

    static Float2 PtClosestToOrigin(const Float2& start, const Float2& end)
    {
        Float2 axis = end - start;
        float length = Magnitude(axis);
        Float2 dir = axis / length;
        float proj = Dot(-start, dir);
        return LinearInterpolate(start, end, Clamp(proj, 0.f, length));
    }

    /// <summary>Compare an axially aligned bounding box to a circle (in 2d, on the XY plane)</summary>
    /// This is a cheap alternative to box vs cylinder
    static bool AABBVsCircleInXY(
        float circleRadius, const std::pair<Float3, Float3>& boundingBox, const Float4x4& localToCircleSpace)
    {
        Float3 pts[] = 
        {
            Float3( boundingBox.first[0],  boundingBox.first[1],  boundingBox.first[2]),
            Float3(boundingBox.second[0],  boundingBox.first[1],  boundingBox.first[2]),
            Float3( boundingBox.first[0], boundingBox.second[1],  boundingBox.first[2]),
            Float3(boundingBox.second[0], boundingBox.second[1],  boundingBox.first[2]),
            Float3( boundingBox.first[0],  boundingBox.first[1], boundingBox.second[2]),
            Float3(boundingBox.second[0],  boundingBox.first[1], boundingBox.second[2]),
            Float3( boundingBox.first[0], boundingBox.second[1], boundingBox.second[2]),
            Float3(boundingBox.second[0], boundingBox.second[1], boundingBox.second[2])
        };

        Float2 testPts[dimof(pts)];
        for (unsigned c=0; c<dimof(pts); ++c) {
                // Z part is ignored completely
            testPts[c] = Truncate(TransformPoint(localToCircleSpace, pts[c]));
        }

        unsigned faces[6][4] = 
        {
            { 0, 1, 2, 3 },
            { 1, 5, 6, 2 },
            { 5, 4, 7, 6 },
            { 4, 0, 3, 7 },
            { 4, 5, 1, 0 },
            { 3, 2, 6, 7 },
        };
        
            //  We've made 6 rhomboids in 2D. We want to compare these to the circle
            //  at the origin, with the given radius.
            //  We need to find the point on the rhomboid that is closest to the origin.
            //  this will always lie on an edge (unless the origin is actually within the
            //  rhomboid). So we can just find the point on each edge is that closest to
            //  the origin, and see if that's within the radius.

        for (unsigned f=0; f<6; ++f) {
                //  first, is the origin within the rhumboid. Our pts are arranged in 
                //  winding order. So we can use a simple pt in convex polygon test.
            if (PtInConvexPolygon(faces[f], &faces[f][4], testPts, Float2(0.f, 0.f))) {
                return true; // pt is inside, so we have an interestion.
            }

            for (unsigned e=0; e<4; ++e) {
                auto edgeStart  = testPts[faces[f][e]];
                auto edgeEnd    = testPts[faces[f][(e+1)%4]];

                auto pt = PtClosestToOrigin(edgeStart, edgeEnd);
                if (MagnitudeSquared(pt) <= (circleRadius*circleRadius)) {
                    return true; // closest pt is within circle -- so return true
                }
            }
        }

        return false;
    }

    static Float2 Radial2Cart2D(float theta, float r)
    {
        auto t = XlSinCos(theta);
        return r * Float2(std::get<0>(t), std::get<1>(t));
    }

    static bool IsBlueNoiseGoodPoint(const std::vector<Float2>& existingPts, const Float2& testPt, float dRSq)
    {
        for (auto i=existingPts.begin(); i!=existingPts.end(); ++i) {
            if (MagnitudeSquared(testPt - *i) < dRSq) {
                return false;
            }
        }
        return true;
    }

    std::vector<Float2> GenerateBlueNoisePlacements(float radius, unsigned count)
    {
            //  Create new placements arranged in a equally spaced pattern
            //  around the circle.
            //  We're going to use a blue-noise pattern to calculate points
            //  in this circle. This method can be used for generate poisson
            //  disks. Here is the reference:
            //  http://www.cs.ubc.ca/~rbridson/docs/bridson-siggraph07-poissondisk.pdf

            //  We can use an expectation for the ratio of the inner circle radius to the
            //  outer radius to decide on the radius used for the poisson calculations.
            //
            //  From this page, we can see the "ideal" density values for the best circle
            //  packing results. Our method won't produce the ideal results, but so we
            //  need to use an expected density that is smaller.
            //  http://hydra.nat.uni-magdeburg.de/packing/cci/cci.html
        const float expectedDensity = 0.65f;

        const float bigCircleArea = gPI * radius * radius;
        const float littleCircleArea = bigCircleArea * expectedDensity / float(count);
        const float littleCircleRadius = sqrt(littleCircleArea / gPI);
        const float dRSq = 4*littleCircleRadius*littleCircleRadius;

        const unsigned k = 30;
        std::vector<Float2> workingSet;
        workingSet.reserve(count);
        workingSet.push_back(
            Radial2Cart2D(
                rand() * (2.f * gPI / float(RAND_MAX)),
                LinearInterpolate(.125f * radius, .25f * radius, rand() / float(RAND_MAX))));

        const unsigned iterationCount = 2 * count - 1;
        for (unsigned c=0; c<iterationCount && workingSet.size() < count; ++c) {
            assert(!workingSet.empty());
            unsigned index = rand() % unsigned(workingSet.size());

                // look for a good random connector
            bool gotGoodPt = false;
            float startAngle = rand() * (2.f * gPI / float(RAND_MAX));
            for (unsigned t=0; t<k; ++t) {
                Float2 pt = workingSet[index] + Radial2Cart2D(
                    startAngle + float(t) * (2.f * gPI / float(k)),
                    littleCircleRadius * (2.f + rand() / float(RAND_MAX)));

                if (MagnitudeSquared(pt) > radius * radius) {
                    continue;   // bad pt; outside of large radius. We need the centre to be within the large radius
                }

                    // note -- we can accelerate this test with 
                    //         a 2d lookup grid 
                if (IsBlueNoiseGoodPoint(workingSet, pt, dRSq)) {
                    gotGoodPt = true;
                    workingSet.push_back(pt);
                    break;
                }
            }

                // if we couldn't find a good connector, we have to erase the original pt 
            if (!gotGoodPt) {
                workingSet.erase(workingSet.begin() + index);

                    //  Note; there can be weird cases where the original point is remove
                    //  in these cases, we have to add back a new starter point. We can't
                    //  let the working state get empty
                if (workingSet.empty()) {
                    workingSet.push_back(
                        Radial2Cart2D(
                            rand() * (2.f * gPI / float(RAND_MAX)),
                            LinearInterpolate(.125f * radius, .25f * radius, rand() / float(RAND_MAX))));
                }
            }
        }

            // if we didn't get enough, we just have to insert randoms
        while (workingSet.size() < count) {
            auto rndpt = Radial2Cart2D(
                rand() * (2.f * gPI / float(RAND_MAX)),
                rand() * radius / float(RAND_MAX));
            workingSet.push_back(rndpt);
        }

        return std::move(workingSet);
    }

    void ScatterPlacements::PerformScatter(
        const Float3& centre, const char modelName[], const char materialName[])
    {
            // Our scatter algorithm is a little unique
            //  * find the number of objects with the same model & material within 
            //      a cylinder around the given point
            //  * either add or remove one from that total
            //  * create a new random distribution of items around the centre point
            //  * clamp those items to the terrain surface
            //  * delete the old items, and add those new items to the terrain
            // This way items will usually get scattered around in a good distribution
            // But it's very random. So the artist has only limited control.

        uint64 modelGuid = Hash64(modelName);
        auto oldPlacements = _editor->Find_BoxIntersection(
            centre - Float3(_radius, _radius, _radius),
            centre + Float3(_radius, _radius, _radius),
            [=](const SceneEngine::PlacementsEditor::ObjectDef& objectDef) -> bool
            {
                if (objectDef._model == modelGuid) {
                        // Make sure the object bounding box intersects with a cylinder around "centre"
                        // box vs cylinder is a little expensive. But since the cylinder axis is just +Z
                        // perhaps we could just treat this as a 2d problem, and just do circle vs rhomboid
                        //
                        // This won't produce the same result as cylinder vs box for the caps of the cylinder
                        //      -- but we don't really care in this case.
                    Float4x4 localToCircle = Combine(objectDef._localToWorld, AsFloat4x4(-centre));
                    return AABBVsCircleInXY(_radius, objectDef._localSpaceBoundingBox, localToCircle);
                }

                return false;
            });

            //  We have a list of placements using the same model, and within the placement area.
            //  We want to either add or remove one.

        _editor->DeletePlacements(oldPlacements);
            
            //  Note that the blur noise method we're using will probably not work 
            //  well with very small numbers of placements. So we're going to limit 
            //  the bottom range.

        float bigCircleArea = gPI * _radius * _radius;
        auto noisyPts = GenerateBlueNoisePlacements(_radius, unsigned(bigCircleArea*_density/100.f));

            //  Now add new placements for all of these pts.
            //  We need to clamp them to the terrain surface as we do this

        for (auto p=noisyPts.begin(); p!=noisyPts.end(); ++p) {
            Float2 pt = *p + Truncate(centre);
            float height = SceneEngine::GetTerrainHeight(
                *Sample::MainTerrainFormat.get(), Sample::MainTerrainConfig, Sample::MainTerrainCoords, 
                pt);

            auto objectToWorld = AsFloat4x4(Expand(pt, height));
            Combine_InPlace(RotationZ(rand() * 2.f * gPI / float(RAND_MAX)), objectToWorld);
            _editor->AddPlacement(AsFloat3x4(objectToWorld), modelName, materialName);
        }
    }

    void ScatterPlacements::Render(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext)
    {
        if (_hasHoverPoint) {
            RenderCylinderHighlight(context, parserContext, _hoverPoint, _radius);
        }
    }

    const char* ScatterPlacements::GetName() const  { return "ScatterPlace"; }

    auto ScatterPlacements::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>  
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&ScatterPlacements::_radius), 1.f, 100.f, FloatParameter::Logarithmic, "Size"),
            FloatParameter(ManipulatorParameterOffset(&ScatterPlacements::_density), 0.1f, 100.f, FloatParameter::Linear, "Density")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    auto ScatterPlacements::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>    { return std::make_pair(nullptr, 0); }

    ScatterPlacements::ScatterPlacements(
        std::shared_ptr<SelectedModel> selectedModel,
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    {
        _spawnTimer = 0;
        _selectedModel = std::move(selectedModel);
        _editor = std::move(editor);
        _hasHoverPoint = false;
        _hoverPoint = Float3(0.f, 0.f, 0.f);
        _radius = 20.f;
        _density = 1.f;
    }
    
    ScatterPlacements::~ScatterPlacements() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const auto Id_SelectedModel = InteractableId_Make("SelectedModel");
    static ButtonFormatting ButtonNormalState    ( ColorB(127, 192, 127,  64), ColorB(164, 192, 164, 255) );
    static ButtonFormatting ButtonMouseOverState ( ColorB(127, 192, 127,  64), ColorB(255, 255, 255, 160) );
    static ButtonFormatting ButtonPressedState   ( ColorB(127, 192, 127,  64), ColorB(255, 255, 255,  96) );

    void PlacementsWidgets::Render(
        IOverlayContext* context, Layout& layout, 
        Interactables& interactables, InterfaceState& interfaceState)
    {
        auto controlsRect = DrawManipulatorControls(
            context, layout, interactables, interfaceState,
            *_manipulators[_activeManipulatorIndex], "Placements tools");


            //      Our placement display is simple... 
            //
            //      Draw the name of the selected model at the bottom of
            //      the screen. If we click it, we should pop up the model browser
            //      so we can reselect.

        const auto lineHeight   = 20u;
        const auto horizPadding = 75u;
        const auto vertPadding  = 50u;
        const auto selectedRectHeight = lineHeight + 20u;
        auto maxSize = layout.GetMaximumSize();
        Rect selectedRect(
            Coord2(maxSize._topLeft[0] + horizPadding, maxSize._bottomRight[1] - vertPadding - selectedRectHeight),
            Coord2(std::min(maxSize._bottomRight[0], controlsRect._bottomRight[1]) - horizPadding, maxSize._bottomRight[1] - vertPadding));

        DrawButtonBasic(
            context, selectedRect, _selectedModel->_modelName.c_str(),
            FormatButton(interfaceState, Id_SelectedModel, 
                ButtonNormalState, ButtonMouseOverState, ButtonPressedState));
        interactables.Register(Interactables::Widget(selectedRect, Id_SelectedModel));

        if (_browser && _browserActive) {
            Rect browserRect(maxSize._topLeft, maxSize._bottomRight - Int2(0, vertPadding + selectedRectHeight));
            Layout browserLayout(browserRect);
            _browser->Render(context, browserLayout, interactables, interfaceState);
        }
    }

    bool PlacementsWidgets::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
        if (interfaceState.TopMostId() == Id_SelectedModel && input.IsRelease_LButton()) {
            _browserActive = !_browserActive;
            return true;
        }

        if (_browser && _browserActive) {
            auto result = _browser->SpecialProcessInput(interfaceState, input);
            if (!result._selectedModel.empty()) {
                _selectedModel->_modelName = result._selectedModel;
                _browserActive = false; // dismiss browser on select
            }

            if (result._consumed) { return true; }
        }

        if (input.IsRelease_LButton()) {
            const auto Id_SelectedManipulatorLeft = InteractableId_Make("SelectedManipulatorLeft");
            const auto Id_SelectedManipulatorRight = InteractableId_Make("SelectedManipulatorRight");
            auto topMost = interfaceState.TopMostWidget();
            if (topMost._id == Id_SelectedManipulatorLeft) {
                    // go back one manipulator
                _activeManipulatorIndex = (_activeManipulatorIndex + _manipulators.size() - 1) % _manipulators.size();
                return true;
            } else if (topMost._id == Id_SelectedManipulatorRight) {
                    // go forward one manipulator
                _activeManipulatorIndex = (_activeManipulatorIndex + 1) % _manipulators.size();
                return true;
            }
        }

        if (HandleManipulatorsControls(interfaceState, input, *_manipulators[_activeManipulatorIndex])) {
            return true;
        }

        if (interfaceState.GetMouseOverStack().empty()
            && _manipulators[_activeManipulatorIndex]->OnInputEvent(input, *_hitTestResolver)) {
            return true;
        }

        return false;
    }

    void PlacementsWidgets::RenderToScene(
        RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext)
    {
        _manipulators[_activeManipulatorIndex]->Render(context, parserContext);
    }

    PlacementsWidgets::PlacementsWidgets(
        std::shared_ptr<SceneEngine::PlacementsEditor> editor, 
        std::shared_ptr<HitTestResolver> hitTestResolver)
    {
        auto browser = std::make_shared<ModelBrowser>("game\\objects\\Env", editor->GetModelFormat());
        _browserActive = false;
        _activeManipulatorIndex = 0;

        auto selectedModel = std::make_shared<SelectedModel>();

        std::vector<std::unique_ptr<IManipulator>> manipulators;
        manipulators.push_back(std::make_unique<PlaceSingle>(selectedModel, editor));
        manipulators.push_back(std::make_unique<ScatterPlacements>(selectedModel, editor));
        manipulators.push_back(std::make_unique<SelectAndEdit>(editor));

        _editor = std::move(editor);
        _hitTestResolver = std::move(hitTestResolver);        
        _browser = std::move(browser);
        _manipulators = std::move(manipulators);
        _selectedModel = std::move(selectedModel);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PlacementsManipulatorsManager::Pimpl
    {
    public:
        std::shared_ptr<SceneEngine::PlacementsManager>     _placementsManager;
        std::shared_ptr<SceneEngine::PlacementsEditor>      _editor;
        std::shared_ptr<DebugScreensSystem>     _screens;
        std::shared_ptr<PlacementsWidgets>      _placementsDispl;
        std::shared_ptr<HitTestResolver>        _hitTestResolver;
    };

    void PlacementsManipulatorsManager::RenderWidgets(RenderCore::IDevice* device, const Float4x4& viewProjTransform)
    {
        _pimpl->_screens->Render(device, viewProjTransform);
    }

    void PlacementsManipulatorsManager::RenderToScene(
        RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext)
    {
        _pimpl->_placementsDispl->RenderToScene(context, parserContext);
    }

    auto PlacementsManipulatorsManager::GetInputLister() -> std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener>
    {
        return _pimpl->_screens;
    }

    PlacementsManipulatorsManager::PlacementsManipulatorsManager(
        std::shared_ptr<SceneEngine::PlacementsManager> placementsManager,
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<SceneEngine::ISceneParser> sceneParser,
        std::shared_ptr<SceneEngine::TechniqueContext> techniqueContext)
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_screens = std::make_shared<DebugScreensSystem>();
        pimpl->_placementsManager = std::move(placementsManager);
        pimpl->_editor = pimpl->_placementsManager->CreateEditor();
        pimpl->_hitTestResolver = std::make_shared<HitTestResolver>(terrainManager, sceneParser, techniqueContext);
        pimpl->_placementsDispl = std::make_shared<PlacementsWidgets>(pimpl->_editor, pimpl->_hitTestResolver);
        pimpl->_screens->Register(pimpl->_placementsDispl, "Placements", DebugScreensSystem::SystemDisplay);
        _pimpl = std::move(pimpl);
    }

    PlacementsManipulatorsManager::~PlacementsManipulatorsManager()
    {}

}

