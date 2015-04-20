// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/IThreadContext_Forward.h"
#include <string>

namespace SceneEngine 
{
    class IntersectionTestContext;
    class IntersectionTestScene;
    class LightingParserContext;
}

namespace RenderOverlays { namespace DebuggingDisplay { class InputSnapshot; } }

namespace ToolsRig
{
    class IManipulator
    {
    public:
        virtual bool OnInputEvent(
            const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt, 
            const SceneEngine::IntersectionTestContext& hitTestContext,
            const SceneEngine::IntersectionTestScene& hitTestScene) = 0;
        virtual void Render(
            RenderCore::IThreadContext* context, 
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
}

