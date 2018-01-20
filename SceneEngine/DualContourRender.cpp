// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DualContourRender.h"
#include "DualContour.h"
#include "OITInternal.h"
#include "LightingParserContext.h"
#include "SceneEngineUtils.h"
#include "RefractionsBuffer.h"  // for BuildDuplicatedDepthBuffer

#include "../RenderCore/Format.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Techniques/TechniqueMaterial.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Math/Transformations.h"

// #include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h"

namespace SceneEngine
{
    using namespace RenderCore;

    static const ::Assets::ResChar s_techniqueConfig[] = "xleres/techniques/cloudvolume.tech";

    class DualContourRenderer::Pimpl
    {
    public:
        Metal::VertexBuffer _vertexBuffer;
        Metal::IndexBuffer _indexBuffer;
        Format _indexFormat;
        unsigned _indexCount;

        Techniques::ShaderVariationSet _basicMaterial;
        RenderCore::SharedPkt _materialConstants;

        std::shared_ptr<::Assets::DependencyValidation> _dependencyValidation;
    };

    static void WriteIndexData(void* destination, unsigned indexSize, const DualContourMesh& mesh)
    {
        if (indexSize == 2) {
            auto indices = (unsigned short*)destination;
            for (auto i=mesh._quads.cbegin(); i!=mesh._quads.cend(); ++i, indices+=6) {
                    //  This quad should become 2 triangles. We can choose 
                    //  where to split the quad. Note that winding order is 
                    //  difficult. So back face culling is disabled.
                    //  Input vertices are in Z shape.
                auto v0 = mesh._vertices[i->_verts[0]]._pt;
                auto v1 = mesh._vertices[i->_verts[1]]._pt;
                auto v2 = mesh._vertices[i->_verts[2]]._pt;
                auto v3 = mesh._vertices[i->_verts[3]]._pt;

                    // split based on which is the shortest splitter
                auto l0 = MagnitudeSquared(v2 - v1);
                auto l1 = MagnitudeSquared(v3 - v0);

                if (l0 < l1) {
                    indices[0] = (unsigned short)i->_verts[0];
                    indices[1] = (unsigned short)i->_verts[1];
                    indices[2] = (unsigned short)i->_verts[2];
                    indices[3] = (unsigned short)i->_verts[2];
                    indices[4] = (unsigned short)i->_verts[1];
                    indices[5] = (unsigned short)i->_verts[3];
                } else {
                    indices[0] = (unsigned short)i->_verts[0];
                    indices[1] = (unsigned short)i->_verts[1];
                    indices[2] = (unsigned short)i->_verts[3];
                    indices[3] = (unsigned short)i->_verts[0];
                    indices[4] = (unsigned short)i->_verts[3];
                    indices[5] = (unsigned short)i->_verts[2];
                }
            }
        } else if (indexSize == 4) {
            auto indices = (unsigned int*)destination;
            for (auto i=mesh._quads.cbegin(); i!=mesh._quads.cend(); ++i, indices+=6) {
                    //  This quad should become 2 triangles. We can choose 
                    //  where to split the quad. Note that winding order is 
                    //  difficult. So back face culling is disabled.
                    //  Input vertices are in Z shape.
                auto v0 = mesh._vertices[i->_verts[0]]._pt;
                auto v1 = mesh._vertices[i->_verts[1]]._pt;
                auto v2 = mesh._vertices[i->_verts[2]]._pt;
                auto v3 = mesh._vertices[i->_verts[3]]._pt;

                    // split based on which is the shortest splitter
                auto l0 = MagnitudeSquared(v2 - v1);
                auto l1 = MagnitudeSquared(v3 - v0);

                if (l0 < l1) {
                    indices[0] = i->_verts[0]; indices[1] = i->_verts[1]; indices[2] = i->_verts[2];
                    indices[3] = i->_verts[2]; indices[4] = i->_verts[1]; indices[5] = i->_verts[3];
                } else {
                    indices[0] = i->_verts[0]; indices[1] = i->_verts[1]; indices[2] = i->_verts[3];
                    indices[3] = i->_verts[0]; indices[4] = i->_verts[3]; indices[5] = i->_verts[2];
                }
            }
        } else {
            assert(0);
        }
    }

    static void MainRender(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext,
        unsigned techniqueIndex, const DualContourRenderer::Pimpl* pimpl)
    {
        TRY {
            using namespace RenderCore::Techniques;
            auto variation = pimpl->_basicMaterial.FindVariation(parserContext, techniqueIndex, s_techniqueConfig);
            if (variation._shader._shaderProgram != nullptr) {
                variation._shader.Apply(*context, parserContext, 
                    {
                        MakeLocalTransformPacket(
                            Identity<Float4x4>(),
                            ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                        pimpl->_materialConstants
                    });

                context->Bind(MakeResourceList(pimpl->_vertexBuffer), sizeof(DualContourMesh::Vertex), 0);
                context->Bind(pimpl->_indexBuffer, pimpl->_indexFormat);

                context->Bind(Topology::TriangleList);
                context->DrawIndexed(pimpl->_indexCount);
            }
        }
        CATCH_ASSETS(parserContext)
        CATCH(...) {} 
        CATCH_END
    }

    static void ResolveOIT(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext,
        TransparencyTargetsBox& targets,
        Metal::ShaderResourceView& duplicatedDepthBuffer)
    {
            //  We wrote depth information to our transparency targets
            //  Each pixel, we need to find the distance the ray travels
            //  through the volume, and use that to occlude and bounce 
            //  light.

        CATCH_ASSETS_BEGIN
            context->BindPS(MakeResourceList(
                targets._fragmentIdsTextureSRV, 
                targets._nodeListBufferSRV, duplicatedDepthBuffer));

            context->Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
            context->Bind(Techniques::CommonResources()._dssReadOnly);

            auto& resolveShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "xleres/basic2D.vsh:fullscreen:vs_*", 
                "xleres/forward/transparency/cloudresolve.psh:main:ps_*");
            context->Bind(resolveShader);
            context->BindPS(MakeResourceList(0, parserContext.GetGlobalTransformCB()));
            SetupVertexGeneratorShader(*context);
            context->Draw(4);
            context->UnbindPS<RenderCore::Metal::ShaderResourceView>(0, 3);
        CATCH_ASSETS_END(parserContext)
    }

    static void RenderMainSceneViaOIT(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext,
        const DualContourRenderer::Pimpl* pimpl)
    {
        TRY {
                //  Render first to offscreen OIT targets
                //  We can use this to determine the shadowing along
                //  the given ray.
                //  Note that we might not need colour information in
                //  our OIT targets (maybe just depth). By

            Metal::ViewportDesc mainViewport(*context);
            SavedTargets prevTargets(*context);
            Metal::ShaderResourceView duplicatedDepthBuffer;

                // note that we have to duplicate the depth buffer here.
                //  It's not ideal. We don't want to duplicate the depth
                //  buffer too many times per frame. Ideally we would do it
                //  just once after the opaque geometry is rendered, and then
                //  reuse that texture for all effects that might need it.
                //
                //      actually, we can skip this now -- because we never write opaque pixels during the resolve step.
            // auto* dsv = prevTargets.GetDepthStencilView();
            // if (dsv) {
            //     auto dsResource = Metal::ExtractResource<ID3D::Resource>(dsv);
            //     assert(dsResource);
            //     duplicatedDepthBuffer = BuildDuplicatedDepthBuffer(context, dsResource.get());
            // }

            auto& transparencyTargets = 
				ConsoleRig::FindCachedBox2<TransparencyTargetsBox>(
                    unsigned(mainViewport.Width), unsigned(mainViewport.Height), false, true);
            OrderIndependentTransparency_ClearAndBind(*context, transparencyTargets, duplicatedDepthBuffer);
            context->Bind(Techniques::CommonResources()._dssReadOnly);  // never write to depth (even for very opaque pixels)
            context->Bind(Techniques::CommonResources()._cullDisable);  // we need to write both front and back faces (but the pixel shader will treat them differently)
            MainRender(context, parserContext, 4, pimpl);


            // unbind the depth buffer, and create a shader resource view for the depth buffer
                //  it doesn't need to be duplicated for this case
#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
            context->GetUnderlying()->OMSetRenderTargets(1, prevTargets.GetRenderTargets(), nullptr);
            if (prevTargets.GetDepthStencilView()) {
                duplicatedDepthBuffer = Metal::ShaderResourceView(Metal::ExtractResource<ID3D::Resource>(
                    prevTargets.GetDepthStencilView()).get(), 
                    {{TextureViewWindow::Aspect::Depth}});
            }
#endif

                //  Revert to the main scene targets, and "resolve" the
                //  render using the depth information from the transparent targets
                //  Note that many targets will have zero depth. So it might be useful
                //  to consider some fast paths for rejecting pixels that don't need
                //  the resolve step. For example, if we knew some bounding volume 
                //  for the object, we could restrict the resolve step to that approximate
                //  bounding volume.
            ResolveOIT(context, parserContext, transparencyTargets, duplicatedDepthBuffer);

            prevTargets.ResetToOldTargets(*context);        // (rebind the depth buffer)
        }
        CATCH_ASSETS(parserContext)
        CATCH(...) {} 
        CATCH_END
    }

    void DualContourRenderer::Render(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext,
        unsigned techniqueIndex) const
    {
            // render as a solid object (into main scene or shadow scene)
        context->Bind(Techniques::CommonResources()._dssReadWrite);
        // context->Bind(Techniques::CommonResources()._defaultRasterizer);
        MainRender(context, parserContext, techniqueIndex, _pimpl.get());
    }

    void DualContourRenderer::RenderUnsortedTrans(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext,
        unsigned techniqueIndex) const
    {
            // (intended to be used when rendering wireframe)
        context->Bind(Techniques::CommonResources()._dssReadOnly);
        // context->Bind(Techniques::CommonResources()._defaultRasterizer);
        context->Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
        MainRender(context, parserContext, techniqueIndex, _pimpl.get());
    }

    void DualContourRenderer::RenderAsCloud( 
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext)
    {
            // render as a cloud, during a post-opaque rendering
        RenderMainSceneViaOIT(context, parserContext, _pimpl.get());
    }

    const std::shared_ptr<::Assets::DependencyValidation>& DualContourRenderer::GetDependencyValidation()
    {
        return _pimpl->_dependencyValidation;
    }

    DualContourRenderer::DualContourRenderer(const DualContourMesh& mesh)
    {
        unsigned indexSize = (mesh._vertices.size() <= 0xffff) ? 2 : 4;
        auto ibDataCount = mesh._quads.size() * 6;
        auto ibData = std::make_unique<unsigned char[]>(ibDataCount*indexSize);
        WriteIndexData(ibData.get(), indexSize, mesh);

        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_indexBuffer = Metal::IndexBuffer(ibData.get(), ibDataCount*indexSize);
        pimpl->_vertexBuffer = Metal::VertexBuffer(
            AsPointer(mesh._vertices.cbegin()), mesh._vertices.size() * sizeof(DualContourMesh::Vertex));
        pimpl->_indexFormat = (indexSize==4)?Format::R32_UINT:Format::R16_UINT;
        pimpl->_indexCount = (unsigned)ibDataCount;

        using namespace Techniques;
        pimpl->_basicMaterial = ShaderVariationSet(
            GlobalInputLayouts::PN, 
            { ObjectCB::LocalTransform, ObjectCB::BasicMaterialConstants },
            ParameterBox());

        const auto& cbLayout = pimpl->_basicMaterial.GetCBLayout(s_techniqueConfig);
        pimpl->_materialConstants = cbLayout.BuildCBDataAsPkt(ParameterBox());

        pimpl->_dependencyValidation = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(pimpl->_dependencyValidation, cbLayout.GetDependencyValidation());

        _pimpl = std::move(pimpl);
    }

    DualContourRenderer::~DualContourRenderer()
    {}

    void    DualContourMesh_DebuggingRender(
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext,
            unsigned techniqueIndex,
            const DualContourMesh& mesh)
    {
            //  Render the mesh from the raw quads data. This will create
            //  vertex buffers and index buffers on the fly -- so it's not
            //  really very efficient. Just for debugging purposes

        TRY 
        {
            using namespace RenderCore;
            using namespace RenderCore::Techniques;

            ShaderVariationSet material(
                GlobalInputLayouts::PN, 
                { ObjectCB::LocalTransform, ObjectCB::BasicMaterialConstants },
                ParameterBox());

            auto shader = material.FindVariation(parserContext, techniqueIndex, "illum");
            if (shader._shader._shaderProgram) {
                shader._shader.Apply(
                    *context, parserContext, 
                    {
                        MakeLocalTransformPacket(
                            Identity<Float4x4>(),
                            ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                        shader._cbLayout->BuildCBDataAsPkt(ParameterBox())
                    });

                Metal::VertexBuffer vb(
                    AsPointer(mesh._vertices.cbegin()), mesh._vertices.size() * sizeof(DualContourMesh::Vertex));

                unsigned indexSize = (mesh._vertices.size() <= 0xffff) ? 2 : 4;
                auto ibDataCount = mesh._quads.size() * 6;
                auto ibData = std::make_unique<unsigned char[]>(ibDataCount*indexSize);
                WriteIndexData(ibData.get(), indexSize, mesh);

                Metal::IndexBuffer ib(ibData.get(), ibDataCount*indexSize);

                context->Bind(MakeResourceList(vb), sizeof(DualContourMesh::Vertex), 0);
                context->Bind(ib, (indexSize==4)?Format::R32_UINT:Format::R16_UINT);

                context->Bind(Topology::TriangleList);
                context->Bind(Techniques::CommonResources()._dssReadWrite);

                context->DrawIndexed(unsigned(ibDataCount));
            }
        }
        CATCH_ASSETS(parserContext)
        CATCH(...) {} 
        CATCH_END
    }

}
