// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if (!FLEX_USE_VTABLE(FLEX_INTERFACE)) && FLEX_IS_CONCRETE_CONTEXT(FLEX_INTERFACE)
    }
    using FLEX_MAKE_BASE_NAME(FLEX_INTERFACE) = Detail::ICLASSNAME(FLEX_INTERFACE);
#else
        // "Base_XXX" becomes an alias for "IXXX"
    using FLEX_MAKE_BASE_NAME(FLEX_INTERFACE) = FLEX_MAKE_INTERFACE_NAME(FLEX_INTERFACE);
#endif


#undef IMETHOD
#undef IPURE
#undef ICLASSNAME
#undef IDESTRUCTOR
#undef FLEX_INTERFACE
