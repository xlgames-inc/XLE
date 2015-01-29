// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "../Utility/MathConstants.h"
#include "../Transform.h"	// for GlobalState cbuffer

#define Real	float
#define Complex float2

RWTexture2D<uint>	WorkingTextureRealPart : register(u0);
RWTexture2D<uint>	WorkingTextureImaginaryPart : register(u1);

RWTexture2D<uint>	WorkingTextureXRealPart : register(u2);
RWTexture2D<uint>	WorkingTextureXImaginaryPart : register(u3);

RWTexture2D<uint>	WorkingTextureYRealPart : register(u4);
RWTexture2D<uint>	WorkingTextureYImaginaryPart : register(u5);

#define AVOID_TRIG

Complex ComplexMultiply(Complex lhs, Complex rhs)
{
	float realPart		= lhs.x * rhs.x - lhs.y * rhs.y;
	float imaginaryPart = lhs.y * rhs.x + lhs.x * rhs.y;
	return Complex(realPart, imaginaryPart);		
}

Complex ComplexExponential(Real real)
{
		//	using "Euler's formula"
		//	exp(ix) = cos(x) + i.sin(x)
	float2 sinecosine;
	sincos(real, sinecosine.x, sinecosine.y);
	return Complex(sinecosine.y, sinecosine.x);
}

Complex ComplexConjugate(Complex input)
{
	return Complex(input.x, -input.y);
}

Complex Get(uint2 coord)
{
	uint encodedValueReal		 = WorkingTextureRealPart[coord];
	uint encodedValueImaginary	 = WorkingTextureImaginaryPart[coord];
	return Complex(asfloat(encodedValueReal), asfloat(encodedValueImaginary));
}

void Set(uint2 coord, Complex newValue)
{
	WorkingTextureRealPart[coord]		 = asuint(float(newValue.x));
	WorkingTextureImaginaryPart[coord]	 = asuint(float(newValue.y));
}

void Scale(uint2 coord, float scaleValue)
{
	Set(coord, Get(coord)*scaleValue);
}

void Innerloop(uint2 pairCoord, uint2 matchCoord, Complex factor)
{
	const Complex product = ComplexMultiply(factor, Get(matchCoord));	//   Second term of two-point transform
	Complex pairValue = Get(pairCoord);
	Set(matchCoord, pairValue - product);								//   Transform for fi + pi
	Set(pairCoord, pairValue + product);								//   Transform for fi
}

void Swap(uint2 coordZero, uint2 coordOne)
{
	uint t = WorkingTextureRealPart[coordZero];
	WorkingTextureRealPart[coordZero] = WorkingTextureRealPart[coordOne];
	WorkingTextureRealPart[coordOne] = t;

	t = WorkingTextureImaginaryPart[coordZero];
	WorkingTextureImaginaryPart[coordZero] = WorkingTextureImaginaryPart[coordOne];
	WorkingTextureImaginaryPart[coordOne] = t;
}

void FFT_Row(uint rowIndex, uint N, bool inverse)
{
	const Real pi = inverse ? 3.14159265358979323846 : -3.14159265358979323846;

		//
		//		We have to do the "butterfly" bit position reversal...
		//		Maybe there's some way to remove half the bit reversals
		//		(because the output of the first batch of FFTs is used
		//		as the input for the next batch of FFTs)
		//

	{
		uint i2 = N >> 1;
		uint j = 0;
		for (uint i=0; i<N-1; i++) {
			if (i < j) {
				uint2 iCoord = uint2(i, rowIndex);
				uint2 jCoord = uint2(j, rowIndex);
				Swap(iCoord, jCoord);
			}
			uint k = i2;
			while (k <= j) {
				j -= k;
				k >>= 1;
			}
			j += k;
		}
	}	

#if !defined(AVOID_TRIG)

		//		Iteration through dyads, quadruples, octads and so on...
		//		  -- todo -- set "N" to a constant for the optimiser?
	for (uint step = 1; step < N; step <<= 1) {

		const uint jump = step << 1;				//   jump to the next entry of the same transform factor
		const Real delta = float(pi) / float(step);	//   Angle increment
		const Real sine = sin(delta * Real(.5));			//   Auxiliary sin(delta / 2)
		const Complex multiplier = Complex(Real(-2.) * sine * sine, sin(delta));	//   multiplier for trigonometric recurrence
		Complex factor = Complex(1., 0.);			//   Start value for transform factor, fi = 0

			//
			//   Iteration through groups of different transform factor
			//				
		for (uint group = 0; group < step; ++group) {

				//   Iteration within group 
			for (uint pair = group; pair < N; pair += jump) {
				uint2 pairCoord = uint2(pair, rowIndex);
				uint2 matchCoord = uint2(pair + step, rowIndex);
				Innerloop(pairCoord, matchCoord, factor);
			}

				//   Successive transform factor via trigonometric recurrence
			factor = ComplexMultiply(multiplier, factor) + factor;
		}

	}

#else

	Real c1 = Real(-1.0); 
	Real c2 = Real(0.0);
	uint l2 = 1;
	[flatten] for (uint step = 1; step < N; step <<= 1) {
		uint l1 = l2;
		l2 <<= 1;
		Real u1 = Real(1.0); 
		Real u2 = Real(0.0);
		[flatten] for (uint j=0;j<l1;j++) {
			[flatten] for (uint i=j;i<N;i+=l2) {
				uint2 pairCoord = uint2(i, rowIndex);
				uint2 matchCoord = uint2(i + l1, rowIndex);
				Innerloop(pairCoord, matchCoord, Complex(u1, u2));
			}
			Real z =  u1 * c1 - u2 * c2;
			u2 = u1 * c2 + u2 * c1;
			u1 = z;
		}
		c2 = sqrt((Real(1.0) - c1) * Real(0.5));
		if (!inverse) 
			c2 = -c2;
		c1 = sqrt((Real(1.0) + c1) * Real(0.5));
	}

#endif

	[branch] if (!inverse) {
		const float multiplier = 1.f/float(N);
		for (uint i=0; i<N; i++) {
			Scale(uint2(i, rowIndex), multiplier);
		}
	}
}

void FFT_Column(uint columnIndex, uint N, bool inverse)
{
	const Real pi = inverse ? 3.14159265358979323846 : -3.14159265358979323846;

		//
		//		We have to do the "butterfly" bit position reversal...
		//		Maybe there's some way to remove half the bit reversals
		//		(because the output of the first batch of FFTs is used
		//		as the input for the next batch of FFTs)
		//

	{
		uint i2 = N >> 1;
		uint j = 0;
		for (uint i=0; i<N-1; i++) {
			if (i < j) {
				uint2 iCoord = uint2(columnIndex, i);
				uint2 jCoord = uint2(columnIndex, j);
				Swap(iCoord, jCoord);
			}
			uint k = i2;
			while (k <= j) {
				j -= k;
				k >>= 1;
			}
			j += k;
		}
	}	

#if !defined(AVOID_TRIG)

		//		Iteration through dyads, quadruples, octads and so on...
		//		  -- todo -- set "N" to a constant for the optimiser?
	for (uint step = 1; step < N; step <<= 1) {

		const uint jump = step << 1;				//   jump to the next entry of the same transform factor
		const Real delta = float(pi) / float(step);	//   Angle increment
		const Real sine = sin(delta * Real(.5));			//   Auxiliary sin(delta / 2)
		const Complex multiplier = Complex(Real(-2.) * sine * sine, sin(delta));	//   multiplier for trigonometric recurrence
		Complex factor = Complex(Real(1.), Real(0));			//   Start value for transform factor, fi = 0

			//
			//   Iteration through groups of different transform factor
			//				
		for (uint group = 0; group < step; ++group) {

				//   Iteration within group 
			for (uint pair = group; pair < N; pair += jump) {
				uint2 pairCoord = uint2(columnIndex, pair);
				uint2 matchCoord = uint2(columnIndex, pair + step);
				Innerloop(pairCoord, matchCoord, factor);
			}

				//   Successive transform factor via trigonometric recurrence
			factor = ComplexMultiply(multiplier, factor) + factor;
		}

	}

#else

	Real c1 = Real(-1.0); 
	Real c2 = Real(0.0);
	uint l2 = 1;
	[flatten] for (uint step = 1; step < N; step <<= 1) {
		uint l1 = l2;
		l2 <<= 1;
		Real u1 = Real(1.0); 
		Real u2 = Real(0.0);
		[flatten] for (uint j=0;j<l1;j++) {
			[flatten] for (uint i=j;i<N;i+=l2) {
				uint2 pairCoord = uint2(columnIndex, i);
				uint2 matchCoord = uint2(columnIndex, i + l1);
				Innerloop(pairCoord, matchCoord, Complex(u1, u2));
			}
			Real z =  u1 * c1 - u2 * c2;
			u2 = u1 * c2 + u2 * c1;
			u1 = z;
		}
		c2 = sqrt((Real(1.0) - c1) * Real(0.5));
		if (!inverse) 
			c2 = -c2;
		c1 = sqrt((Real(1.0) + c1) * Real(0.5));
	}

#endif

	[branch] if (!inverse) {
		const float multiplier = 1.f/float(N);
		for (uint i=0; i<N; i++) {
			Scale(uint2(columnIndex, i), multiplier);
		}
	}
}

#define ThreadsPerGroup 32

bool DoInverseTransform()
{
	#if defined(DO_INVERSE)
		return bool(DO_INVERSE);
	#else
		return false;
	#endif
}

[numthreads(ThreadsPerGroup, 1, 1)]
	void FFT2D_1(uint3 dispatchThreadId : SV_DispatchThreadID, uint3 threadId : SV_GroupThreadID)
{
		//
		//		Calculate the FFT for a given texture.
		//		Use an inplace calculation (assume input texture is in "butterfly" reordering)
		//
		//		Starting with basic code from:
		//			http://www.librow.com/articles/article-10
		//		See also dimensional FFT here:
		//			http://paulbourke.net/miscellaneous/dft/
		//
		//		This is using the standard Cooley–Tukey algorithm
		//
		//		2D FFT transforms each row by a 1D FFT,
		//		and then transforms each column by a 1D FFT.
		//
		//		So, we can distribute this across multiple threads
		//		and thread groups by doing a single 1D FFT in
		//		each thread... It avoids having to parrallize the
		//		1D FFT calculation (which can be parrallized, but is
		//		simplier if it's not)
		//
		//		If we assume the input texture is square, we can do this
		//		with a single dispatch. (Actually we can't, because there's
		//		no way to sync all threads across all groups)
		//

	uint2 dimensions;
	WorkingTextureRealPart.GetDimensions(dimensions.x, dimensions.y);
	FFT_Row(dispatchThreadId.x, dimensions.x, DoInverseTransform());
}

[numthreads(ThreadsPerGroup, 1, 1)]
	void FFT2D_2(uint3 dispatchThreadId : SV_DispatchThreadID, uint3 threadId : SV_GroupThreadID)
{
	uint2 dimensions;
	WorkingTextureRealPart.GetDimensions(dimensions.x, dimensions.y);
	FFT_Column(dispatchThreadId.x, dimensions.y, DoInverseTransform());
}

Texture2D<uint>		SetupTexture0RealPart;
Texture2D<uint>		SetupTexture0ImaginaryPart;
Texture2D<uint>		SetupTexture1RealPart;
Texture2D<uint>		SetupTexture1ImaginaryPart;

void WriteSetupResult(Complex result, int2 kCoords, float2 k, float magK)
{
	WorkingTextureRealPart[kCoords.xy]			= asuint(float(result.x));
	WorkingTextureImaginaryPart[kCoords.xy]		= asuint(float(result.y));

	uint2 x = uint2(0, 0);
	uint2 y = uint2(0, 0);
	if (magK > 0.00001f) {
		Complex xDisplValue = ComplexMultiply(Complex(0.f, -k.x / magK), result);
		Complex yDisplValue = ComplexMultiply(Complex(0.f, -k.y / magK), result);

			// this code and cause compiler errors if written in certain ways.
		x = uint2(asuint(float(xDisplValue.x)), asuint(float(xDisplValue.y)));
		y = uint2(asuint(float(yDisplValue.x)), asuint(float(yDisplValue.y)));
	}

	WorkingTextureXRealPart[kCoords.xy]			= x.x;
	WorkingTextureXImaginaryPart[kCoords.xy]	= x.y;

	WorkingTextureYRealPart[kCoords.xy]			= y.x;
	WorkingTextureYImaginaryPart[kCoords.xy]	= y.y;
}

Complex LoadH0(uint2 kCoords)
{
		// use "SpectrumFade" to fade between to "H0" fields
	return Complex(
		lerp(asfloat(SetupTexture0RealPart[kCoords.xy]),			asfloat(SetupTexture1RealPart[kCoords.xy]),			SpectrumFade),
		lerp(asfloat(SetupTexture0ImaginaryPart[kCoords.xy]),		asfloat(SetupTexture1ImaginaryPart[kCoords.xy]),	SpectrumFade));
}

[numthreads(32, 32, 1)]
	void Setup(uint3 dispatchThreadId : SV_DispatchThreadID)
{
		//		in theory, negKCoords should be exactly a mirror image of kCoords.
		//			But this is causing some wierd problems at low time values.
		//			(maybe because -k sometimes becomes the same as k?)
		//		It seems to work better if we only flip x (or just skip the -k term completely)
		//		Other implementations do the same thing. It's a little wierd, I'm not sure what
		//		the justification is.
	int2 kCoords = int2(dispatchThreadId.xy);
	int2 negKCoords = int2(GridWidth-1-dispatchThreadId.x, dispatchThreadId.y);

	Complex h0k		= LoadH0(   kCoords);
	Complex h0Negk	= LoadH0(negKCoords);

	#if (DO_FREQ_BOOST==1)
		const float freqBoost = 2.f;
	#else
		const float freqBoost = 1.f;
	#endif

		// "gridMidPoint" must be perfectly balanced so that k.x for the first
		//	pixel in a row is exactly negative one times the k.x for the last
		//	pixel in a row.
		//	This is required for "USE_MIRROR_OPT" to work. The most inituitive
		//	way to get this to work is to just add .5f to the pixel coordinates.
		//	eg:
		//		(.5f - 32) = -(63.5 - 32)
	float2 gridMidPoint = float2(float(GridWidth)/2.f, float(GridHeight)/2.f);
	const float2 k = float2(
		freqBoost * 2.f * pi * (float(kCoords.x) + .5f - gridMidPoint.x) / PhysicalWidth,
		freqBoost * 2.f * pi * (float(kCoords.y) + .5f - gridMidPoint.y) / PhysicalHeight);
	const float gravitationalConstant = 9.8f;
	float magK = length(k);
	float w = sqrt(magK*gravitationalConstant);

		//		This code can be used to cause the animation
		//		to wrap after a certain period of time
		// const float wrappingTime = 256.f;
		// const float w0 = 2.f * pi / wrappingTime;
		// w = floor(w/w0)*w0;

		//	Note that half of the grid should end up being the conjugate of the other half
		//		.. so we can actually write the results into 2 grid places here
	Complex result0 = 
		  ComplexMultiply(h0k,						ComplexExponential( w*Time))
		+ ComplexMultiply(ComplexConjugate(h0Negk), ComplexExponential(-w*Time))
		;
	WriteSetupResult(result0, kCoords, k, magK);

	#if USE_MIRROR_OPT==1
		Complex result1 = ComplexConjugate(result0);
		WriteSetupResult(result1, negKCoords, float2(-k.x, k.y), magK);
	#endif
}


