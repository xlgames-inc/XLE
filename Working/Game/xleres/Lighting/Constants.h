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
static const float RefractiveIndexMax = 1.8f;
static const float ReflectionBlurrinessFromRoughness = 5.f;

//  For reference -- here are some "F0" values taken from
//  https://seblagarde.wordpress.com/2011/08/17/feeding-a-physical-based-lighting-mode/
//      F0(1.0) = 0
//      F0(1.8) = 0.082
//      F0(2.0) = 0.111
//      F0(2.2) = 0.141
//      F0(2.5) = 0.184
//      F0(3.0) = 0.25
// Quartz    0.045593921
// ice       0.017908907
// Water     0.020373188
// Alcohol   0.01995505
// Glass     0.04
// Milk      0.022181983
// Ruby      0.077271957
// Crystal   0.111111111
// Diamond   0.171968833
// Skin      0.028

// We can choose 2 options for defining the "specular" parameter
//  a) Linear against "refractive index"
//      this is more expensive, and tends to mean that the most
//      useful values of "specular" are clustered around 0 (where there
//      is limited fraction going on)
//  b) Linear against "F0"
//      a little cheaper, and more direct. There is a linear relationship
//      between the "specular" parameter and the brightness of the
//      "center" part of the reflection. So it's easy to understand.
//  Also note that we have limited precision for the specular parameter, if
//  we're reading it from texture maps... So it seems to make sense to make
//  it linear against F0.
#define SPECULAR_LINEAR_AGAINST_F0 1

// Note -- maybe this should vary with the "specular" parameter. The point where there
// is no refraction solution should match the point where the lighting becomes 100% reflection
static const float SpecularTransmissionIndexOfRefraction = 1.5f;

#endif
