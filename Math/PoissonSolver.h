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
            CG_Precon, PlainCG, 
            ForwardEuler, SOR, 
            Multigrid
        };

        struct AMat2D
        {
            unsigned _wh;
            float _a0;
            float _a1;
        };
        
            // Solve for x in A * x = b
            // Returns the number of iterations performed during solving
        unsigned Solve(ScalarField1D x, const ScalarField1D& b, Method method);
        void SetA(AMat2D A);

        PoissonSolver(AMat2D A);
        PoissonSolver(PoissonSolver&& moveFrom);
        PoissonSolver& operator=(PoissonSolver&& moveFrom);
        PoissonSolver();
        ~PoissonSolver();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

