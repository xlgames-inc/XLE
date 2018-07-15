// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace RenderCore { class IThreadContext; }
namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace GUILayer 
{
    public ref class IOverlaySystem abstract
    {
    public:
            //  There are some problems using std::shared_ptr<> with
            //  managed methods. It's fine if the caller and implementor
            //  are both in GUILayer.dll.
            //
            //  But these methods can't be exported across dlls without
            //  using #pragma make_public... And that doesn't work with
            //  template types
        // typedef RenderOverlays::DebuggingDisplay::IInputListener IInputListener;
        // virtual std::shared_ptr<IInputListener> GetInputListener() = 0;

        virtual void RenderToScene(
            RenderCore::IThreadContext& device, 
            RenderCore::Techniques::ParsingContext& parserContext) = 0; 
        virtual void RenderWidgets(
            RenderCore::IThreadContext& device, 
            RenderCore::Techniques::ParsingContext& parserContext) = 0;
        virtual void SetActivationState(bool newState) = 0;

        virtual ~IOverlaySystem() {}
    };
}

