// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GenerateAO.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../SceneEngine/LightInternal.h"    // for shadow projection constants;
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Assets/SharedStateSet.h"
#include "../../RenderCore/Metal/ShaderResource.h"
#include "../../RenderCore/Metal/RenderTargetView.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Math/Transformations.h"
#include "../../Math/ProjectionMath.h"

namespace ToolsRig
{
    using namespace RenderCore;
    using ModelScaffold = RenderCore::Assets::ModelScaffold;
    using MaterialScaffold = RenderCore::Assets::MaterialScaffold;
    using ModelRenderer = RenderCore::Assets::ModelRenderer;
    using SharedStateSet = RenderCore::Assets::SharedStateSet;

    class AoGen::Pimpl
    {
    public:
        Desc        _settings;

        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;
        using SRV = Metal::ShaderResourceView;
        using DSV = Metal::DepthStencilView;
        using UAV = Metal::UnorderedAccessView;
        ResLocator  _cubeLocator;
        DSV         _cubeDSV;
        SRV         _cubeSRV;

        ResLocator  _miniLocator;
        UAV         _miniUAV;

        const Metal::ComputeShader* _stepDownShader;
        std::shared_ptr<::Assets::DependencyValidation> _depVal;
    };

    static void SetupCubeMapShadowProjection(
        Metal::DeviceContext& metalContext,
        Techniques::ParsingContext& parserContext,
        float nearClip, float farClip);

    float AoGen::CalculateSkyDomeOcclusion(
        Metal::DeviceContext& metalContext,
        const ModelRenderer& renderer,
        RenderCore::Assets::SharedStateSet& sharedStates,
        const RenderCore::Assets::MeshToModel& meshToModel,
        const Float3& samplePoint)
    {
            //
            // We want to calculate a rough approximation for the amount of
            // the sky dome that is occluded at the given sample point.
            //
            // We can use this value to occlude indirect light (assuming that
            // geometry that occludes the sky dome will also occlude indirect
            // light -- which should be reasonable for indirect sources
            // out side of the model).
            //
            // But we can also use this to occlude direct light from the sky
            // dome.
            //
            // To generate the value, we need to render a half-cube-map (we
            // are only interested in illumination coming from above the equator)
            // of depths around the sample point.
            //
            // For each texel in the cube map, if there is a value in the depth
            // texture, then there must be a nearby occluder in that direction. 
            // We weight the texel's occlusion by it's solid angle and sum up
            // all of the occlusion.
            //

            //
            // To render the object, we can use the normal ModelRenderer object.
            // The ModelRenderer might be a bit heavy-weight (we could alternatively
            // just parse through the ModelScaffold (for example, like 
            // MaterialSceneParser::DrawModel)
            // But the ModelRenderer is perhaps the most future-proof approach.
            //
            // We can re-use the shadow rendering path for this (because it works
            // in the same way; with the geometry shader splitting the geometry
            // between viewports, depth-only output & alpha test support)
            //  -- and, after all, we are rendering a kind of shadow
            //

        SceneEngine::SavedTargets savedTargets(&metalContext);

        metalContext.Clear(_pimpl->_cubeDSV, 1.f, 0u);
        metalContext.Bind(ResourceList<Metal::RenderTargetView, 0>(), &_pimpl->_cubeDSV);

            // configure rendering for the shadow shader
        const auto& settings = _pimpl->_settings;
        const auto& commonRes = Techniques::CommonResources();
        Metal::ViewportDesc viewport(
            0.f, 0.f, float(settings._renderResolution), float(settings._renderResolution), 
            0.f, 1.f);
        metalContext.Bind(viewport);
        metalContext.Bind(commonRes._defaultRasterizer);

        Techniques::TechniqueContext techniqueContext;
        techniqueContext._runtimeState.SetParameter((const utf8*)"SHADOW_CASCADE_MODE", 1u);            // arbitrary projection mode
        techniqueContext._runtimeState.SetParameter((const utf8*)"FRUSTUM_FILTER", 31u);                // enable writing to 5 frustums
        techniqueContext._runtimeState.SetParameter((const utf8*)"OUTPUT_SHADOW_PROJECTION_COUNT", 5u);
        Techniques::ParsingContext parserContext(techniqueContext);

            // We shouldn't need to fill in the "global transform" constant buffer
            // The model renderer will manage local transform constant buffers internally,
            // we should only need to set the shadow projection constants.
        
        SetupCubeMapShadowProjection(metalContext, parserContext, settings._minDistance, settings._maxDistance);

            // Render the model onto our cube map surface
        sharedStates.CaptureState(&metalContext);
        TRY {
            renderer.Render(
                RenderCore::Assets::ModelRendererContext(
                    &metalContext, parserContext, Techniques::TechniqueIndex::ShadowGen),
                sharedStates, AsFloat4x4(Float3(-samplePoint)), meshToModel);
        } CATCH(...) {
            sharedStates.ReleaseState(&metalContext);
            savedTargets.ResetToOldTargets(&metalContext);
            throw;
        } CATCH_END
        sharedStates.ReleaseState(&metalContext);
        savedTargets.ResetToOldTargets(&metalContext);

            //
            // Now we have to read-back the results from the cube map,
            // and weight by the solid angle. Let's do this with a 
            // compute shader. We'll split each face into 16 squares, and
            // assign a separate thread to each. The output will be a 4x4x5
            // texture of floats
            //

        metalContext.BindCS(MakeResourceList(_pimpl->_cubeSRV));
        metalContext.BindCS(MakeResourceList(_pimpl->_miniUAV));

        metalContext.Bind(*_pimpl->_stepDownShader);
        metalContext.Dispatch(1u);

        metalContext.UnbindCS<Metal::ShaderResourceView>(0, 1);
        metalContext.UnbindCS<Metal::UnorderedAccessView>(0, 1);

        auto& bufferUploads = RenderCore::Assets::Services::GetBufferUploads();
        auto readback = bufferUploads.Resource_ReadBack(*_pimpl->_miniLocator);

            // Note that we're currently using the full face for the side faces
            // (as opposed to just half of the face, which would correspond to
            // a hemisphere)
            // We could ignore some of the texels in "readback" to only use
            // the top half of the side faces.
        const float solidAngleFace = 4.f * gPI / 6.f;
        const float solidAngleTotal = 5.f * solidAngleFace;
        float occlusionTotal = 0.f;
        for (unsigned f=0; f<5; ++f) {
            auto pitches = readback->GetPitches(f);
            auto* d = (float*)readback->GetData(f);
            for (unsigned y=0; y<4; ++y)
                for (unsigned x=0; x<4; ++x)
                    occlusionTotal += PtrAdd(d, y*pitches._rowPitch)[x];
        }

            // Our final result is a proportion of the sampled sphere that is 
            // occluded.
        return occlusionTotal / solidAngleTotal;
    }

    AoGen::AoGen(const Desc& settings)
    {
            // _renderResolution must be a multiple of 4 -- this is required
            // for the step-down compute shader to work correctly.
        if ((settings._renderResolution%4)!=0)
            Throw(::Exceptions::BasicLabel("Working texture in AOGen must have dimensions that are a multiple of 4"));

        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = settings;

        const unsigned cubeFaces = 5;

        using namespace BufferUploads;
        auto& bufferUploads = RenderCore::Assets::Services::GetBufferUploads();
        _pimpl->_cubeLocator = bufferUploads.Transaction_Immediate(
            CreateDesc( 
                BindFlag::DepthStencil | BindFlag::ShaderResource,
                0, GPUAccess::Read|GPUAccess::Write,
                TextureDesc::Plain2D(
                    settings._renderResolution, settings._renderResolution, 
                    Metal::NativeFormat::R24G8_TYPELESS, 1, cubeFaces),
                "AoGen"));
        _pimpl->_cubeDSV = Metal::DepthStencilView(_pimpl->_cubeLocator->GetUnderlying(), Metal::NativeFormat::D24_UNORM_S8_UINT, Metal::ArraySlice(cubeFaces));
        _pimpl->_cubeSRV = Metal::ShaderResourceView(_pimpl->_cubeLocator->GetUnderlying(), Metal::NativeFormat::R24_UNORM_X8_TYPELESS, cubeFaces);

        _pimpl->_miniLocator = bufferUploads.Transaction_Immediate(
            CreateDesc( 
                BindFlag::UnorderedAccess,
                0, GPUAccess::Write,
                TextureDesc::Plain2D(4, 4, Metal::NativeFormat::R32_FLOAT, 1, cubeFaces),
                "AoGenMini"));
        _pimpl->_miniUAV = Metal::UnorderedAccessView(_pimpl->_miniLocator->GetUnderlying());

        _pimpl->_stepDownShader = &::Assets::GetAssetDep<Metal::ComputeShader>(
            "game/xleres/toolshelper/aogenprocess.sh:CubeMapStepDown:cs_*");

        _pimpl->_depVal = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_pimpl->_depVal, _pimpl->_stepDownShader->GetDependencyValidation());
    }

    AoGen::~AoGen() {}
    const std::shared_ptr<::Assets::DependencyValidation>& AoGen::GetDependencyValidation() const
    {
        return _pimpl->_depVal;
    }



    static void SetupCubeMapShadowProjection(
        Metal::DeviceContext& metalContext,
        Techniques::ParsingContext& parserContext,
        float nearClip, float farClip)
    {
            // set 5 faces of the cubemap tr
        auto basicProj = PerspectiveProjection(
            -1.f, -1.f, 1.f, 1.f, nearClip, farClip,
            Techniques::GetDefaultClipSpaceType());

        Float4x4 cubeViewMatrices[6] = 
        {
            MakeCameraToWorld(Float3( 0.f,  0.f,  1.f), Float3( 1.f,  0.f,  0.f), Zero<Float3>()),
            MakeCameraToWorld(Float3( 1.f,  0.f,  0.f), Float3( 0.f,  0.f,  1.f), Zero<Float3>()),
            MakeCameraToWorld(Float3( 0.f,  1.f,  0.f), Float3( 0.f,  0.f,  1.f), Zero<Float3>()),
            MakeCameraToWorld(Float3(-1.f,  0.f,  0.f), Float3( 0.f,  0.f,  1.f), Zero<Float3>()),
            MakeCameraToWorld(Float3( 0.f, -1.f,  0.f), Float3( 0.f,  0.f,  1.f), Zero<Float3>()),
            MakeCameraToWorld(Float3( 0.f,  0.f, -1.f), Float3(-1.f,  0.f,  0.f), Zero<Float3>())
        };

        SceneEngine::CB_ArbitraryShadowProjection shadowProj;
        shadowProj._projectionCount = 5;
        for (unsigned c=0; c<dimof(cubeViewMatrices); ++c) {
            shadowProj._worldToProj[c] = Combine(InvertOrthonormalTransform(cubeViewMatrices[c]), basicProj);
            shadowProj._minimalProj[c] = ExtractMinimalProjection(shadowProj._worldToProj[c]);
        }

        parserContext.SetGlobalCB(
            metalContext, Techniques::TechniqueContext::CB_ShadowProjection,
            &shadowProj, sizeof(shadowProj));
    }

}

