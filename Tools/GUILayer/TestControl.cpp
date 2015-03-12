// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) //  : function compiled as native :

#include "TestControl.h"
#include "EngineControlInternal.h"
#include "EngineDevice.h"
#include "IWindowRig.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../RenderOverlays/OverlayContext.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/IThreadContext.h"

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
        intrusive_ptr<RenderOverlays::Font> _font;
        RenderPostSceneResources(const Desc& desc);
    };

    RenderPostSceneResources::RenderPostSceneResources(const Desc& desc)
    {
        _font = RenderOverlays::GetX2Font("DosisExtraBold", desc._fontSize);
    }

    static PlatformRig::FrameRig::RenderResult DummyRenderFrame(RenderCore::IThreadContext& context)
    {
        const char text[] = "Hello World!... It's me, XLE!";
        
        using namespace RenderOverlays;
        auto& res = RenderCore::Techniques::FindCachedBox<RenderPostSceneResources>(RenderPostSceneResources::Desc(64));
        TextStyle style(*res._font);
        ColorB col(0xffffffff);
        
        auto contextStateDesc = context.GetStateDesc();
        
            //      Render text using a IOverlayContext
		auto overlayContext = std::unique_ptr<ImmediateOverlayContext, AlignedDeletor<ImmediateOverlayContext>>(
			(ImmediateOverlayContext*)XlMemAlign(sizeof(ImmediateOverlayContext), 16));
		#pragma push_macro("new")
		#undef new
            new(overlayContext.get()) ImmediateOverlayContext(&context);
		#pragma pop_macro("new")
		
        overlayContext->CaptureState();
        overlayContext->DrawText(
            std::make_tuple(
                Float3(0.f, 0.f, 0.f), 
                Float3(float(contextStateDesc._viewportDimensions[0]), float(contextStateDesc._viewportDimensions[1]), 0.f)),
            1.f, &style, col, TextAlignment::Center, text, nullptr);

        return PlatformRig::FrameRig::RenderResult(false);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void TestControl::Render(
        RenderCore::IThreadContext& threadContext, 
        IWindowRig& windowRig)
    {
        auto& frameRig = windowRig.GetFrameRig();
        frameRig.ExecuteFrame(
            threadContext, windowRig.GetPresentationChain().get(),
            nullptr, nullptr, DummyRenderFrame);
    }

    TestControl::TestControl() {}
    TestControl::~TestControl() {}

}

