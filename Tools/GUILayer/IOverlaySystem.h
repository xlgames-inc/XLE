// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace RenderCore { class IThreadContext; class IResource; }
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

        virtual void Render(
            RenderCore::IThreadContext& threadContext, 
			const std::shared_ptr<RenderCore::IResource>& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext) = 0; 
		virtual void SetActivationState(bool newState) {}

        virtual ~IOverlaySystem() {}
    };
}

