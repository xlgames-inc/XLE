// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainManipulators.h"
#include "ManipulatorsUtil.h"
#include "../RenderOverlays/DebuggingDisplay.h"
#include "../RenderOverlays/Font.h"

#include "../SceneEngine/LightingParserContext.h"
#include "../SceneEngine/LightingParser.h"
#include "../SceneEngine/SceneParser.h"
#include "../SceneEngine/Terrain.h"
#include "../SceneEngine/TerrainUberSurface.h"
#include "../SceneEngine/SceneEngineUtility.h"
#include "../SceneEngine/IntersectionTest.h"

#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/ResourceBox.h"

#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/RenderUtils.h"

#include "../Math/ProjectionMath.h"
#include "../Math/Transformations.h"
#include "../Utility/TimeUtils.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"

extern unsigned FrameRenderCount;

namespace Tools
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //      M A N I P U L A T O R S             //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static Float2 RoundDownToInteger(Float2 input)
    {
        return Float2(XlFloor(input[0] + 0.5f), XlFloor(input[1] + 0.5f));
    }

    static Float2 RoundUpToInteger(Float2 input)
    {
        return Float2(XlCeil(input[0] - 0.5f), XlCeil(input[1] - 0.5f));
    }

    using SceneEngine::IntersectionTestContext;
    using SceneEngine::IntersectionTestScene;
    static std::pair<Float3, bool> FindTerrainIntersection(
        const IntersectionTestContext& context, const IntersectionTestScene& scene,
        const Int2 screenCoords)
    {
        auto result = scene.UnderCursor(context, screenCoords, IntersectionTestScene::Type::Terrain);
        if (result._type == IntersectionTestScene::Type::Terrain) {
            return std::make_pair(result._worldSpaceCollision, true);
        }
        return std::make_pair(Float3(0.f, 0.f, 0.f), false);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    IManipulator::~IManipulator() {}

    class CommonManipulator : public IManipulator
    {
    public:
            // IManipulator interface
        virtual bool    OnInputEvent(
            const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt, 
            const IntersectionTestContext& hitTestContext,
            const IntersectionTestScene& hitTestScene);
        virtual void    Render(RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext);

        virtual void    PerformAction(const Float3& worldSpacePosition, float size, float strength) = 0;
        virtual void    SetActivationState(bool) {}
        virtual std::string GetStatusText() const { return std::string(); }

        CommonManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);

    protected:
        std::pair<Float3, bool> _currentWorldSpaceTarget;
        std::pair<Float3, bool> _targetOnMouseDown;
        std::shared_ptr<SceneEngine::TerrainManager> _terrainManager;
        Int2        _mouseCoords;
        float       _strength;
        float       _size;
        unsigned    _lastPerform;
        unsigned    _lastRenderCount0, _lastRenderCount1;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    bool    CommonManipulator::OnInputEvent(
        const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt, 
        const IntersectionTestContext& hitTestContext,
        const IntersectionTestScene& hitTestScene)
    {
        const bool shiftHeld = evnt.IsHeld(RenderOverlays::DebuggingDisplay::KeyId_Make("shift"));
        if (evnt._wheelDelta) {
                // on wheel delta, change effect size
            if (shiftHeld) {
                _strength = std::max(0.f, _strength + 3.f * evnt._wheelDelta / 120.f);
            } else {
                _size = std::max(1.f, _size + 3.f * evnt._wheelDelta / 120.f);
            }
        }

            //  We can do the terrain intersection test now, but it requires setting up 
            //  new device and lighting parser contexts. We need to know the viewport -- and the only way 
            //  to do that is to get it from the windows HWND!
        Int2 newMouseCoords(evnt._mousePosition[0], evnt._mousePosition[1]);
            // only do the terrain test if we get some kind of movement
        if (((XlAbs(_mouseCoords[0] - newMouseCoords[0]) > 1 || XlAbs(_mouseCoords[1] - newMouseCoords[1]) > 1)
            && (FrameRenderCount > _lastRenderCount0)) || evnt.IsPress_LButton()) {

            _currentWorldSpaceTarget = FindTerrainIntersection(hitTestContext, hitTestScene, newMouseCoords);
            _lastPerform = 0;
            _mouseCoords = newMouseCoords;
            _lastRenderCount0 = FrameRenderCount;

            if (evnt.IsPress_LButton()) {
                _targetOnMouseDown = _currentWorldSpaceTarget;
            }
        }

        if (evnt.IsHeld_LButton()) {
                // perform action -- (like raising or lowering the terrain)
            if (_currentWorldSpaceTarget.second && (Millisecond_Now() - _lastPerform) > 33 && (FrameRenderCount > _lastRenderCount1)) {

                TRY {
                    PerformAction(_currentWorldSpaceTarget.first, _size, shiftHeld?(-_strength):_strength);
                } CATCH (...) {
                } CATCH_END
                
                _lastPerform = Millisecond_Now();
                _lastRenderCount1 = FrameRenderCount;
            }
            return true;
        }
        return false;
    }

    void RenderCylinderHighlight(
        RenderCore::Metal::DeviceContext* context, 
        SceneEngine::LightingParserContext& parserContext,
        Float3& centre, float radius)
    {
        using namespace RenderCore::Metal;
            // unbind the depth buffer
        SceneEngine::SavedTargets savedTargets(context);
        context->GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);

            // create shader resource view for the depth buffer
        ShaderResourceView depthSrv;
        if (savedTargets.GetDepthStencilView())
            depthSrv = ShaderResourceView(ExtractResource<ID3D::Resource>(
                savedTargets.GetDepthStencilView()).get(), 
                (NativeFormat::Enum)DXGI_FORMAT_R24_UNORM_X8_TYPELESS);     // note -- assuming D24S8 depth buffer! We need a better way to get the depth srv

        TRY
        {
                // note -- we might need access to the MSAA defines for this shader
            auto& shaderProgram = Assets::GetAssetDep<ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*",
                "game/xleres/ui/terrainmanipulators.sh:ps_circlehighlight:ps_*");
            
            struct HighlightParameters
            {
                Float3 _center;
                float _radius;
            } highlightParameters = { centre, radius };
            ConstantBufferPacket constantBufferPackets[2];
            constantBufferPackets[0] = RenderCore::MakeSharedPkt(highlightParameters);

            auto& circleHighlight = Assets::GetAssetDep<DeferredShaderResource>("game/xleres/DefaultResources/circlehighlight.png");
            const ShaderResourceView* resources[] = { &depthSrv, &circleHighlight.GetShaderResource() };

            BoundUniforms boundLayout(shaderProgram);
            RenderCore::Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);
            boundLayout.BindConstantBuffer(Hash64("CircleHighlightParameters"), 0, 1);
            boundLayout.BindShaderResource(Hash64("DepthTexture"), 0, 1);
            boundLayout.BindShaderResource(Hash64("HighlightResource"), 1, 1);

            context->Bind(shaderProgram);
            boundLayout.Apply(*context, 
                parserContext.GetGlobalUniformsStream(),
                UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets), resources, dimof(resources)));

            context->Bind(RenderCore::Techniques::CommonResources()._blendAlphaPremultiplied);
            context->Bind(RenderCore::Techniques::CommonResources()._dssDisable);
            context->Bind(Topology::TriangleStrip);
            context->GetUnderlying()->IASetInputLayout(nullptr);

                // note --  this will render a full screen quad. we could render cylinder geometry instead,
                //          because this decal only affects the area within a cylinder. But it's just for
                //          tools, so the easy way should be fine.
            context->Draw(4);

            ID3D::ShaderResourceView* srv = nullptr;
            context->GetUnderlying()->PSSetShaderResources(3, 1, &srv);
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH(...) {} 
        CATCH_END

        savedTargets.ResetToOldTargets(context);
    }

    void    CommonManipulator::Render(
                    RenderCore::Metal::DeviceContext* context, 
                    SceneEngine::LightingParserContext& parserContext)
    {
            //  Draw a highlight on the area that we're going to modify. Since we want this to behave like a decal, 
            //  it's best to do this rendering after the gbuffer is fully prepared, and we should render onto the
            //  lighting buffer.
            //  In theory, we could stencil out the terrain, as well -- so we only actually draw onto terrain 
            //  geometry.
        if (_currentWorldSpaceTarget.second) {
            RenderCylinderHighlight(context, parserContext, _currentWorldSpaceTarget.first, _size);
        }
    }

    CommonManipulator::CommonManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : _terrainManager(std::move(terrainManager))
    {
        _currentWorldSpaceTarget = std::make_pair(Float3(0,0,0), false);
        _targetOnMouseDown = std::make_pair(Float3(0,0,0), false);
        _mouseCoords = Int2(0,0);
        _strength = 1.f;
        _size = 20.f;
        _lastPerform = 0;
        _lastRenderCount0 = _lastRenderCount1 = 0;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class RaiseLowerManipulator : public CommonManipulator
    {
    public:
        virtual void    PerformAction(const Float3& worldSpacePosition, float size, float strength);
        virtual const char* GetName() const { return "Raise and Lower"; }

        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const { return std::make_pair(nullptr, 0); }

        RaiseLowerManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);
    private:
        float _powerValue;
    };

    void    RaiseLowerManipulator::PerformAction(const Float3& worldSpacePosition, float size, float strength)
    {
            //
            //      Use the uber surface interface to change these values
            //          -- this will make sure all of the cells get updated as needed
            //
        auto *i = _terrainManager->GetUberSurfaceInterface();
        if (i) {
            i->AdjustHeights(_terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(worldSpacePosition)), size, .05f * strength, _powerValue);
        }
    }

    auto RaiseLowerManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&RaiseLowerManipulator::_strength), 1.f, 100.f, FloatParameter::Logarithmic, "Strength"),
            FloatParameter(ManipulatorParameterOffset(&RaiseLowerManipulator::_size), 0.1f, 500.f, FloatParameter::Linear, "Size"),
            FloatParameter(ManipulatorParameterOffset(&RaiseLowerManipulator::_powerValue), 1.f/8.f, 8.f, FloatParameter::Linear, "ShapeControl")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    RaiseLowerManipulator::RaiseLowerManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : CommonManipulator(std::move(terrainManager))
    {
        _powerValue = 1.f/8.f;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class SmoothManipulator : public CommonManipulator
    {
    public:
        virtual void    PerformAction(const Float3& worldSpacePosition, float size, float strength);
        virtual const char* GetName() const { return "Smooth"; }

        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const;

        SmoothManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);
    private:
        float _standardDeviation;
        float _filterRadius;
        unsigned _flags;
    };

    void    SmoothManipulator::PerformAction(const Float3& worldSpacePosition, float size, float strength)
    {
        auto *i = _terrainManager->GetUberSurfaceInterface();
        if (i) {
            i->Smooth(_terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(worldSpacePosition)), size, unsigned(_filterRadius), _standardDeviation, _strength, _flags);
        }
    }

    auto SmoothManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&SmoothManipulator::_size), 0.1f, 500.f, FloatParameter::Linear, "Size"),
            FloatParameter(ManipulatorParameterOffset(&SmoothManipulator::_standardDeviation), 1.f, 6.f, FloatParameter::Linear, "Blurriness"),
            FloatParameter(ManipulatorParameterOffset(&SmoothManipulator::_filterRadius), 2.f, 16.f, FloatParameter::Linear, "FilterRadius"),
            FloatParameter(ManipulatorParameterOffset(&SmoothManipulator::_strength), 0.01f, 1.f, FloatParameter::Linear, "Strength")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    auto SmoothManipulator::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>
    {
        static BoolParameter parameters[] = 
        {
            BoolParameter(ManipulatorParameterOffset(&SmoothManipulator::_flags), 0, "SmoothUp"),
            BoolParameter(ManipulatorParameterOffset(&SmoothManipulator::_flags), 1, "SmoothDown")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    SmoothManipulator::SmoothManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : CommonManipulator(std::move(terrainManager))
    {
        _standardDeviation = 3.f;
        _filterRadius = 16.f;
        _flags = 0x3;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class NoiseManipulator : public CommonManipulator
    {
    public:
        virtual void    PerformAction(const Float3& worldSpacePosition, float size, float strength);
        virtual const char* GetName() const { return "Add Noise"; }

        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const { return std::make_pair(nullptr, 0); }

        NoiseManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);
    };

    void    NoiseManipulator::PerformAction(const Float3& worldSpacePosition, float size, float strength)
    {
        auto *i = _terrainManager->GetUberSurfaceInterface();
        if (i) {
            i->AddNoise(_terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(worldSpacePosition)), size, .05f * strength);
        }
    }

    auto NoiseManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&NoiseManipulator::_strength), 1.f, 100.f, FloatParameter::Logarithmic, "Strength"),
            FloatParameter(ManipulatorParameterOffset(&NoiseManipulator::_size), 0.1f, 500.f, FloatParameter::Linear, "Size"),
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    NoiseManipulator::NoiseManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : CommonManipulator(std::move(terrainManager))
    {}

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class CopyHeight : public CommonManipulator
    {
    public:
        virtual void    PerformAction(const Float3& worldSpacePosition, float size, float strength);
        virtual const char* GetName() const { return "Copy Height"; }

        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const;

        CopyHeight(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);
    private:
        float _powerValue;
        unsigned _flags;
    };

    void CopyHeight::PerformAction(const Float3& worldSpacePosition, float size, float strength)
    {
        auto *i = _terrainManager->GetUberSurfaceInterface();
        if (i && _targetOnMouseDown.second) {
            i->CopyHeight(  _terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(worldSpacePosition)), 
                            _terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(_targetOnMouseDown.first)), 
                            size, strength, _powerValue, _flags);
        }
    }

    auto CopyHeight::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&CopyHeight::_strength), 1.f, 100.f, FloatParameter::Logarithmic, "Strength"),
            FloatParameter(ManipulatorParameterOffset(&CopyHeight::_size), 0.1f, 500.f, FloatParameter::Linear, "Size"),
            FloatParameter(ManipulatorParameterOffset(&CopyHeight::_powerValue), 1.f/8.f, 8.f, FloatParameter::Linear, "ShapeControl")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    auto CopyHeight::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>
    {
        static BoolParameter parameters[] = 
        {
            BoolParameter(ManipulatorParameterOffset(&CopyHeight::_flags), 0, "MoveUp"),
            BoolParameter(ManipulatorParameterOffset(&CopyHeight::_flags), 1, "MoveDown")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    CopyHeight::CopyHeight(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : CommonManipulator(std::move(terrainManager))
    {
        _powerValue = 1.f/8.f;
        _flags = 0x3;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class RectangleManipulator : public IManipulator
    {
    public:
            // IManipulator interface
        virtual bool    OnInputEvent(
            const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt, 
            const IntersectionTestContext& hitTestContext,
            const IntersectionTestScene& hitTestScene);
        virtual void    Render(RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext);

        virtual void    PerformAction(const Float3& anchor0, const Float3& anchor1) = 0;
        
        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const { return std::make_pair(nullptr, 0); }
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const { return std::make_pair(nullptr, 0); }
        virtual void SetActivationState(bool) {}
        virtual std::string GetStatusText() const { return std::string(); }

        RectangleManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);

    protected:
        Float3  _firstAnchor;
        bool    _isDragging;
        std::pair<Float3, bool> _secondAnchor;
        std::shared_ptr<SceneEngine::TerrainManager> _terrainManager;
    };

    bool    RectangleManipulator::OnInputEvent(
        const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt, 
        const IntersectionTestContext& hitTestContext,
        const IntersectionTestScene& hitTestScene)
    {
        Int2 mousePosition(evnt._mousePosition[0], evnt._mousePosition[1]);

        if (evnt.IsPress_LButton()) {
                // on lbutton press, we should place a new anchor
            auto intersection = FindTerrainIntersection(hitTestContext, hitTestScene, mousePosition);
            _isDragging = intersection.second;
            if (intersection.second) {
                _firstAnchor = intersection.first;
                _secondAnchor = intersection;
            }
        }

        if (_isDragging) {

            if (evnt.IsHeld_LButton() || evnt.IsRelease_LButton()) {
                    // update the second anchor as we drag
                _secondAnchor = FindTerrainIntersection(hitTestContext, hitTestScene, mousePosition);
            }

            if (evnt.IsRelease_LButton()) {
                    // on release, we should perform the action
                    //  (assuming we released at a valid position)
                _isDragging = false;

                if (_secondAnchor.second) {

                    auto& coords = _terrainManager->GetCoords();
                    Float2 faTerrain = coords.WorldSpaceToTerrainCoords(Truncate(_firstAnchor));
                    Float2 fsTerrain = coords.WorldSpaceToTerrainCoords(Truncate(_secondAnchor.first));
                    Float2 terrainCoordsMins(std::min(faTerrain[0], fsTerrain[0]), std::min(faTerrain[1], fsTerrain[1]));
                    Float2 terrainCoordsMaxs(std::max(faTerrain[0], fsTerrain[0]), std::max(faTerrain[1], fsTerrain[1]));
                    Float2 faWorld = coords.TerrainCoordsToWorldSpace(RoundDownToInteger(terrainCoordsMins));
                    Float2 fsWorld = coords.TerrainCoordsToWorldSpace(RoundUpToInteger(terrainCoordsMaxs));

                    TRY {
                        PerformAction(Expand(faWorld, 0.f), Expand(fsWorld, 0.f));
                    } CATCH(...) {
                    } CATCH_END
                }
            }

        }

        return false;
    }

    void    RectangleManipulator::Render(RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext)
    {
            //  while dragging, we should draw a rectangle highlight on the terrain
        using namespace RenderCore::Metal;
        if (_isDragging && _secondAnchor.second) {
                
                // unbind the depth buffer
            SceneEngine::SavedTargets savedTargets(context);
            context->GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);

                // create shader resource view for the depth buffer
            ShaderResourceView depthSrv;
            if (savedTargets.GetDepthStencilView())
                depthSrv = ShaderResourceView(ExtractResource<ID3D::Resource>(
                    savedTargets.GetDepthStencilView()).get(), 
                    (NativeFormat::Enum)DXGI_FORMAT_R24_UNORM_X8_TYPELESS);     // note -- assuming D24S8 depth buffer! We need a better way to get the depth srv

            TRY
            {
                    // note -- we might need access to the MSAA defines for this shader
                auto& shaderProgram = Assets::GetAssetDep<ShaderProgram>(
                    "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*",
                    "game/xleres/ui/terrainmanipulators.sh:ps_rectanglehighlight:ps_*");

                    // clamp anchor values to the terrain coords size
                auto& coords = _terrainManager->GetCoords();
                Float2 faTerrain = coords.WorldSpaceToTerrainCoords(Truncate(_firstAnchor));
                Float2 fsTerrain = coords.WorldSpaceToTerrainCoords(Truncate(_secondAnchor.first));
                Float2 terrainCoordsMins(std::min(faTerrain[0], fsTerrain[0]), std::min(faTerrain[1], fsTerrain[1]));
                Float2 terrainCoordsMaxs(std::max(faTerrain[0], fsTerrain[0]), std::max(faTerrain[1], fsTerrain[1]));
                Float2 faWorld = coords.TerrainCoordsToWorldSpace(RoundDownToInteger(terrainCoordsMins));
                Float2 fsWorld = coords.TerrainCoordsToWorldSpace(RoundUpToInteger(terrainCoordsMaxs));
            
                struct HighlightParameters
                {
                    Float3 _mins; float _dummy0;
                    Float3 _maxs; float _dummy1;
                } highlightParameters = { 
                    Float3(std::min(faWorld[0], fsWorld[0]), std::min(faWorld[1], fsWorld[1]), 0.f), 0.f, 
                    Float3(std::max(faWorld[0], fsWorld[0]), std::max(faWorld[1], fsWorld[1]), 0.f), 0.f
                };
                ConstantBufferPacket constantBufferPackets[2];
                constantBufferPackets[0] = RenderCore::MakeSharedPkt(highlightParameters);

                auto& circleHighlight = Assets::GetAssetDep<DeferredShaderResource>("game/xleres/DefaultResources/circlehighlight.png");
                const ShaderResourceView* resources[] = { &depthSrv, &circleHighlight.GetShaderResource() };

                BoundUniforms boundLayout(shaderProgram);
                RenderCore::Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);
                boundLayout.BindConstantBuffer(Hash64("RectangleHighlightParameters"), 0, 1);
                boundLayout.BindShaderResource(Hash64("DepthTexture"), 0, 1);
                boundLayout.BindShaderResource(Hash64("HighlightResource"), 1, 1);

                context->Bind(shaderProgram);
                boundLayout.Apply(*context, 
                    parserContext.GetGlobalUniformsStream(),
                    UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets), resources, dimof(resources)));

                context->Bind(RenderCore::Techniques::CommonResources()._blendAlphaPremultiplied);
                context->Bind(RenderCore::Techniques::CommonResources()._dssDisable);
                context->Bind(Topology::TriangleStrip);
                context->GetUnderlying()->IASetInputLayout(nullptr);

                    // note --  this will render a full screen quad. we could render cylinder geometry instead,
                    //          because this decal only affects the area within a cylinder. But it's just for
                    //          tools, so the easy way should be fine.
                context->Draw(4);

                ID3D::ShaderResourceView* srv = nullptr;
                context->GetUnderlying()->PSSetShaderResources(3, 1, &srv);
            } 
            CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
            CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
            CATCH(...) {} 
            CATCH_END

            savedTargets.ResetToOldTargets(context);
        }
    }

    RectangleManipulator::RectangleManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : _terrainManager(terrainManager)
    {
        _isDragging = false;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class FillNoiseManipulator : public RectangleManipulator
    {
    public:
        virtual void    PerformAction(const Float3& anchor0, const Float3& anchor1);
        virtual const char* GetName() const { return "Fill noise"; }
        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const { return std::make_pair(nullptr, 0); }

        FillNoiseManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);

    private:
        float _baseHeight, _noiseHeight, _roughness, _fractalDetail;
    };

    void    FillNoiseManipulator::PerformAction(const Float3& anchor0, const Float3& anchor1)
    {
        auto *i = _terrainManager->GetUberSurfaceInterface();
        if (i) {
            i->FillWithNoise(
                _terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(anchor0)), 
                _terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(anchor1)), 
                _baseHeight, _noiseHeight, _roughness, _fractalDetail);
        }
    }

    auto FillNoiseManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&FillNoiseManipulator::_baseHeight), 0.1f, 1000.f, FloatParameter::Logarithmic, "BaseHeight"),
            FloatParameter(ManipulatorParameterOffset(&FillNoiseManipulator::_noiseHeight), 0.1f, 2000.f, FloatParameter::Logarithmic, "NoiseHeight"),
            FloatParameter(ManipulatorParameterOffset(&FillNoiseManipulator::_roughness), 10.f, 1000.f, FloatParameter::Logarithmic, "Roughness"),
            FloatParameter(ManipulatorParameterOffset(&FillNoiseManipulator::_fractalDetail), 0.1f, 0.9f, FloatParameter::Linear, "FractalDetail")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    FillNoiseManipulator::FillNoiseManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : RectangleManipulator(terrainManager)
    {
        _baseHeight = 250.0f;
        _noiseHeight = 500.f;
        _roughness = 250.f;
        _fractalDetail = 0.5f;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class ErosionManipulator : public RectangleManipulator
    {
    public:
        virtual void            PerformAction(const Float3& anchor0, const Float3& anchor1);
        virtual const char*     GetName() const { return "Erosion simulation"; }
        virtual void            Render(RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext);

        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const;
        
        ErosionManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);

    private:
        Float2  _activeMins;
        Float2  _activeMaxs;
        float _rainQuantityPerFrame, _changeToSoftConstant;
        float _softFlowConstant, _softChangeBackConstant;
        unsigned _flags;
    };

    void    ErosionManipulator::PerformAction(const Float3& anchor0, const Float3& anchor1)
    {
        auto *i = _terrainManager->GetUberSurfaceInterface();
        if (i) {
            _activeMins = _terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(anchor0));
            _activeMaxs = _terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(anchor1));
            _flags = _flags & ~(1<<0);    // start inactive
            i->Erosion_End();
        }
    }

    void    ErosionManipulator::Render(RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext)
    {
        RectangleManipulator::Render(context, parserContext);

            //  Doing the erosion tick here is most convenient (because this is the 
            //  only place we get regular updates).
        if (_flags & (1<<0)) {
            auto *i = _terrainManager->GetUberSurfaceInterface();
            if (i) {
                if (!i->Erosion_IsPrepared()) {
                    i->Erosion_Begin(_activeMins, _activeMaxs);
                }

                SceneEngine::TerrainUberSurfaceInterface::ErosionParameters params(
                    _rainQuantityPerFrame, _changeToSoftConstant, _softFlowConstant, _softChangeBackConstant);
                i->Erosion_Tick(params);
            }
        }

        if (_flags & (1<<1)) {
            auto *i = _terrainManager->GetUberSurfaceInterface();
            if (i && i->Erosion_IsPrepared()) {
                i->Erosion_RenderDebugging(context, parserContext, _terrainManager->GetCoords());
            }
        }
    }

    auto ErosionManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&ErosionManipulator::_rainQuantityPerFrame), 0.00001f, .1f, FloatParameter::Logarithmic, "Rain Quantity"),
            FloatParameter(ManipulatorParameterOffset(&ErosionManipulator::_changeToSoftConstant), 1e-5f, 1e-2f, FloatParameter::Logarithmic, "Change To Soft"),
            FloatParameter(ManipulatorParameterOffset(&ErosionManipulator::_softFlowConstant), 1e-3f, 1.f, FloatParameter::Logarithmic, "Soft Flow"),
            FloatParameter(ManipulatorParameterOffset(&ErosionManipulator::_softChangeBackConstant), 0.75f, 1.f, FloatParameter::Linear, "Soft Change Back")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    auto ErosionManipulator::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>
    {
        static BoolParameter parameters[] = 
        {
            BoolParameter(ManipulatorParameterOffset(&ErosionManipulator::_flags), 0, "Active"),
            BoolParameter(ManipulatorParameterOffset(&ErosionManipulator::_flags), 1, "Draw water"),
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    ErosionManipulator::ErosionManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : RectangleManipulator(terrainManager)
    {
        _rainQuantityPerFrame = 0.0001f;
        _changeToSoftConstant = 0.0001f;
        _softFlowConstant = 0.05f;
        _softChangeBackConstant = 0.9f;
        _flags = 0;
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class RotateManipulator : public RectangleManipulator
    {
    public:
        virtual void    PerformAction(const Float3& anchor0, const Float3& anchor1);
        virtual const char* GetName() const { return "Rotate"; }
        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const { return std::make_pair(nullptr, 0); }
        RotateManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);

    private:
        float _rotationDegrees;
    };

    void    RotateManipulator::PerformAction(const Float3&, const Float3&)
    {
        auto *i = _terrainManager->GetUberSurfaceInterface();
        if (i) {
                // we can't use the parameters passed into this function (because they've
                //  been adjusted to the mins/maxs of a rectangular area, and we've lost
                //  directional information)
            Float2 rotationOrigin = RoundDownToInteger(_terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(_firstAnchor)));
            Float2 farPoint = RoundDownToInteger(_terrainManager->GetCoords().WorldSpaceToTerrainCoords(Truncate(_secondAnchor.first)));

            float radius = Magnitude(rotationOrigin - farPoint);
            Float2 A(farPoint[0] - rotationOrigin[0], farPoint[1] - rotationOrigin[1]);
            Float3 rotationAxis = Normalize(Float3(A[1], -A[0], 0.f));
            i->Rotate(rotationOrigin, radius, rotationAxis, float(_rotationDegrees * M_PI / 180.f));
        }
    }

    auto RotateManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&RotateManipulator::_rotationDegrees), 1.0f, 70.f, FloatParameter::Logarithmic, "RotationDegrees")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    RotateManipulator::RotateManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : RectangleManipulator(terrainManager)
    {
        _rotationDegrees = 10.f;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //      I N T E R F A C E           //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class ManipulatorsInterface::InputListener : public RenderOverlays::DebuggingDisplay::IInputListener
    {
    public:
        bool    OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);

        InputListener(
            std::shared_ptr<ManipulatorsInterface> parent, 
            std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> debugScreensSystem);
    private:
        std::weak_ptr<ManipulatorsInterface> _parent;
        std::weak_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> _debugScreensSystem;
    };

    bool    ManipulatorsInterface::InputListener::OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
    {
        auto p = _parent.lock();
        if (p) {
                // this would probably be better if the input handler was registered / deregistered as the debugging screen became visible
            auto dss = _debugScreensSystem.lock();
            if (!dss || (dss->CurrentScreen(0) && XlFindStringI(dss->CurrentScreen(0), "terrain"))) {
                if (auto a = p->GetActiveManipulator()) {
                    return a->OnInputEvent(evnt, *p->_intersectionTestContext, *p->_intersectionTestScene);
                }
            }
        }
        return false;
    }

    ManipulatorsInterface::InputListener::InputListener(std::shared_ptr<ManipulatorsInterface> parent, std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> debugScreensSystem)
        : _parent(std::move(parent))
        , _debugScreensSystem(std::move(debugScreensSystem))
    {}

    void    ManipulatorsInterface::Render(RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext)
    {
        if (auto a = GetActiveManipulator())
            a->Render(context, parserContext);
    }

    void    ManipulatorsInterface::Update()
    {}

    void    ManipulatorsInterface::SelectManipulator(signed relativeIndex)
    {
        _activeManipulatorIndex = unsigned(_activeManipulatorIndex + relativeIndex + _manipulators.size()) % unsigned(_manipulators.size());
    }

    std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener>   ManipulatorsInterface::CreateInputListener(
        std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> debugScreensSystem)
    {
        return std::make_shared<InputListener>(shared_from_this(), debugScreensSystem);
    }

    ManipulatorsInterface::ManipulatorsInterface(
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionTestContext)
    {
        _activeManipulatorIndex = 0;
        _manipulators.emplace_back(std::make_unique<RaiseLowerManipulator>(terrainManager));
        _manipulators.emplace_back(std::make_unique<SmoothManipulator>(terrainManager));
        _manipulators.emplace_back(std::make_unique<NoiseManipulator>(terrainManager));
        _manipulators.emplace_back(std::make_unique<FillNoiseManipulator>(terrainManager));
        _manipulators.emplace_back(std::make_unique<CopyHeight>(terrainManager));
        _manipulators.emplace_back(std::make_unique<RotateManipulator>(terrainManager));
        _manipulators.emplace_back(std::make_unique<ErosionManipulator>(terrainManager));

        auto intersectionTestScene = std::make_shared<SceneEngine::IntersectionTestScene>(terrainManager);

        _terrainManager = std::move(terrainManager);
        _intersectionTestContext = std::move(intersectionTestContext);
        _intersectionTestScene = std::move(intersectionTestScene);
    }

    ManipulatorsInterface::~ManipulatorsInterface()
    {
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //      G U I   E L E M E N T S           //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    using namespace RenderOverlays;
    using namespace RenderOverlays::DebuggingDisplay;

    static const auto Id_TotalRect = InteractableId_Make("TerrainManipulators");
    static const auto Id_SelectedManipulator = InteractableId_Make("SelectedManipulator");
    static const auto Id_SelectedManipulatorLeft = InteractableId_Make("SelectedManipulatorLeft");
    static const auto Id_SelectedManipulatorRight = InteractableId_Make("SelectedManipulatorRight");

    static const auto Id_CurFloatParameters = InteractableId_Make("CurrentManipulatorParameters");
    static const auto Id_CurFloatParametersLeft = InteractableId_Make("CurrentManipulatorParametersLeft");
    static const auto Id_CurFloatParametersRight = InteractableId_Make("CurrentManipulatorParametersRight");

    static const auto Id_CurBoolParameters = InteractableId_Make("CurrentManipulatorBoolParameters");

    static void DrawAndRegisterLeftRight(IOverlayContext* context, Interactables&interactables, InterfaceState& interfaceState, const Rect& rect, InteractableId left, InteractableId right)
    {
        Rect manipulatorLeft(rect._topLeft, Coord2(LinearInterpolate(rect._topLeft[0], rect._bottomRight[0], 0.5f), rect._bottomRight[1]));
        Rect manipulatorRight(Coord2(LinearInterpolate(rect._topLeft[0], rect._bottomRight[0], 0.5f), rect._topLeft[1]), rect._bottomRight);
        interactables.Register(Interactables::Widget(manipulatorLeft, left));
        interactables.Register(Interactables::Widget(manipulatorRight, right));

        if (interfaceState.HasMouseOver(left)) {
                // draw a little triangle pointing to the left. It's only visible on mouse-over
            const Float2 centerPoint(
                float(rect._topLeft[0] + 16.f),
                float(LinearInterpolate(rect._topLeft[1], rect._bottomRight[1], 0.5f)-1.f));
            float width = XlTan(60.f * float(M_PI) / 180.f) * 5.f;    // building a equalteral triangle
            Float3 pts[] = 
            {
                Expand(Float2(centerPoint + Float2(-width,  0.f)), 0.f),
                Expand(Float2(centerPoint + Float2( 0.f,   -5.f)), 0.f),
                Expand(Float2(centerPoint + Float2( 0.f,    5.f)), 0.f)
            };
            context->DrawTriangle(ProjectionMode::P2D, pts[0], ColorB(0xffffffff), pts[1], ColorB(0xffffffff), pts[2], ColorB(0xffffffff));
        }

        if (interfaceState.HasMouseOver(right)) {
            const Float2 centerPoint(
                float(rect._bottomRight[0] - 16.f),
                float(LinearInterpolate(rect._topLeft[1], rect._bottomRight[1], 0.5f)-1.f));

            float width = XlTan(60.f * float(M_PI) / 180.f) * 5.f;    // building a equalteral triangle
            Float3 pts[] = 
            {
                Expand(Float2(centerPoint + Float2(width,  0.f)), 0.f),
                Expand(Float2(centerPoint + Float2(  0.f, -5.f)), 0.f),
                Expand(Float2(centerPoint + Float2(  0.f,  5.f)), 0.f)
            };
            context->DrawTriangle(ProjectionMode::P2D, pts[0], ColorB(0xffffffff), pts[1], ColorB(0xffffffff), pts[2], ColorB(0xffffffff));
        }
    }

    class WidgetResources
    {
    public:
        class Desc {};

        intrusive_ptr<RenderOverlays::Font> _headingFont;
        WidgetResources(const Desc&);
    };

    WidgetResources::WidgetResources(const Desc&)
    {
        _headingFont = GetX2Font("Raleway", 20);
    }

    Rect DrawManipulatorControls(
        IOverlayContext* context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState,
        IManipulator& manipulator, const char title[])
    {
        auto mainLayoutSize = layout.GetMaximumSize();
        static float desiredWidthPercentage = 40.f/100.f;
        static unsigned screenEdgePadding = 16;

        static ColorB backgroundRectangleColour (  64,   96,   64,  127);
        static ColorB backgroundOutlineColour   ( 192,  192,  192, 0xff);
        static ColorB headerColourNormal        ( 192,  192,  192, 0xff);
        static ColorB headerColourHighlight     (0xff, 0xff, 0xff, 0xff);
        static unsigned lineHeight = 20;

        auto floatParameters = manipulator.GetFloatParameters();
        auto boolParameters = manipulator.GetBoolParameters();
        auto statusText = manipulator.GetStatusText();

        auto& res = RenderCore::Techniques::FindCachedBox<WidgetResources>(WidgetResources::Desc());

        unsigned parameterCount = unsigned(1 + floatParameters.second + boolParameters.second); // (+1 for the selector control)
        if (!statusText.empty()) { ++parameterCount; }
        Coord desiredHeight = 
            parameterCount * lineHeight + (std::max(0u, parameterCount-1) * layout._paddingBetweenAllocations)
            + 25 + layout._paddingBetweenAllocations + 2 * layout._paddingInternalBorder;

        static ButtonFormatting buttonNormalState   (ColorB(127, 192, 127,  64), ColorB(164, 192, 164, 255));
        static ButtonFormatting buttonMouseOverState(ColorB(127, 192, 127,  64), ColorB(255, 255, 255, 160));
        static ButtonFormatting buttonPressedState  (ColorB(127, 192, 127,  64), ColorB(255, 255, 255,  96));
        
        Coord width = unsigned(mainLayoutSize.Width() * desiredWidthPercentage);
        Rect controlsRect(
            Coord2(mainLayoutSize._bottomRight[0] - screenEdgePadding - width, mainLayoutSize._bottomRight[1] - screenEdgePadding - desiredHeight),
            Coord2(mainLayoutSize._bottomRight[0] - screenEdgePadding, mainLayoutSize._bottomRight[1] - screenEdgePadding));

        Layout internalLayout(controlsRect);
        
        DrawRectangle(context, controlsRect, backgroundRectangleColour);
        DrawRectangleOutline(context, Rect(controlsRect._topLeft + Coord2(2,2), controlsRect._bottomRight - Coord2(2,2)), 0.f, backgroundOutlineColour);
        interactables.Register(Interactables::Widget(controlsRect, Id_TotalRect));

        TextStyle font(*res._headingFont);
        const auto headingRect = internalLayout.AllocateFullWidth(25);
        context->DrawText(
            std::make_tuple(Float3(float(headingRect._topLeft[0]), float(headingRect._topLeft[1]), 0.f), Float3(float(headingRect._bottomRight[0]), float(headingRect._bottomRight[1]), 0.f)),
            1.f, &font, interfaceState.HasMouseOver(Id_TotalRect)?headerColourHighlight:headerColourNormal, TextAlignment::Center, 
            title, nullptr);

            //
            //      Draw controls for parameters. Starting with the float parameters
            //
        
        for (size_t c=0; c<floatParameters.second; ++c) {
            auto parameter = floatParameters.first[c];
            const auto rect = internalLayout.AllocateFullWidth(lineHeight);
            float* p = (float*)PtrAdd(&manipulator, parameter._valueOffset);

            interactables.Register(Interactables::Widget(rect, Id_CurFloatParameters+c));
            auto formatting = FormatButton(interfaceState, Id_CurFloatParameters+c, buttonNormalState, buttonMouseOverState, buttonPressedState);

                // background (with special shader)
            float alpha;
            if (parameter._scaleType == IManipulator::FloatParameter::Linear) {
                alpha = Clamp((*p - parameter._min) / (parameter._max - parameter._min), 0.f, 1.f);
            } else {
                alpha = Clamp((std::log(*p) - std::log(parameter._min)) / (std::log(parameter._max) - std::log(parameter._min)), 0.f, 1.f);
            }
            context->DrawQuad(
                ProjectionMode::P2D, 
                AsPixelCoords(Coord2(rect._topLeft[0], rect._topLeft[1])),
                AsPixelCoords(Coord2(rect._bottomRight[0], rect._bottomRight[1])),
                ColorB(0xffffffff),
                Float2(0.f, 0.f), Float2(1.f, 1.f), Float2(alpha, 0.f), Float2(alpha, 0.f),
                "Utility\\DebuggingShapes.psh:SmallGridBackground");

                // text label (name and value)
            char buffer[256];
            _snprintf_s(buffer, _TRUNCATE, "%s = %5.1f", parameter._name, *p);
            context->DrawText(
                std::make_tuple(Float3(float(rect._topLeft[0]), float(rect._topLeft[1]), 0.f), Float3(float(rect._bottomRight[0]), float(rect._bottomRight[1]), 0.f)),
                1.f, nullptr, formatting._foreground, TextAlignment::Center, buffer, nullptr);
            
            DrawAndRegisterLeftRight(context, interactables, interfaceState, rect, Id_CurFloatParametersLeft+c, Id_CurFloatParametersRight+c);
        }

            //
            //      Also draw controls for the bool parameters
            //

        for (size_t c=0; c<boolParameters.second; ++c) {
            auto parameter = boolParameters.first[c];
            const auto rect = internalLayout.AllocateFullWidth(lineHeight);
            unsigned* p = (unsigned*)PtrAdd(&manipulator, parameter._valueOffset);
            bool value = !!((*p) & (1<<parameter._bitIndex));

            interactables.Register(Interactables::Widget(rect, Id_CurBoolParameters+c));
            auto formatting = FormatButton(interfaceState, Id_CurBoolParameters+c, buttonNormalState, buttonMouseOverState, buttonPressedState);

            char buffer[256];
            if (value) {
                _snprintf_s(buffer, _TRUNCATE, "<%s>", parameter._name);
            } else 
                _snprintf_s(buffer, _TRUNCATE, "%s", parameter._name);

            context->DrawText(
                std::make_tuple(Float3(float(rect._topLeft[0]), float(rect._topLeft[1]), 0.f), Float3(float(rect._bottomRight[0]), float(rect._bottomRight[1]), 0.f)),
                1.f, nullptr, formatting._foreground, TextAlignment::Center, buffer, nullptr);
        }

            //
            //      Also status text (if any set)
            //

        if (!statusText.empty()) {
            const auto rect = internalLayout.AllocateFullWidth(lineHeight);
            context->DrawText(
                std::make_tuple(AsPixelCoords(rect._topLeft), AsPixelCoords(rect._bottomRight)), 1.f, 
                nullptr, headerColourNormal, TextAlignment::Center, statusText.c_str(), nullptr);
        }

            //
            //      Draw manipulator left/right button
            //          (selects next or previous manipulator tool)
            //

        Rect selectedManipulatorRect = internalLayout.AllocateFullWidth(lineHeight);
        interactables.Register(Interactables::Widget(selectedManipulatorRect, Id_SelectedManipulator));
        DrawButtonBasic(
            context, selectedManipulatorRect, manipulator.GetName(),
            FormatButton(interfaceState, Id_SelectedManipulator, buttonNormalState, buttonMouseOverState, buttonPressedState));

            //  this button is a left/right selector. Create interactable rectangles for the left and right sides
        DrawAndRegisterLeftRight(
            context, interactables, interfaceState, selectedManipulatorRect, 
            Id_SelectedManipulatorLeft, Id_SelectedManipulatorRight);

        return controlsRect;
    }

    static void AdjustFloatParameter(IManipulator& manipulator, const IManipulator::FloatParameter& parameter, float increaseAmount)
    {
        const float clicksFromEndToEnd = 100.f;
        if (parameter._scaleType == IManipulator::FloatParameter::Linear) {
            float adjustment = (parameter._max - parameter._min) / clicksFromEndToEnd;
            float newValue = *(float*)PtrAdd(&manipulator, parameter._valueOffset) + increaseAmount * adjustment;
            newValue = Clamp(newValue, parameter._min, parameter._max);
            *(float*)PtrAdd(&manipulator, parameter._valueOffset) = newValue;
        }
        else 
        if (parameter._scaleType == IManipulator::FloatParameter::Logarithmic) {
            auto p = (float*)PtrAdd(&manipulator, parameter._valueOffset);
            float scale = (std::log(parameter._max) - std::log(parameter._min)) / clicksFromEndToEnd;
            float a = std::log(*p);
            a += increaseAmount * scale;
            *p = Clamp(std::exp(a), parameter._min, parameter._max);
        }
    }

    bool HandleManipulatorsControls(InterfaceState& interfaceState, const InputSnapshot& input, IManipulator& manipulator)
    {
        if (input.IsHeld_LButton()) {
            auto topMost = interfaceState.TopMostWidget();

                //  increase or decrease the parameter values
                    //      stay inside the min/max bounds. How far we go depends on the scale type of the parameter
                    //          * linear -- simple, it's just constant increase or decrease
                    //          * logarithmic -- it's more complex. We must increase by larger amounts as the number gets bigger

            auto floatParameters = manipulator.GetFloatParameters();
            if (topMost._id >= Id_CurFloatParametersLeft && topMost._id <= (Id_CurFloatParametersLeft + floatParameters.second)) {
                auto& parameter = floatParameters.first[topMost._id - Id_CurFloatParametersLeft];
                AdjustFloatParameter(manipulator, parameter, -1.f);
                return true;
            } else if (topMost._id >= Id_CurFloatParametersRight && topMost._id <= (Id_CurFloatParametersRight + floatParameters.second)) {
                auto& parameter = floatParameters.first[topMost._id - Id_CurFloatParametersRight];
                AdjustFloatParameter(manipulator, parameter, 1.f);
                return true;
            }

            auto boolParameters = manipulator.GetBoolParameters();
            if (topMost._id >= Id_CurBoolParameters && topMost._id <= (Id_CurBoolParameters + boolParameters.second)) {
                auto& parameter = boolParameters.first[topMost._id - Id_CurBoolParameters];
                
                unsigned* p = (unsigned*)PtrAdd(&manipulator, parameter._valueOffset);
                *p ^= 1<<parameter._bitIndex;

                return true;
            }
        }

        return false;
    }

    void    ManipulatorsDisplay::Render(IOverlayContext* context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState)
    {
        auto* activeManipulator = _manipulatorsInterface->GetActiveManipulator();
        DrawManipulatorControls(context, layout, interactables, interfaceState, *activeManipulator, "Terrain tools");
    }

    bool    ManipulatorsDisplay::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
        auto topMost = interfaceState.TopMostWidget();
        if (input.IsRelease_LButton()) {
            if (topMost._id == Id_SelectedManipulatorLeft) {
                    // go back one manipulator
                _manipulatorsInterface->SelectManipulator(-1);
                return true;
            }
            else if (topMost._id == Id_SelectedManipulatorRight) {
                    // go forward one manipulator
                _manipulatorsInterface->SelectManipulator(1);
                return true;
            }
        }

        return HandleManipulatorsControls(interfaceState, input, *_manipulatorsInterface->GetActiveManipulator());
    }


    ManipulatorsDisplay::ManipulatorsDisplay(std::shared_ptr<ManipulatorsInterface> interf)
        : _manipulatorsInterface(std::move(interf))
    {}

    ManipulatorsDisplay::~ManipulatorsDisplay()
    {}


}