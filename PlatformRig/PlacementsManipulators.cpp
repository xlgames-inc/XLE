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
#include "../SceneEngine/LightingParserContext.h"
#include "../SceneEngine/CommonResources.h"

#include "../Utility/TimeUtils.h"
#include "../Math/Transformations.h"
#include "../Math/Geometry.h"

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
        ~PlacementsWidgets();

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

    static Float4x4 CalculateWorldToProjection(const RenderCore::CameraDesc& sceneCamera, float viewportAspect)
    {
        auto projectionMatrix = RenderCore::PerspectiveProjection(
            sceneCamera._verticalFieldOfView, viewportAspect,
            sceneCamera._nearClip, sceneCamera._farClip, RenderCore::GeometricCoordinateSpace::RightHanded, 
            #if (GFXAPI_ACTIVE == GFXAPI_DX11) || (GFXAPI_ACTIVE == GFXAPI_DX9)
                RenderCore::ClipSpaceType::Positive);
            #else
                RenderCore::ClipSpaceType::StraddlingZero);
            #endif
        return Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), projectionMatrix);
    }

    std::pair<Float3, Float3> HitTestResolver::CalculateWorldSpaceRay(Int2 screenCoord) const
    {
        RECT clientRect; GetClientRect(GetActiveWindow(), &clientRect);
        float aspect = (clientRect.right - clientRect.left) / float(clientRect.bottom - clientRect.top);
        auto sceneCamera = _sceneParser->GetCameraDesc();
        auto worldToProjection = CalculateWorldToProjection(sceneCamera, aspect);

        Float3 frustumCorners[8];
        CalculateAbsFrustumCorners(frustumCorners, worldToProjection);
        Float3 cameraPosition = ExtractTranslation(sceneCamera._cameraToWorld);

        return RenderCore::BuildRayUnderCursor(
            screenCoord, frustumCorners, cameraPosition, 
            sceneCamera._nearClip, sceneCamera._farClip,
            std::make_pair(Float2(0.f, 0.f), Float2(float(clientRect.right - clientRect.left), float(clientRect.bottom - clientRect.top))));
    }

    Float2 HitTestResolver::ProjectToScreenSpace(const Float3& worldSpaceCoord) const
    {
        RECT clientRect; GetClientRect(GetActiveWindow(), &clientRect);
        float vWidth = float(clientRect.right - clientRect.left);
        float vHeight = float(clientRect.bottom - clientRect.top);
        auto worldToProjection = CalculateWorldToProjection(_sceneParser->GetCameraDesc(), vWidth / vHeight);
        auto projCoords = worldToProjection * Expand(worldSpaceCoord, 1.f);

        return Float2(
            (projCoords[0] / projCoords[3] * 0.5f + 0.5f) * vWidth,
            (projCoords[1] / projCoords[3] * -0.5f + 0.5f) * vHeight);
    }

    RenderCore::CameraDesc HitTestResolver::GetCameraDesc() const 
    { 
        return _sceneParser->GetCameraDesc();
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
        void SetActivationState(bool);

        SelectAndEdit(
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~SelectAndEdit();

    protected:
        std::shared_ptr<SceneEngine::PlacementsEditor>  _editor;
                  
        class SubOperation
        {
        public:
            enum Type { None, Translate, Scale, Rotate, MoveAcrossTerrainSurface };
            enum Axis { NoAxis, X, Y, Z };

            Type    _type;
            Float3  _parameter;
            Axis    _axisRestriction;
            Coord2  _cursorStart;
            HitTestResolver::Result _anchorTerrainIntersection;

            SubOperation() : _type(None), _parameter(0.f, 0.f, 0.f), _axisRestriction(NoAxis), _cursorStart(0, 0) {}
        };
        SubOperation _activeSubop;

        std::shared_ptr<SceneEngine::PlacementsEditor::ITransaction> _transaction;
        Float3 _anchorPoint;

        SceneEngine::PlacementsEditor::ObjTransDef TransformObject(
            const SceneEngine::PlacementsEditor::ObjTransDef& inputObj);
    };

    SceneEngine::PlacementsEditor::ObjTransDef SelectAndEdit::TransformObject(
        const SceneEngine::PlacementsEditor::ObjTransDef& inputObj)
    {
        Float4x4 transform;
        if (_activeSubop._type == SubOperation::Rotate) {
            transform = AsFloat4x4(-_anchorPoint);

            if (XlAbs(_activeSubop._parameter[0]) > 0.f) {
                Combine_InPlace(transform, RotationX(_activeSubop._parameter[0]));
            }

            if (XlAbs(_activeSubop._parameter[1]) > 0.f) {
                Combine_InPlace(transform, RotationY(_activeSubop._parameter[1]));
            }

            if (XlAbs(_activeSubop._parameter[2]) > 0.f) {
                Combine_InPlace(transform, RotationZ(_activeSubop._parameter[2]));
            }

            Combine_InPlace(transform, _anchorPoint);
        } else if (_activeSubop._type == SubOperation::Scale) {
            transform = AsFloat4x4(-_anchorPoint);
            Combine_InPlace(transform, ArbitraryScale(_activeSubop._parameter));
            Combine_InPlace(transform, _anchorPoint);
        } else if (_activeSubop._type == SubOperation::Translate) {
            transform = AsFloat4x4(_activeSubop._parameter);
        } else if (_activeSubop._type == SubOperation::MoveAcrossTerrainSurface) {
                //  move across terrain surface is a little different... 
                //  we have a 2d translation in XY. But then the Z values should be calculated
                //  from the terrain height.
            Float2 finalXY = Truncate(ExtractTranslation(inputObj._localToWorld)) + Truncate(_activeSubop._parameter);
            float terrainHeight = GetTerrainHeight(
                *Sample::MainTerrainFormat.get(), Sample::MainTerrainConfig, Sample::MainTerrainCoords, 
                finalXY);
            transform = AsFloat4x4(-ExtractTranslation(inputObj._localToWorld) + Expand(finalXY, terrainHeight));
        } else {
            return inputObj;
        }

        auto res = inputObj;
        res._localToWorld = Combine(res._localToWorld, transform);
        return res;
    }

    bool SelectAndEdit::OnInputEvent(
        const InputSnapshot& evnt,
        const HitTestResolver& hitTestContext)
    {
        bool consume = false;
        if (_transaction) {
            static const auto keyG = KeyId_Make("g");
            static const auto keyS = KeyId_Make("s");
            static const auto keyR = KeyId_Make("r");
            static const auto keyM = KeyId_Make("m");

            static const auto keyX = KeyId_Make("x");
            static const auto keyY = KeyId_Make("y");
            static const auto keyZ = KeyId_Make("z");

            SubOperation::Type newSubOp = SubOperation::None;
            if (evnt.IsPress(keyG)) { newSubOp = SubOperation::Translate; consume = true; }
            if (evnt.IsPress(keyS)) { newSubOp = SubOperation::Scale; consume = true; }
            if (evnt.IsPress(keyR)) { newSubOp = SubOperation::Rotate; consume = true; }
            if (evnt.IsPress(keyM)) { newSubOp = SubOperation::MoveAcrossTerrainSurface; consume = true; }

            if (newSubOp != SubOperation::None && newSubOp != _activeSubop._type) {
                    //  we have to "restart" the transaction. This returns everything
                    //  to it's original place
                _transaction->UndoAndRestart();
                _activeSubop._type = newSubOp;
                _activeSubop._parameter = Float3(0.f, 0.f, 0.f);
                _activeSubop._axisRestriction = SubOperation::NoAxis;
                _activeSubop._cursorStart = evnt._mousePosition;
                _activeSubop._anchorTerrainIntersection = HitTestResolver::Result();

                if (newSubOp == SubOperation::MoveAcrossTerrainSurface) {
                    _activeSubop._anchorTerrainIntersection = hitTestContext.DoHitTest(evnt._mousePosition);
                }
            }

            if (_activeSubop._type != SubOperation::None) {
                Float3 oldParameter = _activeSubop._parameter;

                if (evnt.IsPress(keyX)) { _activeSubop._axisRestriction = SubOperation::X; _activeSubop._parameter = Float3(0.f, 0.f, 0.f); consume = true; }
                if (evnt.IsPress(keyY)) { _activeSubop._axisRestriction = SubOperation::Y; _activeSubop._parameter = Float3(0.f, 0.f, 0.f); consume = true; }
                if (evnt.IsPress(keyZ)) { _activeSubop._axisRestriction = SubOperation::Z; _activeSubop._parameter = Float3(0.f, 0.f, 0.f); consume = true; }

                if (evnt._mouseDelta[0] || evnt._mouseDelta[1]) {
                        //  we always perform a manipulator's action in response to a mouse movement.
                    if (_activeSubop._type == SubOperation::Rotate) {

                            //  rotate by checking the angle of the mouse cursor relative to the 
                            //  anchor point (in screen space)
                        auto ssAnchor = hitTestContext.ProjectToScreenSpace(_anchorPoint);
                        float ssAngle1 = XlATan2(evnt._mousePosition[1] - ssAnchor[1], evnt._mousePosition[0] - ssAnchor[0]);
                        float ssAngle0 = XlATan2(_activeSubop._cursorStart[1] - ssAnchor[1], _activeSubop._cursorStart[0] - ssAnchor[0]);

                        unsigned axisIndex = 2;
                        switch (_activeSubop._axisRestriction) {
                        case SubOperation::X: axisIndex = 0; break;
                        case SubOperation::Y: axisIndex = 1; break;
                        case SubOperation::NoAxis:
                        case SubOperation::Z: axisIndex = 2; break;
                        }

                        _activeSubop._parameter = Float3(0.f, 0.f, 0.f);
                        _activeSubop._parameter[axisIndex] = ssAngle0 - ssAngle1;

                    } else if (_activeSubop._type == SubOperation::Scale) {

                            //  Scale based on the distance (in screen space) between the cursor
                            //  and the anchor point, and compare that to the distance when we
                            //  first started this operation

                        auto ssAnchor = hitTestContext.ProjectToScreenSpace(_anchorPoint);
                        float ssDist1 = Magnitude(evnt._mousePosition - ssAnchor);
                        float ssDist0 = Magnitude(_activeSubop._cursorStart - ssAnchor);
                        float scaleFactor = 1.f;
                        if (ssDist0 > 0.f) {
                            scaleFactor = ssDist1 / ssDist0;
                        }

                        switch (_activeSubop._axisRestriction) {
                        case SubOperation::X: _activeSubop._parameter = Float3(scaleFactor, 0.f, 0.f); break;
                        case SubOperation::Y: _activeSubop._parameter = Float3(0.f, scaleFactor, 0.f); break;
                        case SubOperation::Z: _activeSubop._parameter = Float3(0.f, 0.f, scaleFactor); break;
                        case SubOperation::NoAxis: _activeSubop._parameter = Float3(scaleFactor, scaleFactor, scaleFactor); break;
                        }

                    } else if (_activeSubop._type == SubOperation::Translate) {

                            //  We always translate across a 2d plane. So we need to define a plane 
                            //  based on the camera position and the anchor position.
                            //  We will calculate an intersection between a world space ray under the
                            //  cursor and that plane. That point (in 3d) will be the basis of the 
                            //  translation we apply.
                            //
                            //  The current "up" translation axis should lie flat on the plane. We
                            //  also want the camera "right" to lie close to the plane.

                        auto currentCamera = hitTestContext.GetCameraDesc();
                        Float3 upAxis = ExtractUp_Cam(currentCamera._cameraToWorld);
                        Float3 rightAxis = ExtractRight_Cam(currentCamera._cameraToWorld);
                        assert(Equivalent(MagnitudeSquared(upAxis), 1.f, 1e-6f));
                        assert(Equivalent(MagnitudeSquared(rightAxis), 1.f, 1e-6f));

                        switch (_activeSubop._axisRestriction) {
                        case SubOperation::X: upAxis = Float3(1.f, 0.f, 0.f); break;
                        case SubOperation::Y: upAxis = Float3(0.f, 1.f, 0.f); break;
                        case SubOperation::Z: upAxis = Float3(0.f, 0.f, 1.f); break;
                        }

                        rightAxis = rightAxis - upAxis * Dot(upAxis, rightAxis);
                        if (MagnitudeSquared(rightAxis) < 1e-6f) {
                            rightAxis = ExtractUp_Cam(currentCamera._cameraToWorld);
                            rightAxis = rightAxis - upAxis * Dot(upAxis, rightAxis);
                        }

                        Float3 planeNormal = Cross(upAxis, rightAxis);
                        Float4 plane = Expand(planeNormal, -Dot(planeNormal, _anchorPoint));

                        auto ray = hitTestContext.CalculateWorldSpaceRay(evnt._mousePosition);
                        float dst = RayVsPlane(ray.first, ray.second, plane);
                        if (dst >= 0.f && dst <= 1.f) {
                            auto intersectionPt = LinearInterpolate(ray.first, ray.second, dst);
                            float transRight = Dot(intersectionPt - _anchorPoint, rightAxis);
                            float transUp = Dot(intersectionPt - _anchorPoint, upAxis);

                            switch (_activeSubop._axisRestriction) {
                            case SubOperation::X: _activeSubop._parameter = Float3(transUp, 0.f, 0.f); break;
                            case SubOperation::Y: _activeSubop._parameter = Float3(0.f, transUp, 0.f); break;
                            case SubOperation::Z: _activeSubop._parameter = Float3(0.f, 0.f, transUp); break;
                            case SubOperation::NoAxis:
                                _activeSubop._parameter = transRight * rightAxis + transUp * upAxis;
                                break;
                            }
                        }

                    } else if (_activeSubop._type == SubOperation::MoveAcrossTerrainSurface) {

                            //  We want to find an intersection point with the terrain, and then 
                            //  compare the XY coordinates of that to the anchor point

                        auto collision = hitTestContext.DoHitTest(evnt._mousePosition);
                        if (collision._type == HitTestResolver::Result::Terrain
                            && _activeSubop._anchorTerrainIntersection._type == HitTestResolver::Result::Terrain) {
                            _activeSubop._parameter = Float3(
                                collision._worldSpaceCollision[0] - _anchorPoint[0],
                                collision._worldSpaceCollision[1] - _anchorPoint[1],
                                0.f);
                        }

                    }
                }

                if (_activeSubop._parameter != oldParameter) {
                        // push these changes onto the transaction
                    unsigned count = _transaction->GetObjectCount();
                    for (unsigned c=0; c<count; ++c) {
                        auto& originalState = _transaction->GetObjectOriginalState(c);
                        auto newState = TransformObject(originalState);
                        _transaction->SetObject(c, newState);
                    }
                }
            }
        }
        

            //  On lbutton click, attempt to do hit detection
            //  select everything that intersects with the given ray
        if (evnt.IsRelease_LButton()) {

            if (_activeSubop._type != SubOperation::None) {

                if (_transaction) { 
                    _transaction->Commit();  
                    _transaction.reset();
                }
                _activeSubop._type = SubOperation::None;

            } else {

                auto worldSpaceRay = hitTestContext.CalculateWorldSpaceRay(evnt._mousePosition);
                auto selected = _editor->Find_RayIntersection(worldSpaceRay.first, worldSpaceRay.second);

                    // replace the currently active selection
                if (_transaction) {
                    _transaction->Commit();
                }
                _transaction = _editor->Transaction_Begin(AsPointer(selected.cbegin()), AsPointer(selected.cend()));

                    //  Reset the anchor point
                    //  There are a number of different possible ways we could calculate
                    //      the anchor point... But let's find the world space bounding box
                    //      that encloses all of the objects and get the centre of that box.
                Float3 totalMins(FLT_MAX, FLT_MAX, FLT_MAX), totalMaxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);
                unsigned objCount = _transaction->GetObjectCount();
                for (unsigned c=0; c<objCount; ++c) {
                    auto obj = _transaction->GetObject(c);
                    auto localBoundingBox = _transaction->GetLocalBoundingBox(c);
                    auto worldSpaceBounding = TransformBoundingBox(AsFloat3x4(obj._localToWorld), localBoundingBox);
                    totalMins[0] = std::min(worldSpaceBounding.first[0], totalMins[0]);
                    totalMins[1] = std::min(worldSpaceBounding.first[1], totalMins[1]);
                    totalMins[2] = std::min(worldSpaceBounding.first[2], totalMins[2]);
                    totalMaxs[0] = std::max(worldSpaceBounding.second[0], totalMaxs[0]);
                    totalMaxs[1] = std::max(worldSpaceBounding.second[1], totalMaxs[1]);
                    totalMaxs[2] = std::max(worldSpaceBounding.second[2], totalMaxs[2]);
                }
                _anchorPoint = LinearInterpolate(totalMins, totalMaxs, 0.5f);

            }

            return true;
        }

        return consume;
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

    class HighlightShaders
    {
    public:
        class Desc {};

        RenderCore::Metal::ShaderProgram* _drawHighlight;
        RenderCore::Metal::BoundUniforms _drawHighlightUniforms;

        const Assets::DependencyValidation& GetDependancyValidation() const   { return *_validationCallback; }

        HighlightShaders(const Desc&);
    protected:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };

    HighlightShaders::HighlightShaders(const Desc&)
    {
        auto* drawHighlight = &::Assets::GetAssetDep<RenderCore::Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/effects/outlinehighlight.psh:main:ps_*");

        RenderCore::Metal::BoundUniforms uniforms(*drawHighlight);
        SceneEngine::TechniqueContext::BindGlobalUniforms(uniforms);

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, &drawHighlight->GetDependancyValidation());

        _validationCallback = std::move(validationCallback);
        _drawHighlight = std::move(drawHighlight);
        _drawHighlightUniforms = std::move(uniforms);
    }

    static void RenderHighlight(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext,
        SceneEngine::PlacementsEditor* editor,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd)
    {
        TRY {
            using namespace SceneEngine;
            using namespace RenderCore;
            SavedTargets savedTargets(context);
            const auto& viewport = savedTargets.GetViewports()[0];

            auto& offscreen = FindCachedBox<CommonOffscreenTarget>(
                CommonOffscreenTarget::Desc(unsigned(viewport.Width), unsigned(viewport.Height), 
                Metal::NativeFormat::R8G8B8A8_UNORM));

            context->Bind(MakeResourceList(offscreen._rtv), nullptr);
            context->Clear(offscreen._rtv, Float4(0.f, 0.f, 0.f, 0.f));
            context->Bind(RenderCore::Metal::Topology::TriangleList);
            editor->RenderFiltered(context, parserContext, 0, filterBegin, filterEnd);

            savedTargets.ResetToOldTargets(context);

                //  now we can render these objects over the main image, 
                //  using some filtering

            context->BindPS(MakeResourceList(offscreen._srv));

            auto& shaders = FindCachedBoxDep<HighlightShaders>(HighlightShaders::Desc());
            shaders._drawHighlightUniforms.Apply(
                *context, 
                parserContext.GetGlobalUniformsStream(), RenderCore::Metal::UniformsStream());
            context->Bind(*shaders._drawHighlight);
            context->Bind(SceneEngine::CommonResources()._blendAlphaPremultiplied);
            context->Bind(SceneEngine::CommonResources()._dssDisable);
            context->Bind(RenderCore::Metal::Topology::TriangleStrip);
            context->Draw(4);
        } 
        CATCH (const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); } 
        CATCH (const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); } 
        CATCH_END
    }

    void SelectAndEdit::Render(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext)
    {
        std::vector<std::pair<uint64, uint64>> activeSelection;

        if (_transaction) {
            activeSelection.reserve(_transaction->GetObjectCount());
            for (unsigned c=0; c<_transaction->GetObjectCount(); ++c) {
                activeSelection.push_back(_transaction->GetGuid(c));
            }
        }

        if (!activeSelection.empty()) {
                //  If we have some selection, we need to render it
                //  to an offscreen buffer, and we can perform some
                //  operation to highlight the objects in that buffer.
                //
                //  Note that we could get different results by doing
                //  this one placement at a time -- but we will most
                //  likely get the most efficient results by rendering
                //  all of objects that require highlights in one go.
            RenderHighlight(
                context, parserContext, _editor.get(),
                AsPointer(activeSelection.begin()), AsPointer(activeSelection.end()));
        }
    }

    const char* SelectAndEdit::GetName() const                                            { return "Select And Edit"; }
    auto SelectAndEdit::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>  { return std::make_pair(nullptr, 0); }
    auto SelectAndEdit::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>    { return std::make_pair(nullptr, 0); }
    void SelectAndEdit::SetActivationState(bool) 
    {
        if (_transaction) {
            _transaction->Cancel();
            _transaction.reset();
        }
        _activeSubop._type = SubOperation::None;
        _anchorPoint = Float3(0.f, 0.f, 0.f);
    }

    SelectAndEdit::SelectAndEdit(
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    {
        _editor = editor;
        _anchorPoint = Float3(0.f, 0.f, 0.f);
    }

    SelectAndEdit::~SelectAndEdit()
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
        void SetActivationState(bool);

        PlaceSingle(
            std::shared_ptr<SelectedModel> selectedModel,
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~PlaceSingle();

    protected:
        Millisecond                     _spawnTimer;
        std::shared_ptr<SelectedModel>  _selectedModel;
        std::shared_ptr<SceneEngine::PlacementsEditor> _editor;
        unsigned                        _rendersSinceHitTest;

        std::shared_ptr<SceneEngine::PlacementsEditor::ITransaction> _transaction;

        void MoveObject(const Float3& newLocation);
    };

    void PlaceSingle::MoveObject(const Float3& newLocation)
    {
        if (!_transaction) {
            _transaction = _editor->Transaction_Begin(nullptr, nullptr);
        }

        SceneEngine::PlacementsEditor::ObjTransDef newState(
            AsFloat4x4(newLocation), _selectedModel->_modelName, std::string());

        TRY {
            if (!_transaction->GetObjectCount()) {
                _transaction->Create(newState);
            } else {
                _transaction->SetObject(0, newState);
            }
        } CATCH (...) {
        } CATCH_END
    }

    bool PlaceSingle::OnInputEvent(
        const InputSnapshot& evnt,
        const HitTestResolver& hitTestContext)
    {
        //  If we get a click on the terrain, then we should perform 
            //  whatever placement operation is required (eg, creating new placements)
        if (_selectedModel->_modelName.empty()) {
            if (_transaction) {
                _transaction->Cancel();
                _transaction.reset();
            }
            return false;
        }

        if (_rendersSinceHitTest > 0) {
            _rendersSinceHitTest = 0;

            auto test = hitTestContext.DoHitTest(evnt._mousePosition);
            if (test._type == HitTestResolver::Result::Terrain) {

                    //  This is a spawn event. We should add a new item of the selected model
                    //  at the point clicked.
                MoveObject(test._worldSpaceCollision);
            }
        }

        if (evnt.IsRelease_LButton()) {
            if (_transaction) {
                _transaction->Commit();
                _transaction.reset();
            }
        }

        return false;
    }

    void PlaceSingle::Render(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext)
    {
        ++_rendersSinceHitTest;
        if (_transaction && _transaction->GetObjectCount()) {
            std::vector<SceneEngine::PlacementGUID> objects;
            objects.reserve(_transaction->GetObjectCount());
            for (unsigned c=0; c<_transaction->GetObjectCount(); ++c) {
                objects.push_back(_transaction->GetGuid(c));
            }

            RenderHighlight(
                context, parserContext, _editor.get(),
                AsPointer(objects.begin()), AsPointer(objects.end()));
        }
    }

    const char* PlaceSingle::GetName() const                                            { return "Place single"; }
    auto PlaceSingle::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>  { return std::make_pair(nullptr, 0); }
    auto PlaceSingle::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>    { return std::make_pair(nullptr, 0); }
    void PlaceSingle::SetActivationState(bool) 
    {
        if (_transaction) {
            _transaction->Cancel();
            _transaction.reset();
        }
    }

    PlaceSingle::PlaceSingle(
        std::shared_ptr<SelectedModel> selectedModel,
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    {
        _spawnTimer = 0;
        _selectedModel = std::move(selectedModel);
        _editor = std::move(editor);
        _rendersSinceHitTest = 0;
    }
    
    PlaceSingle::~PlaceSingle() {}

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
        void SetActivationState(bool);

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

            // todo --  this could benefit from a "relax" phase that would just shift
            //          things into a more evenly spaced arrangement

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
            [=](const SceneEngine::PlacementsEditor::ObjIntersectionDef& objectDef) -> bool
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

        auto trans = _editor->Transaction_Begin(AsPointer(oldPlacements.cbegin()), AsPointer(oldPlacements.cend()));
        for (unsigned c=0; c<trans->GetObjectCount(); ++c) {
            trans->Delete(c);
        }
            
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
            trans->Create(SceneEngine::PlacementsEditor::ObjTransDef(
                objectToWorld, modelName, materialName));
        }

        trans->Commit();
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

    void ScatterPlacements::SetActivationState(bool) {}

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
            auto newManipIndex = _activeManipulatorIndex;
            if (topMost._id == Id_SelectedManipulatorLeft) {
                    // go back one manipulator
                newManipIndex = (_activeManipulatorIndex + _manipulators.size() - 1) % _manipulators.size();
            } else if (topMost._id == Id_SelectedManipulatorRight) {
                    // go forward one manipulator
                newManipIndex = (_activeManipulatorIndex + 1) % _manipulators.size();
            }
            if (newManipIndex != _activeManipulatorIndex) {
                _manipulators[_activeManipulatorIndex]->SetActivationState(false);
                _activeManipulatorIndex = newManipIndex;
                _manipulators[_activeManipulatorIndex]->SetActivationState(true);
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
        selectedModel->_modelName = "game\\objects\\Env\\02_harihara\\001_hdeco\\backpack_mockup.cgf";

        std::vector<std::unique_ptr<IManipulator>> manipulators;
        manipulators.push_back(std::make_unique<PlaceSingle>(selectedModel, editor));
        manipulators.push_back(std::make_unique<ScatterPlacements>(selectedModel, editor));
        manipulators.push_back(std::make_unique<SelectAndEdit>(editor));

        manipulators[0]->SetActivationState(true);

        _editor = std::move(editor);
        _hitTestResolver = std::move(hitTestResolver);        
        _browser = std::move(browser);
        _manipulators = std::move(manipulators);
        _selectedModel = std::move(selectedModel);
    }
    
    PlacementsWidgets::~PlacementsWidgets()
    {
        TRY { 
            _manipulators[_activeManipulatorIndex]->SetActivationState(false);
        } CATCH (...) {
        } CATCH_END
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

