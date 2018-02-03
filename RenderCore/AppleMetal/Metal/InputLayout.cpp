// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "Shader.h"
#include "Format.h"
#include "TextureView.h"
#include "PipelineLayout.h"
#include "State.h"
#include "Resource.h"
#include "../../Types.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/StringFormat.h"
#include "../../../Utility/PtrUtils.h"
#include "../../../Utility/ArithmeticUtils.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    void BoundInputLayout::Apply(DeviceContext&, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws
    {
        assert(0);
    }

    BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& program)
    {
    }

    BoundInputLayout::BoundInputLayout(IteratorRange<const MiniInputElementDesc*> layout, const ShaderProgram& program)
    {
    }

    BoundInputLayout::BoundInputLayout() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    void BoundUniforms::Apply(
            DeviceContext& context,
            unsigned streamIdx,
            const UniformsStream& stream) const
    {
        assert(0);
    }

    BoundUniforms::BoundUniforms(
        const ShaderProgram& shader,
        const PipelineLayoutConfig& pipelineLayout,
        const UniformsStreamInterface& interface0,
        const UniformsStreamInterface& interface1,
        const UniformsStreamInterface& interface2,
        const UniformsStreamInterface& interface3)
    {}

    BoundUniforms::~BoundUniforms() {}

    BoundUniforms::BoundUniforms() {}

}}

