// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(NODES_OUTPUT_H)
#define NODES_OUTPUT_H

#include "../gbuffer.h"

GBufferEncoded PerPixelOutput(
  float3 diffuseAlbedo,
  float3 worldSpaceNormal,

  PerPixelMaterialParam material,

  float blendingAlpha,
  float normalMapAccuracy,
  float cookedAmbientOcclusion,
  float cookedLightOcclusion)
{
  GBufferValues values;
  values.diffuseAlbedo = diffuseAlbedo;
  values.worldSpaceNormal = worldSpaceNormal;
  values.material = material;
  values.blendingAlpha = blendingAlpha;
  values.normalMapAccuracy = normalMapAccuracy;
  values.cookedAmbientOcclusion = cookedAmbientOcclusion;
  values.cookedLightOcclusion = cookedLightOcclusion;
  return Encode(values);
}

#endif
