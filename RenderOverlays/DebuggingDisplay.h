// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IOverlayContext.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/StringUtils.h"     // for StringSection
#include "../Core/Types.h"
#include <vector>
#include <map>

namespace RenderOverlays { class TextStyle; }

namespace RenderOverlays { namespace DebuggingDisplay
{
    typedef int         Coord;
    typedef Int2        Coord2;

    inline Coord2 AsCoord2(const Float2& input) { return Coord2(Coord(input[0]), Coord(input[1])); }
    inline Float2 AsFloat2(const Coord2& input) { return Float2(float(input[0]), float(input[1])); }

    struct Rect ///////////////////////////////////////////////////////////////////////
    {
        Coord2      _topLeft, _bottomRight;
        Rect(const Coord2& topLeft, const Coord2& bottomRight) : _topLeft(topLeft), _bottomRight(bottomRight) {}
        Rect() {}

        Coord       Width() const     { return _bottomRight[0] - _topLeft[0]; }
        Coord       Height() const    { return _bottomRight[1] - _topLeft[1]; }
    };

    struct Layout /////////////////////////////////////////////////////////////////////
    {
        Rect    _maximumSize;
        Coord   _maxRowWidth;
        Coord   _caretX, _caretY;
        Coord   _currentRowMaxHeight;
        Coord   _paddingInternalBorder;
        Coord   _paddingBetweenAllocations;

        Layout(const Rect& maximumSize);
        Rect    AllocateFullWidth(Coord height);
        Rect    AllocateFullHeight(Coord width);
        Rect    AllocateFullHeightFraction(float proportionOfWidth);
        Rect    AllocateFullWidthFraction(float proportionOfHeight);
        Rect    Allocate(Coord2 dimensions);
        Rect    GetMaximumSize() const { return _maximumSize; }
        Coord   GetWidthRemaining() const;
    };

    typedef uint64 InteractableId;
    InteractableId InteractableId_Make(StringSection<char> name);

    typedef uint32 KeyId;
    KeyId KeyId_Make(StringSection<char> name);

    class InterfaceState;

    ///////////////////////////////////////////////////////////////////////////////////
    class Interactables
    {
    public:
        struct Widget
        {
            Rect _rect;
            InteractableId _id;
            Widget(const Rect& rect, InteractableId id) : _rect(rect), _id(id) {}
        };

        void                Register(const Widget& widget);
        std::vector<Widget> Intersect(const Coord2& position) const;
        InterfaceState      BuildInterfaceState(const Coord2& mousePosition, unsigned mouseButtonsHeld);
        Interactables();
    protected:
        std::vector<Widget> _widgets;
    };

    ///////////////////////////////////////////////////////////////////////////////////
    class InterfaceState
    {
    public:
        InterfaceState();
        InterfaceState( const Coord2& mousePosition, unsigned mouseButtonsHeld, 
                        const std::vector<Interactables::Widget>& mouseStack);

        bool                    HasMouseOver(InteractableId id);
        InteractableId          TopMostId() const                                   { return (!_mouseOverStack.empty())?_mouseOverStack[_mouseOverStack.size()-1]._id:0; }
        Interactables::Widget   TopMostWidget() const                               { return (!_mouseOverStack.empty())?_mouseOverStack[_mouseOverStack.size()-1]:Interactables::Widget(Rect(), 0); }
        bool                    IsMouseButtonHeld(unsigned buttonIndex = 0) const   { return !!(_mouseButtonsHeld&(1<<buttonIndex)); }
        Coord2                  MousePosition() const                               { return _mousePosition; }

        const std::vector<Interactables::Widget>& GetMouseOverStack() const         { return _mouseOverStack; }

    protected:
        std::vector<Interactables::Widget> _mouseOverStack;
        Coord2      _mousePosition;
        unsigned    _mouseButtonsHeld;
    };

    ///////////////////////////////////////////////////////////////////////////////////
    class InputSnapshot
    {
    public:
        unsigned        _mouseButtonsTransition;
        unsigned        _mouseButtonsDown;
        unsigned        _mouseButtonsDblClk;
        Coord2          _mousePosition;
        Coord2          _mouseDelta;
        signed          _wheelDelta;
        Utility::ucs2   _pressedChar;

        struct ActiveButton
        {
            KeyId       _name;
            bool        _transition;
            bool        _state;

            ActiveButton(KeyId name, bool transition, bool state) 
                : _name(name), _transition(transition), _state(state) {}
        };
        std::vector<ActiveButton>   _activeButtons;

        InputSnapshot() : _mouseButtonsTransition(0), _mouseButtonsDblClk(0), _mouseButtonsDown(0), _wheelDelta(0), _mousePosition(0,0), _mouseDelta(0,0), _pressedChar(0) {}
        InputSnapshot(  unsigned buttonsDown, unsigned buttonsTransition, signed wheelDelta, 
                        Coord2 mousePosition, Coord2 mouseDelta, Utility::ucs2 pressedChar=0) 
            : _mouseButtonsTransition(buttonsTransition), _mouseButtonsDown(buttonsDown)
            , _wheelDelta(wheelDelta), _mousePosition(mousePosition), _mouseDelta(mouseDelta)
            , _pressedChar(pressedChar), _mouseButtonsDblClk(0) {}

            //
            //      Each mouse button gets 2 bits, meaning 4 possible states:
            //          Press, Release, Held, Up
            //  
        bool    IsHeld_LButton() const       { return ((_mouseButtonsDown&1)==1); }
        bool    IsPress_LButton() const      { return ((_mouseButtonsDown&1)==1) && ((_mouseButtonsTransition&1)==1); }
        bool    IsRelease_LButton() const    { return ((_mouseButtonsDown&1)==0) && ((_mouseButtonsTransition&1)==1); }
        bool    IsUp_LButton() const         { return ((_mouseButtonsDown&1)==0) && ((_mouseButtonsTransition&1)==0); }
        bool    IsDblClk_LButton() const     { return !!(_mouseButtonsDblClk & 1); }

        bool    IsHeld_RButton() const       { return ((_mouseButtonsDown&(1<<1))==(1<<1)); }
        bool    IsPress_RButton() const      { return ((_mouseButtonsDown&(1<<1))==(1<<1))  && ((_mouseButtonsTransition&(1<<1))==(1<<1)); }
        bool    IsRelease_RButton() const    { return ((_mouseButtonsDown&(1<<1))==0)       && ((_mouseButtonsTransition&(1<<1))==(1<<1)); }
        bool    IsUp_RButton() const         { return ((_mouseButtonsDown&(1<<1))==0)       && ((_mouseButtonsTransition&(1<<1))==0); }
        bool    IsDblClk_RButton() const     { return !!(_mouseButtonsDblClk & (1<<1)); }

        bool    IsHeld_MButton() const       { return ((_mouseButtonsDown&(1<<2))==(1<<2)); }
        bool    IsPress_MButton() const      { return ((_mouseButtonsDown&(1<<2))==(1<<2))  && ((_mouseButtonsTransition&(1<<2))==(1<<2)); }
        bool    IsRelease_MButton() const    { return ((_mouseButtonsDown&(1<<2))==0)       && ((_mouseButtonsTransition&(1<<2))==(1<<2)); }
        bool    IsUp_MButton() const         { return ((_mouseButtonsDown&(1<<2))==0)       && ((_mouseButtonsTransition&(1<<2))==0); }
        bool    IsDblClk_MButton() const     { return !!(_mouseButtonsDblClk & (1<<2)); }

        template<typename Iterator> static bool IsHeld      (KeyId key, Iterator begin, Iterator end);
        template<typename Iterator> static bool IsPress     (KeyId key, Iterator begin, Iterator end);
        template<typename Iterator> static bool IsRelease   (KeyId key, Iterator begin, Iterator end);
        template<typename Iterator> static bool IsUp        (KeyId key, Iterator begin, Iterator end);

        bool    IsHeld(KeyId key) const       { return IsHeld(key, _activeButtons.begin(), _activeButtons.end()); }
        bool    IsPress(KeyId key) const      { return IsPress(key, _activeButtons.begin(), _activeButtons.end()); }
        bool    IsRelease(KeyId key) const    { return IsRelease(key, _activeButtons.begin(), _activeButtons.end()); }
        bool    IsUp(KeyId key) const         { return IsUp(key, _activeButtons.begin(), _activeButtons.end()); }

        void    Accumulate(const InputSnapshot& newEvnts, const InputSnapshot& lastFrameState);
        void    Reset();
    };

    ///////////////////////////////////////////////////////////////////////////////////
    class IInputListener
    {
    public:
        virtual bool    OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt) = 0;
        virtual ~IInputListener();
    };

    ///////////////////////////////////////////////////////////////////////////////////
    class IWidget
    {
    public:
        virtual void    Render(IOverlayContext& context, Layout& layout, Interactables& interactables, InterfaceState& interfaceState) = 0;
        virtual bool    ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input) = 0;
        virtual         ~IWidget();
    };

    ///////////////////////////////////////////////////////////////////////////////////
    extern const ColorB   RandomPaletteColorTable[];
    extern const size_t   RandomPaletteColorTable_Size;

    void        DrawElipse(IOverlayContext* context, const Rect& rect, ColorB colour);
    void        DrawFilledElipse(IOverlayContext* context, const Rect& rect, ColorB colour);

    void DrawRoundedRectangle(
        IOverlayContext* context, const Rect& rect, 
        ColorB backgroundColour, ColorB outlineColour,
        float borderWidth = 1.f, float roundedProportion = 1.f / 8.f);
    void DrawRoundedRectangleOutline(
        IOverlayContext* context, const Rect& rect, 
        ColorB colour, 
        float width, float roundedProportion = 1.f / 8.f);

    void        DrawRectangle(IOverlayContext* context, const Rect& rect, ColorB colour);
    void        DrawRectangle(IOverlayContext* context, const Rect& rect, float depth, ColorB colour);
    void        DrawRectangleOutline(IOverlayContext* context, const Rect& rect, float depth = 0.f, ColorB colour = ColorB(0xff000000));

    Coord       DrawText(IOverlayContext* context, const Rect& rect, TextStyle* textStyle, ColorB colour, StringSection<> text);
    Coord       DrawText(IOverlayContext* context, const Rect& rect, float depth, TextStyle* textStyle, ColorB colour, StringSection<> text);
    Coord       DrawText(IOverlayContext* context, const Rect& rect, float depth, TextStyle* textStyle, ColorB colour, TextAlignment::Enum alignment, StringSection<> text);
    Coord       DrawFormatText(IOverlayContext* context, const Rect& rect, TextStyle* textStyle, ColorB colour, const char text[], ...);
    Coord       DrawFormatText(IOverlayContext* context, const Rect& rect, float depth, TextStyle* textStyle, ColorB colour, const char text[], ...);
    Coord       DrawFormatText(IOverlayContext* context, const Rect& rect, float depth, TextStyle* textStyle, ColorB colour, TextAlignment::Enum alignment, const char text[], ...);
    void        DrawHistoryGraph(IOverlayContext* context, const Rect& rect, float values[], unsigned valuesCount, unsigned maxValuesCount, float& minValueHistory, float& maxValueHistory);
    void        DrawHistoryGraph_ExtraLine(IOverlayContext* context, const Rect& rect, float values[], unsigned valuesCount, unsigned maxValuesCount, float minValue, float maxValue);

    void        DrawTriangles(IOverlayContext* context, const Coord2 triangleCoordinates[], const ColorB triangleColours[], unsigned triangleCount);
    void        DrawLines(IOverlayContext* context, const Coord2 lineCoordinates[], const ColorB lineColours[], unsigned lineCount);

    Float3      AsPixelCoords(Coord2 input);
    Float3      AsPixelCoords(Coord2 input, float depth);
    Float3      AsPixelCoords(Float2 input);
    Float3      AsPixelCoords(Float3 input);
    std::tuple<Float3, Float3> AsPixelCoords(const Rect& rect);

    ///////////////////////////////////////////////////////////////////////////////////
    typedef std::tuple<Float3, Float3>      AABoundingBox;

    void        DrawBoundingBox(
        IOverlayContext* context, const AABoundingBox& box, 
        const Float3x4& localToWorld, 
        ColorB entryColour, unsigned partMask = 0x3);

    void        DrawFrustum(
        IOverlayContext* context, const Float4x4& worldToProjection, 
        ColorB entryColour, unsigned partMask = 0x3);

    ///////////////////////////////////////////////////////////////////////////////////
    //          S C R O L L   B A R S

    class ScrollBar
    {
    public:
        class Coordinates
        {
        public:
            struct Flags
            {
                enum Enum { NoUpDown = 1<<0, Horizontal = 1<<1 };
                typedef unsigned BitField;
            };

            Coordinates(const Rect& rect, float minValue, float maxValue, 
                        float visibleWindowSize, Flags::BitField flags = 0);

            bool    Collapse() const;
            Rect    InteractableRect() const        { return _interactableRect; }
            Rect    ScrollArea() const              { return _scrollAreaRect; }
            Rect    UpArrow() const                 { return _upArrowRect; }
            Rect    DownArrow() const               { return _downArrowRect; }

            Rect    Thumb(float value) const;

            float   PixelsToValue(Coord pixels) const;
            float   MinValue() const                    { return _valueBase; }
            float   MaxValue() const                    { return _maxValue; }

        protected:
            float   _valueToPixels;
            float   _valueBase;
            float   _maxValue;

            Coord   _pixelsBase;
            Coord   _windowHeight;
            Coord   _thumbHeight;

            Rect    _interactableRect;
            Rect    _scrollAreaRect;
            Rect    _upArrowRect;
            Rect    _downArrowRect;
            Flags::BitField _flags;

            Coord   ValueToPixels(float value) const;
        };

        bool                ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input);
        float               CalculateCurrentOffset(const Coordinates& coordinates) const;
        float               CalculateCurrentOffset(const Coordinates& coordinates, float oldValue) const;
        InteractableId      GetID() const;
        bool                IsDragging() const { return _draggingScrollBar; }
        void                ProcessDelta(float delta) const;

        ScrollBar(InteractableId id=0, Coordinates::Flags::BitField flags=0);
    protected:
        InteractableId  _id;
        mutable Coord   _scrollOffsetPixels;
        mutable float   _resolvedScrollOffset;
        mutable float   _pendingDelta;
        bool            _draggingScrollBar;
        Coordinates::Flags::BitField _flags;
    };

    void DrawScrollBar(IOverlayContext* context, const ScrollBar::Coordinates& coordinates, float thumbPosition, ColorB fillColour = ColorB(0xffffffff), ColorB outlineColour = ColorB(0xff000000));

    void HScrollBar_Draw(IOverlayContext* context, const ScrollBar::Coordinates& coordinates, float thumbPosition);
    void HScrollBar_DrawLabel(IOverlayContext* context, const Rect& rect);
    void HScrollBar_DrawGridBackground(IOverlayContext* context, const Rect& rect);

    ///////////////////////////////////////////////////////////////////////////////////
    //          T A B L E S
    struct TableElement
    {
        std::string _label; ColorB _bkColour;
        TableElement(const std::string& label, ColorB bkColour = ColorB(0xff000000)) : _label(label), _bkColour(bkColour) {}
        TableElement(const char label[],  ColorB bkColour = ColorB(0xff000000)) : _label(label), _bkColour(bkColour) {}
        TableElement() : _bkColour(0xff000000) {}
    };
    void DrawTableHeaders(IOverlayContext* context, const Rect& rect, const IteratorRange<std::pair<std::string, unsigned>*>& fieldHeaders, ColorB bkColor, Interactables* interactables=NULL);
    void DrawTableEntry(IOverlayContext* context, const Rect& rect, const IteratorRange<std::pair<std::string, unsigned>*>& fieldHeaders, const std::map<std::string, TableElement>& entry);

    ///////////////////////////////////////////////////////////////////////////////////
    class DebugScreensSystem : public IInputListener
    {
    public:
        bool        OnInputEvent(const InputSnapshot& evnt);
        void        Render(IOverlayContext& overlayContext, const Rect& viewport);
        
        enum Type { InPanel, SystemDisplay };
        void        Register(std::shared_ptr<IWidget> widget, const char name[], Type type = InPanel);

        void        SwitchToScreen(unsigned panelIndex, const char name[]);
        bool        SwitchToScreen(unsigned panelIndex, uint64 hashCode);
        void        SwitchToScreen(const char name[]);
        const char* CurrentScreen(unsigned panelIndex);
        
        struct WidgetAndName 
        {
            std::shared_ptr<IWidget>    _widget;
            std::string                 _name;
            uint64                      _hashCode;
        };
        const std::vector<WidgetAndName>& GetWidgets() const { return _widgets; }

        bool    ConsumedInputEvent()       { return _consumedInputEvent; }
        void    ResetConsumedInputEvent()  { _consumedInputEvent = false; }

        DebugScreensSystem();
        ~DebugScreensSystem();

    private:
        Interactables   _currentInteractables;
        InterfaceState  _currentInterfaceState;
        
        std::vector<WidgetAndName> _widgets;
        std::vector<WidgetAndName> _systemWidgets;

        struct Panel
        {
            size_t      _widgetIndex;
            float       _size;
            bool        _horizontalDivider;
            std::string _backButton;
        };
        std::vector<Panel> _panels;

        Coord2      _currentMouse;
        unsigned    _currentMouseHeld;
        bool        _consumedInputEvent;

        void    RenderPanelControls(        IOverlayContext*    context,
                                            unsigned            panelIndex, const std::string& name, Layout&layout, bool allowDestroy,
                                            Interactables&      interactables, InterfaceState& interfaceState);
        bool    ProcessInputPanelControls(  InterfaceState&     interfaceState, const InputSnapshot&    evnt);
    };

    ///////////////////////////////////////////////////////////////////////////////////
    inline bool IsGood(const Rect& rect)
    {
        return  rect._topLeft[0] < rect._bottomRight[0]
            &&  rect._topLeft[1] < rect._bottomRight[1];
    }

    template<typename Iterator> bool InputSnapshot::IsHeld(KeyId key, Iterator begin, Iterator end)
    {
        for (auto i=begin; i!=end; ++i)
            if (i->_name==key) return i->_state;
        return false;
    }

    template<typename Iterator> bool InputSnapshot::IsPress(KeyId key, Iterator begin, Iterator end)
    {
        for (auto i=begin; i!=end; ++i)
            if (i->_name==key) return i->_state && i->_transition;
        return false;
    }

    template<typename Iterator> bool InputSnapshot::IsRelease(KeyId key, Iterator begin, Iterator end)
    {
        for (auto i=begin; i!=end; ++i)
            if (i->_name==key) return !i->_state && i->_transition;
        return false;
    }

    template<typename Iterator> bool InputSnapshot::IsUp(KeyId key, Iterator begin, Iterator end)
    {
        for (auto i=begin; i!=end; ++i)
            if (i->_name==key) return !i->_state && !i->_transition;
        return true;
    }

}}

