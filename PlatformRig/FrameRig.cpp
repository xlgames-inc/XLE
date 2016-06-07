// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FrameRig.h"
#include "AllocationProfiler.h"
#include "OverlaySystem.h"
#include "MainInputHandler.h"

#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/IAnnotator.h"
#include "../RenderOverlays/Font.h"
#include "../RenderOverlays/DebuggingDisplay.h"

#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/ParsingContext.h"

#include "../Assets/CompileAndAsyncManager.h"
#include "../Assets/AssetServices.h"

#include "../Utility/TimeUtils.h"
#include "../Utility/IntrusivePtr.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Profiling/CPUProfiler.h"

#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/Console.h"
#include "../Core/Types.h"

#include <tuple>

#include "../ConsoleRig/IncludeLUA.h"

namespace PlatformRig
{
    using namespace RenderOverlays;
    using namespace RenderOverlays::DebuggingDisplay;

    class FrameRateRecorder
    {
    public:
        void PushFrameDuration(uint64 duration);
        std::tuple<float, float, float> GetPerformanceStats() const;

        FrameRateRecorder();
        ~FrameRateRecorder();

    private:
        uint64      _frequency;
        uint64      _durationHistory[64];
        unsigned    _bufferStart, _bufferEnd;
    };

    class FrameRigDisplay : public RenderOverlays::DebuggingDisplay::IWidget
    {
    public:
        void    Render(IOverlayContext& context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState);
        bool    ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input);

        FrameRigDisplay(
            std::shared_ptr<DebugScreensSystem> debugSystem,
            const AccumulatedAllocations::Snapshot& prevFrameAllocationCount, const FrameRateRecorder& frameRate);
        ~FrameRigDisplay();
    protected:
        const AccumulatedAllocations::Snapshot* _prevFrameAllocationCount;
        const FrameRateRecorder* _frameRate;
        unsigned _subMenuOpen;

        std::weak_ptr<DebugScreensSystem> _debugSystem;
    };

    class FrameRig::Pimpl
    {
    public:
        AccumulatedAllocations::Snapshot _prevFrameAllocationCount;
        FrameRateRecorder _frameRate;
        uint64      _prevFrameStartTime;
        float       _timerToSeconds;
        unsigned    _frameRenderCount;
        uint64      _frameLimiter;
        uint64      _timerFrequency;
        bool        _updateAsyncMan;

        std::shared_ptr<OverlaySystemSet> _mainOverlaySys;
        std::shared_ptr<DebugScreensSystem> _debugSystem;
        std::vector<PostPresentCallback> _postPresentCallbacks;

        Pimpl()
        : _prevFrameStartTime(0) 
        , _timerFrequency(GetPerformanceCounterFrequency())
        , _frameRenderCount(0)
        , _frameLimiter(0)
        , _updateAsyncMan(false)
        {
            _timerToSeconds = 1.0f / float(_timerFrequency);
        }
    };

    class FrameRigResources
    {
    public:
        class Desc {};

        intrusive_ptr<RenderOverlays::Font> _frameRateFont;
        intrusive_ptr<RenderOverlays::Font> _smallFrameRateFont;
        intrusive_ptr<RenderOverlays::Font> _tabHeadingFont;

        FrameRigResources(const Desc&);
    };

    FrameRigResources::FrameRigResources(const Desc&)
    {
        auto frameRateFont = RenderOverlays::GetX2Font("Shojumaru", 32);
        auto smallFrameRateFont = RenderOverlays::GetX2Font("PoiretOne", 14);
        auto tabHeadingFont = RenderOverlays::GetX2Font("Raleway", 20);

        _frameRateFont = std::move(frameRateFont);
        _smallFrameRateFont = std::move(smallFrameRateFont);
        _tabHeadingFont = std::move(tabHeadingFont);
    }

///////////////////////////////////////////////////////////////////////////////

    class DebugScreensOverlay : public IOverlaySystem
    {
    public:
        DebugScreensOverlay(std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> debugScreensSystem)
            : _debugScreensSystem(debugScreensSystem)
            , _inputListener(std::make_shared<PlatformRig::DebugScreensInputHandler>(std::move(debugScreensSystem)))
        {
        }

        std::shared_ptr<IInputListener> GetInputListener()  { return _inputListener; }

        void RenderToScene(
            RenderCore::IThreadContext& devContext, 
            SceneEngine::LightingParserContext& parserContext) {}

        void RenderWidgets(
            RenderCore::IThreadContext& device, 
            RenderCore::Techniques::ParsingContext& parserContext)
        {
            _debugScreensSystem->Render(device, &parserContext.GetNamedResources(), parserContext.GetProjectionDesc());
        }

        void SetActivationState(bool) {}

    private:
        std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> _debugScreensSystem;
        std::shared_ptr<DebugScreensInputHandler> _inputListener;
    };

    static std::shared_ptr<IOverlaySystem> CreateDebugScreensOverlay(
        std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> debugScreensSystem)
    {
        return std::make_shared<DebugScreensOverlay>(std::move(debugScreensSystem));
    }

///////////////////////////////////////////////////////////////////////////////

    auto FrameRig::ExecuteFrame(
        RenderCore::IThreadContext& context,
        RenderCore::IPresentationChain* presChain,
        HierarchicalCPUProfiler* cpuProfiler,
        const FrameRenderFunction& renderFunction) -> FrameResult
    {
        CPUProfileEvent_Conditional pEvnt("FrameRig::ExecuteFrame", cpuProfiler);

        assert(presChain);

        uint64 startTime = GetPerformanceCounter();
        if (_pimpl->_frameLimiter) {
            CPUProfileEvent_Conditional pEvnt("FrameLimiter", cpuProfiler);
            while (startTime < _pimpl->_prevFrameStartTime + _pimpl->_frameLimiter) {
                Threading::YieldTimeSlice();
                startTime = GetPerformanceCounter();
            }
        }

        float frameElapsedTime = 1.f/60.f;
        if (_pimpl->_prevFrameStartTime!=0) {
            frameElapsedTime = (startTime - _pimpl->_prevFrameStartTime) * _pimpl->_timerToSeconds;
        }
        _pimpl->_prevFrameStartTime = startTime;

		context.GetAnnotator().Frame_Begin(context, _pimpl->_frameRenderCount);

        if (_pimpl->_updateAsyncMan)
            Assets::Services::GetAsyncMan().Update();

		auto presentationTarget = context.BeginFrame(*presChain);

            //  We must invalidate the cached state at least once per frame.
            //  It appears that the driver might forget bound constant buffers
            //  during the begin frame or present
        context.InvalidateCachedState();

        ////////////////////////////////

        auto renderRes = renderFunction(context, presentationTarget);

        ////////////////////////////////

        // auto f = _pimpl->_frameRate.GetPerformanceStats();
        // auto heapMetrics = AccumulatedAllocations::GetCurrentHeapMetrics();
        // 
        // DrawFrameRate(
        //     context, res._frameRateFont.get(), res._smallDebugFont.get(), std::get<0>(f), std::get<1>(f), std::get<2>(f), 
        //     heapMetrics._usage, _pimpl->_prevFrameAllocationCount._allocationCount);

        {
            if (Tweakable("FrameRigStats", false) && (_pimpl->_frameRenderCount % 64) == (64-1)) {
                auto f = _pimpl->_frameRate.GetPerformanceStats();
                LogInfo << "Ave FPS: " << 1000.f / std::get<0>(f);
                    // todo -- we should get a rolling average of these values
                if (_pimpl->_prevFrameAllocationCount._allocationCount) {
                    LogInfo << "(" << _pimpl->_prevFrameAllocationCount._freeCount << ") frees and (" << _pimpl->_prevFrameAllocationCount._allocationCount << ") allocs during frame. Ave alloc: (" << _pimpl->_prevFrameAllocationCount._allocationsSize / _pimpl->_prevFrameAllocationCount._allocationCount << ").";
                }
            }
        }

        {
            CPUProfileEvent_Conditional pEvnt("Present", cpuProfiler);
            context.Present(*presChain);
        }

        {
            for (auto i=_pimpl->_postPresentCallbacks.begin(); i!=_pimpl->_postPresentCallbacks.end(); ++i) {
                (*i)(context);
            }
        }

		context.GetAnnotator().Frame_End(context);

        uint64 duration = GetPerformanceCounter() - startTime;
        _pimpl->_frameRate.PushFrameDuration(duration);
        ++_pimpl->_frameRenderCount;
        auto accAlloc = AccumulatedAllocations::GetInstance();
        if (accAlloc) {
            _pimpl->_prevFrameAllocationCount = accAlloc->GetAndClear();
        }

        if (renderRes._hasPendingResources) {
            Sleep(16);  // slow down while we're building pending resources
        } else {
            Threading::YieldTimeSlice();    // this might be too extreme. We risk not getting execution back for a long while
        }

        FrameResult result;
        result._elapsedTime = frameElapsedTime;
        result._renderResult = renderRes;
        return result;
    }

    void FrameRig::SetFrameLimiter(unsigned maxFPS)
    {
        if (maxFPS) { _pimpl->_frameLimiter = _pimpl->_timerFrequency / uint64(maxFPS); }
        else { _pimpl->_frameLimiter = 0; }
    }

    void FrameRig::AddPostPresentCallback(const PostPresentCallback& postPresentCallback)
    {
        _pimpl->_postPresentCallbacks.push_back(postPresentCallback);
    }

    std::shared_ptr<OverlaySystemSet>& FrameRig::GetMainOverlaySystem() { return _pimpl->_mainOverlaySys; }
    std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem>& FrameRig::GetDebugSystem() { return _pimpl->_debugSystem; }
    void FrameRig::SetUpdateAsyncMan(bool updateAsyncMan) { _pimpl->_updateAsyncMan = updateAsyncMan; }

    FrameRig::FrameRig(bool isMainFrameRig)
    {
        _pimpl = std::make_unique<Pimpl>();

        _pimpl->_mainOverlaySys = std::make_shared<OverlaySystemSet>();
        _pimpl->_updateAsyncMan = isMainFrameRig;   // only the main frame rig should update the async man (in gui tools the async man update happens in a background thread)

        {
            _pimpl->_debugSystem = std::make_shared<DebugScreensSystem>();
            if (isMainFrameRig)
                _pimpl->_debugSystem->Register(
                    std::make_shared<FrameRigDisplay>(_pimpl->_debugSystem, _pimpl->_prevFrameAllocationCount, _pimpl->_frameRate),
                    "FrameRig", DebugScreensSystem::SystemDisplay);
        }

        _pimpl->_mainOverlaySys->AddSystem(CreateDebugScreensOverlay(_pimpl->_debugSystem));

        LogInfo << "---- Beginning FrameRig ------------------------------------------------------------------";
        auto accAlloc = AccumulatedAllocations::GetInstance();
        if (accAlloc) {
            auto acc = accAlloc->GetAndClear();
            if (acc._allocationCount)
                LogInfo << "(" << acc._freeCount << ") frees and (" << acc._allocationCount << ") allocs during startup. Ave alloc: (" << acc._allocationsSize / acc._allocationCount << ").";
            auto metrics = accAlloc->GetCurrentHeapMetrics();
            if (metrics._blockCount)
                LogInfo << "(" << metrics._blockCount << ") active normal block allocations in (" << metrics._usage / (1024.f*1024.f) << "M bytes). Ave: (" << metrics._usage / metrics._blockCount << ").";
        }

        if (isMainFrameRig) {
            using namespace luabridge;
            auto* luaState = ConsoleRig::Console::GetInstance().GetLuaState();
            getGlobalNamespace(luaState)
                .beginClass<FrameRig>("FrameRig")
                    .addFunction("SetFrameLimiter", &FrameRig::SetFrameLimiter)
                .endClass();
            
            setGlobal(luaState, this, "MainFrameRig");
        }
    }

    FrameRig::~FrameRig() 
    {
        auto* luaState = ConsoleRig::Console::GetInstance().GetLuaState();
        
        bool resetGlobal = false;

        {
            auto existingValue = luabridge::getGlobal(luaState, "MainFrameRig");
            resetGlobal = (existingValue.isUserdata() && ((FrameRig*)existingValue) == this);
        }
    
        if (resetGlobal) {
            // luabridge::setGlobal(luaState, nullptr, "MainFrameRig");
            lua_pushnil(luaState);
            lua_setglobal(luaState, "MainFrameRig");
        }
    }

///////////////////////////////////////////////////////////////////////////////

    void FrameRateRecorder::PushFrameDuration(uint64 duration)
    {
            // (note, in this scheme, one entry is always empty -- so actual capacity is really dimof(_durationHistory)-1)
        _durationHistory[_bufferEnd] = duration;
        _bufferEnd      = (_bufferEnd+1)%dimof(_durationHistory);
        if (_bufferEnd == _bufferStart) {
            _bufferStart = (_bufferStart+1)%dimof(_durationHistory);
        }
    }

    std::tuple<float, float, float> FrameRateRecorder::GetPerformanceStats() const
    {
        unsigned    entryCount = 0;
        uint64      accumulation = 0;
        uint64      minTime = std::numeric_limits<uint64>::max(), maxTime = 0;
        for (unsigned c=_bufferStart; c!=_bufferEnd; c=(c+1)%dimof(_durationHistory)) {
            accumulation += _durationHistory[c];
            minTime = std::min(minTime, _durationHistory[c]);
            maxTime = std::max(maxTime, _durationHistory[c]);
            ++entryCount;
        }

        double averageDuration = double(accumulation) / double(_frequency/1000) / double(entryCount);
        double minDuration = minTime / double(_frequency/1000);
        double maxDuration = maxTime / double(_frequency/1000);
        return std::make_tuple(float(averageDuration), float(minDuration), float(maxDuration));
    }

    FrameRateRecorder::FrameRateRecorder()
    {
        _bufferStart = _bufferEnd = 0;
        _frequency = GetPerformanceCounterFrequency();
    }

    FrameRateRecorder::~FrameRateRecorder() {}

///////////////////////////////////////////////////////////////////////////////

    static const InteractableId Id_FrameRigDisplayMain = InteractableId_Make("FrameRig");
    static const InteractableId Id_FrameRigDisplaySubMenu = InteractableId_Make("FrameRigSubMenu");

    template<typename T> inline const T& FormatButton(InterfaceState& interfaceState, InteractableId id, const T& normalState, const T& mouseOverState, const T& pressedState)
    {
        if (interfaceState.HasMouseOver(id))
            return interfaceState.IsMouseButtonHeld(0)?pressedState:mouseOverState;
        return normalState;
    }

    static const std::string String_IconBegin("game/xleres/defaultresources/icon_");
    static const std::string String_IconEnd(".png");

    void    FrameRigDisplay::Render(IOverlayContext& context, Layout& layout, 
                                    Interactables&interactables, InterfaceState& interfaceState)
    {
        auto& res = RenderCore::Techniques::FindCachedBox<FrameRigResources>(FrameRigResources::Desc());

        using namespace RenderOverlays;
        using namespace RenderOverlays::DebuggingDisplay;
        auto outerRect = layout.GetMaximumSize();

        static Coord rectWidth = 175;
        static Coord padding = 12;
        static Coord margin = 8;
        const auto bigLineHeight = Coord(res._frameRateFont->LineHeight());
        const auto smallLineHeight = Coord(res._smallFrameRateFont->LineHeight());
        const auto tabHeadingLineHeight = Coord(res._tabHeadingFont->LineHeight());
        const Coord rectHeight = bigLineHeight + 3 * margin + smallLineHeight;
        Rect displayRect(
            Coord2(outerRect._bottomRight[0] - rectWidth - padding, outerRect._topLeft[1] + padding),
            Coord2(outerRect._bottomRight[0] - padding, outerRect._topLeft[1] + padding + rectHeight));
        Layout innerLayout(displayRect);
        innerLayout._paddingInternalBorder = margin;
        innerLayout._paddingBetweenAllocations = margin;

        static ColorB normalColor = ColorB(70, 31, 0, 0x9f);
        static ColorB mouseOverColor = ColorB(70, 31, 0, 0xff);
        static ColorB pressed = ColorB(128, 50, 0, 0xff);
        DrawRoundedRectangle(&context, displayRect, 
            FormatButton(interfaceState, Id_FrameRigDisplayMain, normalColor, mouseOverColor, pressed), 
            ColorB::White,
            (interfaceState.HasMouseOver(Id_FrameRigDisplayMain))?4.f:2.f, 1.f / 4.f);

        static ColorB menuBkgrnd(128, 96, 64, 64);
        static ColorB menuBkgrndHigh(128, 96, 64, 192);
        static ColorB tabHeaderColor(0xffffffff);

        auto f = _frameRate->GetPerformanceStats();

        TextStyle bigStyle(*res._frameRateFont);
        DrawFormatText(
            &context, innerLayout.Allocate(Coord2(80, bigLineHeight)),
            &bigStyle, ColorB(0xffffffff), "%.1f", 1000.f / std::get<0>(f));

        TextStyle smallStyle(*res._smallFrameRateFont);
        DrawFormatText(
            &context, innerLayout.Allocate(Coord2(rectWidth - 80 - innerLayout._paddingInternalBorder*2 - innerLayout._paddingBetweenAllocations, smallLineHeight * 2)),
            &smallStyle, ColorB(0xffffffff), "%.1f-%.1f", 1000.f / std::get<2>(f), 1000.f / std::get<1>(f));

        auto heapMetrics = AccumulatedAllocations::GetCurrentHeapMetrics();
        auto frameAllocations = _prevFrameAllocationCount->_allocationCount;

        DrawFormatText(
            &context, innerLayout.AllocateFullWidth(smallLineHeight), 0.f,
            &smallStyle, ColorB(0xffffffff), TextAlignment::Center,
            "%.2fM (%i)", heapMetrics._usage / (1024.f*1024.f), frameAllocations);

        interactables.Register(Interactables::Widget(displayRect, Id_FrameRigDisplayMain));

        TextStyle tabHeader(*res._tabHeadingFont);
        // tabHeader._options.shadow = 0;
        // tabHeader._options.outline = 1;

        auto ds = _debugSystem.lock();
        if (ds) {
            const char* categories[] = {
                "Console", "Terrain", "Browser", "Placements", "Profiler", "Settings", "Test"
            };
            if (_subMenuOpen && (_subMenuOpen-1) < dimof(categories)) {
                    // draw menu of available debug screens
                const Coord2 iconSize(93/2, 88/2);
                unsigned menuHeight = 0;
                Coord2 pt = displayRect._bottomRight + Coord2(0, margin);
                for (signed c=dimof(categories)-1; c>=0; --c) {

                    bool highlight = interfaceState.HasMouseOver(Id_FrameRigDisplaySubMenu+c);

                    Rect rect;
                    if ((_subMenuOpen-1) == unsigned(c) || highlight) {

                            //  Draw the text name for this icon under the icon
                        Coord nameWidth = (Coord)context.StringWidth(1.f, &tabHeader, categories[c], nullptr);
                        rect = Rect(
                            pt - Coord2(std::max(iconSize[0], nameWidth), 0),
                            pt + Coord2(0, Coord(iconSize[1] + tabHeader._font->LineHeight())));

                        auto iconLeft = Coord((rect._topLeft[0] + rect._bottomRight[0] - iconSize[0]) / 2.f);
                        Coord2 iconTopLeft(iconLeft, rect._topLeft[1]);
                        Rect iconRect(iconTopLeft, iconTopLeft + iconSize);

                        DrawRectangle(&context, rect, menuBkgrnd);

                        context.DrawTexturedQuad(
                            ProjectionMode::P2D, 
                            AsPixelCoords(iconRect._topLeft),
                            AsPixelCoords(iconRect._bottomRight),
                            String_IconBegin + categories[c] + String_IconEnd);
                        DrawText(
                            &context, rect, 0.f,
                            &tabHeader, tabHeaderColor, TextAlignment::Bottom,
                            categories[c]);

                    } else {

                        rect = Rect(pt - Coord2(iconSize[0], 0), pt + Coord2(0, iconSize[1]));
                        context.DrawTexturedQuad(
                            ProjectionMode::P2D, 
                            AsPixelCoords(rect._topLeft),
                            AsPixelCoords(rect._bottomRight),
                            String_IconBegin + categories[c] + String_IconEnd);

                    }

                    interactables.Register(Interactables::Widget(rect, Id_FrameRigDisplaySubMenu+c));
                    pt = rect._topLeft - Coord2(margin, 0);
                    menuHeight = std::max(menuHeight, unsigned(rect._bottomRight[1] - rect._topLeft[1]));
                }

                    //  List all of the screens that are part of this category. They become hot spots
                    //  to activate that screen

                Layout screenListLayout(Rect(Coord2(0, pt[1] + menuHeight + margin), outerRect._bottomRight));

                const Coord2 smallIconSize(93/4, 88/4);
                auto lineHeight = std::max(smallIconSize[1], tabHeadingLineHeight);
                const auto screens = ds->GetWidgets();
                for (auto i=screens.cbegin(); i!=screens.cend(); ++i) {
                    if (i->_name.find(categories[_subMenuOpen-1]) != std::string::npos) {
                        unsigned width = (unsigned)context.StringWidth(1.f, &tabHeader, i->_name.c_str(), nullptr);
                        auto rect = screenListLayout.AllocateFullWidth(lineHeight);
                        rect._topLeft[0] = rect._bottomRight[0] - width;

                        DrawRectangle(&context, 
                            Rect(rect._topLeft - Coord2(2 + margin + smallIconSize[0],2), rect._bottomRight + Coord2(2,2)), 
                            interfaceState.HasMouseOver(i->_hashCode) ? menuBkgrndHigh : menuBkgrnd);

                        context.DrawTexturedQuad(
                            ProjectionMode::P2D, 
                            AsPixelCoords(Coord2(rect._topLeft - Coord2(smallIconSize[0] + margin, 0))),
                            AsPixelCoords(Coord2(rect._topLeft[0]-margin, rect._bottomRight[1])),
                            String_IconBegin + categories[_subMenuOpen-1] + String_IconEnd);
                        DrawText(
                            &context, rect, 0.f,
                            &tabHeader, tabHeaderColor, TextAlignment::Left,
                            i->_name.c_str());

                        interactables.Register(Interactables::Widget(rect, i->_hashCode));
                    }
                }
            }
        }
    }

    bool    FrameRigDisplay::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
        auto topMost = interfaceState.TopMostWidget();
        if (input.IsPress_LButton() || input.IsRelease_LButton()) {
            if (topMost._id == Id_FrameRigDisplayMain) {
                if (input.IsRelease_LButton()) {
                    _subMenuOpen = unsigned(!_subMenuOpen);
                }
                return true;
            } else if (topMost._id >= Id_FrameRigDisplaySubMenu && topMost._id < Id_FrameRigDisplaySubMenu + 32) {
                if (input.IsRelease_LButton()) {
                    _subMenuOpen = unsigned(topMost._id - Id_FrameRigDisplaySubMenu + 1);
                }
                return true;
            }

            auto ds = _debugSystem.lock();
            const auto screens = ds->GetWidgets();
            if (ds) {
                if (std::find_if(screens.cbegin(), screens.cend(),
                    [&](const DebugScreensSystem::WidgetAndName& w) { return w._hashCode == topMost._id; }) != screens.cend()) {
                    if (input.IsRelease_LButton() && ds->SwitchToScreen(0, topMost._id)) {
                        _subMenuOpen = 0;
                    }
                    return true;
                }
            }
        }

        return false;
    }

    FrameRigDisplay::FrameRigDisplay(
        std::shared_ptr<DebugScreensSystem> debugSystem,
        const AccumulatedAllocations::Snapshot& prevFrameAllocationCount, const FrameRateRecorder& frameRate)
    {
        _frameRate = &frameRate;
        _prevFrameAllocationCount = &prevFrameAllocationCount;
        _debugSystem = std::move(debugSystem);
        _subMenuOpen = 0;
    }

    FrameRigDisplay::~FrameRigDisplay()
    {}

}

