// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(LIGHT_PROPERTIES_H)
#define LIGHT_PROPERTIES_H

struct DirectionalLightProperties
{
    float3 negativeLightDirection;
    float3 lightColor;
};

cbuffer SystemInput_Lights
{
    float3 SI_NegativeLightDirection;
    float3 SI_LightColor;
};

DirectionalLightProperties GetSystemStruct_DirectionalLightProperties()
{
        //  when using the editor, we pull these values from a system
        //  constants buffer. We don't have to do any extra processing,
        //  we just get the constants directly from the system.
    DirectionalLightProperties result;
    result.negativeLightDirection = SI_NegativeLightDirection;
    result.lightColor = SI_LightColor;
    return result;
}

#endif
