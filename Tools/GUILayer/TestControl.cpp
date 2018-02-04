// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) //  : function compiled as native :

#include "TestControl.h"
#include "IOverlaySystem.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "IWindowRig.h"
#include "DelayedDeleteQueue.h"
#include "ExportedNativeTypes.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../RenderOverlays/OverlayContext.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../ConsoleRig/ResourceBox.h"

namespace GUILayer 
{
///////////////////////////////////////////////////////////////////////////////////////////////////

    class RenderPostSceneResources
    {
    public:
        class Desc 
        {
        public:
            unsigned _fontSize;
            Desc(unsigned fontSize) : _fontSize(fontSize) {}
        };
		std::shared_ptr<RenderOverlays::Font> _font;
        RenderPostSceneResources(const Desc& desc);
    };

    RenderPostSceneResources::RenderPostSceneResources(const Desc& desc)
    {
        _font = RenderOverlays::GetX2Font("DosisExtraBold", desc._fontSize);
    }

#pragma unmanaged
    static PlatformRig::FrameRig::RenderResult DummyRenderFrame(
        RenderCore::IThreadContext& context, 
        const RenderCore::ResourcePtr& presentationResource)
    {
        RenderCore::Techniques::AttachmentPool namedRes;
        auto contextStateDesc = context.GetStateDesc();
        namedRes.Bind(RenderCore::FrameBufferProperties{
            contextStateDesc._viewportDimensions[0], 
            contextStateDesc._viewportDimensions[1], RenderCore::TextureSamples::Create()});
        namedRes.Bind(0u, presentationResource);
        RenderCore::Techniques::RenderPassInstance rpi(
            context, {{RenderCore::SubpassDesc{{0}}}},
            0u, namedRes);

        const char text[] = "Hello World!... It's me, XLE!";
        
        using namespace RenderOverlays;
        auto& res = ConsoleRig::FindCachedBox<RenderPostSceneResources>(RenderPostSceneResources::Desc(64));
        TextStyle style(res._font);
        ColorB col(0xffffffff);
        
            //      Render text using a IOverlayContext
		// auto overlayContext = std::unique_ptr<ImmediateOverlayContext, AlignedDeletor<ImmediateOverlayContext>>(
		// 	(ImmediateOverlayContext*)XlMemAlign(sizeof(ImmediateOverlayContext), 16));
		// #pragma push_macro("new")
		// #undef new
        //     new(overlayContext.get()) ImmediateOverlayContext(&context);
		// #pragma pop_macro("new")
        ImmediateOverlayContext overlayContext(context);
		
        overlayContext.CaptureState();
        overlayContext.DrawText(
            std::make_tuple(
                Float3(0.f, 0.f, 0.f), 
                Float3(float(contextStateDesc._viewportDimensions[0]), float(contextStateDesc._viewportDimensions[1]), 0.f)),
            &style, col, TextAlignment::Center, text);

        return PlatformRig::FrameRig::RenderResult(false);
    }
#pragma managed

///////////////////////////////////////////////////////////////////////////////////////////////////

    bool TestControl::Render(
        RenderCore::IThreadContext& threadContext, 
        IWindowRig& windowRig)
    {
        auto& frameRig = windowRig.GetFrameRig();
        frameRig.ExecuteFrame(
            threadContext, windowRig.GetPresentationChain().get(),
            nullptr, DummyRenderFrame);
        return true;
    }

    TestControl::TestControl(Control^ control) : EngineControl(control) {}
    TestControl::~TestControl() {}

}

