// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Vector.h"
#include "../Utility/UTFUtils.h"
#include <vector>

namespace PlatformRig
{
	class IInputListener; class InputContext; class InputSnapshot;

    class InputTranslator
    {
    public:
        void    OnMouseMove         (const InputContext& context, signed newX,       signed newY);
        void    OnMouseButtonChange (const InputContext& context, signed newX, signed newY, unsigned index,    bool newState);
        void    OnMouseButtonDblClk (const InputContext& context, signed newX, signed newY, unsigned index);
        void    OnKeyChange         (const InputContext& context, unsigned keyCode,  bool newState);
        void    OnChar              (const InputContext& context, ucs2 chr);
        void    OnMouseWheel        (const InputContext& context, signed wheelDelta);
        void    OnFocusChange       (const InputContext& context);

        void    AddListener         (std::weak_ptr<IInputListener> listener);

        Int2    GetMousePosition();

        InputTranslator();
        ~InputTranslator();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        unsigned        GetMouseButtonState() const;

        void            Publish(
			const InputContext& context, 
			const InputSnapshot& snapShot);
        const char*     AsKeyName(unsigned keyCode);
    };
}