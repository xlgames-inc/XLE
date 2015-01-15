// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FlexUtil.h"

#if FLEX_USE_VTABLE(FLEX_INTERFACE)

    class FLEX_MAKE_INTERFACE_NAME(FLEX_INTERFACE);

#else

    class FLEX_INTERFACE;
    typedef FLEX_INTERFACE   FLEX_MAKE_INTERFACE_NAME(FLEX_INTERFACE);

#endif

