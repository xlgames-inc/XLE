// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FluidAdvection.h"
#include "../Math/RegularNumberField.h"

#pragma warning(disable:4714)
#pragma push_macro("new")
#undef new
#include <Eigen/Dense>
#pragma pop_macro("new")

namespace SceneEngine
{
    using ScalarField2D = XLEMath::ScalarField2D<Eigen::VectorXf>;
    using VectorField2D = VectorField2DSeparate<Eigen::VectorXf>;
    using ScalarField3D = XLEMath::ScalarField3D<Eigen::VectorXf>;
    using VectorField3D = VectorField3DSeparate<Eigen::VectorXf>;

    UInt3 As3DDims(UInt2 input)     { return Expand(input, 1u); }
    UInt3 As3DBorder(UInt2 input)   { return UInt3(1,1,0); }
    UInt3 As3DDims(UInt3 input)     { return input; }
    UInt3 As3DBorder(UInt3 input)   { return UInt3(1,1,1); }

    template<typename OutType, typename InType>
        OutType ConvertVector(const InType& in)
        {
            OutType result; 
            unsigned i=0u; 
            for (; i<std::min((unsigned)InType::dimension, (unsigned)OutType::dimension); ++i)
                result[i] = (OutType::value_type)in[i];
            for (; i<(unsigned)OutType::dimension; ++i)
                result[i] = (OutType::value_type)0;
            return result;
        }

    template<unsigned SamplingFlags, typename Field>
        static typename Field::ValueType AdvectRK4(
            const Field& velFieldT0, const Field& velFieldT1,
            typename Field::Coord pt, typename Field::FloatCoord velScale)
        {
            const auto s = velScale;
            const auto halfS = decltype(s)(s / 2);
    
            auto startTap = ConvertVector<typename Field::FloatCoord>(pt);
            auto k1 = velFieldT0.Load(pt);
            auto k2 = .5f * velFieldT0.Sample<SamplingFlags>(startTap + MultiplyAcross(halfS, k1))
                    + .5f * velFieldT1.Sample<SamplingFlags>(startTap + MultiplyAcross(halfS, k1))
                    ;
            auto k3 = .5f * velFieldT0.Sample<SamplingFlags>(startTap + MultiplyAcross(halfS, k2))
                    + .5f * velFieldT1.Sample<SamplingFlags>(startTap + MultiplyAcross(halfS, k2))
                    ;
            auto k4 = velFieldT1.Sample<SamplingFlags>(startTap + MultiplyAcross(s, k3));
    
            auto finalVel = (1.f / 6.f) * (k1 + 2.f * k2 + 2.f * k3 + k4);
            return startTap + MultiplyAcross(s, finalVel);
        }

    template<unsigned SamplingFlags, typename Field>
        static typename Field::ValueType AdvectRK4(
            const Field& velFieldT0, const Field& velFieldT1,
            typename Field::FloatCoord pt, typename Field::FloatCoord velScale)
        {
            const auto s = velScale;
            const auto halfS = decltype(s)(s / 2);

                // when using a float point input, we need bilinear interpolation
            auto k1 = velFieldT0.Sample<SamplingFlags>(pt);
            auto k2 = .5f * velFieldT0.Sample<SamplingFlags>(pt + MultiplyAcross(halfS, k1))
                    + .5f * velFieldT1.Sample<SamplingFlags>(pt + MultiplyAcross(halfS, k1))
                    ;
            auto k3 = .5f * velFieldT0.Sample<SamplingFlags>(pt + MultiplyAcross(halfS, k2))
                    + .5f * velFieldT1.Sample<SamplingFlags>(pt + MultiplyAcross(halfS, k2))
                    ;
            auto k4 = velFieldT1.Sample<SamplingFlags>(pt + MultiplyAcross(s, k3));

            auto finalVel = (1.f / 6.f) * (k1 + 2.f * k2 + 2.f * k3 + k4);
            return pt + MultiplyAcross(s, finalVel);
        }

    template<typename Type> static Type MaxValue();
    template<> static float MaxValue()         { return FLT_MAX; }
    template<> static Float2 MaxValue()        { return Float2(FLT_MAX, FLT_MAX); }
    template<> static Float3 MaxValue()        { return Float3(FLT_MAX, FLT_MAX, FLT_MAX); }
    static float   MinAcross(float lhs, float rhs)   { return std::min(lhs, rhs); }
    static Float2  MinAcross(Float2 lhs, Float2 rhs) { return Float2(std::min(lhs[0], rhs[0]), std::min(lhs[1], rhs[1])); }
    static Float3  MinAcross(Float3 lhs, Float3 rhs) { return Float3(std::min(lhs[0], rhs[0]), std::min(lhs[1], rhs[1]), std::min(lhs[2], rhs[2])); }
    static float   MaxAcross(float lhs, float rhs)   { return std::max(lhs, rhs); }
    static Float2  MaxAcross(Float2 lhs, Float2 rhs) { return Float2(std::max(lhs[0], rhs[0]), std::max(lhs[1], rhs[1])); }
    static Float3  MaxAcross(Float3 lhs, Float3 rhs) { return Float3(std::max(lhs[0], rhs[0]), std::max(lhs[1], rhs[1]), std::max(lhs[2], rhs[2])); }

    // static Float2 ClampAcross(const Float2& input, const Float2& mins, const Float2& maxs)
    // {
    //     return Float2(Clamp(input[0], mins[0], maxs[0]), Clamp(input[1], mins[1], maxs[1]));
    // }
    // 
    // static Float3 ClampAcross(const Float3& input, const Float3& mins, const Float3& maxs)
    // {
    //     return Float3(Clamp(input[0], mins[0], maxs[0]), Clamp(input[1], mins[1], maxs[1]), Clamp(input[2], mins[2], maxs[2]));
    // }

    template<unsigned WrappingFlags, typename VectorType>
        static VectorType ApplyBoundary(
            const VectorType& input, 
            const VectorType& dims)
    {
        VectorType result;
        static_assert(RNFSample::WrapY == (RNFSample::WrapX<<1), "Expecting wrapping flags to be sequential bits");
        static_assert(RNFSample::WrapZ == (RNFSample::WrapX<<2), "Expecting wrapping flags to be sequential bits");
        for (unsigned c=0; c<VectorType::dimension; ++c) {
            if (WrappingFlags & (RNFSample::WrapX<<c)) {
                result[c] = XlFMod(input[c]+dims[c], dims[c]);
            } else {
                result[c] = Clamp(input[c], 0.f, dims[c]-1.f-1e-5f);
            }
        }
        return result;
    }

    template<unsigned SamplingFlags, typename Field>
        typename Field::ValueType LoadWithNearbyRange(
            typename Field::ValueType& minNeighbour, 
            typename Field::ValueType& maxNeighbour, const Field& field, typename Field::FloatCoord pt)
        {
            typename Field::ValueType predictorParts[Field::NeighborCount];
            float predictorWeights[Field::BilinearWeightCount];
            field.GatherNeighbors(predictorParts, predictorWeights, pt, SamplingFlags);
            
            minNeighbour =  MaxValue<typename Field::ValueType>();
            maxNeighbour = -MaxValue<typename Field::ValueType>();
            for (unsigned c=0; c<Field::NeighborCount; ++c) {
                minNeighbour = MinAcross(predictorParts[c], minNeighbour);
                maxNeighbour = MaxAcross(predictorParts[c], maxNeighbour);
            }

            if (constant_expression<(SamplingFlags & RNFSample::Cubic)==0>::result()) {
                Field::ValueType result =  predictorWeights[0] * predictorParts[0];
                for (unsigned i=1; i<Field::BilinearWeightCount; ++i)   // hopefully the compiler should unroll this loop (which is short, there are only 4 or 8 weights)
                    result += predictorWeights[i] * predictorParts[i];
                return result;
            } else {
                return field.Sample<SamplingFlags>(pt);
            }
        }
    
    template<unsigned WrappingFlags, typename Field, typename VelField>
        static void PerformAdvection_Internal(
            Field dstValues, Field srcValues, 
            VelField velFieldT0, VelField velFieldT1,
            float deltaTime, const AdvectionSettings& settings)
    {
        //
        // This is the advection step. We will use the method of characteristics.
        //
        // We have a few different options for the stepping method:
        //  * basic euler forward integration (ie, just step forward in time)
        //  * forward integration method divided into smaller time steps
        //  * Runge-Kutta integration
        //  * Modified MacCormack methods
        //  * Back and Forth Error Compensation and Correction (BFECC)
        //
        // Let's start without any complex boundary conditions.
        //
        // We have to be careful about how the velocity sample is aligned with
        // the grid cell. Incorrect alignment will produce a bias in the way that
        // we interpolate the field.
        //
        // We could consider offsetting the velocity field by half a cell (see
        // Visual Simulation of Smoke, Fedkiw, et al)
        //
        // Also consider Semi-Lagrangian methods for large timesteps (when the CFL
        // number is larger than 1)
        //

        const auto advectionMethod = settings._method;
        const auto adjvectionSteps = settings._subSteps;

        assert(dstValues.Dimensions() == srcValues.Dimensions());
        assert(dstValues.Dimensions() == velFieldT0.Dimensions());
        assert(dstValues.Dimensions() == velFieldT1.Dimensions());
        const UInt3 dims = As3DDims(dstValues.Dimensions());

            // when the border condition is "margin" we create a 1 cell margin on that
            // edge that will be read from, but not written to
        UInt3 margin = As3DBorder(dstValues.Dimensions());
        if (settings._borderX != AdvectionBorder::Margin) margin[0] = 0;
        if (settings._borderY != AdvectionBorder::Margin) margin[1] = 0;
        if (settings._borderZ != AdvectionBorder::Margin) margin[2] = 0;

        using FloatCoord = typename VelField::FloatCoord;
        using Coord = typename VelField::Coord;
        const auto velFieldScale = ConvertVector<FloatCoord>(
            Float3(
                float(dims[0]-2*margin[0]),
                float(dims[1]-2*margin[1]),
                float(dims[2]-2*margin[2])));   // (grid size without borders)
        const auto clampMax = ConvertVector<FloatCoord>(dims);

        if (advectionMethod == AdvectionMethod::ForwardEuler) {

                //  For each cell in the grid, trace backwards
                //  through the velocity field to find an approximation
                //  of where the point was in the previous frame.

            for (unsigned z=margin[2]; z<dims[2]-margin[2]; ++z)
                for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                    for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {
                        auto coord = ConvertVector<Coord>(UInt3(x, y, z));
                        auto startVel = velFieldT1.Load(coord);
                        FloatCoord tap = ConvertVector<FloatCoord>(coord) - MultiplyAcross(deltaTime * velFieldScale, startVel);
                        tap = ApplyBoundary<WrappingFlags>(tap, clampMax);
                        dstValues.Write(coord, srcValues.Sample<0>(tap));
                    }

        } else if (advectionMethod == AdvectionMethod::ForwardEulerDiv) {

            auto stepScale = decltype(velFieldScale)(deltaTime * velFieldScale / float(adjvectionSteps));
            for (unsigned z=margin[2]; z<dims[2]-margin[2]; ++z)
                for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                    for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {

                        auto coord = ConvertVector<Coord>(UInt3(x, y, z));
                        auto tap = ConvertVector<FloatCoord>(UInt3(x, y, z));
                        auto vel = velFieldT0.Load(coord);
                        for (unsigned s=1; ; ++s) {
                            tap -= MultiplyAcross(stepScale, vel);
                            tap = ApplyBoundary<WrappingFlags>(tap, clampMax);
                            if (s>=adjvectionSteps) break;

                            vel = LinearInterpolate(
                                velFieldT0.Sample<0>(tap),
                                velFieldT1.Sample<0>(tap),
                                s / float(adjvectionSteps-1));
                        }

                        dstValues.Write(coord, srcValues.Sample<WrappingFlags>(tap));
                    }

        } else if (advectionMethod == AdvectionMethod::RungeKutta) {

            if (settings._interpolation == AdvectionInterp::Bilinear) {

                const auto SamplingFlags = WrappingFlags;
                for (unsigned z=margin[2]; z<dims[2]-margin[2]; ++z)
                    for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                        for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {

                                // This is the RK4 version
                                // We'll use the average of the velocity field at t and
                                // the velocity field at t+dt as an estimate of the field
                                // at t+.5*dt

                                // Note that we're tracing the velocity field backwards.
                                // So doing k1 on velField1, and k4 on velFieldT0
                                //      -- hoping this will interact with the velocity diffusion more sensibly
                            auto coord = ConvertVector<Coord>(UInt3(x, y, z));
                            const auto tap = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, coord, -deltaTime * velFieldScale);
                            dstValues.Write(coord, srcValues.Sample<SamplingFlags>(tap));

                        }

            } else {

                const auto SamplingFlags = RNFSample::Cubic|WrappingFlags;
                for (unsigned z=margin[2]; z<dims[2]-margin[2]; ++z)
                    for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                        for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {
                            auto coord = ConvertVector<Coord>(UInt3(x, y, z));
                            const auto tap = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, coord, -deltaTime * velFieldScale);
                            dstValues.Write(coord, srcValues.Sample<SamplingFlags>(tap));
                        }

            }

        } else if (advectionMethod == AdvectionMethod::MacCormackRK4) {

                //
                // This is a modified MacCormack scheme, as described in An Unconditionally
                // Stable MacCormack Method -- Selle & Fedkiw, et al.
                //  http://physbam.stanford.edu/~fedkiw/papers/stanford2006-09.pdf
                //
                // It's also similar to the (oddly long nammed) Back And Forth Error Compensation 
                // and Correction (BFECC).
                //
                // Basically, we want to run an initial predictor step, then run a backwards
                // advection to find an intermediate point. The difference between the value at
                // the initial point and the intermediate point is used as a error term.
                //
                // This way, we get an improved estimate, but with only 2 advection steps.
                //
                // We need to use some advection method for the forward and advection steps. Often
                // a semi-lagrangian method is used (particularly velocities and timesteps are large
                // with respect to the grid size). 
                //
                // But here, we'll use RK4.
                //
                // We also need a way to check for overruns and oscillation cases. Selle & Fedkiw
                // suggest using a normal semi-Lagrangian method in these cases. We'll try a simplier
                // method and just clamp.
                //

            if (settings._interpolation == AdvectionInterp::Bilinear) {

                const auto SamplingFlags = WrappingFlags;
                for (unsigned z=margin[2]; z<dims[2]-margin[2]; ++z)
                    for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                        for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {

                            auto coord = ConvertVector<Coord>(UInt3(x, y, z));

                                // advect backwards in time first, to find the predictor
                            const auto predictor = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, coord, -deltaTime * velFieldScale);
                                // advect forward again to find the error tap
                            const auto reversedTap = AdvectRK4<SamplingFlags>(velFieldT0, velFieldT1, predictor, deltaTime * velFieldScale);

                            auto originalValue = srcValues.Load(coord);
                            auto reversedValue = srcValues.Sample<SamplingFlags>(reversedTap);
                            Field::ValueType finalValue;

                                // Here we clamp the final result within the range of the neighbour cells of the 
                                // original predictor. This prevents the scheme from becoming unstable (by avoiding
                                // irrational values for 0.5f * (originalValue - reversedValue)
                            const bool doRangeClamping = true;
                            if (constant_expression<doRangeClamping>::result()) {
                                typename Field::ValueType minNeighbour, maxNeighbour;
                                auto predictorValue = LoadWithNearbyRange<SamplingFlags>(minNeighbour, maxNeighbour, srcValues, predictor);
                                finalValue = typename Field::ValueType(predictorValue + .5f * (originalValue - reversedValue));
                                finalValue = MaxAcross(finalValue, minNeighbour);
                                finalValue = MinAcross(finalValue, maxNeighbour);
                            } else {
                                auto predictorValue = srcValues.Sample<SamplingFlags>(predictor);
                                finalValue = typename Field::ValueType(predictorValue + .5f * (originalValue - reversedValue));
                            }

                            dstValues.Write(coord, finalValue);

                        }   

            } else {

                const auto SamplingFlags = RNFSample::Cubic|WrappingFlags;
                for (unsigned z=margin[2]; z<dims[2]-margin[2]; ++z)
                    for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                        for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {

                            auto coord = ConvertVector<Coord>(UInt3(x, y, z));
                            const auto predictor = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, coord, -deltaTime * velFieldScale);
                            const auto reversedTap = AdvectRK4<SamplingFlags>(velFieldT0, velFieldT1, predictor, deltaTime * velFieldScale);

                            auto originalValue = srcValues.Load(coord);
                            auto reversedValue = srcValues.Sample<SamplingFlags>(reversedTap);

                            Field::ValueType minNeighbour, maxNeighbour;
                            auto predictorValue = LoadWithNearbyRange<SamplingFlags>(minNeighbour, maxNeighbour, srcValues, predictor);
                            auto finalValue = Field::ValueType(predictorValue + .5f * (originalValue - reversedValue));
                            finalValue = MaxAcross(finalValue, minNeighbour);
                            finalValue = MinAcross(finalValue, maxNeighbour);

                            dstValues.Write(coord, finalValue);

                        }

            }

        }

    }

    template<typename Field, typename VelField>
        void PerformAdvection(
            Field dstValues, Field srcValues, 
            VelField velFieldT0, VelField velFieldT1,
            float deltaTime, const AdvectionSettings& settings)
    {
            // it's awkward, but we need to convertion between the
            // variables "settings._border..." and the compile time
            // static RNFSample flags. Only a fixed number of variations
            // are supported... others will fall back to clamping in all
            // directions.
        if (    settings._borderX == AdvectionBorder::Wrap 
            &&  settings._borderY != AdvectionBorder::Wrap 
            &&  settings._borderZ != AdvectionBorder::Wrap) {

            PerformAdvection_Internal<RNFSample::WrapX|RNFSample::ClampY|RNFSample::ClampZ>(
                dstValues, srcValues, velFieldT0, velFieldT1, deltaTime, settings);

        } else if ( settings._borderX != AdvectionBorder::Wrap 
            &&      settings._borderY == AdvectionBorder::Wrap 
            &&      settings._borderZ != AdvectionBorder::Wrap) {

            PerformAdvection_Internal<RNFSample::ClampX|RNFSample::WrapY|RNFSample::ClampZ>(
                dstValues, srcValues, velFieldT0, velFieldT1, deltaTime, settings);

        } else if ( settings._borderX != AdvectionBorder::Wrap 
            &&      settings._borderY != AdvectionBorder::Wrap 
            &&      settings._borderZ == AdvectionBorder::Wrap) {

            PerformAdvection_Internal<RNFSample::ClampX|RNFSample::ClampY|RNFSample::WrapZ>(
                dstValues, srcValues, velFieldT0, velFieldT1, deltaTime, settings);

        } else if ( settings._borderX == AdvectionBorder::Wrap 
            &&      settings._borderY == AdvectionBorder::Wrap 
            &&      settings._borderZ == AdvectionBorder::Wrap) {

            PerformAdvection_Internal<RNFSample::WrapX|RNFSample::WrapY|RNFSample::WrapZ>(
                dstValues, srcValues, velFieldT0, velFieldT1, deltaTime, settings);

        } else if ( settings._borderX == AdvectionBorder::Wrap 
            &&      settings._borderY == AdvectionBorder::Wrap 
            &&      settings._borderZ != AdvectionBorder::Wrap) {

            PerformAdvection_Internal<RNFSample::WrapX|RNFSample::WrapY|RNFSample::ClampZ>(
                dstValues, srcValues, velFieldT0, velFieldT1, deltaTime, settings);

        } else {

            assert(settings._borderX != AdvectionBorder::Wrap && settings._borderY != AdvectionBorder::Wrap && settings._borderZ != AdvectionBorder::Wrap);
            PerformAdvection_Internal<RNFSample::ClampX|RNFSample::ClampY|RNFSample::ClampZ>(
                dstValues, srcValues, velFieldT0, velFieldT1, deltaTime, settings);

        }
    }

    AdvectionSettings::AdvectionSettings()
    {
        _method = AdvectionMethod::MacCormackRK4;
        _interpolation = AdvectionInterp::Bilinear;
        _subSteps = 4;
        _borderX = AdvectionBorder::Margin; 
        _borderY = AdvectionBorder::Margin; 
        _borderZ = AdvectionBorder::Margin;
    }

    AdvectionSettings::AdvectionSettings(
        AdvectionMethod method,
        AdvectionInterp interpolation,
        unsigned        subSteps,
        AdvectionBorder borderX, AdvectionBorder borderY, AdvectionBorder borderZ)
    {
        _method = method;
        _interpolation = interpolation;
        _subSteps = subSteps;
        _borderX = borderX;
        _borderY = borderY;
        _borderZ = borderZ;
    }

    template void PerformAdvection(
        ScalarField2D, ScalarField2D, 
        VectorField2D, VectorField2D,
        float, const AdvectionSettings&);

    template void PerformAdvection(
        VectorField2D, VectorField2D, 
        VectorField2D, VectorField2D,
        float, const AdvectionSettings&);

    template void PerformAdvection(
        ScalarField3D, ScalarField3D, 
        VectorField3D, VectorField3D,
        float, const AdvectionSettings&);

    template void PerformAdvection(
        VectorField3D, VectorField3D, 
        VectorField3D, VectorField3D,
        float, const AdvectionSettings&);
}

