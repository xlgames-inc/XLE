// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../RenderOverlays/OverlayContext.h"
#include "../../SceneEngine/CommonResources.h"
#include "../../SceneEngine/ResourceBox.h"

namespace Sample
{

    /// <summary>Resource box example</summary>
    /// This is an example of a cached resource box. These boxes are used to store
    /// generic resources in a convenient way.
    ///
    /// There are a number of useful advantages to this pattern:
    /// <list>
    ///  <item> Boxes are ostensibly global, but we can control over the life
    ///         time, and we can build a list of all of the existing boxes at
    ///         any point. That means we can build metrics for memory usage, etc.
    ///
    ///  <item> This is really convenient when we want to have a bunch of resources
    ///         that are shared by a large number of objects (a good example is
    ///         the resource box in SceneEngine/CommonResources.h).
    ///
    ///  <item> We can also collect resources that are related. This is handy for
    ///         invalidation. A resource box can have an Assets::DependencyValidation
    ///         object associated. This can be invalidated by any contained resource,
    ///         and when it's invalidated, the entire box will be rebuild. This is
    ///         great for vertex input layout objects -- because if the vertex shader
    ///         is invalidated, we also want to rebuild the input layout.
    ///
    ///  <item> Some resource need to be invalidated when the main window changes size
    ///         (for back buffers, etc). We can use this abstraction to destroy all
    ///         these types of resources in a convenient way.
    ///
    ///  <item> It's just a handy way to avoid cluttering up classes with pointers to
    ///         this and that... Without a system like this, we would need a lot of shared
    ///         pointers to things, and would need to pass this from function to function,
    ///         class to class. It's inconvenient. This pattern allows us to access the
    ///         resources we need with minimal extra code.
    /// </list>
    ///
    /// Mostly it's just for convenience. It's a way to share and reuse resources with
    /// minimal boring code.
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
        _font = RenderOverlays::GetX2Font("ui/font/yd_ygo540.ttf", desc._fontSize);
    }

    /// <summary>Renders some text to a device</summary>
    /// This is intended as a simple first-steps rendering example.
    /// We show 2 ways to render text onto the screen.
    void RenderPostScene(RenderCore::Metal::DeviceContext* context)
    {
            //
            //  Let's render some text in the middle of the screen.
            //  I'll show 2 ways to do it.
            //      1. using a IOverlayContext
            //      2. using RenderOverlay::Font directly
            //
        const char text[] = {
            "Hello World!... It's me, XLE!"
        };
        const bool textRenderingMethod = 1;

        using namespace RenderOverlays;
        auto& res = SceneEngine::FindCachedBox<RenderPostSceneResources>(RenderPostSceneResources::Desc(64));
        TextStyle style(*res._font);
        ColorB col(0xffffffff);

        RenderCore::Metal::ViewportDesc viewport(*context);
        if (constant_expression<textRenderingMethod==0>::result()) {

                //      Render text using a IOverlayContext
            ImmediateOverlayContext overlayContext(context);
            overlayContext.CaptureState();
            overlayContext.DrawText(
                std::make_tuple(Float3(0.f, 0.f, 0.f), Float3(viewport.Width, viewport.Height, 0.f)),
                1.f, &style, col, TextAlignment::Center, text, nullptr);

        } else {

                //      Render text directly to the scene using RenderOverlays stuff.
                //      This requires some more low-level code, so it's less convenient

            auto& commonRes = SceneEngine::CommonResources();
            context->Bind(commonRes._blendStraightAlpha);
            context->Bind(commonRes._dssReadWrite);
            context->Bind(commonRes._defaultRasterizer);
            context->BindPS(RenderCore::MakeResourceList(commonRes._defaultSampler));

            ucs4 buffer[1024];
            utf8_2_ucs4((const utf8*)text, XlStringLen(text), buffer, dimof(buffer));
            Quad quad = Quad::MinMax(Float2(0.f, 0.f), Float2(viewport.Width, viewport.Height));

            auto alignment = style.AlignText(quad, UIALIGN_CENTER, buffer, -1);
            style.Draw(
                context, alignment[0], alignment[1],
                buffer, -1, 
                0.f, 1.f, 0.f, 0.f,
                col.AsUInt32(), UI_TEXT_STATE_NORMAL, 0.f, &quad);

        }
    }

}