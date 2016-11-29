// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(LIGHTING_ALGORITHM_H)
#define LIGHTING_ALGORITHM_H

#include "../CommonResources.h"
#include "../Utility/MathConstants.h"
#include "../Utility/Misc.h"

float Square(float x) { return x*x; }

float RefractiveIndexToF0(float refractiveIndex)
{
        // (note -- the 1.f here assumes one side of the interface is air)
	return Square((refractiveIndex - 1.f) / (refractiveIndex + 1.f));
}

float F0ToRefractiveIndex(float F0)
{
	float sqrx = sqrt(F0);
	return (-F0-1.f) / (F0-1.f) - 2.f * sqrx / (-1.f + sqrx) / (1.f + sqrx);
}

float SchlickFresnelCore(float VdotH)
{
	float A = 1.0f - saturate(VdotH);
	float sq = A*A;
	float cb = sq*sq;
	return cb*A;	// we could also consider just using the cubed value here
}

float SchlickFresnelF0(float3 viewDirection, float3 halfVector, float F0)
{
		// Note that we're using the half vector as a parameter to the fresnel
		// equation. See also "Physically Based Lighting in Call of Duty: Black Ops"
		// (Lazarov/Treyarch) for another example of this. The theory is this:
		//      If we imagine the surface as being microfacetted, then the facets
		//      that are reflecting light aren't actually flat on the surface... They
		//      are the facets that are raised towards the viewer. Our "halfVector" is
		//      actually an approximation of an average normal of these active facets.
		// For a perfect mirror surface, maybe we could consider using the normal
		// instead of the half vector here? Might be nice for intense reflections
		// from puddles of water, etc. Maybe we could also just use the roughness
		// value to interpolate between half vector and normal...?
		//
		// We can also consider a "spherical gaussian approximation" for fresnel. See:
		//	https://seblagarde.wordpress.com/2012/06/03/spherical-gaussien-approximation-for-blinn-phong-phong-and-fresnel/
		//	pow(1 - dotEH, 5) = exp2((-5.55473 * EdotH - 6.98316) * EdotH)
		// It seems like the performance difference might be hardware dependent
		// Also, maybe we should consider matching the gaussian approximation to the full
		// fresnel equation; rather than just Schlick's approximation.
	float q = SchlickFresnelCore(dot(viewDirection, halfVector));
	return F0 + (1.f - F0) * q;	// (note, use lerp for this..?)
}

float SchlickFresnelF0_Modified(float3 viewDirection, float3 halfVector, float F0)
{
		// In this modified version, we attempt to reduce the extreme edges of
		// the reflection by imposing an upper limit.
		// The extreme edges of the reflection can often highlight rendering
		// inaccuracies (such as lack of occlusion and local reflections).
		// So, oftening it off helps to reduce problems.
	float q = SchlickFresnelCore(dot(viewDirection, halfVector));
	float upperLimit = min(1.f, 50.f * (F0+0.001f));
	return F0 + (upperLimit - F0) * q;
}

float3 SchlickFresnelF0(float3 viewDirection, float3 halfVector, float3 F0)
{
	float q = SchlickFresnelCore(dot(viewDirection, halfVector));
	return lerp(F0, 1.f, q);
}

float3 SchlickFresnelF0_Modified(float3 viewDirection, float3 halfVector, float3 F0)
{
	float q = SchlickFresnelCore(dot(viewDirection, halfVector));
	float3 upperLimit = min(1.f, 50.f * (F0+0.001f));
	return F0 + (upperLimit - F0) * q;
}

float SchlickFresnel(float3 viewDirection, float3 halfVector, float refractiveIndex)
{
    float F0 = RefractiveIndexToF0(refractiveIndex);
    return SchlickFresnelF0(viewDirection, halfVector, F0);
}

float CalculateMipmapLevel(float2 texCoord, uint2 textureSize)
{
		// Based on OpenGL 4.2 spec chapter 3.9.11 equation 3.21
		// This won't automatically match the results given by all
		// hardware -- but it should be a good alternative to the
		// built in hardware mipmap calculation when an explicit
		// formula is required.
	float2 et = abs(texCoord * textureSize);
	float2 dx = ddx(et), dy = ddy(et);
	float d = max(dot(dx, dx), dot(dy, dy));
	return 0.5f*log2(d); // == log2(sqrt(d))
}

void OrenNayar_CalculateInputs(float roughness, out float rho, out float shininess)
{
	    // rho = roughness * roughness;
	    // shininess = 1.f - roughness;
    rho = 1.f;
    shininess = 2.0f / (roughness*roughness);
}

float4 ReadReflectionHemiBox( float3 direction,
                                Texture2D face12, Texture2D face34, Texture2D face5,
                                uint2 textureSize, uint defaultMipMap)
{
	float3 absDirection = abs(direction);
	direction.z = absDirection.z;

	float2 tc;
    uint textureIndex = 0;

		// Simple non-interpolated lookup to start
	[branch] if (absDirection.z > absDirection.x && absDirection.z > absDirection.y) {
		tc = 0.5f + 0.5f * direction.xy / direction.z;
        textureIndex = 2;
	} else if (absDirection.x > absDirection.y && absDirection.x > absDirection.z) {
		tc = 0.5f + 0.5f * direction.yz / absDirection.x;
		if (direction.x > 0.f) {
			tc.x = 1.f-tc.x;
			textureIndex = 0;
		} else {
			textureIndex = 1;
		}
	} else {
		tc = 0.5f + 0.5f * direction.xz / absDirection.y;
		tc.y = 1.f-tc.y;
		if (direction.y > 0.f) {
			textureIndex = 0;
		} else {
			tc.x = 1.f-tc.x;
			textureIndex = 1;
		}
	}

        // note --  mipmap calculation is incorrect on edges, where
        //          we flip from one texture to another.
        //          It's a problem... let's just use point sampling for now
    const bool usePointSampling = true;
    if (usePointSampling) {
        uint mipMap = defaultMipMap;
        int2 t = saturate(tc)*textureSize;
        t >>= mipMap;
        [branch] if (textureIndex == 2) {
            return face5.Load(int3(t, mipMap));
	    } else if (textureIndex == 0) {
		    return face12.Load(int3(t, mipMap));
	    } else {
		    return face34.Load(int3(t, mipMap));
	    }
    } else {
        uint mipMap = CalculateMipmapLevel(tc, textureSize);
        [branch] if (textureIndex == 2) {
            return face5.SampleLevel(ClampingSampler, tc, mipMap);
	    } else if (textureIndex == 0) {
		    return face12.SampleLevel(ClampingSampler, tc, mipMap);
	    } else {
		    return face34.SampleLevel(ClampingSampler, tc, mipMap);
	    }
    }
}

float2 EquirectangularMappingCoord(float3 direction)
{
		// note -- 	the trigonometry here is a little inaccurate. It causes shaking
		//			when the camera moves. We might need to replace it with more
		//			accurate math.
	float theta = atan2(direction.y, direction.x);
	float inc = asin(direction.z); // atan(direction.z * rsqrt(dot(direction.xy, direction.xy)));

	float x = 0.5f + 0.5f*(theta / (1.f*pi));
	float y = .5f-(inc / pi);
	return float2(x, y);
}

float2 HemiEquirectangularMappingCoord(float3 direction)
{
	float theta = atan2(direction.x, direction.y);
	float inc = atan(direction.z * rsqrt(dot(direction.xy, direction.xy)));

	float x = 0.5f + 0.5f*(theta / (1.f*pi));
	float y = 1.f-(inc / (.5f*pi));
	return float2(x, y);
}

float SimplifiedOrenNayer(float3 normal, float3 viewDirection, float3 lightDirection, float rho, float shininess)
{
		//
		//		Using a simplified Oren-Nayar implementation, without any lookup tables
		//		See http://blog.selfshadow.com/publications/s2012-shading-course/gotanda/s2012_pbs_beyond_blinn_notes_v3.pdf
		//		for details.
		//
		//		This is the non-spherical harmonic version (equation 25)
		//
		//		See also original Oren-Nayar paper:
		//			http://www1.cs.columbia.edu/CAVE/publications/pdfs/Oren_SIGGRAPH94.pdf
		//

	const float NL	 = dot(normal, lightDirection);
	const float NE	 = dot(normal, viewDirection);
	const float EL	 = dot(viewDirection, lightDirection);
	float A			 = EL - NE * NL;
	const float E0	 = 1.f;		// (from the Oren-Nayar paper, E0 is the irradiance when the facet is illuminated head-on)
	float B;
	/*if (A >= 0.f) {		// (better to branch or unwrap?)
		B = min(1.f, NL / NE);		(this branch is causing a wierd discontinuity. There must be something wrong with the equation)
	} else*/ {
		B = NL;
	}

	const float C = (1.0f / (2.22222f + .1f * shininess)) * A * B;
	const float D = NL * (1.0f - 1.0f / (2.f + .65 * shininess)) + C;
	return saturate(E0 * rho / pi * D);
}

float TriAceSpecularOcclusion(float NdotV, float ao)
{
    // This is the "Specular Occlusion" parameter suggested by Tri-Ace.
    // This equation is not physically based, but there are some solid
    // principles. Actually, our ambient occlusion term isn't
    // fully physically based, either.
    //
    // Let's assume that the "AO" factor represents the quantity of a
    // hemidome around the normal that is occluded. We can also assume
    // that the occluded parts are evenly distributed around the lowest
    // elevation parts of the dome.
    //
    // So, given an angle between the normal and the view, we want to know
    // how much of the specular peak will be occluded.
    // (See the Tri-Ace slides from cedec2011 for more details)
    // The result should vary based on roughness. But Tri-Ace found that it
    // was more efficient just to ignore that.
    //
    // Actually, I guess we could use the HdotV there, instead of NdotV, also.
    // That might encourage less occlusion.
    float q = (NdotV + ao);
    return saturate(q * q - 1.f + ao);
    // d*d + 2*d*a + a*a - 1 + a
    // d*d - 1 +     a*(2*d + 1)
    // a*a - 1 + a + d*(2*a + d)
}

float3 AdjSkyCubeMapCoords(float3 input)
{
	//float theta = 140.f * 3.14159f / 180.f;
	//viewFrustumVector.xy = float2(
	//	viewFrustumVector.x * cos(theta) - viewFrustumVector.y * sin(theta),
	//	viewFrustumVector.x * sin(theta) + viewFrustumVector.y * cos(theta));
	return float3(input.x, input.z, -input.y);
}

float3 InvAdjSkyCubeMapCoords(float3 input)
{
	return float3(input.x, -input.z, input.y);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

float PowerForHalfRadius(float halfRadius, float powerFraction)
{
		// attenuation = power / (distanceSq+1);
		// attenuation * (distanceSq+1) = power
		// (power*0.5f) * (distanceSq+1) = power
		// .5f*power = distanceSq+1
		// power = (distanceSq+1) / .5f
	return ((halfRadius*halfRadius)+1.f) * (1.f/(1.f-powerFraction));
}

float DistanceAttenuation(float distanceSq, float power)
{
	return power / (distanceSq+1.f);
}

float CalculateRadiusLimitAttenuation(float distanceSq, float lightRadius)
{
	// Calculate the drop-off towards the edge of the light radius...
	float D = distanceSq; D *= D; D *= D;
	float R = lightRadius; R *= R; R *= R; R *= R;
	return 1.f - saturate(3.f * D / R);
}

float3 CalculateHt(float3 i, float3 o, float iorIncident, float iorOutgoing)
{
		// Calculate the half vector used in transmitted specular equation
	return -normalize(iorIncident * i + iorOutgoing * o);
}

bool CalculateTransmissionIncident(out float3 i, float3 ot, float3 m, float iorIncident, float iorOutgoing)
{
    // here, m is the half vector (microfacet normal), and ot
    // is the direction to the viewer

    // m = -(1/l)(iorIncident * i + iorOutgoing * o);
    //  where l is length of (iorIncident * i + iorOutgoing * o)
    // -m * l = iorIncident * i + iorOutgoing * o
    // -m * l - iorOutgoing * o = iorIncident * i
    // i = -m * l / iorIncident - iorOutgoing / iorIncident * o

#if 0
	#if 1
		float flip = (iorIncident > iorOutgoing)?-1:1;
	    float c = dot(ot, (-1.f * flip) * m);
	    float b = iorOutgoing * c;
	    // if (c < 0.f || c >= 1.f) return false; // return float3(0,1,0);

	    // float a = sqrt(iorOutgoing*iorOutgoing - b*b);
	    // float a = sqrt(iorOutgoing*iorOutgoing - iorOutgoing*iorOutgoing*c*c);
	    // float a = iorOutgoing * sqrt(1.f - c*c);
	    // float asq = iorOutgoing*iorOutgoing*(1.f - c*c);
	    // if (asq >= iorIncident*iorIncident) return false;
	    // float e = sqrt(iorIncident*iorIncident - asq);
	    // float e = sqrt(iorIncident*iorIncident - iorOutgoing*iorOutgoing*(1.f - c*c));
	    float etaSq = Sq(iorOutgoing/iorIncident);
	    // float e = sqrt(iorIncident*iorIncident*(1.f - etaSq*(1.f - c*c)));
	    // float e = iorIncident*sqrt(1.f - etaSq + etaSq*c*c);
		float k = 1.f + etaSq*(c*c-1.f);
		if (k < 0.f) return false;
		float e = iorIncident*sqrt(k);
	    float l = flip * (b - e);
	#else
		float b = iorOutgoing * dot(ot, m);
		float a = sqrt(iorOutgoing*iorOutgoing - b*b);
		float e = sqrt(iorIncident*iorIncident - a*a);
		float l = e - b;
	#endif

	i = -m * l / iorIncident - iorOutgoing / iorIncident * ot;
#else
	float flip = (iorIncident > iorOutgoing)?1:-1;
	float c = dot(ot, m);

	float eta = iorOutgoing/iorIncident;
	float k = 1.f + Sq(eta)*(c*c-1.f);
	if (k < 0.f) return false;

    float l = eta * c - flip * sqrt(k);
	i = m * l - eta * ot;

	// Note that it's identical to CalculateTransmissionOutgoing (as should really
	// be expected), except with the parmeters swapped. We could generalize this
	// in a single function

#endif

    return true;
}

float RefractionIncidentAngleDerivative2(float odotm, float iorIncident, float iorOutgoing)
{
	// Similar to RefractionIncidentAngleDerivative, except now we're looking at
	// the relationship between odotm and odoti. Here O, the outgoing direction is
	// fixed, but M and I can change.
	//
	// idoto = l * odotm - eta
	//		 = (eta * c - sqrt(k)) * odotm - eta
	// where c=odotm && k=1+Sq(eta)*(c*c-1)
	// idoto = (eta * c - sqrt(1 + Sq(eta)*(c*c-1))) * c - eta
	//		 = eta*c*c - c*sqrt(1+Sq(eta)*(c*c-1)) - eta
	//		 = eta*c*c - c*sqrt(Sq(eta)*Sq(c)-Sq(eta)+1) - eta
	// 		 = eta*c*c - sqrt(Sq(eta)*c^4-Sq(eta)*Sq(c)+Sq(c)) - eta
	//		 = a*(x^2-1) - sqrt(a^2*x^4-a^2*x^2+x^2)
	// acos(a*(cos(x)^2-1) - sqrt(a^2*cos(x)^4-a^2*cos(x)^2+cos(x)^2))
	//
	// dev:
	// (a sin(2 x)-(sin(2 x) (a^2 cos(2 x)+1))/(sqrt(2) sqrt(cos^2(x) (a^2 cos(2 x)-a^2+2))))/sqrt(1-(sqrt(cos^2(x) (a^2 cos^2(x)-a^2+1))-a cos^2(x)+a)^2)
	// approx:
	// (a sin(2 x)-(0.707107 sin(2 x) (a^2 cos(2 x)+1))/(cos^2(x) (a^2 cos(2 x)-a^2+2))^0.5)/(1-((cos^2(x) (a^2 cos^2(x)-a^2+1))^0.5-a cos^2(x)+a)^2)^0.5

	float eta = iorOutgoing/iorIncident;
	float a = eta;
	float cosx = min(odotm, 0.99f);			// c

	float sinx = sqrt(1.f - cosx*cosx);		// b
	float sin2x = 2.f*cosx*sinx;			// d
	float cos2x = Sq(cosx) - Sq(sinx);		// f
	float sqr2 = sqrt(2.f);

	float A = sin2x * (a*a*cos2x+1) / (sqr2 * sqrt(Sq(cosx)*(a*a*cos2x-a*a+2)));
	float B = sqrt(1.f - Sq(sqrt(Sq(cosx)*(a*a*Sq(cosx)-a*a+1))-a*Sq(cosx)+a));
	float angleDev = (a * sin2x - A) / B;

	const bool useApproximation = true;
	if (useApproximation) {
		// This is an approximation of the full equation
		// It was matched by hand. It's not a perfect approximation. But it's
		// pretty close. The error seems visually negligable.
		float p = pow(1.f-a, -.35f);
		float W = 1.f - pow(1.f-cosx, p);
		angleDev = W * a - 1.f;
	}
	return -angleDev;
}

float RefractionIncidentAngleDerivative(float odotm, float iorIncident, float iorOutgoing)
{
	// We want to find the rate of change of the incident angle as
	// the microfacet normal changes. In this case, we are assuming that
	// the outgoing direction is constant.
	// One way to do this is to look at the derivative of idotm with
	// respect to odotm. Given that o is constant, changes in odotm
	// represent changes in the microfacet normal.
	// Of course, the angle between I and M isn't actually what we need,
	// because both I and M are moving.
	// However, there is a simple relationship between these, so it makes
	// the calculations easy. And maybe it's a good approximation.
	//
	// We can relate idotm to odotm...
	// idotm = l - eta * c;
	// 		 = eta * c - flip * sqrt(k) - eta * c
	//		 = -flip * sqrt(1.f + Sq(eta)*(Sq(odotm)-1.f))
	//
	// WolframAlpha derivative:
	// (d)/(dx)(sqrt(1+a^2 (-1+x^2))) = (a^2 x)/sqrt(a^2 (x^2-1)+1)
	//
	// Derivative of angle (as opposed to dot product,
	// assuming x > 0 && a > 0:
	// (a cot(x) abs(sin(x)))/sqrt(1-a^2 sin^2(x))
	// where cos(x) is odotm

	float eta = iorOutgoing/iorIncident;
	float flip = (iorIncident > iorOutgoing)?1:-1;

	float cosx = odotm;
	float sinxSq = 1.f - cosx*cosx;
	float sinx = sqrt(sinxSq);
	float cotx = cosx/sinx;
	float a = eta;
	// float angleDev = a * cotx * sinx / sqrt(1.f - Sq(a) * sinxSq);
	float k = sqrt(1.f + Sq(a) * (Sq(odotm) - 1.f));
	float angleDev = a * odotm / k;
	return flip * 2.f * angleDev;		// here, 2.f is a fudge factor because this is an approximation
}

float3 CalculateTransmissionOutgoing(float3 i, float3 m, float iorIncident, float iorOutgoing)
{
	float c = dot(i, m);
	float eta = iorIncident / iorOutgoing;
	float s = (iorIncident > iorOutgoing)?-1:1; // sign(dot(i, n))
	// return (eta * c - s * sqrt(1.f + eta * (c*c - 1.f))) * m - eta * i;

	// there maybe a small error in the Walter07 paper... Expecting eta^2 here --
	// float k = 1.f + eta*eta*(c*c - 1.f);
	float k = 1.f + Sq(eta)*(c*c - 1.f);
	if (k < 0.f) return 0.0.xxx;
	return (eta * c - s * sqrt(k)) * m - eta * i;
}

#endif
