// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CloudsForm.h"
#include "FluidHelper.h"
#include "Fluid.h"
#include "FluidAdvection.h"
#include "../Utility/Meta/ClassAccessorsImpl.h"

namespace SceneEngine
{
    static float KelvinToCelsius(float kelvin) { return kelvin - 273.15f; }
    static float CelsiusToKelvin(float celius) { return celius + 273.15f; }

    static float PressureAtAltitude(float kilometers)
    {
            //
            //  Returns a value in pascals.
            //  For a given altitude, we can calculate the standard
            //  atmospheric pressure.
            //
            //  Actually the "wetness" of the air should affect the lapse
            //  rate... But we'll ignore that here, and assume all of the 
            //  air is dry for this calculation.
            //
            //  We could build in a humidity constant into the simulation.
            //  This should adjust the lapse rate. Also the T0 temperature
            //  values should be adjustable.
            //
            //  See here for more information:
            //      https://en.wikipedia.org/wiki/Atmospheric_pressure
            //
        const float g = 9.81f / 1000.f; // (in km/second)
        const float p0 = 101325.f;      // (in pascals, 1 standard atmosphere)
        const float T0 = CelsiusToKelvin(23.0f);         // (in kelvin, base temperature) (about 20 degrees)
        const float Rd = 287.058f;      // (in J . kg^-1 . K^-1. This is the "specific" gas constant for dry air. It is R / M, where R is the gas constant, and M is the ideal gas constant)

            //  We have a few choices for the lape rate here.
            //      we're could use a value close to the "dry adiabatic lapse rate" (around 9.8 kelvin/km)
            //      Or we could use a value around 6 or 7 -- this is the average lapse rate in the troposphere
            //  see https://en.wikipedia.org/wiki/Lapse_rate
            //  The minimum value for lapse rate in the troposhere should be around 4
            //      (see http://www.iac.ethz.ch/edu/courses/bachelor/vertiefung/atmospheric_physics/Slides_2012/buoyancy.pdf)
        const float tempLapseRate = 6.5f;

        // roughly: p0 * std::exp(kilometers * g / (1000.f * T0 * Rd));     (see wikipedia page)
        // see also the "hypsometric equation" equation, similar to above
        return p0 * std::pow(1.f - kilometers * tempLapseRate / T0, g / (tempLapseRate * Rd));
    }

    static float ExnerFunction(float pressure)
    {

            //
            //  The potential temperature is defined based on the atmosphere pressure.
            //  So, we can go backwards as well and get the temperature from the potential
            //  temperature (if we know the pressure)
            //
            //  Note that if "pressure" comes from PressureAtAltitude, we will be 
            //  multiplying by p0, and then dividing it away again.
            //
            //  The Exner function is ratio of the potential temperature and absolute temperature.
            //  So,
            //      temperature = ExnerFunction * potentialTemperature
            //      potentialTemperature = temperature / ExnerFunction
            //

        const float p0 = 101325.f;  // (in pascals, 1 standard atmosphere)
        const float Rd = 287.058f;  // in J . kg^-1 . K^-1. Gas constant for dry air
        const float cp = 1005.f;    // in J . kg^-1 . K^-1. heat capacity of dry air at constant pressure
        const float kappa = Rd/cp;
        return std::pow(pressure/p0, kappa);
    }
    
///////////////////////////////////////////////////////////////////////////////////////////////////

    class Troposphere
    {
    public:
        float   GetVaporMixingRatio(UInt2 gridCoord) const;
        float   GetPotentialTemperature(UInt2 gridCoord) const;
        float   AltitudeKm(unsigned gridY) const;
        float   ZScale() const;

        Troposphere(
            UInt2 gridDims,
            float altitudeMinKm, float altitudeMaxKm,
            float airTemperature,
            float relativeHumidity);
        Troposphere();
        ~Troposphere();
    private:
        UInt2   _gridDims;
        float   _altitudeMin, _altitudeMax;
        float   _relativeHumidity;
        float   _airTemperature;
    };

    float Troposphere::GetVaporMixingRatio(UInt2 gridCoord) const
    {
        auto potentialTemp = GetPotentialTemperature(gridCoord);
        auto altitudeKm = AltitudeKm(gridCoord[1]);            // simulating 2 km of atmosphere
        auto pressure = PressureAtAltitude(altitudeKm);     // (precalculate these pressures)
        auto exner = ExnerFunction(pressure);
        auto T = KelvinToCelsius(potentialTemp * exner);
        auto saturationPressure = 100.f * 6.1094f * XlExp((17.625f * T) / (T + 243.04f));
        const auto Rd = 287.058f;  // in J . kg^-1 . K^-1. Gas constant for dry air
        const auto Rv = 461.495f;  // in J . kg^-1 . K^-1. Gas constant for water vapor
        const auto gasConstantRatio = Rd/Rv;   // ~0.622f;
        auto equilibriumMixingRatio = 
            gasConstantRatio * (pressure / (pressure-saturationPressure) - 1.f);
        // RH = vaporMixingRatio/saturationMixingRatio
        return _relativeHumidity * equilibriumMixingRatio;
    }

    float Troposphere::GetPotentialTemperature(UInt2 gridCoord) const
    {
        const auto altitudeKm = AltitudeKm(gridCoord[1]);
        auto pressure = PressureAtAltitude(altitudeKm);     // (precalculate these pressures)
        auto exner = ExnerFunction(pressure);
        const float tempLapseRate = 6.5f;
        return (_airTemperature - tempLapseRate * altitudeKm) / exner;
    }

    float Troposphere::AltitudeKm(unsigned gridY) const
    {
        return LinearInterpolate(
            _altitudeMin, _altitudeMax, float(gridY)/float(_gridDims[1]));
    }

    float Troposphere::ZScale() const
    {
        return (_altitudeMax-_altitudeMin) / float(_gridDims[1]);
    }

    Troposphere::Troposphere(
        UInt2 gridDims,
        float altitudeMin, float altitudeMax,
        float airTemperature,
        float relativeHumidity) 
    {
        _gridDims = gridDims;
        _altitudeMin = altitudeMin;
        _altitudeMax = altitudeMax;
        _airTemperature = airTemperature;
        _relativeHumidity = relativeHumidity;
    }

    Troposphere::Troposphere() {}
    Troposphere::~Troposphere() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class CloudsForm2D::Pimpl
    {
    public:
        VectorX _velU[3];
        VectorX _velV[3];

        VectorX _vaporMixingRatio[2];      // qv
        VectorX _condensedMixingRatio[2];   // qc
        VectorX _potentialTemperature[2];   // theta

        UInt2 _dimsWithoutBorder;
        UInt2 _dimsWithBorder;
        unsigned _N;

        PoissonSolver _poissonSolver;

        DiffusionHelper _velocityDiffusion;
        DiffusionHelper _vaporDiffusion;
        DiffusionHelper _condensedDiffusion;
        DiffusionHelper _temperatureDiffusion;
        EnforceIncompressibilityHelper _incompressibility;

        Troposphere _troposphere;
    };

    void CloudsForm2D::Tick(float deltaTime, const Settings& settings)
    {
        float dt = deltaTime;
        const auto N = _pimpl->_N;
        const auto dims = _pimpl->_dimsWithBorder;

        auto& velUT0 = _pimpl->_velU[0];
        auto& velUT1 = _pimpl->_velU[1];
        auto& velUSrc = _pimpl->_velU[2];
        auto& velUWorking = _pimpl->_velU[2];

        auto& velVT0 = _pimpl->_velV[0];
        auto& velVT1 = _pimpl->_velV[1];
        auto& velVSrc = _pimpl->_velV[2];
        auto& velVWorking = _pimpl->_velV[2];

        auto& potTempT1 = _pimpl->_potentialTemperature[1];
        auto& potTempWorking = _pimpl->_potentialTemperature[0];
        auto& qvT1 = _pimpl->_vaporMixingRatio[1];
        auto& qvSrc = _pimpl->_vaporMixingRatio[0];
        auto& qvWorking = _pimpl->_vaporMixingRatio[0];
        auto& qcT1 = _pimpl->_condensedMixingRatio[1];
        auto& qcWorking = _pimpl->_condensedMixingRatio[0];

            //
            // Following Mark Jason Harris' PhD dissertation,
            // we will simulate condensation and buoyancy of
            // water vapour in the atmosphere.  
            //
            // We use a lot of "mixing ratios" here. The mixing ratio
            // is the ratio of a component in a mixture, relative to all
            // other components. see:
            //  https://en.wikipedia.org/wiki/Mixing_ratio
            //
            // So, if a single component makes up the entirity of a mixture,
            // then the mixing ratio convergences on infinity.
            //

            // Vorticity confinement (additional) force
        VorticityConfinement(
            VectorField2D(&velUSrc, &velVSrc, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),           // last frame results
            settings._vorticityConfinement, deltaTime);

            // The strength of buoyancy is proportional to the reciprocal of
            // the referenceVirtualPotentialTemperature. So we can just the 
            // amount of bouyancy by changing this number.
        const auto zScale = _pimpl->_troposphere.ZScale();
        const auto g = 9.81f / 1000.f / zScale;  // (in km/second then div by zScale)

            // Buoyancy force
        const UInt2 border(1,1);
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                
                    //
                    // As described by Harris, we will ignore the effect of local
                    // pressure changes, and use his equation (2.10)
                    //
                const auto i = y*dims[0]+x;
                auto potentialTemp = potTempT1[i];
                auto vapourMixingRatio = qvT1[i];
                auto condensationMixingRatio = qcT1[i];

                    //
                    // In atmosphere thermodynamics, the "virtual temperature" of a parcel of air
                    // is a concept that allows us to simplify some equations. Given a "moist" packet
                    // of air -- that is, a packet with some water vapor -- it should behave the same
                    // as a dry packet of air at some temperature.
                    // That is what the virtual temperature is -- the temperature of a dry packet of air
                    // that would behave the same the given moist packet.
                    //
                    // It seems that the virtual temperature, for realistic vapour mixing ratios,
                    // is close to linear against the vapour mixing ratio. So we can
                    // use a simple equation to find it.
                    //
                    //  see also -- https://en.wikipedia.org/wiki/Virtual_temperature
                    //
                    // The "potential temperature" of a parcel is proportional to the "temperature"
                    // of that parcel.
                    // and, since
                    //  virtual temperature ~= T . (1 + 0.61qv)
                    // we can apply the same equation to the potential temperature.
                    //
                auto virtualPotentialTemp = 
                    potentialTemp * (1.f + 0.61f * vapourMixingRatio);

                auto altitudeKm = _pimpl->_troposphere.AltitudeKm(y);
                auto pressure = PressureAtAltitude(altitudeKm);     // (precalculate these pressures)
                auto exner = ExnerFunction(pressure);
                const auto referenceVirtualPotentialTemperature = 295.f / exner;

                    //
                    // As per Harris, we use the condenstation mixing ratio for the "hydrometeors" mixing
                    // ratio here. Note that this final equation is similar to our simple smoke buoyancy
                    //  --  the force up is linear against temperatures and density of particles 
                    //      in the air
                    // We need to scale the velocity according the scaling system of our grid. In the CFD
                    // system, coordinates are in grid units. 
                    // 
                // auto B = 
                //     g * (virtualPotentialTemp / referenceVirtualPotentialTemperature - condensationMixingRatio);
                // B /= zScale;
                // 
                //     // B is now our buoyant force per unit mass -- so it is just our acceleration.
                // velVSrc[i] -= B;
                (void)virtualPotentialTemp; (void)referenceVirtualPotentialTemperature; (void)condensationMixingRatio;

                const auto temp = potentialTemp * exner;
                const auto ambientTemperature = _pimpl->_troposphere.GetPotentialTemperature(UInt2(x, y)) * exner;

                // VT = T * (1 + 0.61f * qv)
                // TB = (VT - AT) / AT
                // TB = VT / AT - 1
                // TB = T/AT - 1 + 0.61f * T / AT * qv
                // auto virtualTemperature = temp * (1.f + 0.61f * vapourMixingRatio);
                // auto temperatureBuoyancy = (virtualTemperature - ambientTemperature) / ambientTemperature;
                auto temperatureBuoyancy = (temp/ambientTemperature) * (1.f + 0.61f * vapourMixingRatio) - 1.f;
                auto B = g * (temperatureBuoyancy * settings._buoyancyAlpha - condensationMixingRatio * settings._buoyancyBeta);
                velVSrc[i] += B / zScale;
            }

        for (unsigned c=0; c<N; ++c) {
            velUT0[c] = velUT1[c];
            velVT0[c] = velVT1[c];
            velUWorking[c] = velUT1[c] + dt * velUSrc[c];
            velVWorking[c] = velVT1[c] + dt * velVSrc[c];

            qcWorking[c] = qcT1[c]; //  + dt * qcSrc[c];
            qvWorking[c] = qvT1[c] + dt * qvSrc[c];
            potTempWorking[c] = potTempT1[c]; // + dt * temperatureSrc[c];
        }

        _pimpl->_velocityDiffusion.Execute(
            _pimpl->_poissonSolver,
            VectorField2D(&velUWorking, &velVWorking, _pimpl->_dimsWithBorder),
            settings._viscosity, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Velocity");

        AdvectionSettings advSettings {
            (AdvectionMethod)settings._advectionMethod, 
            (AdvectionInterp)settings._interpolationMethod, settings._advectionSteps,
            AdvectionBorder::Margin, AdvectionBorder::Margin, AdvectionBorder::Margin
        };
        PerformAdvection(
            VectorField2D(&velUT1,      &velVT1,        _pimpl->_dimsWithBorder),
            VectorField2D(&velUWorking, &velVWorking,   _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0,      &velVT0,        _pimpl->_dimsWithBorder),
            VectorField2D(&velUWorking, &velVWorking,   _pimpl->_dimsWithBorder),
            deltaTime, advSettings);
        
        ReflectUBorder2D(velUT1, _pimpl->_dimsWithBorder, ~0u);
        ReflectVBorder2D(velVT1, _pimpl->_dimsWithBorder, ~0u);
        _pimpl->_incompressibility.Execute(
            _pimpl->_poissonSolver,
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            (PoissonSolver::Method)settings._enforceIncompressibilityMethod,
            ~0u, false);

            // note -- advection of all 3 of these properties should be very
            // similar, since the velocity field doesn't change. Rather that 
            // performing the advection multiple times, we could just do it
            // once and reuse the result for each.
            // Actually, we could even use the same advection result we got
            // while advecting velocity -- it's unclear how that would change
            // the result (especially since that advection happens before we
            // enforce incompressibility).
        _pimpl->_condensedDiffusion.Execute(
            _pimpl->_poissonSolver,
            ScalarField2D(&qcWorking, _pimpl->_dimsWithBorder),
            settings._condensedDiffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Condensed");
        PerformAdvection(
            ScalarField2D(&qcT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&qcWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

        _pimpl->_vaporDiffusion.Execute(
            _pimpl->_poissonSolver,
            ScalarField2D(&qvWorking, _pimpl->_dimsWithBorder),
            settings._vaporDiffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Vapor");
        PerformAdvection(
            ScalarField2D(&qvT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&qvWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

        _pimpl->_temperatureDiffusion.Execute(
            _pimpl->_poissonSolver,
            ScalarField2D(&potTempWorking, _pimpl->_dimsWithBorder),
            settings._temperatureDiffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Temperature");
        PerformAdvection(
            ScalarField2D(&potTempT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&potTempWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

            // Perform condenstation after advection
            // Does it matter much if we do this before or after advection?
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {

                    // When the water vapour mixing ratio exceeds a certain point, condensation
                    // may occur. This point is called the saturation point. It depends on
                    // the temperature of the parcel.

                const auto i = y*dims[0]+x;
                auto& potentialTemp = potTempT1[i];
                auto& vapourMixingRatio = qvT1[i];
                auto& condensationMixingRatio = qcT1[i];

                    // There are a lot of constants here... Maybe there is a better
                    // way to express this equation?

                auto altitudeKm = _pimpl->_troposphere.AltitudeKm(y);
                auto pressure = PressureAtAltitude(altitudeKm);     // (precalculate these pressures)
                auto exner = ExnerFunction(pressure);
                auto T = KelvinToCelsius(potentialTemp * exner);

                    // We can calculate the partial pressure for water vapor at saturation point.
                    // Note that this pressure value is much smaller than the pressure value
                    // calculated from PressureAtAltitude because it is a "partial" pressure.
                    // It is the pressure for the vapor part only.
                    //
                    // (Here the pressure is calculated assuming all air is dry air, 
                    // but it's not clear if that has a big impact). At this saturation
                    // point, condensation can start to occur. And condensation is what we're
                    // interested in!
                    //
                    // It would be useful if we could find a simplier relationship between
                    // this value and potentialTemp -- by precalculating any terms that are
                    // only dependent on the pressure (which is constant with altitude).
                    //
                    // See https://en.wikipedia.org/wiki/Clausius%E2%80%93Clapeyron_relation
                    // This is called the August-Roche-Magnus formula.
                    //
                    // Note that this is very slightly different from the Harris' paper.
                    // I'm using the equation from wikipedia, which gives a result in hectopascals.
                    //
                    // See also http://www.vaisala.com/Vaisala%20Documents/Application%20notes/Humidity_Conversion_Formulas_B210973EN-F.pdf
                    // ("HUMIDITY CONVERSION FORMULAS") for an alternative formula.
                    //
                    //      (in pascals)
                auto saturationPressure = 100.f * 6.1094f * XlExp((17.625f * T) / (T + 243.04f));

                    // We can use this to calculate the equilibrium point for the vapour mixing
                    // ratio. 
                    // However -- this might be a little inaccurate because it relies on our
                    // calculation of the pressure from PressureAtAltitude. And that doesn't
                    // take into account the humidity (which might be changing as a result of
                    // the condensating occuring?)
                    
                    //      The following is derived from
                    //      ws  = es / (p - es) . Rd/Rv
                    //          = (-p / (es-p) - 1) . Rd/Rv
                    //          = c.(p / (p-es) - 1), where c = Rd/Rv ~= 0.622
                    //      (see http://www.geog.ucsb.edu/~joel/g266_s10/lecture_notes/chapt03/oh10_3_01/oh10_3_01.html)
                    //      It seems that a common approximation is just to ignore the es in
                    //      the denominator of the first equation (since it should be small compared to total
                    //      pressure). But that seems like a minor optimisation? Why not use the full equation?

                const auto Rd = 287.058f;  // in J . kg^-1 . K^-1. Gas constant for dry air
                const auto Rv = 461.495f;  // in J . kg^-1 . K^-1. Gas constant for water vapor
                const auto gasConstantRatio = Rd/Rv;   // ~0.622f;
                float equilibriumMixingRatio = 
                    gasConstantRatio * (pressure / (pressure-saturationPressure) - 1.f);

                    // Once we know our mixing ratio is above the equilibrium point -- how quickly should we
                    // get condensation? Delta time should be a factor here, but the integration isn't very
                    // accurate.
                    // Adjusting mixing ratios like this seems awkward. But I guess that the values tracked
                    // should generally be small relative to the total mixture (ie, ratios should be much
                    // smaller than 1.f). Otherwise changing one ratio effectively changes the meaning of
                    // the other (given that they are ratios against all other substances in the mixture).
                    // But, then again, in this simple model the condensationMixingRatio doesn't effect the 
                    // equilibriumMixingRatio equation. Only the change in temperature (which is adjusted 
                    // here) effects the equilibriumMixingRatio.

                auto difference = vapourMixingRatio - equilibriumMixingRatio;
                auto deltaCondensation = std::min(1.f, deltaTime * settings._condensationSpeed) * difference;
                deltaCondensation = std::max(deltaCondensation, -condensationMixingRatio);
                vapourMixingRatio -= deltaCondensation;
                condensationMixingRatio += deltaCondensation;

                    // Delta condensation should effect the temperature, as well
                    // When water vapour condenses, it releases its latent heat.
                    // Note that the change in temperature will change the equilibrium
                    // mixing ratio (for the next update).

                    // Note -- "latentHeatOfVaporization" value comes from Harris' paper.
                    //          But we should check this. It appears that the units may be
                    //          out in that paper. It maybe should be 2.5 x 10^6, or thereabouts?

                const auto latentHeatOfVaporization = 2260.f * 1000.f;  // in J . kg^-1 for water at 0 degrees celsius
                const auto cp = 1005.f;                                 // in J . kg^-1 . K^-1. heat capacity of dry air at constant pressure
                auto deltaPotTemp = latentHeatOfVaporization / (cp * exner) * deltaCondensation;
                potentialTemp += deltaPotTemp * settings._temperatureChangeSpeed;
            }

        for (unsigned c=0; c<N; ++c) {
            velUSrc[c] = 0.f;
            velVSrc[c] = 0.f;
            qvSrc[c] = 0.f;
        }
    }

    void CloudsForm2D::OldTick(float deltaTime, const Settings& settings)
    {
        float dt = deltaTime;
        const auto N = _pimpl->_N;
        const auto dims = _pimpl->_dimsWithBorder;

        auto& velUT0 = _pimpl->_velU[0];
        auto& velUT1 = _pimpl->_velU[1];
        auto& velUSrc = _pimpl->_velU[2];
        auto& velUWorking = _pimpl->_velU[2];

        auto& velVT0 = _pimpl->_velV[0];
        auto& velVT1 = _pimpl->_velV[1];
        auto& velVSrc = _pimpl->_velV[2];
        auto& velVWorking = _pimpl->_velV[2];

        auto& potTempT1 = _pimpl->_potentialTemperature[1];
        auto& potTempWorking = _pimpl->_potentialTemperature[0];
        auto& qvT1 = _pimpl->_vaporMixingRatio[1];
        auto& qvSrc = _pimpl->_vaporMixingRatio[0];
        auto& qvWorking = _pimpl->_vaporMixingRatio[0];
        auto& qcT1 = _pimpl->_condensedMixingRatio[1];
        auto& qcWorking = _pimpl->_condensedMixingRatio[0];

            //
            // Following Mark Jason Harris' PhD dissertation,
            // we will simulate condensation and buoyancy of
            // water vapour in the atmosphere.  
            //
            // We use a lot of "mixing ratios" here. The mixing ratio
            // is the ratio of a component in a mixture, relative to all
            // other components. see:
            //  https://en.wikipedia.org/wiki/Mixing_ratio
            //
            // So, if a single component makes up the entirity of a mixture,
            // then the mixing ratio convergences on infinity.
            //

            // Vorticity confinement (additional) force
        VorticityConfinement(
            VectorField2D(&velUSrc, &velVSrc, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),           // last frame results
            settings._vorticityConfinement, deltaTime);

            // The strength of buoyancy is proportional to the reciprocal of
            // the referenceVirtualPotentialTemperature. So we can just the 
            // amount of bouyancy by changing this number.
        const auto g = 9.81f / 1000.f;  // (in km/second)
        const auto ambientTemperature = CelsiusToKelvin(23.f);

        const auto zScale = _pimpl->_troposphere.ZScale();

            // Buoyancy force
        const UInt2 border(1,1);
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                
                    //
                    // As described by Harris, we will ignore the effect of local
                    // pressure changes, and use his equation (2.10)
                    //
                const auto i = y*dims[0]+x;
                auto potentialTemp = potTempT1[i];
                auto vapourMixingRatio = qvT1[i];
                auto condensationMixingRatio = qcT1[i];

                    //
                    // In atmosphere thermodynamics, the "virtual temperature" of a parcel of air
                    // is a concept that allows us to simplify some equations. Given a "moist" packet
                    // of air -- that is, a packet with some water vapor -- it should behave the same
                    // as a dry packet of air at some temperature.
                    // That is what the virtual temperature is -- the temperature of a dry packet of air
                    // that would behave the same the given moist packet.
                    //
                    // It seems that the virtual temperature, for realistic vapour mixing ratios,
                    // is close to linear against the vapour mixing ratio. So we can
                    // use a simple equation to find it.
                    //
                    //  see also -- https://en.wikipedia.org/wiki/Virtual_temperature
                    //
                    // The "potential temperature" of a parcel is proportional to the "temperature"
                    // of that parcel.
                    // and, since
                    //  virtual temperature ~= T . (1 + 0.61qv)
                    // we can apply the same equation to the potential temperature.
                    //
                auto virtualPotentialTemp = 
                    potentialTemp * (1.f + 0.61f * vapourMixingRatio);

                auto altitudeKm = _pimpl->_troposphere.AltitudeKm(y);
                auto pressure = PressureAtAltitude(altitudeKm);     // (precalculate these pressures)
                auto exner = ExnerFunction(pressure);
                const auto referenceVirtualPotentialTemperature = 295.f / exner;

                    //
                    // As per Harris, we use the condenstation mixing ratio for the "hydrometeors" mixing
                    // ratio here. Note that this final equation is similar to our simple smoke buoyancy
                    //  --  the force up is linear against temperatures and density of particles 
                    //      in the air
                    // We need to scale the velocity according the scaling system of our grid. In the CFD
                    // system, coordinates are in grid units. 
                    // 
                // auto B = 
                //     g * (virtualPotentialTemp / referenceVirtualPotentialTemperature - condensationMixingRatio);
                // B /= zScale;
                // 
                //     // B is now our buoyant force per unit mass -- so it is just our acceleration.
                // velVSrc[i] -= B;
                (void)virtualPotentialTemp; (void)referenceVirtualPotentialTemperature; (void)condensationMixingRatio;

                const auto temp = potentialTemp * exner;
                auto virtualTemperature = temp * (1.f + 0.61f * vapourMixingRatio);
                auto temperatureBuoyancy = (virtualTemperature - ambientTemperature) / ambientTemperature;
                auto B = g * (temperatureBuoyancy * settings._buoyancyAlpha - condensationMixingRatio * settings._buoyancyBeta);
                velVSrc[i] += B / zScale;
            }

        for (unsigned c=0; c<N; ++c) {
            velUT0[c] = velUT1[c];
            velVT0[c] = velVT1[c];
            velUWorking[c] = velUT1[c] + dt * velUSrc[c];
            velVWorking[c] = velVT1[c] + dt * velVSrc[c];

            qcWorking[c] = qcT1[c]; //  + dt * qcSrc[c];
            qvWorking[c] = qvT1[c] + dt * qvSrc[c];
            potTempWorking[c] = potTempT1[c]; // + dt * temperatureSrc[c];
        }

        _pimpl->_velocityDiffusion.Execute(
            _pimpl->_poissonSolver,
            VectorField2D(&velUWorking, &velVWorking, _pimpl->_dimsWithBorder),
            settings._viscosity, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Velocity");

        AdvectionSettings advSettings {
            (AdvectionMethod)settings._advectionMethod, 
            (AdvectionInterp)settings._interpolationMethod, settings._advectionSteps,
            AdvectionBorder::Margin, AdvectionBorder::Margin, AdvectionBorder::Margin
        };
        PerformAdvection(
            VectorField2D(&velUT1,      &velVT1,        _pimpl->_dimsWithBorder),
            VectorField2D(&velUWorking, &velVWorking,   _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0,      &velVT0,        _pimpl->_dimsWithBorder),
            VectorField2D(&velUWorking, &velVWorking,   _pimpl->_dimsWithBorder),
            deltaTime, advSettings);
        
        ReflectUBorder2D(velUT1, _pimpl->_dimsWithBorder, ~0u);
        ReflectVBorder2D(velVT1, _pimpl->_dimsWithBorder, ~0u);
        _pimpl->_incompressibility.Execute(
            _pimpl->_poissonSolver,
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            (PoissonSolver::Method)settings._enforceIncompressibilityMethod,
            ~0u, false);

            // note -- advection of all 3 of these properties should be very
            // similar, since the velocity field doesn't change. Rather that 
            // performing the advection multiple times, we could just do it
            // once and reuse the result for each.
            // Actually, we could even use the same advection result we got
            // while advecting velocity -- it's unclear how that would change
            // the result (especially since that advection happens before we
            // enforce incompressibility).
        _pimpl->_condensedDiffusion.Execute(
            _pimpl->_poissonSolver,
            ScalarField2D(&qcWorking, _pimpl->_dimsWithBorder),
            settings._condensedDiffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Condensed");
        PerformAdvection(
            ScalarField2D(&qcT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&qcWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

        _pimpl->_vaporDiffusion.Execute(
            _pimpl->_poissonSolver,
            ScalarField2D(&qvWorking, _pimpl->_dimsWithBorder),
            settings._vaporDiffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Vapor");
        PerformAdvection(
            ScalarField2D(&qvT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&qvWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

        _pimpl->_temperatureDiffusion.Execute(
            _pimpl->_poissonSolver,
            ScalarField2D(&potTempWorking, _pimpl->_dimsWithBorder),
            settings._temperatureDiffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Temperature");
        PerformAdvection(
            ScalarField2D(&potTempT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&potTempWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

            // Perform condenstation after advection
            // Does it matter much if we do this before or after advection?
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {

                    // When the water vapour mixing ratio exceeds a certain point, condensation
                    // may occur. This point is called the saturation point. It depends on
                    // the temperature of the parcel.

                const auto i = y*dims[0]+x;
                auto& potentialTemp = potTempT1[i];
                auto& vapourMixingRatio = qvT1[i];
                auto& condensationMixingRatio = qcT1[i];

                    // There are a lot of constants here... Maybe there is a better
                    // way to express this equation?

                auto altitudeKm = _pimpl->_troposphere.AltitudeKm(y);
                auto pressure = PressureAtAltitude(altitudeKm);     // (precalculate these pressures)
                auto exner = ExnerFunction(pressure);
                auto T = KelvinToCelsius(potentialTemp * exner);

                    // We can calculate the partial pressure for water vapor at saturation point.
                    // Note that this pressure value is much smaller than the pressure value
                    // calculated from PressureAtAltitude because it is a "partial" pressure.
                    // It is the pressure for the vapor part only.
                    //
                    // (Here the pressure is calculated assuming all air is dry air, 
                    // but it's not clear if that has a big impact). At this saturation
                    // point, condensation can start to occur. And condensation is what we're
                    // interested in!
                    //
                    // It would be useful if we could find a simplier relationship between
                    // this value and potentialTemp -- by precalculating any terms that are
                    // only dependent on the pressure (which is constant with altitude).
                    //
                    // See https://en.wikipedia.org/wiki/Clausius%E2%80%93Clapeyron_relation
                    // This is called the August-Roche-Magnus formula.
                    //
                    // Note that this is very slightly different from the Harris' paper.
                    // I'm using the equation from wikipedia, which gives a result in hectopascals.
                    //
                    // See also http://www.vaisala.com/Vaisala%20Documents/Application%20notes/Humidity_Conversion_Formulas_B210973EN-F.pdf
                    // ("HUMIDITY CONVERSION FORMULAS") for an alternative formula.
                    //
                    //      (in pascals)
                auto saturationPressure = 100.f * 6.1094f * XlExp((17.625f * T) / (T + 243.04f));

                    // We can use this to calculate the equilibrium point for the vapour mixing
                    // ratio. 
                    // However -- this might be a little inaccurate because it relies on our
                    // calculation of the pressure from PressureAtAltitude. And that doesn't
                    // take into account the humidity (which might be changing as a result of
                    // the condensating occuring?)
                    
                    //      The following is derived from
                    //      ws  = es / (p - es) . Rd/Rv
                    //          = (-p / (es-p) - 1) . Rd/Rv
                    //          = c.(p / (p-es) - 1), where c = Rd/Rv ~= 0.622
                    //      (see http://www.geog.ucsb.edu/~joel/g266_s10/lecture_notes/chapt03/oh10_3_01/oh10_3_01.html)
                    //      It seems that a common approximation is just to ignore the es in
                    //      the denominator of the first equation (since it should be small compared to total
                    //      pressure). But that seems like a minor optimisation? Why not use the full equation?

                const auto Rd = 287.058f;  // in J . kg^-1 . K^-1. Gas constant for dry air
                const auto Rv = 461.495f;  // in J . kg^-1 . K^-1. Gas constant for water vapor
                const auto gasConstantRatio = Rd/Rv;   // ~0.622f;
                float equilibriumMixingRatio = 
                    gasConstantRatio * (pressure / (pressure-saturationPressure) - 1.f);

                    // Once we know our mixing ratio is above the equilibrium point -- how quickly should we
                    // get condensation? Delta time should be a factor here, but the integration isn't very
                    // accurate.
                    // Adjusting mixing ratios like this seems awkward. But I guess that the values tracked
                    // should generally be small relative to the total mixture (ie, ratios should be much
                    // smaller than 1.f). Otherwise changing one ratio effectively changes the meaning of
                    // the other (given that they are ratios against all other substances in the mixture).
                    // But, then again, in this simple model the condensationMixingRatio doesn't effect the 
                    // equilibriumMixingRatio equation. Only the change in temperature (which is adjusted 
                    // here) effects the equilibriumMixingRatio.

                auto difference = vapourMixingRatio - equilibriumMixingRatio;
                auto deltaCondensation = std::min(1.f, deltaTime * settings._condensationSpeed) * difference;
                deltaCondensation = std::max(deltaCondensation, 0.f); // -condensationMixingRatio);
                vapourMixingRatio -= deltaCondensation;
                condensationMixingRatio += deltaCondensation;

                    // Delta condensation should effect the temperature, as well
                    // When water vapour condenses, it releases its latent heat.
                    // Note that the change in temperature will change the equilibrium
                    // mixing ratio (for the next update).

                    // Note -- "latentHeatOfVaporization" value comes from Harris' paper.
                    //          But we should check this. It appears that the units may be
                    //          out in that paper. It maybe should be 2.5 x 10^6, or thereabouts?

                const auto latentHeatOfVaporization = 2260.f * 1000.f;  // in J . kg^-1 for water at 0 degrees celsius
                const auto cp = 1005.f;                                 // in J . kg^-1 . K^-1. heat capacity of dry air at constant pressure
                auto deltaPotTemp = latentHeatOfVaporization / (cp * exner) * deltaCondensation;
                potentialTemp += deltaPotTemp * settings._temperatureChangeSpeed;
            }

        for (unsigned c=0; c<N; ++c) {
            velUSrc[c] = 0.f;
            velVSrc[c] = 0.f;
            qvSrc[c] = 0.f;
        }
    }

    void CloudsForm2D::AddVapor(UInt2 coords, float amount)
    {
        if (coords[0] < _pimpl->_dimsWithoutBorder[0] && coords[1] < _pimpl->_dimsWithoutBorder[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * _pimpl->_dimsWithBorder[0];
            _pimpl->_vaporMixingRatio[0][i] += amount;
        }
    }

    void CloudsForm2D::AddVelocity(UInt2 coords, Float2 vel)
    {
        if (coords[0] < _pimpl->_dimsWithoutBorder[0] && coords[1] < _pimpl->_dimsWithoutBorder[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * _pimpl->_dimsWithBorder[0];
            _pimpl->_velU[2][i] += vel[0];
            _pimpl->_velV[2][i] += vel[1];
        }
    }

    void CloudsForm2D::RenderDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode)
    {
        switch (debuggingMode) {
        case FluidDebuggingMode::Density:
            RenderFluidDebugging2D(
                metalContext, parserContext, RenderFluidMode::Scalar,
                _pimpl->_dimsWithBorder, 
                { _pimpl->_condensedMixingRatio[1].data() });
            break;

        case FluidDebuggingMode::Velocity:
            RenderFluidDebugging2D(
                metalContext, parserContext, RenderFluidMode::Vector,
                _pimpl->_dimsWithBorder, 
                { _pimpl->_velU[1].data(), _pimpl->_velV[1].data() });
            break;

        case FluidDebuggingMode::Temperature:
            RenderFluidDebugging2D(
                metalContext, parserContext, RenderFluidMode::Scalar,
                _pimpl->_dimsWithBorder, 
                { _pimpl->_potentialTemperature[1].data() });
            break;
        }
    }

    UInt2 CloudsForm2D::GetDimensions() const { return _pimpl->_dimsWithBorder; }

    CloudsForm2D::CloudsForm2D(UInt2 dimensions)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_dimsWithoutBorder = dimensions;
        _pimpl->_dimsWithBorder = dimensions + UInt2(2, 2);
        auto N = _pimpl->_dimsWithBorder[0] * _pimpl->_dimsWithBorder[1];
        _pimpl->_N = N;

        for (unsigned c=0; c<dimof(_pimpl->_velU); ++c) {
            _pimpl->_velU[c] = VectorX(N);
            _pimpl->_velV[c] = VectorX(N);
            _pimpl->_velU[c].fill(0.f);
            _pimpl->_velV[c].fill(0.f);
        }

        const float airTemp = CelsiusToKelvin(23.f);
        const auto relativeHumidity = .75f; // 75%
        const float altitudeMinKm = 0.5f;
        const float altitudeMaxKm = 1.5f;
        _pimpl->_troposphere = Troposphere(_pimpl->_dimsWithBorder, altitudeMinKm, altitudeMaxKm, airTemp, relativeHumidity);
        
        for (unsigned c=0; c<dimof(_pimpl->_vaporMixingRatio); ++c) {

                // It should be ok to start with constant (or near constant) for potential temperature
                // this means that everything is adiabatic -- and that the temperature varies
                // with altitude in a standard manner.
                // Note that it might be better to start with a noise field -- just so we
                // get some initial randomness into the simulation.
            _pimpl->_potentialTemperature[c] = VectorX(N);
            _pimpl->_condensedMixingRatio[c] = VectorX(N);
            _pimpl->_vaporMixingRatio[c] = VectorX(N);

                // we should start with some vapor in the atmosphere. The
                // amount depends on the humidity of the atmosphere.
                // The "specific humidity" is the ratio of the vapor mass to 
                // total mass -- and this is approximate equal to the vapor
                // mixing ratio (because the ratio is very small).
                // So, we can use formulas for specific humidity to calculate a
                // starting point.

            const auto& dims = _pimpl->_dimsWithBorder;
            for (unsigned y=0; y<dims[1]; ++y)
                for (unsigned x=0; x<dims[0]; ++x) {
                    auto i = x*dims[0]+y;
                    _pimpl->_potentialTemperature[c][i] = _pimpl->_troposphere.GetPotentialTemperature(UInt2(x, y));
                    _pimpl->_vaporMixingRatio[c][i] = _pimpl->_troposphere.GetVaporMixingRatio(UInt2(x, y));

                        // Starting with zero condensation... But we could initialise
                        // with with a noise field; just to get started.
                    _pimpl->_condensedMixingRatio[c][i] = 0.f;
                }

        }

        _pimpl->_poissonSolver = PoissonSolver(2, &_pimpl->_dimsWithBorder[0]);
    }

    CloudsForm2D::~CloudsForm2D(){}

    CloudsForm2D::Settings::Settings()
    {
        _viscosity = 0.05f;
        _condensedDiffusionRate = 0.f;
        _vaporDiffusionRate = 0.f;
        _temperatureDiffusionRate = 2.f;
        _diffusionMethod = 0;
        _advectionMethod = 3;
        _advectionSteps = 4;
        _enforceIncompressibilityMethod = 0;
        _vorticityConfinement = 0.75f;
        _interpolationMethod = 0;
        _buoyancyAlpha = 1.f;
        _buoyancyBeta = 1.f;
        _condensationSpeed = 60.f;
        _temperatureChangeSpeed = .25f;
    }
}


template<> const ClassAccessors& GetAccessors<SceneEngine::CloudsForm2D::Settings>()
{
    using Obj = SceneEngine::CloudsForm2D::Settings;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add(u("Viscosity"), DefaultGet(Obj, _viscosity),  DefaultSet(Obj, _viscosity));
        props.Add(u("CondensedDiffusionRate"), DefaultGet(Obj, _condensedDiffusionRate),  DefaultSet(Obj, _condensedDiffusionRate));
        props.Add(u("VaporDiffusionRate"), DefaultGet(Obj, _vaporDiffusionRate),  DefaultSet(Obj, _vaporDiffusionRate));
        props.Add(u("TemperatureDiffusionRate"), DefaultGet(Obj, _temperatureDiffusionRate),  DefaultSet(Obj, _temperatureDiffusionRate));
        props.Add(u("DiffusionMethod"), DefaultGet(Obj, _diffusionMethod),  DefaultSet(Obj, _diffusionMethod));

        props.Add(u("AdvectionMethod"), DefaultGet(Obj, _advectionMethod),  DefaultSet(Obj, _advectionMethod));
        props.Add(u("AdvectionSteps"), DefaultGet(Obj, _advectionSteps),  DefaultSet(Obj, _advectionSteps));
        props.Add(u("InterpolationMethod"), DefaultGet(Obj, _interpolationMethod),  DefaultSet(Obj, _interpolationMethod));

        props.Add(u("EnforceIncompressibility"), DefaultGet(Obj, _enforceIncompressibilityMethod),  DefaultSet(Obj, _enforceIncompressibilityMethod));
        props.Add(u("VorticityConfinement"), DefaultGet(Obj, _vorticityConfinement),  DefaultSet(Obj, _vorticityConfinement));
        props.Add(u("BuoyancyAlpha"), DefaultGet(Obj, _buoyancyAlpha),  DefaultSet(Obj, _buoyancyAlpha));
        props.Add(u("BuoyancyBeta"), DefaultGet(Obj, _buoyancyBeta),  DefaultSet(Obj, _buoyancyBeta));

        props.Add(u("CondensationSpeed"), DefaultGet(Obj, _condensationSpeed),  DefaultSet(Obj, _condensationSpeed));
        props.Add(u("TemperatureChangeSpeed"), DefaultGet(Obj, _temperatureChangeSpeed),  DefaultSet(Obj, _temperatureChangeSpeed));
        
        init = true;
    }
    return props;
}

