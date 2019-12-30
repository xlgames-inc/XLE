// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "Format.h"
#include "Buffer.h"
#include "../../Types.h"
#include "../../RenderUtils.h"
#include "../../UniformsStream.h"
#include "../../ResourceList.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Utility/MiniHeap.h"
#include <memory>
#include <vector>

#include "IncludeDX11.h"		// required for Internal::SetConstantBuffersFn, etc

namespace RenderCore { class VertexBufferView; class CompiledShaderByteCode; }

namespace RenderCore { namespace Metal_DX11
{
	namespace Internal
	{
		typedef void (__stdcall ID3D::DeviceContext::*SetConstantBuffersFn)(unsigned int, unsigned int, ID3D::Buffer *const *);
        typedef void (__stdcall ID3D::DeviceContext::*SetShaderResourcesFn)(unsigned int, unsigned int, ID3D::ShaderResourceView *const *);
		typedef void (__stdcall ID3D::DeviceContext::*SetSamplersFn)(unsigned int, unsigned int, ID3D::SamplerState *const *);
		typedef void (__stdcall ID3D::DeviceContext::*SetUnorderedAccessViewsFn)(unsigned int, unsigned int, ID3D::UnorderedAccessView *const *, const unsigned int *);
	}

    class ShaderProgram;
	class ComputeShader;
	class DeviceContext;
	class PipelineLayoutConfig
	{
	public:
	};

    class BoundInputLayout
    {
    public:
		void Apply(DeviceContext& context, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws;
		bool AllAttributesBound() const { return _allAttributesBound; }

        BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const CompiledShaderByteCode& shader);
        BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& shader);
		
		struct SlotBinding
        {
            IteratorRange<const MiniInputElementDesc*> _elements;
            unsigned _instanceStepDataRate;     // set to 0 for per vertex, otherwise a per-instance rate
        };
		BoundInputLayout(
            IteratorRange<const SlotBinding*> layouts,
            const ShaderProgram& program);

        explicit BoundInputLayout(DeviceContext& context);
		BoundInputLayout();
        ~BoundInputLayout();

		BoundInputLayout(BoundInputLayout&& moveFrom) never_throws;
		BoundInputLayout& operator=(BoundInputLayout&& moveFrom) never_throws;

        typedef ID3D::InputLayout*  UnderlyingType;
        UnderlyingType              GetUnderlying() const { return _underlying.get(); }

    private:
        intrusive_ptr<ID3D::InputLayout>	_underlying;
		std::vector<unsigned>				_vertexStrides;
		bool								_allAttributesBound = true;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class BoundUniforms
    {
    public:
		void Apply(
            DeviceContext& context,
            unsigned streamIdx,
            const UniformsStream& stream) const;

		void UnbindShaderResources(DeviceContext& context, unsigned streamIndex) const;

        uint64_t _boundUniformBufferSlots[4];
        uint64_t _boundResourceSlots[4];
		uint64_t _boundSamplerSlots[4];

        BoundUniforms(
            const ShaderProgram& shader,
            const PipelineLayoutConfig& pipelineLayout,
            const UniformsStreamInterface& interface0 = {},
            const UniformsStreamInterface& interface1 = {},
            const UniformsStreamInterface& interface2 = {},
            const UniformsStreamInterface& interface3 = {});
		BoundUniforms(
            const ComputeShader& shader,
            const PipelineLayoutConfig& pipelineLayout,
            const UniformsStreamInterface& interface0 = {},
            const UniformsStreamInterface& interface1 = {},
            const UniformsStreamInterface& interface2 = {},
            const UniformsStreamInterface& interface3 = {});
        BoundUniforms();
        ~BoundUniforms();

        BoundUniforms(const BoundUniforms& copyFrom) = default;
        BoundUniforms& operator=(const BoundUniforms& copyFrom) = default;
		BoundUniforms(BoundUniforms&& moveFrom) = default;
        BoundUniforms& operator=(BoundUniforms&& moveFrom) = default;
    private:
        class StageBinding
        {
        public:
            class Binding
            {
            public:
                unsigned _shaderSlot;
                unsigned _inputInterfaceSlot;
                mutable Buffer _savedCB;
            };
            std::vector<Binding>    _shaderConstantBindings;
            std::vector<Binding>    _shaderResourceBindings;
			std::vector<Binding>    _shaderSamplerBindings;

			StageBinding() = default;
            StageBinding(StageBinding&& moveFrom) = default;
            StageBinding& operator=(StageBinding&& moveFrom) = default;
			StageBinding(const StageBinding& copyFrom) = default;
			StageBinding& operator=(const StageBinding& copyFrom) = default;
        };

        StageBinding    _stageBindings[ShaderStage::Max];

		class InterfaceResourcesHelper;

		bool BindConstantBuffer(
			const InterfaceResourcesHelper& helper,
			IteratorRange<const intrusive_ptr<ID3D::ShaderReflection>*> reflections,
			uint64_t hashName, unsigned slot, unsigned uniformsStream,
            IteratorRange<const ConstantBufferElementDesc*> elements);
        bool BindShaderResource(
			const InterfaceResourcesHelper& helper,
			IteratorRange<const intrusive_ptr<ID3D::ShaderReflection>*> reflections,
			uint64_t hashName, unsigned slot, unsigned uniformsStream);
		bool BindSampler(
			const InterfaceResourcesHelper& helper,
			IteratorRange<const intrusive_ptr<ID3D::ShaderReflection>*> reflections,
			uint64_t hashName, unsigned slot, unsigned uniformsStream);
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class BoundClassInterfaces
    {
    public:
        void Bind(uint64 hashName, unsigned bindingArrayIndex, const char instance[]);

        const std::vector<intrusive_ptr<ID3D::ClassInstance>>& GetClassInstances(ShaderStage stage) const;

        BoundClassInterfaces(const ShaderProgram& shader);
        BoundClassInterfaces();
        ~BoundClassInterfaces();

        BoundClassInterfaces(BoundClassInterfaces&& moveFrom);
        BoundClassInterfaces& operator=(BoundClassInterfaces&& moveFrom);
    private:
        class StageBinding
        {
        public:
            intrusive_ptr<ID3D::ShaderReflection>   _reflection;
            intrusive_ptr<ID3D::ClassLinkage>       _linkage;
            std::vector<intrusive_ptr<ID3D::ClassInstance>> _classInstanceArray;

            StageBinding();
            ~StageBinding();
            StageBinding(StageBinding&& moveFrom);
            StageBinding& operator=(StageBinding&& moveFrom);
			StageBinding(const StageBinding& copyFrom);
			StageBinding& operator=(const StageBinding& copyFrom);
        };

        StageBinding    _stageBindings[ShaderStage::Max];
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

	class SamplerState;
	class ShaderResourceView;
	class DeviceContext;
	
	/// <summary>Bind uniforms at numeric binding points</summary>
	class NumericUniformsInterface
	{
	public:
		template<int Count> void Bind(const ResourceList<ShaderResourceView, Count>&);
		template<int Count> void Bind(const ResourceList<SamplerState, Count>&);
		template<int Count> void Bind(const ResourceList<ConstantBuffer, Count>&);
		template<int Count> void Bind(const ResourceList<UnorderedAccessView, Count>&);

		void Reset();

		NumericUniformsInterface(DeviceContext& context, ShaderStage shaderStage);
		NumericUniformsInterface();
        ~NumericUniformsInterface();
    protected:
        DeviceContext* _context;
		Internal::SetShaderResourcesFn _setShaderResources;
		Internal::SetSamplersFn _setSamplers;
		Internal::SetConstantBuffersFn _setConstantBuffers;
		Internal::SetUnorderedAccessViewsFn _setUnorderedAccessViews;
		ShaderStage _stage;
    };

	template<int Count, typename Type, typename UnderlyingType>
		void CopyArrayOfUnderlying(UnderlyingType* (&output)[Count], const ResourceList<Type, Count>& input)
		{
			for (unsigned c=0; c<Count; ++c) {
				output[c] = input._buffers[c]->GetUnderlying();
			}
		}

	template<int Count> void NumericUniformsInterface::Bind(const ResourceList<ShaderResourceView, Count>& srvs)
	{
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, srvs);
		for (unsigned c=0; c<Count; ++c)
            _context->_currentSRVs[(unsigned)_stage][srvs._startingPoint+c] = underlyings[c];
		(_context->GetUnderlying()->*_setShaderResources)(srvs._startingPoint, Count, underlyings);
	}

	template<int Count> void NumericUniformsInterface::Bind(const ResourceList<SamplerState, Count>& samplers)
	{
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplers);
		for (unsigned c=0; c<Count; ++c)
            _context->_currentSSs[(unsigned)_stage][samplers._startingPoint+c] = underlyings[c];
		(_context->GetUnderlying()->*_setSamplers)(samplers._startingPoint, Count, underlyings);
	}

	template<int Count> void NumericUniformsInterface::Bind(const ResourceList<ConstantBuffer, Count>& cbs)
	{
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, cbs);
		for (unsigned c=0; c<Count; ++c)
            _context->_currentCBs[(unsigned)_stage][cbs._startingPoint+c] = underlyings[c];
		(_context->GetUnderlying()->*_setConstantBuffers)(cbs._startingPoint, Count, underlyings);
	}

	template<int Count> void NumericUniformsInterface::Bind(const ResourceList<UnorderedAccessView, Count>& uavs)
	{
		ID3D::UnorderedAccessView* underlyings[Count];
		unsigned initialCounts[Count];
		for (unsigned c=0; c<Count; ++c) initialCounts[c] = 0;
		CopyArrayOfUnderlying(underlyings, uavs);
		assert(_setUnorderedAccessViews);
		(_context->GetUnderlying()->*_setUnorderedAccessViews)(uavs._startingPoint, Count, underlyings, initialCounts);
	}


}}

