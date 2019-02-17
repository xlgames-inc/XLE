// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Vector.h"
#include "../Utility/UTFUtils.h"
#include <vector>

namespace RenderOverlays { namespace DebuggingDisplay { class IInputListener; class InputContext; class InputSnapshot; } }

namespace PlatformRig
{
    class InputTranslator
    {
    public:
		using Context = RenderOverlays::DebuggingDisplay::InputContext;

        void    OnMouseMove         (const Context& context, signed newX,       signed newY);
        void    OnMouseButtonChange (const Context& context, signed newX, signed newY, unsigned index,    bool newState);
        void    OnMouseButtonDblClk (const Context& context, signed newX, signed newY, unsigned index);
        void    OnKeyChange         (const Context& context, unsigned keyCode,  bool newState);
        void    OnChar              (const Context& context, ucs2 chr);
        void    OnMouseWheel        (const Context& context, signed wheelDelta);
        void    OnFocusChange       (const Context& context);

        void    AddListener         (std::weak_ptr<RenderOverlays::DebuggingDisplay::IInputListener> listener);

        Int2    GetMousePosition();

        InputTranslator();
        ~InputTranslator();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        unsigned        GetMouseButtonState() const;

        void            Publish(
			const Context& context, 
			const RenderOverlays::DebuggingDisplay::InputSnapshot& snapShot);
        const char*     AsKeyName(unsigned keyCode);
    };
}