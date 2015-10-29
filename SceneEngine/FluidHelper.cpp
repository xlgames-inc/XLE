// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FluidHelper.h"
#include "../ConsoleRig/Log.h"

namespace SceneEngine
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::shared_ptr<PoissonSolver::PreparedMatrix> BuildDiffusionMethod(
        const PoissonSolver& solver, float diffusion, PoissonSolver::Method method,
        unsigned wrapEdges)
    {
        return solver.PrepareDiffusionMatrix(diffusion, method, wrapEdges);
    }

    void DiffusionHelper::Execute(
        PoissonSolver& solver, VectorField2D vectorField,
        float diffusionAmount, float deltaTime,
        PoissonSolver::Method method, unsigned wrapEdges,
        const char name[])
    {
        if (!diffusionAmount || !deltaTime) return;

        if (    !_matrix || _preparedValue != deltaTime * diffusionAmount || method != _preparedMethod 
                || wrapEdges != _preparedWrapEdges) {

            _preparedValue = deltaTime * diffusionAmount;
            _preparedMethod = method;
            _preparedWrapEdges = wrapEdges;
            _matrix = BuildDiffusionMethod(solver, _preparedValue, _preparedMethod, _preparedWrapEdges);
        }

        auto iterationsu = solver.Solve(
            AsScalarField1D(*vectorField._u), *_matrix, AsScalarField1D(*vectorField._u), 
            method);
        auto iterationsv = solver.Solve(
            AsScalarField1D(*vectorField._v), *_matrix, AsScalarField1D(*vectorField._v), 
            method);

        if (name)
            LogInfo << name << " diffusion took: (" << iterationsu << ", " << iterationsv << ") iterations.";
    }

    void DiffusionHelper::Execute(
        PoissonSolver& solver, ScalarField2D field,
        float diffusionAmount, float deltaTime,
        PoissonSolver::Method method, unsigned wrapEdges, const char name[])
    {
        if (!diffusionAmount || !deltaTime) return;

        if (!_matrix || _preparedValue != deltaTime * diffusionAmount || method != _preparedMethod) {
            _preparedValue = deltaTime * diffusionAmount;
            _preparedMethod = method;
            _preparedWrapEdges = wrapEdges;
            _matrix = BuildDiffusionMethod(solver, _preparedValue, _preparedMethod, _preparedWrapEdges);
        }

        auto iterationsu = solver.Solve(
            AsScalarField1D(*field._u), *_matrix, AsScalarField1D(*field._u), 
            method);

        if (name)
            LogInfo << name << " diffusion took: (" << iterationsu << ") iterations.";
    }

    DiffusionHelper::DiffusionHelper() { _preparedValue = 0.f; _preparedMethod = (PoissonSolver::Method)~0u; _preparedWrapEdges = 0u; }
    DiffusionHelper::~DiffusionHelper() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void EnforceIncompressibility(
        VectorField2D velField,
        ScalarField1D qBuffer, ScalarField1D delwBuffer,
        const PoissonSolver& solver, const PoissonSolver::PreparedMatrix& A,
        PoissonSolver::Method method, unsigned wrapEdges)
    {
        //
        // Following Jos Stam's stable fluids, we'll use Helmholtz-Hodge Decomposition
        // to build a projection operator that will force the velocity field to have
        // zero divergence.
        //
        // This is important for meeting the restrictions from the Naver Stokes equations.
        // 
        // For our input vector field, "w", we can decompose it into two parts:
        //      w = u + del . q        (1)
        // where "u" has zero-divergence (and is our output field). The scalar field,
        // "q" is considered error, and just dropped.
        //
        // We find "q" by multiplying equation (1) by del on both sides, and we get:
        //      del . w = del^2 . q    (2)     (given that del . u is zero)
        //
        // and "u" = w - del . q
        //
        // "b = del^2 . q" is the possion equation; and can be solved in the same way
        // we solve for diffusion (though, the matrix "A" is slightly different). 
        //
        // Following Stam's sample code, we'll do this for both u and v at the same time,
        // with the same solution for "q".
        //
        // Also, here to have to consider how we define the discrete divergence of the
        // field. Stam uses .5f * (f[i+1] - f[i-1]). Depending on how we arrange the physical
        // values with respect to the grid, we could alternatively consider f[i] - f[i-1].
        //

        //
        //  Note -- there may be some benefit in doing this step with a multigrid-like approach
        //          that is, we could solve for incompressibility on a resamples low resolution
        //          grid first; to remove low frequency divergence first... And then successively
        //          use higher resolution grids to remove higher frequency divergence. It might
        //          improve large scale details a bit.
        //

        const auto dims = velField.Dimensions();
        UInt2 border(1,1);
        if (wrapEdges & 1<<0) border[0] = 0u;
        if (wrapEdges & 1<<1) border[1] = 0u;

            // note that the "velFieldScale" shouldn't really change the result here
            //      -- but maybe it can help avoid floating point creep?
        auto velFieldScale = Float2(1,1); // Float2(float(dims[0]-2*border[0]), float(dims[1]-2*border[1]));

            // note -- default to wrapping on borders without a margin
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                const auto i  = y*dims[0]+x;
                const auto i0 = y*dims[0]+((x+1)%dims[0]);
                const auto i1 = y*dims[0]+((x+dims[0]-1)%dims[0]);
                const auto i2 = ((y+1)%dims[1])*dims[0]+x;
                const auto i3 = ((y+dims[1]-1)%dims[1])*dims[0]+x;
                delwBuffer._u[i] = 
                    -0.5f * 
                    (
                          ((*velField._u)[i0] - (*velField._u)[i1]) / velFieldScale[0]
                        + ((*velField._v)[i2] - (*velField._v)[i3]) / velFieldScale[1]
                    );
            }
        SmearBorder2D(delwBuffer._u, dims, ~wrapEdges);

            // We're going to use the 'q' from the last frame as a 
            // starting estimate for this frame. The divergence shouldn't
            // change quickly, so this might be a quite accurate estimate.
        auto iterations = solver.Solve(
            qBuffer, A, delwBuffer, 
            method, PoissonSolver::Flags::XContainsEstimate);

        // SmearBorder2D(qBuffer, dims, marginFlags);    // note -- perhaps this will polute the starting estimate for the next frame?
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                const auto i  = y*dims[0]+x;
                const auto i0 = y*dims[0]+((x+1)%dims[0]);
                const auto i1 = y*dims[0]+((x+dims[0]-1)%dims[0]);
                const auto i2 = ((y+1)%dims[1])*dims[0]+x;
                const auto i3 = ((y+dims[1]-1)%dims[1])*dims[0]+x;
                (*velField._u)[i] -= .5f*velFieldScale[0] * (qBuffer._u[i0] - qBuffer._u[i1]);
                (*velField._v)[i] -= .5f*velFieldScale[1] * (qBuffer._u[i2] - qBuffer._u[i3]);
            }

        LogInfo << "EnforceIncompressibility took: " << iterations << " iterations.";
    }

    void EnforceIncompressibility(
        VectorField3D velField,
        const PoissonSolver& solver, const PoissonSolver::PreparedMatrix& A,
        PoissonSolver::Method method)
    {
        const auto dims = velField.Dimensions();
        VectorX delW(dims[0] * dims[1] * dims[2]), q(dims[0] * dims[1] * dims[2]);
        q.fill(0.f);    // when using the "SOR" method, q must be filled in to some initial estimate
        const UInt3 border(1,1,1);
        auto velFieldScale = Float3(float(dims[0]-2*border[0]), float(dims[1]-2*border[1]), float(dims[2]-2*border[2]));
        for (unsigned z=border[2]; z<dims[2]-border[2]; ++z)
            for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
                for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                    const auto i = (z*dims[1]+y)*dims[0]+x;
                    delW[i] = 
                        -0.5f * 
                        (
                              ((*velField._u)[i+1]               - (*velField._u)[i-1]) / velFieldScale[0]
                            + ((*velField._v)[i+dims[0]]         - (*velField._v)[i-dims[0]]) / velFieldScale[1]
                            + ((*velField._w)[i+dims[0]*dims[1]] - (*velField._w)[i-dims[0]*dims[1]])  / velFieldScale[2]
                        );
                }

        SmearBorder3D(delW, dims);
        auto iterations = solver.Solve(
            AsScalarField1D(q), A, AsScalarField1D(delW), 
            method);
        SmearBorder3D(q, dims);

        for (unsigned z=border[2]; z<dims[2]-border[2]; ++z)
            for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
                for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                    const auto i = (z*dims[1]+y)*dims[0]+x;
                    (*velField._u)[i] -= .5f*velFieldScale[0] * (q[i+1]                 - q[i-1]);
                    (*velField._v)[i] -= .5f*velFieldScale[1] * (q[i+dims[0]]           - q[i-dims[0]]);
                    (*velField._w)[i] -= .5f*velFieldScale[2] * (q[i+dims[0]*dims[1]]   - q[i-dims[0]*dims[1]]);
                }

        LogInfo << "EnforceIncompressibility took: " << iterations << " iterations.";
    }

    
    void EnforceIncompressibilityHelper::Execute(
        PoissonSolver& solver, VectorField2D vectorField,
        PoissonSolver::Method method, unsigned wrapEdges)
    {
        if (!_incompressibility || wrapEdges != _preparedWrapEdges) {
            _preparedWrapEdges = wrapEdges;
            _incompressibility = solver.PrepareDivergenceMatrix(
                PoissonSolver::Method::PreconCG, _preparedWrapEdges);
        }

        const auto N = vectorField._dims[0] * vectorField._dims[1];
        if (N != _preparedN) {
            _preparedN = N;
            _buffers[0] = VectorX(_preparedN);
            _buffers[1] = VectorX(_preparedN);
            _buffers[0].fill(0.f);
            _buffers[1].fill(0.f);
        }

        EnforceIncompressibility(
            vectorField, AsScalarField1D(_buffers[0]), AsScalarField1D(_buffers[1]),
            solver, *_incompressibility, (PoissonSolver::Method)method, wrapEdges);
    }

    const float* EnforceIncompressibilityHelper::GetDivergence()
    {
        return _buffers[0].data();
    }

    EnforceIncompressibilityHelper::EnforceIncompressibilityHelper() 
    {
        _preparedWrapEdges = 0u;
        _preparedN = 0u;
        _preparedMethod = (PoissonSolver::Method)~0u;
    }
    EnforceIncompressibilityHelper::~EnforceIncompressibilityHelper() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void VorticityConfinement(
        VectorField2D outputField,
        VectorField2D inputVelocities, float strength, float deltaTime)
    {
        //
        // VorticityConfinement amplifies the existing vorticity at each cell.
        // This is intended to add back errors caused by the discrete equations
        // we're using here.
        //
        // The vorticity can be calculated from the velocity field (by taking the
        // cross product with del. In 2D, this produces a scalar value (which is
        // conceptually a vector in the direction of an imaginary Z axis). We also
        // need to find the divergence of this scalar field.
        //
        // See http://web.stanford.edu/class/cs237d/smoke.pdf for details. In that
        // paper, Fedkiw calculates the vorticity at a half-cell offset from the 
        // velocity field. It's not clear why that was done. We will ignore that, 
        // and calculate vorticity exactly on the velocity field.
        //
        // Note --  like EnforceIncompressibility, it might be helpful to do take a
        //          multigrid-like approach for this step. That is, here we're emphasising
        //          very high frequency vorticity features (because these are the features
        //          that are most likely to be lost in the approximations of the model).
        //          But, by using grids of different resolutions, it might be interesting
        //          to emphasise some higher level vorticity features, as well.
        //

            // todo -- support margins fully!
        const auto dims = inputVelocities.Dimensions();
        VectorX vorticity(dims[0]*dims[1]);
        const UInt2 border(1,1);
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                auto dvydx = .5f * inputVelocities.Load(UInt2(x+1, y))[1] - inputVelocities.Load(UInt2(x-1, y))[1];
                auto dvxdy = .5f * inputVelocities.Load(UInt2(x, y+1))[0] - inputVelocities.Load(UInt2(x, y-1))[0];
                vorticity[y*dims[0]+x] = dvydx - dvxdy;
            }
        SmearBorder2D(vorticity, dims, ~0u);

        Float2 velFieldScale = deltaTime * strength * Float2(float(dims[0]-2*border[0]), float(dims[1]-2*border[1]));
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                    // find the discrete divergence of the absolute vorticity field
                const auto i = y*dims[0]+x;
                Float2 div(
                        .5f * (XlAbs(vorticity[i+1]) - XlAbs(vorticity[i-1])),
                        .5f * (XlAbs(vorticity[i+dims[0]]) - XlAbs(vorticity[i-dims[0]]))
                    );

                float magSq = MagnitudeSquared(div);
                if (magSq > 1e-10f) {
                    div *= XlRSqrt(magSq);

                        // in 2D, the vorticity is in the Z direction. Which means the cross product
                        // with our divergence vector is simple
                    float omega = vorticity[i];
                    auto additionalVel = MultiplyAcross(velFieldScale, Float2(div[1] * omega, -div[0] * omega));
                    outputField.Write(
                        UInt2(x, y),
                        outputField.Load(UInt2(x, y)) + additionalVel);
                }
            }
    }

}



///////////////////////////////////////////////////////////////////////////////////////////////////s
    // for draw debugging...
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"

namespace SceneEngine
{
    void RenderFluidDebugging2D(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        RenderFluidMode debuggingMode,
        UInt2 dimensions, float minValue, float maxValue,
        std::initializer_list<const float*> data)
    {
        CATCH_ASSETS_BEGIN
            using namespace RenderCore;
            using namespace BufferUploads;
            auto& uploads = GetBufferUploads();

            auto dx = dimensions[0], dy = dimensions[1];

            auto desc = CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read|GPUAccess::Write,
                TextureDesc::Plain2D(dx, dy, RenderCore::Metal::NativeFormat::R32_FLOAT),
                "fluid");

            for (unsigned c=0; c<data.size(); ++c) {
                const auto* srcData = data.begin() + c;
                auto pkt = CreateBasicPacket((dx)*(dy)*sizeof(float), *srcData, TexturePitches((dx)*sizeof(float), (dy)*(dx)*sizeof(float)));
                auto tex = uploads.Transaction_Immediate(desc, pkt.get());
                metalContext.BindPS(MakeResourceList(c, Metal::ShaderResourceView(tex->GetUnderlying())));
            }

            float constants[4] = { minValue, maxValue, 0.f, 0.f };
            metalContext.BindPS(MakeResourceList(Metal::ConstantBuffer(constants, sizeof(constants))));

            const ::Assets::ResChar* pixelShader = "";
            if (debuggingMode == RenderFluidMode::Scalar) {
                pixelShader = "game/xleres/cfd/debug.sh:ps_scalarfield:ps_*";
            } else if (debuggingMode == RenderFluidMode::Vector) {
                pixelShader = "game/xleres/cfd/debug.sh:ps_vectorfield:ps_*";
            }

            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic3D.vsh:PT:vs_*", pixelShader);

            Float2 wsDims = dimensions;

            struct Vertex { Float3 position; Float2 texCoord; } 
            vertices[] = 
            {
                { Float3(0.f, 0.f, 0.f), Float2(0.f, 0.f) },
                { Float3(wsDims[0], 0.f, 0.f), Float2(1.f, 0.f) },
                { Float3(0.f, wsDims[1], 0.f), Float2(0.f, 1.f) },
                { Float3(wsDims[0], wsDims[1], 0.f), Float2(1.f, 1.f) }
            };

            Metal::BoundInputLayout inputLayout(Metal::GlobalInputLayouts::PT, shader);
            Metal::BoundUniforms uniforms(shader);
            Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
                
            metalContext.BindPS(MakeResourceList(Techniques::CommonResources()._defaultSampler, Techniques::CommonResources()._linearClampSampler));
            metalContext.Bind(inputLayout);
            uniforms.Apply(metalContext, 
                parserContext.GetGlobalUniformsStream(), Metal::UniformsStream());
            metalContext.Bind(shader);

            metalContext.Bind(MakeResourceList(
                Metal::VertexBuffer(vertices, sizeof(vertices))), sizeof(Vertex), 0);
            metalContext.Bind(Techniques::CommonResources()._cullDisable);
            metalContext.Bind(Metal::Topology::TriangleStrip);
            metalContext.Draw(4);
        CATCH_ASSETS_END(parserContext)
    }

    void RenderFluidDebugging3D(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        RenderFluidMode debuggingMode,
        UInt3 dimensions, float minValue, float maxValue,
        std::initializer_list<const float*> data)
    {
        CATCH_ASSETS_BEGIN
            using namespace RenderCore;
            using namespace BufferUploads;
            auto& uploads = GetBufferUploads();

            auto dx = dimensions[0], dy = dimensions[1], dz = dimensions[2];
            auto pktSize = dx*dy*dz*sizeof(float);
            TexturePitches pitches(dx*sizeof(float), dy*dx*sizeof(float));

            auto desc = CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read|GPUAccess::Write,
                TextureDesc::Plain3D(dx, dy, dz, RenderCore::Metal::NativeFormat::R32_FLOAT),
                "fluid");

            for (unsigned c=0; c<data.size(); ++c) {
                const auto* srcData = data.begin() + c;
                auto pkt = CreateBasicPacket(pktSize, srcData, TexturePitches((dx)*sizeof(float), (dy)*(dx)*sizeof(float)));
                auto tex = uploads.Transaction_Immediate(desc, pkt.get());
                metalContext.BindPS(MakeResourceList(c, Metal::ShaderResourceView(tex->GetUnderlying())));
            }

            float constants[4] = { minValue, maxValue, 0.f, 0.f };
            metalContext.BindPS(MakeResourceList(Metal::ConstantBuffer(constants, sizeof(constants))));

            const ::Assets::ResChar* pixelShader = "";
            if (debuggingMode == RenderFluidMode::Scalar)       pixelShader = "game/xleres/cfd/debug3d.sh:ps_scalarfield:ps_*";
            else if (debuggingMode == RenderFluidMode::Vector)  pixelShader = "game/xleres/cfd/debug3d.sh:ps_vectorfield:ps_*";

            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>("game/xleres/basic3D.vsh:PT:vs_*", pixelShader);
            Float2 wsDims = Truncate(dimensions);

            struct Vertex { Float3 position; Float2 texCoord; } 
            vertices[] = 
            {
                { Float3(0.f, 0.f, 0.f), Float2(0.f, 1.f) },
                { Float3(wsDims[0], 0.f, 0.f), Float2(1.f, 1.f) },
                { Float3(0.f, wsDims[1], 0.f), Float2(0.f, 0.f) },
                { Float3(wsDims[0], wsDims[1], 0.f), Float2(1.f, 0.f) }
            };

            Metal::BoundInputLayout inputLayout(Metal::GlobalInputLayouts::PT, shader);
            Metal::BoundUniforms uniforms(shader);
            Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
                
            metalContext.BindPS(MakeResourceList(
                Techniques::CommonResources()._defaultSampler, 
                Techniques::CommonResources()._linearClampSampler));
            metalContext.Bind(inputLayout);
            uniforms.Apply(metalContext, 
                parserContext.GetGlobalUniformsStream(), Metal::UniformsStream());
            metalContext.Bind(shader);

            metalContext.Bind(MakeResourceList(
                Metal::VertexBuffer(vertices, sizeof(vertices))), sizeof(Vertex), 0);
            metalContext.Bind(Techniques::CommonResources()._cullDisable);
            metalContext.Bind(Metal::Topology::TriangleStrip);
            metalContext.Draw(4);
        CATCH_ASSETS_END(parserContext)
    }
}
