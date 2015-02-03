// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderOverlays/DebuggingDisplay.h"
#include "../RenderOverlays/IOverlayContext.h"
#include "../RenderCore/Metal/Forward.h"

namespace SceneEngine 
{ 
    class LightingParserContext; 
    class TerrainManager; 
    class ISceneParser; 
    class TechniqueContext; 
    class IntersectionTestContext;
    class IntersectionTestScene;
}
namespace RenderCore { class CameraDesc; }

namespace Tools
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

    class IManipulator
    {
    public:
        virtual bool OnInputEvent(
            const InputSnapshot& evnt, 
            const SceneEngine::IntersectionTestContext& hitTestContext,
            const SceneEngine::IntersectionTestScene& hitTestScene) = 0;
        virtual void Render(
            RenderCore::Metal::DeviceContext* context, 
            SceneEngine::LightingParserContext& parserContext) = 0;

        virtual const char* GetName() const = 0;
        virtual std::string GetStatusText() const = 0;

        template<typename T> class Parameter
        {
        public:
            enum ScaleType { Linear, Logarithmic };

            size_t _valueOffset;
            T _min, _max;
            ScaleType _scaleType;
            const char* _name;

            Parameter() {}
            Parameter(size_t valueOffset, T min, T max, ScaleType scaleType, const char name[])
                : _valueOffset(valueOffset), _min(min), _max(max), _scaleType(scaleType), _name(name) {}
        };

        typedef Parameter<float> FloatParameter;

        class BoolParameter
        {
        public:
            size_t      _valueOffset;
            unsigned    _bitIndex;
            const char* _name;

            BoolParameter() {}
            BoolParameter(size_t valueOffset, unsigned bitIndex, const char name[])
                : _valueOffset(valueOffset), _bitIndex(bitIndex), _name(name) {}
        };

            // (warning -- result will probably contain pointers to internal memory within this manipulator)
        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const = 0;     
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const = 0;
        virtual void SetActivationState(bool newState) = 0;

        virtual ~IManipulator();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    Rect DrawManipulatorControls(
        IOverlayContext* context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState,
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
}

