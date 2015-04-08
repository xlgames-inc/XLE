// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NativeManipulators.h"

namespace XLELayer
{
    public interface class IPlacementControls
    {
    public:
        property ActiveManipulatorContext^ ActiveContext { virtual void set(ActiveManipulatorContext^); }
    };
}

