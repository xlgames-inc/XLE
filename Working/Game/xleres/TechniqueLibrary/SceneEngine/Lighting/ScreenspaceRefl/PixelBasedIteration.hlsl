// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(PIXEL_BASED_ITERATION_H)
#define PIXEL_BASED_ITERATION_H

// Complex iteration method that will touch every pixel along a ray
// Intended for reference purposes -- because it's actually not very
// GPU friendly!

uint IterationOperator(
    int2 pixelCapCoord, float entryNDCDepth,
    float exitNDCDepth, inout float ilastQueryDepth);

struct PBI
{
    float4		_clipStart;
    float4		_clipEnd;

    float2		_pixelEntryZW;
    float		_lastQueryDepth;

    bool		_gotIntersection;
    bool		_continueIteration;
    float2		_intersectionCoords;
    float       _distance;
    int			_testCount;
};

float2 DepthCalcX(float2 ndc, PBI ray)
{
        //	here, we're calculating the z and w values of the given ray
        //	for a given point in screen space. This should be used for
        //	"x" dominant rays.
    float4 clipStart = ray._clipStart;
    float4 clipEnd = ray._clipEnd;
    float l = (ndc.x - (clipStart.x/clipStart.w)) / ((clipEnd.x/clipEnd.w) - (clipStart.x/clipStart.w));
    return lerp(clipStart.zw, clipEnd.zw, l);
}

float2 DepthCalcY(float2 ndc, PBI ray)
{
        //	here, we're calculating the z and w values of the given ray
        //	for a given point in screen space. This should be used for
        //	"y" dominant rays.
    float4 clipStart = ray._clipStart;
    float4 clipEnd = ray._clipEnd;
    float l = (ndc.y - (clipStart.y/clipStart.w)) / ((clipEnd.y/clipEnd.w) - (clipStart.y/clipStart.w));
    return lerp(clipStart.zw, clipEnd.zw, l);
}

void PBI_Opr(inout PBI iterator, float2 exitZW, int2 pixelCapCoord, float2 edgeCoords)
{
    float2 entryZW = iterator._pixelEntryZW;
    iterator._pixelEntryZW = exitZW;
    ++iterator._testCount;

        //	We now know the depth values where the ray enters and exits
        //	this pixel.
        //	We can compare this to the values in the depth buffer
        //	to look for an intersection
    float ndc0 = entryZW.x / entryZW.y;
    float ndc1 =  exitZW.x /  exitZW.y;

        //	we have to check to see if we've left the view frustum.
        //	going too deep is probably not likely, but we can pass
        //	in front of the near plane
    if (ndc1 <= 0.f) {
        iterator._continueIteration = false;
        iterator._intersectionCoords = 0.0.xx;
        return;
    }

    uint opResult = IterationOperator(pixelCapCoord, ndc0, ndc1, iterator._lastQueryDepth);
    if (opResult) {
        iterator._intersectionCoords = (opResult==1)?edgeCoords:float2(pixelCapCoord);
        iterator._continueIteration = false;
        iterator._gotIntersection = true;
    }
}

float2 PixelToNDC(float2 pixelCoords, float2 outputDimensions)
{
    return float2(
        (pixelCoords.x *  2.0f - 0.5f) / outputDimensions.x - 1.f,
        (pixelCoords.y * -2.0f + 0.5f) / outputDimensions.y + 1.f);
}

void PBI_OprX(inout PBI iterator, int2 e0, int2 e1, float alpha, int2 pixelCoord, float2 outputDimensions)
{
    float2 edgeIntersection = lerp(float2(e0), float2(e1), alpha);
    float2 exitZW = DepthCalcX(PixelToNDC(edgeIntersection, outputDimensions), iterator);
    PBI_Opr(iterator, exitZW, pixelCoord, edgeIntersection);
}

void PBI_OprY(inout PBI iterator, int2 e0, int2 e1, float alpha, int2 pixelCoord, float2 outputDimensions)
{
    float2 edgeIntersection = lerp(float2(e0), float2(e1), alpha);
    float2 exitZW = DepthCalcY(PixelToNDC(edgeIntersection, outputDimensions), iterator);
    PBI_Opr(iterator, exitZW, pixelCoord, edgeIntersection);
}

struct PBISettings
{
    uint pixelStep;
    uint initialPixelsSkip;
};

PBI PixelBasedIteration(float4 clipStart, float4 clipEnd, float2 outputDimensions, PBISettings settings)
{
    // float2 screenSpaceDirection = normalize(clipEnd.xy/clipEnd.w - clipStart.xy/clipStart.w);
    // int w = int( float(ReflectionDistancePixels) * screenSpaceDirection.x);
    // int h = int(-float(ReflectionDistancePixels) * screenSpaceDirection.y);

    int2 pixelStart = AsZeroToOne(clipStart.xy / clipStart.w) * outputDimensions;
    int2 pixelEnd = AsZeroToOne(clipEnd.xy / clipEnd.w) * outputDimensions;
    int w = pixelEnd.x - pixelStart.x;
    int h = pixelEnd.y - pixelStart.y;

    int ystep = sign(h); h = abs(h);
    int xstep = sign(w); w = abs(w);
    int ddy = 2 * h;  // We may not need to double this (because we're starting from the corner of the pixel)
    int ddx = 2 * w;

    int i=0;
    int errorprev = 0, error = 0; // (start from corner. we don't want to start in the middle of the grid element)
    int x = pixelStart.x, y = pixelStart.y;

            //	step 2 pixel forward
            //		this helps avoid bad intersections greatly. The first pixel is the starter pixel,
            //		the second pixel is the first pixel "cap" we'll test
            //	So we must skip at least 2 pixels before iteration. After that, we can offset
            //	based on a random value -- this will add some noise, but will cover the big
            //	gap between pixel steps.
    if (ddx >= ddy) {
        x += settings.initialPixelsSkip * xstep;
        y += ystep * ddy / ddx;
        errorprev = error = ddy % ddx;
        i += settings.initialPixelsSkip;
    } else {
        y += settings.initialPixelsSkip * ystep;
        x += xstep * ddx / ddy;
        errorprev = error = ddx % ddy;
        i += settings.initialPixelsSkip;
    }

        //	We don't have to visit every single pixel.
        //	use "pixel step" to jump over some pixels. It adds some noise, but not too bad.
    xstep *= settings.pixelStep;
    ystep *= settings.pixelStep;

    PBI iterator;
    iterator._clipStart             = clipStart;
    iterator._clipEnd               = clipEnd;
    iterator._gotIntersection		= false;
    iterator._continueIteration		= true;
    iterator._intersectionCoords	= float2(-1.f, -1.f);
    iterator._testCount				= 0;
    iterator._distance              = 0.f;

    float2 ndcStart = PixelToNDC(float2(x, y), outputDimensions);
    iterator._pixelEntryZW = (ddx >= ddy)?DepthCalcX(ndcStart, iterator):DepthCalcY(ndcStart, iterator);
    // iterator._pixelEntryZW = clipStart.zw;

    //#if defined(DEPTH_IN_LINEAR_COORDS)
        // not implemented
    //#else
    //    iterator._lastQueryDepth = DownSampledDepth[int2(x, y)];
    //#endif
    iterator._lastQueryDepth = 1.f;

    // We're just going crazy with conditions and looping here!
    // Surely there must be a better way to do this!

    if (ddx >= ddy) {
        for (; i<w && iterator._continueIteration; i+=settings.pixelStep) {
            int2 pixelCapCoord = int2(x, y);

            x += xstep;
            error += ddy;

            int2 e0, e1;
            float edgeAlpha;

            if (error >= ddx) {

                y += ystep;
                error -= ddx;

                    //  The cases for what happens here. Each case defines different edges
                    //  we need to check
                if (error != 0) {
                    e0 = int2(x, y); e1 = int2(x, y+ystep);
                    edgeAlpha = error / float(ddx);

                    int2 e0b = int2(x-xstep, y);
                    int2 e1b = int2(x, y);
                    int tri0 = ddx - errorprev;
                    int tri1 = error;
                    PBI_OprX(iterator, e0b, e1b, tri0 / float(tri0+tri1), pixelCapCoord, outputDimensions);
                    if (!iterator._continueIteration) break;
                } else {
                        // passes directly though the corner. Easiest case.
                    e0 = e1 = int2(x, y);
                    edgeAlpha = 0.f;
                }

            } else {
                    // simple -- y isn't changing, just moving to the next "x" grid
                e0 = int2(x, y); e1 = int2(x, y+ystep);
                edgeAlpha = error / float(ddx);
            }

            PBI_OprX(iterator, e0, e1, edgeAlpha, int2(x, y), outputDimensions);
            errorprev = error;
        }
        iterator._distance = i / float(w);
    } else {
        for (; i<h && iterator._continueIteration; i+=settings.pixelStep) {
            int2 pixelCapCoord = int2(x, y);

            y += ystep;
            error += ddx;

            int2 e0, e1;
            float edgeAlpha;

            if (error >= ddy) {

                x += xstep;
                error -= ddy;

                    //  The cases for what happens here. Each case defines different edges
                    //  we need to check
                if (error != 0) {
                    e0 = int2(x, y); e1 = int2(x+xstep, y);
                    edgeAlpha = error / float(ddy);

                    int2 e0b = int2(x, y-ystep);
                    int2 e1b = int2(x, y);
                    int tri0 = ddy - errorprev;
                    int tri1 = error;
                    PBI_OprY(iterator, e0b, e1b, tri0 / float(tri0+tri1), pixelCapCoord, outputDimensions);
                    if (!iterator._continueIteration) break;
                } else {
                        // passes directly though the corner. Easiest case.
                    e0 = e1 = int2(x, y);
                    edgeAlpha = 0.f;
                }

            } else {
                    // simple -- y isn't changing, just moving to the next "x" grid
                e0 = int2(x, y); e1 = int2(x+xstep, y);
                edgeAlpha = error / float(ddy);
            }

            PBI_OprY(iterator, e0, e1, edgeAlpha, int2(x, y), outputDimensions);
            errorprev = error;
        }
        iterator._distance = i / float(h);
    }

    return iterator;
}

#endif
