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


    class PoissonSolver
    {
    public:
        enum Method
        {
            PreconCG, PlainCG, 
            ForwardEuler, SOR, 
            Multigrid
        };

        class PreparedMatrix;
        
            // Solve for x in A * x = b
            // Returns the number of iterations performed during solving
        unsigned Solve(
            ScalarField1D x, 
            const PreparedMatrix& A, 
            const ScalarField1D& b, Method method);
        
        std::shared_ptr<PreparedMatrix> PrepareDiffusionMatrix(
            float centralWeight, float adjWeight, Method method);

        std::shared_ptr<PreparedMatrix> PrepareDivergenceMatrix(Method method);

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

