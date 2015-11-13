// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(LIGHTING_CONSTANTS_H)
#define LIGHTING_CONSTANTS_H

//
// This file contains some fixed constants used in the lighting pipeline
// Some of these constants define the meaning of other constants.
// For example, RefractiveIndexMin/Max define the range of the "Specular"
// material parameter. Changing those values will actually change the meaning
// of "specular" in every material.
//
// Different project might want different values for RefractiveIndexMin/Max
// but the values should be constant and universal within a single project
//

static const float RefractiveIndexMin = 1.0f;
static const float RefractiveIndexMax = 2.5f;
static const float MetalReflectionBoost = 300.f;
static const float ReflectionBlurrinessFromRoughness = 6.f;

#endif
