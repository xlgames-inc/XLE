// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "../../UniformsStream.h"
#include "../../../Utility/IteratorUtils.h"
#include <memory>
#include <vector>

namespace RenderCore
{
	class UniformsStream;
	class UniformsStreamInterface;
	class VertexBufferView;
	class InputElementDesc;
	class MiniInputElementDesc;
	class CompiledShaderByteCode;
}

namespace RenderCore { namespace Metal_Vulkan
{
	class ShaderProgram;
	class DeviceContext;
	class PipelineLayoutConfig
	{
	public:
	};

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class BoundInputLayout
    {
    public:
		void Apply(DeviceContext& context, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws;

        BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const CompiledShaderByteCode& shader);
        BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& shader);
		BoundInputLayout(IteratorRange<const MiniInputElementDesc*> layout, const CompiledShaderByteCode& shader);
        BoundInputLayout(IteratorRange<const MiniInputElementDesc*> layout, const ShaderProgram& shader);
        BoundInputLayout();
        ~BoundInputLayout();

		BoundInputLayout(BoundInputLayout&& moveFrom) never_throws;
		BoundInputLayout& operator=(BoundInputLayout&& moveFrom) never_throws;

        const IteratorRange<const VkVertexInputAttributeDescription*> GetAttributes() const { return MakeIteratorRange(_attributes); }
		const IteratorRange<const VkVertexInputBindingDescription*> GetVBBindings() const { return MakeIteratorRange(_vbBindingDescriptions); }
    private:
        std::vector<VkVertexInputAttributeDescription>	_attributes;
		std::vector<VkVertexInputBindingDescription>	_vbBindingDescriptions;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

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
        BoundUniforms(const BoundUniforms&) = default;
		BoundUniforms& operator=(const BoundUniforms&) = default;
        BoundUniforms(BoundUniforms&&) = default;
        BoundUniforms& operator=(BoundUniforms&&) = default;

    private:
		bool _isComputeShader;
		static const unsigned s_streamCount = 4;
        std::vector<uint32_t> _cbBindingIndices[s_streamCount];
        std::vector<uint32_t> _srvBindingIndices[s_streamCount];
		uint64_t _shaderBindingMask[s_streamCount];
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class BoundClassInterfaces
    {
    public:
        void Bind(uint64_t hashName, unsigned bindingArrayIndex, const char instance[]) {}

        BoundClassInterfaces(const ShaderProgram& shader) {}
        BoundClassInterfaces() {}
        ~BoundClassInterfaces() {}

        BoundClassInterfaces(BoundClassInterfaces&& moveFrom) {}
        BoundClassInterfaces& operator=(BoundClassInterfaces&& moveFrom) { return *this; }
    };

}}

