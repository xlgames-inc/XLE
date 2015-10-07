// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(METRICS_H)
#define METRICS_H

struct MetricsStructure
{
    uint    TranslucentSampleCount;
    uint    MaxTranslucentSampleCount;
    uint    PixelsWithTranslucentSamples;

    uint    TotalClusterCount;
    uint    TotalTileCount;
    uint    LightCullCount;
    uint    LightCalculateCount;
    uint    ClusterErrorCount;

    uint    StocasticTransLitFragmentCount;
    uint    StocasticTransPartialLitFragmentCount;

    uint    Placeholder[6];
};

#define _METRICS

#endif
