// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(ITERRAIN_TEXTURING_H)
#define ITERRAIN_TEXTURING_H

struct TerrainTextureOutput
{
    float3 diffuseAlbedo;
    float3 tangentSpaceNormal;
    float specularity;
};

interface ITerrainTexturing
{
    TerrainTextureOutput Calculate(float3 worldPosition, float2 dhdxy, uint materialId, float2 texCoord);
};

ITerrainTexturing MainTexturing;

///////////////////////////////////////////////////////////////////////////////////////////////////

Texture2DArray DiffuseAtlas		: register( t8);
Texture2DArray NormalsAtlas		: register( t9);
Texture2DArray SpecularityAtlas	: register(t10);

TerrainTextureOutput Blend(in TerrainTextureOutput A, in TerrainTextureOutput B, float alpha)
{
    TerrainTextureOutput result;
    result.diffuseAlbedo        = lerp(A.diffuseAlbedo, B.diffuseAlbedo, alpha);
    result.tangentSpaceNormal   = A.tangentSpaceNormal;
    result.specularity          = A.specularity;
    return result;
}

TerrainTextureOutput AddWeighted(in TerrainTextureOutput A, in TerrainTextureOutput B, float weight)
{
    TerrainTextureOutput result;
    result.diffuseAlbedo        = A.diffuseAlbedo + B.diffuseAlbedo * weight;
    result.tangentSpaceNormal   = A.tangentSpaceNormal;
    result.specularity          = A.specularity;
    return result;
}

TerrainTextureOutput TerrainTextureOutput_Blank()
{
    TerrainTextureOutput result;
    result.diffuseAlbedo        = 0.0.xxx;
    result.tangentSpaceNormal   = float3(0,0,1);
    result.specularity          = 1.f;
    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class DummyMaterial : ITerrainTexturing
{
    TerrainTextureOutput Calculate(float3 worldPosition, float2 dhdxy, uint materialId, float2 textureCoord);
};

TerrainTextureOutput DummyMaterial::Calculate(float3 worldPosition, float2 dhdxy, uint materialId, float2 textureCoord)
{
    return TerrainTextureOutput_Blank();
}

#endif
