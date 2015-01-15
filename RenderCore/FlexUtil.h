// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#define FLEX_CONTEXT_CONCRETE        1
#define FLEX_CONTEXT_INTERFACE       2


#define FLEX_STITCH(A, B)               A##B
#define FLEX_USE_VTABLE(X)              FLEX_STITCH(FLEX_USE_VTABLE_, X)
#define FLEX_IS_CONCRETE_CONTEXT(X)     (FLEX_STITCH(FLEX_CONTEXT_,X) == FLEX_CONTEXT_CONCRETE)
#define FLEX_MAKE_INTERFACE_NAME(X)     FLEX_STITCH(I, X)
#define FLEX_MAKE_BASE_NAME(X)          FLEX_STITCH(Base_, X)
#define FLEX_MAKE_IGNORE_NAME(X)        FLEX_STITCH(Ignore_, X)

