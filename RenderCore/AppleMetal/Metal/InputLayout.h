// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../UniformsStream.h"
#include "../../Types.h"
#include "../../../Utility/IteratorUtils.h"
#include "../../../Externals/Misc/OCPtr.h"

@class MTLVertexDescriptor;

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

    class BoundUniforms
    {
    public:
        void Apply(
            DeviceContext& context,
            unsigned streamIdx,
            const UniformsStream& stream) const;

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
        BoundUniforms(BoundUniforms&& moveFrom) never_throws;
        BoundUniforms& operator=(BoundUniforms&& moveFrom) never_throws;

    private:
        struct CB { unsigned streamIdx, slot; uint64_t hashName; DEBUG_ONLY(std::string _name;) };
        std::vector<CB> _cbs;
        struct SRV { unsigned streamIdx, slot; uint64_t hashName; DEBUG_ONLY(std::string _name;) };
        std::vector<SRV> _srvs;

        // KenD -- Metal improvement? -- story pointers to MTLFunctions seems a bit invasive or heavy
        TBC::OCPtr<id> _vf; // MTLFunction
        TBC::OCPtr<id> _ff; // MTLFunction
    };
}}
