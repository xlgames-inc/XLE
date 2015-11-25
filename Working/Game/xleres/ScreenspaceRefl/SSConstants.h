// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

// Constants for tweaking screenspace reflections

#if !defined(SS_CONSTANTS_H)
#define SS_CONSTANTS_H

// Screenspace reflections is only enabled for very smooth materials
// Materials with roughness values higher than this will never receive reflections
static const float RoughnessThreshold = 0.25f;

// Reject pixels with small F0 values. This corresponds to the "specular" parameter.
static const float F0Threshold = 0.05f;

// When downsampling, we reject clusters of normals with high discontinuity
// This prevents getting wierd reflections on corners (though corners may tend
// to look less reflective as a result)
// The higher this number, the more pixels will be rejected.
static const float NormalDiscontinuityThreshold = .5f;

static const float DepthMinThreshold = 0.f;
static const float DepthMaxThreshold = 1.f;

// Limit the maximum distance to look for reflections with this constant.
// The value is in world space distance.
static const float MaxReflectionDistanceWorld = 5.f;

// Number of tests per ray when building the mask and reflection.
// We could use a variable number of tests depending on how many pixels the
// ray touches... But it seems like a small fixed number should be better
// suited to GPUs.
// Anyway; iterating accurately over every pixel along the ray is
// actually really awkward
static const uint MaskTestsPerRay = 8;
static const uint ReflectionInitialTestsPerRay = 6;
static const uint ReflectionDetailTestsPerRay = 6;

#endif
