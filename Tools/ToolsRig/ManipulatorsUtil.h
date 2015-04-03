// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderOverlays/IOverlayContext.h"

namespace SceneEngine 
{ 
    class LightingParserContext; 
    class TerrainManager; 
    class ISceneParser; 
    class TechniqueContext; 
    class IntersectionTestContext;
    class IntersectionTestScene;
}
namespace RenderCore { namespace Techniques { class CameraDesc; } }

namespace ToolsRig
{
    using namespace RenderOverlays;
    using namespace RenderOverlays::DebuggingDisplay;

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> inline const T& FormatButton(InterfaceState& interfaceState, InteractableId id, const T& normalState, const T& mouseOverState, const T& pressedState)
    {
        if (interfaceState.HasMouseOver(id))
            return interfaceState.IsMouseButtonHeld(0)?pressedState:mouseOverState;
        return normalState;
    }

    class ButtonFormatting
    {
    public:
        ColorB  _background;
        ColorB  _foreground;
        ButtonFormatting(ColorB background, ColorB foreground) : _background(background), _foreground(foreground) {}
    };

    inline void DrawButtonBasic(IOverlayContext* context, const Rect& rect, const char label[], ButtonFormatting formatting)
    {
        DrawRectangle(context, rect, formatting._background);
        DrawRectangleOutline(context, rect, 0.f, formatting._foreground);
        // DrawText(context, rect, 0.f, nullptr, formatting._foreground, manipulatorName);
        context->DrawText(
            std::make_tuple(Float3(float(rect._topLeft[0]), float(rect._topLeft[1]), 0.f), Float3(float(rect._bottomRight[0]), float(rect._bottomRight[1]), 0.f)),
            1.f, nullptr, formatting._foreground, TextAlignment::Center, label, nullptr);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class IManipulator;

    Rect DrawManipulatorControls(
        IOverlayContext* context, DebuggingDisplay::Layout& layout, Interactables&interactables, InterfaceState& interfaceState,
        IManipulator& manipulator, const char title[]);

    bool HandleManipulatorsControls(
        InterfaceState& interfaceState, const InputSnapshot& input, 
        IManipulator& manipulator);

    void RenderCylinderHighlight(
        RenderCore::Metal::DeviceContext* context, 
        SceneEngine::LightingParserContext& parserContext,
        Float3& centre, float radius);

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T, typename T2> size_t ManipulatorParameterOffset(T2 T::*member)
    {
            //  we want the offset of the member relative to a casted IManipulator pointer
            //  (here, t should normally be equal to basePtr)
        T* t = (T*)nullptr;
        IManipulator* basePtr = (IManipulator*)t;
        return size_t(&(t->*member)) - size_t(basePtr);
    }

    std::pair<Float3, bool> FindTerrainIntersection(
        const SceneEngine::IntersectionTestContext& context, const SceneEngine::IntersectionTestScene& scene,
        const Int2 screenCoords);

    inline Float2 RoundDownToInteger(Float2 input) { return Float2(XlFloor(input[0] + 0.5f), XlFloor(input[1] + 0.5f)); }
    inline Float2 RoundUpToInteger(Float2 input)   { return Float2(XlCeil(input[0] - 0.5f), XlCeil(input[1] - 0.5f)); }
}

