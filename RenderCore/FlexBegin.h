// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FlexUtil.h"

#pragma warning(disable:4141)       // 'virtual' : used more than once

//
//      pattern:
// #define FLEX_INTERFACE Device
// #include "FlexBegin.h"
// class ICLASSNAME(Device) {};
// #include "FlexEnd.h"
//
//      Note that "ICLASSNAME" takes the name as a parameter. This must
//      be the same as the FLEX_INTERFACE define.
//      This is done for 2 reasons:
//          * Maybe helps readability slightly -- it makes sense for the
//              class name to be at that point
//          * Doxygen parses it easier
//

#if FLEX_USE_VTABLE(FLEX_INTERFACE) 

    #define ICLASSNAME(X)       FLEX_MAKE_INTERFACE_NAME(X)

    #define IMETHOD             virtual
    #define IDESTRUCTOR         virtual ~FLEX_MAKE_INTERFACE_NAME(FLEX_INTERFACE)() {}
    #define IPURE               = 0

#else

    class FLEX_INTERFACE;
    typedef FLEX_INTERFACE   FLEX_MAKE_INTERFACE_NAME(FLEX_INTERFACE);

    #if FLEX_IS_CONCRETE_CONTEXT(FLEX_INTERFACE)

        namespace Detail {
        #define ICLASSNAME(X)       FLEX_MAKE_IGNORE_NAME(X)
        #define IDESTRUCTOR         ~FLEX_MAKE_IGNORE_NAME(FLEX_INTERFACE)() {}

    #else

        #define ICLASSNAME(X)       X
        #define IDESTRUCTOR         ~FLEX_INTERFACE();

    #endif

    #define IMETHOD
    #define IPURE

#endif

