// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>
#include <assert.h>

namespace XLEMath
{
    struct ScalarField1D
    {
        float* _u;
        unsigned _count;

        float& operator[](unsigned index)
        {
            assert(index < _count);
            return _u[index];
        }

        const float& operator[](unsigned index) const
        {
            assert(index < _count);
            return _u[index];
        }
    };

    /// <summary>Solves "Poisson equations", such as the heat equation</summary>
    /// A Poisson equation is a partial differential equations that involves the
    /// Laplace operator (del). The Laplace operator measures the "divergence" of
    /// a field of numbers -- that is, the derivatives with respect to the cardinal
    /// axes (eg, X, Y, Z).
    ///
    /// A Poisson equation imposes some restriction on the divergence of a number
    /// field, and we most solve to mean this restriction.
    ///
    /// This is useful for many simulations and problems. For example, the "heat equation"
    /// is a Poisson equation -- this simulates how heat (or a electro magnetic field, or
    /// other fields) diffuse in space.
    ///
    /// We can't solve Poisson equations perfectly -- we must use an estimate. There are 
    /// many different methods that can produce estimates; all with different metrics 
    /// for performance, accuracy and hardware suitability. Sometimes the best method to
    /// use in one case is not ideal in another.
    ///
    /// This class aims to encapsulate the implementation details and math involved in 
    /// calculating the solution -- and provide a simple reusable interface.
    class PoissonSolver
    {
    public:
        enum Method
        {
            PreconCG, PlainCG, 
            ForwardEuler, SOR, 
            Multigrid
        };

        struct Flags
        {
            enum Enum { XContainsEstimate = 1<<0 };
            using BitField = unsigned;
        };

        class PreparedMatrix;
        
            // Solve for x in A * x = b
            // Returns the number of iterations performed during solving
        unsigned Solve(
            ScalarField1D x, const PreparedMatrix& A, const ScalarField1D& b, 
            Method method, Flags::BitField flags = 0u) const;
        
        std::shared_ptr<PreparedMatrix> PrepareDiffusionMatrix(
            float diffusionAmount, Method method, unsigned marginFlags, bool wrapEdges) const;
        std::shared_ptr<PreparedMatrix> PrepareDivergenceMatrix(
            Method method, unsigned marginFlags, bool wrapEdges) const;

        PoissonSolver(unsigned dimensionality, unsigned dimensions[]);
        PoissonSolver(PoissonSolver&& moveFrom);
        PoissonSolver& operator=(PoissonSolver&& moveFrom);
        PoissonSolver();
        ~PoissonSolver();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

