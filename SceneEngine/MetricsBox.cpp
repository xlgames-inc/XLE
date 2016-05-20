// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MetricsBox.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Assets/Assets.h"
#include "../Utility/StringFormat.h"

namespace SceneEngine
{
    using namespace RenderCore;
    MetricsBox::MetricsBox(const Desc& desc) 
    {
        auto& uploads = GetBufferUploads();
        ResourceDesc metricsBufferDesc;
        metricsBufferDesc._type = ResourceDesc::Type::LinearBuffer;
        metricsBufferDesc._bindFlags = BindFlag::UnorderedAccess|BindFlag::StructuredBuffer|BindFlag::ShaderResource;
        metricsBufferDesc._cpuAccess = 0;
        metricsBufferDesc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
        metricsBufferDesc._allocationRules = 0;
        metricsBufferDesc._linearBufferDesc._structureByteSize = sizeof(unsigned)*16;
        metricsBufferDesc._linearBufferDesc._sizeInBytes = metricsBufferDesc._linearBufferDesc._structureByteSize;
        auto metricsBuffer = uploads.Transaction_Immediate(metricsBufferDesc);

        _metricsBufferUAV = Metal::UnorderedAccessView(metricsBuffer->ShareUnderlying());
        _metricsBufferSRV = Metal::ShaderResourceView(metricsBuffer->ShareUnderlying());
    }

    MetricsBox::~MetricsBox() {}


    void RenderGPUMetrics(
        RenderCore::Metal::DeviceContext& context,
        LightingParserContext& parsingContext,
        const ::Assets::ResChar shaderName[],
        std::initializer_list<const ::Assets::ResChar*> valueSources,
        unsigned protectStates)
    {
        using States = ProtectState::States;
        const States::BitField effectedStates = 
            States::DepthStencilState | States::BlendState
            | States::Topology | States::InputLayout | States::VertexBuffer
            ;
        ProtectState savedStates(context, effectedStates & protectStates);

            // Utility function for writing GPU metrics values to the screen.
            // This is useful when we have metrics values that don't reach the
            // CPU. Ie, they are written to UAV resources (or other resources)
            // and then read back on the GPU during the same frame. The geometry
            // shader converts the numbers into a string, and textures appropriately.
            // So a final value can be written to the screen without ever touching
            // the CPU.
        const auto& shader = ::Assets::GetAssetDep<Metal::DeepShaderProgram>(
            (StringMeld<MaxPath, ::Assets::ResChar>() << shaderName << ":metricsrig_main:!vs_*").get(),
            "game/xleres/utility/metricsrender.gsh:main:gs_*",
            "game/xleres/utility/metricsrender.psh:main:ps_*",
            "", "", 
            (StringMeld<64>() << "VALUE_SOURCE_COUNT=" << valueSources.size()).get());
        Metal::BoundClassInterfaces boundInterfaces(shader);
        for (unsigned c=0; c<valueSources.size(); ++c)
            boundInterfaces.Bind(Hash64("ValueSource"), c, valueSources.begin()[c]);
        context.Bind(shader, boundInterfaces);

        const auto* metricsDigits = "game/xleres/DefaultResources/metricsdigits.dds:T";
        context.BindPS(MakeResourceList(3, ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>(metricsDigits).GetShaderResource()));

        Metal::BoundUniforms uniforms(shader);
        Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
        uniforms.BindConstantBuffers(1, {"$Globals"});
        uniforms.BindShaderResources(1, {"MetricsObject"});

        Metal::ViewportDesc viewport(context);
        unsigned globalCB[4] = { unsigned(viewport.Width), unsigned(viewport.Height), 0, 0 };
        uniforms.Apply(
            context, parsingContext.GetGlobalUniformsStream(),
            Metal::UniformsStream(
                { MakeSharedPkt(globalCB) }, 
                { &parsingContext.GetMetricsBox()->_metricsBufferSRV }));

        context.Unbind<Metal::VertexBuffer>();
        context.Unbind<Metal::BoundInputLayout>();
        context.Bind(Metal::Topology::PointList);
        context.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
        context.Bind(Techniques::CommonResources()._dssDisable);
        context.Draw((unsigned)valueSources.size());

        uniforms.UnbindShaderResources(context, 1);
    }
}
