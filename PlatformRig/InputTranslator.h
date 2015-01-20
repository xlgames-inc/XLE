// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Vector.h"
#include "../Utility/UTFUtils.h"
#include <vector>

namespace RenderOverlays { namespace DebuggingDisplay { class IInputListener; class InputSnapshot; } }

namespace PlatformRig
{
    class InputTranslator
    {
    public:
        void    OnMouseMove         (signed newX,       signed newY);
        void    OnMouseButtonChange (unsigned index,    bool newState);
        void    OnKeyChange         (unsigned keyCode,  bool newState);
        void    OnChar              (ucs2 chr);
        void    OnMouseWheel        (signed wheelDelta);

        void    AddListener         (std::weak_ptr<RenderOverlays::DebuggingDisplay::IInputListener> listener);

        Int2    GetMousePosition();

        InputTranslator();
        ~InputTranslator();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        unsigned        GetMouseButtonState() const;

        void            Publish(const RenderOverlays::DebuggingDisplay::InputSnapshot& snapShot);
        const char*     AsKeyName(unsigned keyCode);
    };
}