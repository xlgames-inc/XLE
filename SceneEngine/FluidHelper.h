// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Math/RegularNumberField.h"
#include "../Math/PoissonSolver.h"
#include "../Math/Vector.h"
#include <memory>

#pragma warning(disable:4714)
#pragma push_macro("new")
#undef new
#include <Eigen/Dense>
#pragma pop_macro("new")

namespace SceneEngine
{
    using VectorX = Eigen::VectorXf;
    using MatrixX = Eigen::MatrixXf;
    using VectorField2D = XLEMath::VectorField2DSeparate<VectorX>;
    using VectorField3D = XLEMath::VectorField3DSeparate<VectorX>;
    using ScalarField2D = XLEMath::ScalarField2D<VectorX>;
    using ScalarField3D = XLEMath::ScalarField3D<VectorX>;

    inline ScalarField1D AsScalarField1D(VectorX& v) { return ScalarField1D { v.data(), (unsigned)v.size() }; }

    class DiffusionHelper
    {
    public:
        void Execute(
            PoissonSolver& solver, VectorField2D vectorField,
            float diffusionAmount, float deltaTime,
            PoissonSolver::Method method = PoissonSolver::Method::PreconCG, unsigned wrapEdges = 0u,
            const char name[] = nullptr);

        void Execute(
            PoissonSolver& solver, ScalarField2D field,
            float diffusionAmount, float deltaTime,
            PoissonSolver::Method method = PoissonSolver::Method::PreconCG,  unsigned wrapEdges = 0u,
            const char name[] = nullptr);

        DiffusionHelper();
        ~DiffusionHelper();
    private:
        float       _preparedValue;
        unsigned    _preparedWrapEdges;
        PoissonSolver::Method _preparedMethod;
        std::shared_ptr<PoissonSolver::PreparedMatrix> _matrix;
    };

    class EnforceIncompressibilityHelper
    {
    public:
        void Execute(
            PoissonSolver& solver, VectorField2D vectorField,
            PoissonSolver::Method method = PoissonSolver::Method::PreconCG, unsigned wrapEdges = 0u);
        const float* GetDivergence();

        EnforceIncompressibilityHelper();
        ~EnforceIncompressibilityHelper();
    protected:
        std::shared_ptr<PoissonSolver::PreparedMatrix> _incompressibility;

        unsigned    _preparedWrapEdges;
        PoissonSolver::Method _preparedMethod;
        unsigned    _preparedN;
        VectorX     _buffers[2];
    };

    void VorticityConfinement(
        VectorField2D outputField,
        VectorField2D inputVelocities, float strength, float deltaTime);

    void EnforceIncompressibility(
        VectorField2D velField,
        ScalarField1D qBuffer, ScalarField1D delwBuffer,
        const PoissonSolver& solver, const PoissonSolver::PreparedMatrix& A,
        PoissonSolver::Method method, unsigned marginFlags, bool wrapEdges);
        
    void EnforceIncompressibility(
        VectorField3D velField,
        const PoissonSolver& solver, const PoissonSolver::PreparedMatrix& A,
        PoissonSolver::Method method);

    class LightingParserContext;
    enum RenderFluidMode { Scalar, Vector };
    void RenderFluidDebugging2D(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        RenderFluidMode debuggingMode,
        UInt2 dimensions, float minValue, float maxValue,
        std::initializer_list<const float*> data);

    void RenderFluidDebugging3D(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        RenderFluidMode debuggingMode,
        UInt3 dimensions, float minValue, float maxValue,
        std::initializer_list<const float*> data);

}

