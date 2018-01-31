// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../UniformsStream.h"
#include "../../Types.h"
#include "../../../Utility/IteratorUtils.h"
#include <vector>

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
        BoundInputLayout(IteratorRange<const MiniInputElementDesc*> layout, const ShaderProgram& program);
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
    private:
    };

}}

