// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Math.h"

#if MATHLIBRARY_ACTIVE == MATHLIBRARY_CML
    #pragma warning(push)
    #pragma warning(disable:4512)       // assignment operator could not be generated
    #include <cml/quaternion.h>
    #include <cml/mathlib/interpolation.h>
    #pragma warning(pop)
#endif

namespace Math
{
    #if MATHLIBRARY_ACTIVE == MATHLIBRARY_CML

        typedef cml::quaternion< float, cml::fixed<>, cml::scalar_first, cml::positive_cross >      Quaternion;

    #endif

    Quaternion SphericalInterpolate(const Quaternion& lhs, const Quaternion& rhs, float alpha);



            ////   I M P L E M E N T A T I O N S   /////

    inline Quaternion SphericalInterpolate(const Quaternion& lhs, const Quaternion& rhs, float alpha)
    {
        return cml::slerp(lhs, rhs, alpha);
    }
}

