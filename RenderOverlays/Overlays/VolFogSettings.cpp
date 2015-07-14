// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "VolFogSettings.h"
#include "../../SceneEngine/VolumetricFog.h"

namespace Overlays
{
    static float Saturate(float value) { return std::max(std::min(value, 1.f), 0.f); }

    static void DrawHorizontalScrollBar(IOverlayContext* context, const ScrollBar::Coordinates& coordinates, float thumbPosition)
    {
        const auto r = coordinates.InteractableRect();
        float t = Saturate((thumbPosition - coordinates.MinValue()) / float(coordinates.MaxValue() - coordinates.MinValue()));
        context->DrawQuad(
            ProjectionMode::P2D,
            AsPixelCoords(Coord2(r._topLeft[0], r._topLeft[1])),
            AsPixelCoords(Coord2(r._bottomRight[0], r._bottomRight[1])),
            ColorB(0xffffffff),
            Float2(0.f, 0.f), Float2(1.f, 1.f), Float2(t, 0.f), Float2(t, 0.f),
            "Utility\\DebuggingShapes.psh:ScrollBarShader");
    }

    static void DrawLabelBackground(IOverlayContext* context, const Rect& rect)
    {
        context->DrawQuad(
            ProjectionMode::P2D,
            AsPixelCoords(Coord2(rect._topLeft[0], rect._topLeft[1])),
            AsPixelCoords(Coord2(rect._bottomRight[0], rect._bottomRight[1])),
            ColorB(0xffffffff),
            Float2(0.f, 0.f), Float2(1.f, 1.f), Float2(0.f, 0.f), Float2(0.f, 0.f),
            "Utility\\DebuggingShapes.psh:TagShader");
    }

    static void DrawGridBackground(IOverlayContext* context, const Rect& rect)
    {
        context->DrawQuad(
            ProjectionMode::P2D,
            AsPixelCoords(Coord2(rect._topLeft[0], rect._topLeft[1])),
            AsPixelCoords(Coord2(rect._bottomRight[0], rect._bottomRight[1])),
            ColorB(0xffffffff),
            Float2(0.f, 0.f), Float2(1.f, 1.f), Float2(0.f, 0.f), Float2(0.f, 0.f),
            "Utility\\DebuggingShapes.psh:GridBackgroundShader");
    }

    ////////////////////////////////////////////////////////////////////////////////////

    void    VolumetricFogSettings::Render(  IOverlayContext* context, Layout& layout, 
                                            Interactables& interactables, InterfaceState& interfaceState)
    {
        using namespace RenderOverlays::DebuggingDisplay;
        layout.AllocateFullWidth(64);

        class ScrollBarObj
        {
        public:
            float *_member;
            const char* _name;
            float _min, _max;
        };

        ScrollBarObj objects[dimof(VolumetricFogSettings::_scrollers)] = 
        {
            { &_materialSettings->_density,             "Density",              0.f,   1.f },
            { &_materialSettings->_noiseDensityScale,   "NoiseDensityScale",    0.f,   1.f },
            { &_materialSettings->_noiseSpeed,          "NoiseSpeed",           0.001f,   10.f },
            // { &_materialSettings->_heightStart,         "HeightStart",          0.f,   300.f },
            // { &_materialSettings->_heightEnd,           "HeightEnd",            0.f,   300.f },

            { &_materialSettings->_forwardColour[0],    "ForwardR",             0.f,   1.f },
            { &_materialSettings->_forwardColour[1],    "ForwardG",             0.f,   1.f },
            { &_materialSettings->_forwardColour[2],    "ForwardB",             0.f,   1.f },
            // { &_materialSettings->_forwardBrightness,   "ForwardBrightness",    0.f,   50.f },

            { &_materialSettings->_backColour[0],       "BackR",                0.f,   1.f },
            { &_materialSettings->_backColour[1],       "BackG",                0.f,   1.f },
            { &_materialSettings->_backColour[2],       "BackB",                0.f,   1.f },
            // { &_materialSettings->_backBrightness,      "BackBrightness",       0.f,   50.f },

            { &_materialSettings->_ESM_C,               "ESM_C",                0.f,   150.f },
            { &_materialSettings->_jitteringAmount,     "JitteringAmount",      0.f,   1.f }
        };

        const auto scrollBarId = InteractableId_Make("ScrollBar");
        for (unsigned q=0; q<dimof(objects); ++q) {
            static Coord2 scrollBarSize(600, 24);
            Rect windAngle0 = layout.Allocate(scrollBarSize);

            Rect gridBackgroundRect = windAngle0;
            gridBackgroundRect._topLeft[0] += 128; gridBackgroundRect._bottomRight[0] -= 4;
            gridBackgroundRect._topLeft[1] += 4; gridBackgroundRect._bottomRight[1] -= 4;
            DrawGridBackground(context, gridBackgroundRect);
            
            Rect labelRect = windAngle0;
            labelRect._bottomRight[0] = labelRect._topLeft[0] + 256;
            DrawLabelBackground(context, labelRect);
            
            Rect textRect = windAngle0;
            textRect._topLeft[0] += 32;
            /*Coord a = */DrawFormatText(context, textRect, 1.f, nullptr, ColorB(0xffffffff), objects[q]._name);

            Rect scrollBar = windAngle0;
            scrollBar._topLeft[0] = labelRect._bottomRight[0];
            scrollBar._topLeft[0] += 16;
            scrollBar._bottomRight[0] -= 16;

            ScrollBar::Coordinates scrollCoordinates(
                scrollBar, objects[q]._min, objects[q]._max, (objects[q]._max - objects[q]._min)/40.f,
                ScrollBar::Coordinates::Flags::NoUpDown|ScrollBar::Coordinates::Flags::Horizontal);
            *objects[q]._member = _scrollers[q].CalculateCurrentOffset(scrollCoordinates, *objects[q]._member);
            DrawHorizontalScrollBar(context, scrollCoordinates, *objects[q]._member);
            interactables.Register(
                Interactables::Widget(scrollCoordinates.InteractableRect(), scrollBarId+q));

            DrawFormatText(context, scrollBar, 0.f, 1.f, nullptr, ColorB(0xffffffff), TextAlignment::Right, "%.3f", *objects[q]._member);
        }
    }

    bool    VolumetricFogSettings::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
            // allow the scroller we're currently dragging to go first...
        for (unsigned c=0; c<dimof(_scrollers); ++c)
            if (_scrollers[c].IsDragging()) {
                if (_scrollers[c].ProcessInput(interfaceState, input))
                    return true;
                break;
            }

        for (unsigned c=0; c<dimof(_scrollers); ++c) {
            if (_scrollers[c].ProcessInput(interfaceState, input)) {
                return true;
            }
        }
        return false;
    }

    VolumetricFogSettings::VolumetricFogSettings(SceneEngine::VolumetricFogMaterial& materialSettings)
    : _materialSettings(&materialSettings)
    {
        const auto scrollBarId = InteractableId_Make("ScrollBar");
        for (unsigned c=0; c<dimof(_scrollers); ++c) {
            _scrollers[c] = ScrollBar(scrollBarId+c, ScrollBar::Coordinates::Flags::Horizontal);
        }
    }

    VolumetricFogSettings::~VolumetricFogSettings() {}



}


