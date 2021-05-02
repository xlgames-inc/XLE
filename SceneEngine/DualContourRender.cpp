// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DualContourRender.h"
#include "DualContour.h"
#include "OITInternal.h"
#include "SceneEngineUtils.h"
#include "RefractionsBuffer.h"  // for BuildDuplicatedDepthBuffer
#include "MetalStubs.h"

#include "../RenderCore/Format.h"
#include "../RenderCore/BufferView.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/CommonUtils.h"
#include "../RenderCore/Assets/PredefinedCBLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../FixedFunctionModel/PreboundShaders.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Math/Transformations.h"
#include "../xleres/FileList.h"

// #include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h"

namespace SceneEngine
{
    using namespace RenderCore;

    static const ::Assets::ResChar s_techniqueConfig[] = "xleres/techniques/cloudvolume.tech";

    class DualContourRenderer::Pimpl
    {
    public:
        IResourcePtr _vertexBuffer;
        IResourcePtr _indexBuffer;
        Format _indexFormat;
        unsigned _indexCount;

        FixedFunctionModel::SimpleShaderVariationManager _basicMaterial;
        RenderCore::SharedPkt _materialConstants;

        ::Assets::DependencyValidation _dependencyValidation;
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
        RenderCore::Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex, const DualContourRenderer::Pimpl* pimpl)
    {
        TRY {
            using namespace RenderCore::Techniques;
            auto variation = pimpl->_basicMaterial.FindVariation(parserContext, techniqueIndex, s_techniqueConfig);
            if (variation._shader._shaderProgram != nullptr) {
                variation._shader.Apply(*context, parserContext, 
					{VertexBufferView{pimpl->_vertexBuffer}});

				variation._shader.ApplyUniforms(*context, 0, parserContext.GetGlobalUniformsStream());
				ConstantBufferView cbvs[] {
                    MakeLocalTransformPacket(
                        Identity<Float4x4>(),
                        ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                    pimpl->_materialConstants
                };
				variation._shader.ApplyUniforms(*context, 1, UniformsStream{MakeIteratorRange(cbvs)});

                context->Bind(
					*(Metal::Resource*)pimpl->_indexBuffer->QueryInterface(typeid(Metal::Resource).hash_code()), 
					pimpl->_indexFormat);

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
        RenderCore::Techniques::ParsingContext& parserContext,
        TransparencyTargetsBox& targets,
        Metal::ShaderResourceView& duplicatedDepthBuffer)
    {
            //  We wrote depth information to our transparency targets
            //  Each pixel, we need to find the distance the ray travels
            //  through the volume, and use that to occlude and bounce 
            //  light.

        CATCH_ASSETS_BEGIN
            context->GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(
                targets._fragmentIdsTextureSRV, 
                targets._nodeListBufferSRV, duplicatedDepthBuffer));

            context->Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
            context->Bind(Techniques::CommonResources()._dssReadOnly);

            auto& resolveShader = ::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
                "xleres/forward/transparency/cloudresolve.pixel.hlsl:main:ps_*");
            context->Bind(resolveShader);
            context->GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(0, parserContext.GetGlobalTransformCB()));
            SetupVertexGeneratorShader(*context);
            context->Draw(4);
            MetalStubs::UnbindPS<RenderCore::Metal::ShaderResourceView>(*context, 0, 3);
        CATCH_ASSETS_END(parserContext)
    }

    static void RenderMainSceneViaOIT(
        RenderCore::Metal::DeviceContext* context, 
        RenderCore::Techniques::ParsingContext& parserContext,
        const DualContourRenderer::Pimpl* pimpl)
    {
        TRY {
                //  Render first to offscreen OIT targets
                //  We can use this to determine the shadowing along
                //  the given ray.
                //  Note that we might not need colour information in
                //  our OIT targets (maybe just depth). By

            ViewportDesc mainViewport = context->GetBoundViewport();
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
#if GFXAPI_TARGET == GFXAPI_DX11	// platformtemp
            context->GetUnderlying()->OMSetRenderTargets(1, prevTargets.GetRenderTargets(), nullptr);
            if (prevTargets.GetDepthStencilView()) {
                duplicatedDepthBuffer = Metal::ShaderResourceView(
					Metal::AsResourcePtr(Metal::ExtractResource<ID3D::Resource>(prevTargets.GetDepthStencilView())), 
                    {{TextureViewDesc::Aspect::Depth}});
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
        RenderCore::Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex) const
    {
            // render as a solid object (into main scene or shadow scene)
        context->Bind(Techniques::CommonResources()._dssReadWrite);
        // context->Bind(Techniques::CommonResources()._defaultRasterizer);
        MainRender(context, parserContext, techniqueIndex, _pimpl.get());
    }

    void DualContourRenderer::RenderUnsortedTrans(
        RenderCore::Metal::DeviceContext* context, 
        RenderCore::Techniques::ParsingContext& parserContext,
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
        RenderCore::Techniques::ParsingContext& parserContext)
    {
            // render as a cloud, during a post-opaque rendering
        RenderMainSceneViaOIT(context, parserContext, _pimpl.get());
    }

    const ::Assets::DependencyValidation& DualContourRenderer::GetDependencyValidation()
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
        pimpl->_indexBuffer = RenderCore::Techniques::CreateStaticIndexBuffer(MakeIteratorRange(ibData.get(), PtrAdd(ibData.get(), ibDataCount*indexSize)));
        pimpl->_vertexBuffer = RenderCore::Techniques::CreateStaticVertexBuffer(MakeIteratorRange(mesh._vertices));
        pimpl->_indexFormat = (indexSize==4)?Format::R32_UINT:Format::R16_UINT;
        pimpl->_indexCount = (unsigned)ibDataCount;

        using namespace Techniques;
        pimpl->_basicMaterial = FixedFunctionModel::SimpleShaderVariationManager(
            GlobalInputLayouts::PN, 
            { ObjectCB::LocalTransform, ObjectCB::BasicMaterialConstants },
            ParameterBox());

        const auto& cbLayout = pimpl->_basicMaterial.GetCBLayout(s_techniqueConfig);
        pimpl->_materialConstants = cbLayout.BuildCBDataAsPkt(ParameterBox(), RenderCore::Techniques::GetDefaultShaderLanguage());

        pimpl->_dependencyValidation = ::Assets::GetDepValSys().Make();
		if (cbLayout.GetDependencyValidation())
			pimpl->_dependencyValidation.RegisterDependency(cbLayout.GetDependencyValidation());

        _pimpl = std::move(pimpl);
    }

    DualContourRenderer::~DualContourRenderer()
    {}

    void    DualContourMesh_DebuggingRender(
            RenderCore::IThreadContext& context, 
            RenderCore::Techniques::ParsingContext& parserContext,
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

            FixedFunctionModel::SimpleShaderVariationManager material(
                GlobalInputLayouts::PN, 
                { ObjectCB::LocalTransform, ObjectCB::BasicMaterialConstants },
                ParameterBox());

			auto& metalContext = *Metal::DeviceContext::Get(context);

            auto shader = material.FindVariation(parserContext, techniqueIndex, "illum.tech");
            if (shader._shader._shaderProgram) {
				shader._shader.ApplyUniforms(metalContext, 0, parserContext.GetGlobalUniformsStream());
				ConstantBufferView cbvs[] = {
                    MakeLocalTransformPacket(
                        Identity<Float4x4>(),
                        ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                    shader._cbLayout->BuildCBDataAsPkt(ParameterBox(), RenderCore::Techniques::GetDefaultShaderLanguage())
                };
				shader._shader.ApplyUniforms(metalContext, 1, UniformsStream{MakeIteratorRange(cbvs)});

                auto vb = RenderCore::Techniques::CreateStaticVertexBuffer(MakeIteratorRange(mesh._vertices));

                unsigned indexSize = (mesh._vertices.size() <= 0xffff) ? 2 : 4;
                auto ibDataCount = mesh._quads.size() * 6;
                auto ibData = std::vector<unsigned char>(ibDataCount*indexSize);
                WriteIndexData(ibData.data(), indexSize, mesh);

                auto ib = RenderCore::Techniques::CreateStaticIndexBuffer(MakeIteratorRange(ibData));
				
				shader._shader.Apply(
                    metalContext, parserContext, 
					{VertexBufferView{vb}});
                metalContext.Bind(
					*(Metal::Resource*)ib->QueryInterface(typeid(Metal::Resource).hash_code()), 
					(indexSize==4)?Format::R32_UINT:Format::R16_UINT);

                metalContext.Bind(Topology::TriangleList);
                metalContext.Bind(Techniques::CommonResources()._dssReadWrite);

                metalContext.DrawIndexed(unsigned(ibDataCount));
            }
        }
        CATCH_ASSETS(parserContext)
        CATCH(...) {} 
        CATCH_END
    }

}
