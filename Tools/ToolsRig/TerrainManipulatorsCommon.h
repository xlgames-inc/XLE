// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IManipulator.h"
#include "../../Math/Vector.h"

namespace SceneEngine
{
    class TerrainManager;
}

namespace ToolsRig
{
    class CommonManipulator : public IManipulator
    {
    public:
            // IManipulator interface
        virtual bool    OnInputEvent(
            const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt, 
            const SceneEngine::IntersectionTestContext& hitTestContext,
            const SceneEngine::IntersectionTestScene& hitTestScene);
        virtual void    Render(RenderCore::IThreadContext* context, SceneEngine::LightingParserContext& parserContext);

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

    class RectangleManipulator : public IManipulator
    {
    public:
            // IManipulator interface
        virtual bool    OnInputEvent(
            const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt, 
            const SceneEngine::IntersectionTestContext& hitTestContext,
            const SceneEngine::IntersectionTestScene& hitTestScene);
        virtual void    Render(RenderCore::IThreadContext* context, SceneEngine::LightingParserContext& parserContext);

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
}

