// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

/*!

    \brief Main interface for controlling rendering behaviour

    # Fundamental Rendering

    RenderCore is used to manage basic rendering behaviour. RenderCore always runs on top of another API,
    called the "low-level API." Normally this will be DirectX or OpenGL.
    
    Responsibilities for RenderCore include:
    \li Initialisation and destruction of the low level API
    \li Creating and destruction of low level API objects
    \li Begin & end rendering, and present to screen
    \li Managing user configuration parameters (like fullscreen mode, selected resolution)
    \li Performing the actually rendering commands

    ## Interfaces
    Note that there are 2 interfaces to RenderCore. This is because RenderCore is used in 2 very different
    ways.
    
    1. There are some clients that are not "graphics-aware." These clients don't want to implement any new rendering 
        behaviour, they just want to integrate rendering within a large game. These clients just want to set 
        settings like screen size and rendering settings, loading and destroying rendering assets, and 
        sometimes integrate with third party rendering (like Scaleform)
    2. Other clients are "graphics-aware." These clients want to create rendering behaviour
        (for example, implementing some rendering technique.)

    In other words, most clients will use interface 1.
    Only clients that want to create new rendering behaviour will use interface 2. 

    The advantage is we don't need to centralise all rendering behaviour is a single place. 
    Instead of having 1 library that does all of the rendering, it's better to have many small libraries 
    that each implement only a single rendering feature.

    ## Non graphics-aware interface
    The first interface is the main "RenderCore" interface. This is the non "graphics aware" interface:
    \li RenderCore::IPresentationChain
    \li RenderCore::IDevice
    
    This interface can be exported from a DLL. It will insulate clients from the details of
    the low level API.

    RenderCore::IDevice::QueryInterface can be used to access extension interfaces for the device. For
    example, this might include platform-specific interfaces like RenderCore::IDeviceDX11.

    ## Graphics-aware interface
    The second interface is RenderCore::Metal. This is the low level interface that can be used to
    create new rendering behaviour.

    RenderCore::Metal is a non-centralised rendering interface. This means that there are many objects
    to interact with, and each one as a small set of interface functions.

    In DirectX7/8/9 era rendering engines often had a giant "device" interface, with behaviour for every
    aspect of rendering. But RenderCore::Metal attempts to avoid centralising interface methods in a single
    place.

    RenderCore::Metal is mostly a thin layer over the low-level API. The interface for RenderCore::Metal is 
    platform-independent, but the implementation is very platform dependent.

    The implementation for RenderCore::Metal is in the following (platform specific) directories:
        \li RenderCore/DX11/Metal and
        \li RenderCore/OpenGLES/Metal

    However, when including headers, don't include from these directories. Instead, include headers from:
        \li RenderCore/Metal

    These will redirect to the correct platform header. For example, in DirectX mode, 
    RenderCore/Metal/DeviceContext.h will automatically include RenderCore/DX11/Metal/DeviceContext.h.

    Likewise, the platform specific implementations are in the following namespaces:
        \li RenderCore::Metal_DX11 and
        \li RenderCore::Metal_OpenGLES

    But don't use these namespaces directly. Instead use RenderCore::Metal. This will contain the symbols
    from whatever platform is currently active.

    Good starting places for RenderCore::Metal are:
        \li RenderCore::Metal
        \li RenderCore::Metal::DeviceContext

    ## Example code

    IDevice example code:

        \code{.cpp}
            auto renderDevice        = RenderCore::CreateDevice();
            auto presentationChain   = renderDevice->CreatePresentationChain(
                window.GetHWND(), 
                clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

            for (;;) {
                renderDevice->BeginFrame(presentationChain.get());
                    DoRender();
                presentationChain->Present();
            }
        \endcode

    RenderCore::Metal example code:

        \code{.cpp}
            using namespace RenderCore;
            auto& debuggingShader = Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/deferred/tileddebugging/beams.vsh:main:vs_*", 
                "game/xleres/deferred/tileddebugging/beams.gsh:main:gs_*", 
                "game/xleres/deferred/tileddebugging/beams.psh:main:ps_*",
                shadowProjectionConstants?"SHADOWS=1":"");

            Metal::BoundUniforms uniforms(debuggingShader);
            uniforms.BindConstantBuffer("RecordedTransform", 0, nullptr, 0);
            uniforms.BindConstantBuffer("GlobalTransform", 1, nullptr, 0);
            uniforms.BindConstantBuffer("$Globals", 2, nullptr, 0);
            uniforms.BindConstantBuffer("ShadowProjection", 3, nullptr, 0);
            const unsigned TileWidth = 16, TileHeight = 16;
            uint32 shaderGlobals[4] = { (mainViewportWidth + TileWidth - 1) / TileWidth, 
                                        (mainViewportHeight + TileHeight + 1) / TileHeight, 
                                        0, 0 };
            std::shared_ptr<std::vector<uint8>> constants[]
                = { nullptr, nullptr, MakeSharedPacket(shaderGlobals), nullptr };
            const Metal::ConstantBuffer* prebuiltBuffers[]   
                = { &savedGlobalTransform, &lightingParserContext.GetGlobalTransformCB(), nullptr, shadowProjectionConstants };
            uniforms.Apply(*context, constants, prebuiltBuffers, dimof(constants), nullptr, 0);
                                
            context->BindVS(MakeResourceList(   tileLightingResources._debuggingTextureSRV[0], 
                                                tileLightingResources._debuggingTextureSRV[1]));
            context->Bind(Metal::DepthStencilState());
            SetupVertexGeneratorShader(context);
            context->Bind(Metal::Topology::PointList);
            context->Bind(debuggingShader);
            context->Draw(shaderGlobals[0]*shaderGlobals[1]);
        \endcode

        So, while RenderCore::IDevice can be used by anyone, RenderCore::Metal requires some knowledge of rendering
        basics.

*/
namespace RenderCore
{

#if defined(DOXYGEN)
    /*!
        \brief Low level interface for rendering

        RenderCore::Metal is a namespace rename, of the form:
        \code
            using Metal = Metal_DX11;
        \endcode

        This is a really handy way to make references like RenderCore::Metal::DeviceContext configurable
        between platforms. Unfortunately, Doxygen doesn't fully support it. So RenderCore::Metal appears
        empty in our generated documentation.

        Please jump to one of the RenderCore::Metal implementations:
        \sa RenderCore::Metal_DX11
        \sa RenderCore::Metal_OpenGLES
    */
    namespace Metal {}
#endif
}



