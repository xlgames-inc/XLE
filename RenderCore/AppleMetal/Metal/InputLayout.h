// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../UniformsStream.h"
#include "../../Types.h"
#include "../../../Utility/IteratorUtils.h"
#include "../../../Externals/Misc/OCPtr.h"

@class MTLVertexDescriptor;
@class MTLRenderPipelineReflection;
@protocol MTLRenderPipelineState;

namespace RenderCore { class VertexBufferView; class SharedPkt; }

namespace RenderCore { namespace Metal_AppleMetal
{
    class ShaderProgram;
    class PipelineLayoutConfig;
    class DeviceContext;

    class BoundInputLayout
    {
    public:
        void Apply(DeviceContext& context, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws;

        BoundInputLayout();
        BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& program);

        struct SlotBinding
        {
            IteratorRange<const MiniInputElementDesc*> _elements;
            unsigned _instanceStepDataRate;     // set to 0 for per vertex, otherwise a per-instance rate
        };
        BoundInputLayout(
            IteratorRange<const SlotBinding*> layouts,
            const ShaderProgram& program);

        TBC::OCPtr<MTLVertexDescriptor> _vertexDescriptor;

        bool AllAttributesBound() const { return _allAttributesBound; }
        bool _allAttributesBound = true; // Metal HACK - Metal validation can help determine that bindings are correct
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class DeviceContext;
    using ConstantBufferPacket = SharedPkt;
    class ConstantBuffer;
    class ShaderResourceView;
    class GraphicsPipeline;

    class StreamMapping
    {
    public:
        struct CB { unsigned _uniformStreamSlot; unsigned _shaderSlot; DEBUG_ONLY(std::string _name;) };
        std::vector<CB> _cbs;

        struct SRV { unsigned _uniformStreamSlot; unsigned _shaderSlot; DEBUG_ONLY(std::string _name;) };
        std::vector<SRV> _srvs;
    };

    struct UnboundInterface
    {
        UniformsStreamInterface _interface[4];
    };

    class BoundUniforms
    {
    public:
        void Apply(DeviceContext& context, unsigned streamIdx, const UniformsStream& stream) const;

        // DavidJ -- note -- we can't calculate these correctly in the "unbound" interface case. So we just regard everything as biund
        uint64_t _boundUniformBufferSlots[4];
        uint64_t _boundResourceSlots[4];

        BoundUniforms(
            const ShaderProgram& shader,
            const PipelineLayoutConfig& pipelineLayout,
            const UniformsStreamInterface& interface0 = {},
            const UniformsStreamInterface& interface1 = {},
            const UniformsStreamInterface& interface2 = {},
            const UniformsStreamInterface& interface3 = {});
        BoundUniforms();
        ~BoundUniforms();

        BoundUniforms(const BoundUniforms&) = delete;
        BoundUniforms& operator=(const BoundUniforms&) = delete;

        BoundUniforms(BoundUniforms&& moveFrom) never_throws;
        BoundUniforms& operator=(BoundUniforms&& moveFrom) never_throws;

        static void Apply_UnboundInterfacePath(
            GraphicsPipeline& context,
            MTLRenderPipelineReflection* pipelineReflection,
            const UnboundInterface& unboundInterface,
            unsigned streamIdx,
            const UniformsStream& stream);

    private:
        StreamMapping _preboundInterfaceVS[4];
        StreamMapping _preboundInterfacePS[4];
        std::vector<std::pair<ShaderStage, unsigned>> _unboundCBs;
        std::vector<std::pair<ShaderStage, unsigned>> _unbound2DSRVs;
        std::vector<std::pair<ShaderStage, unsigned>> _unboundCubeSRVs;

        std::shared_ptr<UnboundInterface> _unboundInterface;
    };
}}
