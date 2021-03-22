// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "ShaderReflection.h"
#include "Shader.h"
#include "Format.h"
#include "PipelineLayout.h"
#include "DeviceContext.h"
#include "Pools.h"
#include "../../Format.h"
#include "../../Types.h"
#include "../../BufferView.h"
#include "../../UniformsStream.h"
#include "../../../OSServices/Log.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../Utility/ArithmeticUtils.h"
#include "../../../Utility/StringFormat.h"
#include <sstream>
#include <map>
#include <set>

#include "IncludeVulkan.h"

namespace RenderCore { namespace Metal_Vulkan
{
	struct ReflectionVariableInformation
	{
		SPIRVReflection::Binding _binding = {};
		SPIRVReflection::StorageType _storageType = SPIRVReflection::StorageType::Unknown;
		DescriptorType _slotType = DescriptorType::ConstantBuffer;
		StringSection<> _name;
	};

	ReflectionVariableInformation GetReflectionVariableInformation(
		const SPIRVReflection& reflection, SPIRVReflection::ObjectId objectId)
	{
		ReflectionVariableInformation result;

		auto n = LowerBound(reflection._names, objectId);
		if (n != reflection._names.end() && n->first == objectId)
			result._name = n->second;

		auto b = LowerBound(reflection._bindings, objectId);
		if (b != reflection._bindings.end() && b->first == objectId)
			result._binding = b->second;

		// Using the type info in reflection, figure out what descriptor slot is associated
		// The spir-v type system is fairly rich, but we don't really need to interpret everything
		// in it. We just need to know enough to figure out the descriptor set slot type.
		// We'll try to be a little flexible to try to avoid having to support all spir-v typing 
		// exhaustively

		auto v = LowerBound(reflection._variables, objectId);
		if (v != reflection._variables.end() && v->first == objectId) {

			result._storageType = v->second._storage;
			auto typeToLookup = v->second._type;

			auto p = LowerBound(reflection._pointerTypes, typeToLookup);
			if (p != reflection._pointerTypes.end() && p->first == typeToLookup)
				typeToLookup = p->second._targetType;

			auto t = LowerBound(reflection._basicTypes, typeToLookup);
			if (t != reflection._basicTypes.end() && t->first == typeToLookup) {
				switch (t->second) {
				case SPIRVReflection::BasicType::SampledImage:
				case SPIRVReflection::BasicType::Image:
					// image types can map onto different input slots, so we may need to be
					// more expressive here
					result._slotType = DescriptorType::Texture;
					break;

				case SPIRVReflection::BasicType::Sampler:
					result._slotType = DescriptorType::Sampler;
					break;

				default:
					#if defined(_DEBUG)
						std::cout << "Could not understand type information for input " << result._name << std::endl;
					#endif
					break;
				}
			} else {
				if (std::find(reflection._structTypes.begin(), reflection._structTypes.end(), typeToLookup) != reflection._structTypes.end()) {
					// a structure will require some kind of buffer as input
					result._slotType = DescriptorType::ConstantBuffer;

					// In this case, the name we're interested in isn't actually the variable
					// name itself, but instead the name of the struct type. As per HLSL, this
					// is the name we use for binding
					auto n = LowerBound(reflection._names, typeToLookup);
					if (n != reflection._names.end() && n->first == typeToLookup)
						result._name = n->second;
				} else {
					#if defined(_DEBUG)
						std::cout << "Could not understand type information for input " << result._name << std::endl;
					#endif
				}
			}
		}

		return result;
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static VkVertexInputRate AsVkVertexInputRate(InputDataRate dataRate)
	{
		switch (dataRate) {
		case InputDataRate::PerVertex: return VK_VERTEX_INPUT_RATE_VERTEX;
		case InputDataRate::PerInstance: return VK_VERTEX_INPUT_RATE_INSTANCE;
		}
	}

	BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const CompiledShaderByteCode& shader)
	{
		// find the vertex inputs into the shader, and match them against the input layout
		auto vertexStrides = CalculateVertexStrides(layout);

		SPIRVReflection reflection(shader.GetByteCode());
		_attributes.reserve(layout.size());

		unsigned inputDataRatePerVB[vertexStrides.size()];
		for (auto&i:inputDataRatePerVB) i = ~0u;

		// Build the VkVertexInputAttributeDescription in the order of the
		// input slots to make it easy to generate the trackingOffset separately
		// for each input
		for (unsigned vbIndex=0; vbIndex<vertexStrides.size(); ++vbIndex) {
			unsigned trackingOffset = 0;
			for (unsigned c=0; c<layout.size(); ++c) {
				const auto& e = layout[c];
				if (e._inputSlot != vbIndex) continue;
				
				auto hash = Hash64(e._semanticName) + e._semanticIndex;
				auto offset = e._alignedByteOffset == ~0x0u ? trackingOffset : e._alignedByteOffset;
				trackingOffset = offset + BitsPerPixel(e._nativeFormat) / 8;

				auto i = LowerBound(reflection._inputInterfaceQuickLookup, hash);
				if (i == reflection._inputInterfaceQuickLookup.end() || i->first != hash)
					continue;   // Could not be bound

				VkVertexInputAttributeDescription desc;
				desc.location = i->second._location;
				desc.binding = e._inputSlot;
				desc.format = (VkFormat)AsVkFormat(e._nativeFormat);
				desc.offset = offset;
				_attributes.push_back(desc);

				if (inputDataRatePerVB[e._inputSlot] != ~0u) {
					// This is a unique restriction for Vulkan -- the data rate is on the vertex buffer
					// binding, not the attribute binding. This means that we can't mix data rates
					// for the same input slot.
					//
					// We could get around this by splitting a single binding into 2 _vbBindingDescriptions
					// (effectively binding the same VB twice, one for each data rate)
					// Then we would also need to remap the vertex buffer assignments when they are applied
					// via vkCmdBindVertexBuffers.
					//
					// However, I think this restriction is actually pretty practical. It probably makes 
					// more sense to just enforce this idea on all gfx-apis. The client can double up their
					// bindings if they really need to; but in practice they probably are already using
					// a separate VB for the per-instance data anyway.
					if (inputDataRatePerVB[e._inputSlot] != (unsigned)AsVkVertexInputRate(e._inputSlotClass))
						Throw(std::runtime_error("In Vulkan, the data rate for all attribute bindings from a given input vertex buffer must be the same. That is, if you want to mix data rates in a draw call, you must use separate vertex buffers for each data rate."));
				} else {
					inputDataRatePerVB[e._inputSlot] = (unsigned)AsVkVertexInputRate(e._inputSlotClass);
				}

				if (e._inputSlotClass == InputDataRate::PerInstance && e._instanceDataStepRate != 0 && e._instanceDataStepRate != 1)
					Throw(std::runtime_error("Instance step data rates other than 1 not supported"));
			}
		}

		_vbBindingDescriptions.reserve(vertexStrides.size());
		for (unsigned b=0; b<(unsigned)vertexStrides.size(); ++b) {
			// inputDataRatePerVB[b] will only be ~0u if there were no successful
			// binds for this bind slot
			if (inputDataRatePerVB[b] == ~0u)
				continue;
			assert(vertexStrides[b] != 0);
			_vbBindingDescriptions.push_back({b, vertexStrides[b], (VkVertexInputRate)inputDataRatePerVB[b]});
		}

		_pipelineRelevantHash = Hash64(AsPointer(_attributes.begin()), AsPointer(_attributes.end()));
		_pipelineRelevantHash = Hash64(AsPointer(_vbBindingDescriptions.begin()), AsPointer(_vbBindingDescriptions.end()), _pipelineRelevantHash);
		CalculateAllAttributesBound(reflection);
	}

	BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& shader)
	: BoundInputLayout(layout, shader.GetCompiledCode(ShaderStage::Vertex))
	{
	}

	BoundInputLayout::BoundInputLayout(
		IteratorRange<const SlotBinding*> layouts,
		const CompiledShaderByteCode& shader)
	{
		SPIRVReflection reflection(shader.GetByteCode());
		_vbBindingDescriptions.reserve(layouts.size());

		for (unsigned slot=0; slot<layouts.size(); ++slot) {
			bool boundAtLeastOne = false;
			uint32_t accumulatingOffset = (uint32_t)0;
			for (unsigned ei=0; ei<layouts[slot]._elements.size(); ++ei) {
				const auto& e = layouts[slot]._elements[ei];
				auto hash = e._semanticHash;

				auto i = LowerBound(reflection._inputInterfaceQuickLookup, hash);
				if (i == reflection._inputInterfaceQuickLookup.end() || i->first != hash) {
					accumulatingOffset += BitsPerPixel(e._nativeFormat) / 8;
					continue;
				}

				VkVertexInputAttributeDescription desc;
				desc.location = i->second._location;
				desc.binding = slot;
				desc.format = (VkFormat)AsVkFormat(e._nativeFormat);
				desc.offset = accumulatingOffset;
				_attributes.push_back(desc);

				accumulatingOffset += BitsPerPixel(e._nativeFormat) / 8;
				boundAtLeastOne = true;
			}

			if (boundAtLeastOne) {
				auto inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
				if (layouts[slot]._instanceStepDataRate != 0)
					inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
				_vbBindingDescriptions.push_back({slot, accumulatingOffset, inputRate});
			}
		}

		_pipelineRelevantHash = Hash64(AsPointer(_attributes.begin()), AsPointer(_attributes.end()));
		_pipelineRelevantHash = Hash64(AsPointer(_vbBindingDescriptions.begin()), AsPointer(_vbBindingDescriptions.end()), _pipelineRelevantHash);
		CalculateAllAttributesBound(reflection);
	}

	void BoundInputLayout::CalculateAllAttributesBound(const SPIRVReflection& reflection)
	{
		_allAttributesBound = true;
		for (const auto&v:reflection._entryPoint._interface) {
			auto reflectionVariable = GetReflectionVariableInformation(reflection, v);
			if (reflectionVariable._storageType != SPIRVReflection::StorageType::Input) continue;
			if (reflectionVariable._binding._location == ~0u) continue;
			auto loc = reflectionVariable._binding._location;

			auto existing = std::find_if(
				_attributes.begin(), _attributes.end(), 
				[loc](const auto& c) { return c.location == loc; });
			_allAttributesBound &= (existing != _attributes.end());
		}
	}

	BoundInputLayout::BoundInputLayout(
		IteratorRange<const SlotBinding*> layouts,
		const ShaderProgram& shader)
	: BoundInputLayout(layouts, shader.GetCompiledCode(ShaderStage::Vertex))
	{
	}

	BoundInputLayout::BoundInputLayout() : _pipelineRelevantHash(0ull), _allAttributesBound(true) {}
	BoundInputLayout::~BoundInputLayout() {}

		////////////////////////////////////////////////////////////////////////////////////////////////

	enum class UniformStreamType { ResourceView, ImmediateData, Sampler, Dummy };

	class BoundUniforms::ConstructionHelper
	{
	public:
		std::map<unsigned, std::pair<unsigned, const RenderCore::DescriptorSetSignature*>> _fixedDescriptorSets;
		const UniformsStreamInterface* _looseUniforms;
		const CompiledPipelineLayout* _pipelineLayout;
			
		std::vector<AdaptiveSetBindingRules> _adaptiveSetRules;
		std::vector<PushConstantBindingRules> _pushConstantsRules;
		std::vector<FixedDescriptorSetBindingRules> _fixedDescriptorSetRules;

		uint64_t _boundLooseUniformBuffers = 0;
		uint64_t _boundLooseResources = 0;
		uint64_t _boundLooseSamplerStates = 0;

		void AddLooseUniformBinding(
			UniformStreamType uniformStreamType,
			unsigned outputDescriptorSet, unsigned outputDescriptorSetSlot,
			unsigned inputUniformStreamIdx, uint32_t shaderStageMask)
		{
			auto adaptiveSet = std::find_if(
				_adaptiveSetRules.begin(), _adaptiveSetRules.end(),
				[outputDescriptorSet](const auto& c) { return c._descriptorSetIdx == outputDescriptorSet; });
			if (adaptiveSet == _adaptiveSetRules.end()) {
				_adaptiveSetRules.push_back(
					AdaptiveSetBindingRules { outputDescriptorSet, shaderStageMask, _pipelineLayout->GetDescriptorSetLayout(outputDescriptorSet) });
				adaptiveSet = _adaptiveSetRules.end()-1;
				auto bindings = _pipelineLayout->GetDescriptorSetLayout(outputDescriptorSet)->GetDescriptorSlots();
				adaptiveSet->_sig = std::vector<DescriptorSlot> { bindings.begin(), bindings.end() };
				adaptiveSet->_shaderUsageMask = (1ull << uint64_t(outputDescriptorSetSlot));
			} else {
				adaptiveSet->_shaderStageMask |= shaderStageMask;
				adaptiveSet->_shaderUsageMask |= (1ull << uint64_t(outputDescriptorSetSlot));
			}

			if (uniformStreamType != UniformStreamType::Dummy) {
				std::vector<LooseUniformBind>* binds;
				if (uniformStreamType == UniformStreamType::ImmediateData) {
					binds = &adaptiveSet->_immediateDataBinds;
					_boundLooseUniformBuffers |= (1ull << uint64_t(inputUniformStreamIdx));
				} else if (uniformStreamType == UniformStreamType::ResourceView) {
					binds = &adaptiveSet->_resourceViewBinds;
					_boundLooseResources |= (1ull << uint64_t(inputUniformStreamIdx));
				} else {
					assert(uniformStreamType == UniformStreamType::Sampler);
					binds = &adaptiveSet->_samplerBinds;
					_boundLooseSamplerStates |= (1ull << uint64_t(inputUniformStreamIdx));
				}

				auto existing = std::find_if(
					binds->begin(), binds->end(),
					[outputDescriptorSetSlot](const auto&c) { return c._descSetSlot == outputDescriptorSetSlot; });
				if (existing != binds->end()) {
					if (existing->_inputUniformStreamIdx != inputUniformStreamIdx)
						Throw(std::runtime_error(""));		// Attempting to bind 2 different inputs a single descriptor set slot
				} else {
					binds->push_back({outputDescriptorSetSlot, inputUniformStreamIdx});
				}
			}
		}

		void BindReflection(const SPIRVReflection& reflection, uint32_t shaderStageMask)
		{
			// We'll need an input value for every binding in the shader reflection
			for (const auto&v:reflection._variables) {
				auto reflectionVariable = GetReflectionVariableInformation(reflection, v.first);
				uint64_t hashName = reflectionVariable._name.IsEmpty() ? 0 : Hash64(reflectionVariable._name.begin(), reflectionVariable._name.end());

				// The _descriptorSet value can be ~0u for push constants, vertex attribute inputs, etc
				if (reflectionVariable._binding._descriptorSet != ~0u) {
					assert(!reflectionVariable._name.IsEmpty());
					auto fixedDescSet = _fixedDescriptorSets.find(reflectionVariable._binding._descriptorSet);
					if (fixedDescSet == _fixedDescriptorSets.end()) {

						// We need to got to the pipeline layout to find the signature for the descriptor set
						if (reflectionVariable._binding._descriptorSet >= _pipelineLayout->GetDescriptorSetCount())
							Throw(std::runtime_error(""));	// variable off pipeline layout

						auto descSetSigBindings = _pipelineLayout->GetDescriptorSetLayout(reflectionVariable._binding._descriptorSet)->GetDescriptorSlots();

						auto srv = std::find(_looseUniforms->_resourceViewBindings.begin(), _looseUniforms->_resourceViewBindings.end(), hashName);
						if (srv != _looseUniforms->_resourceViewBindings.end()) {

							// We matched this shader input to a SRV in the loose uniforms input
							// We also need to find this in the descriptor set.
							// Since this is an adaptive descriptor set, we don't have to worry about the name
							// assigned to the particular slot. We just need to know the that type is what
							// we expect

							/*if (descSetSig._layout->_slots[b.second._bindingPoint]._type != RenderCore::DescriptorSetSignature::SlotType::Texture)
								Throw(std::runtime_error(""));*/

							if (reflectionVariable._binding._bindingPoint >= descSetSigBindings.size() || (descSetSigBindings[reflectionVariable._binding._bindingPoint]._type != DescriptorType::Texture && descSetSigBindings[reflectionVariable._binding._bindingPoint]._type != DescriptorType::UnorderedAccessTexture && descSetSigBindings[reflectionVariable._binding._bindingPoint]._type != DescriptorType::ConstantBuffer && descSetSigBindings[reflectionVariable._binding._bindingPoint]._type != DescriptorType::UnorderedAccessBuffer))
								Throw(std::runtime_error(""));

							auto inputSlot = std::distance(_looseUniforms->_resourceViewBindings.begin(), srv);
							AddLooseUniformBinding(
								UniformStreamType::ResourceView,
								reflectionVariable._binding._descriptorSet, reflectionVariable._binding._bindingPoint,
								inputSlot, shaderStageMask);

						} else {
							auto cb = std::find(_looseUniforms->_immediateDataBindings.begin(), _looseUniforms->_immediateDataBindings.end(), hashName);
							if (cb != _looseUniforms->_immediateDataBindings.end()) {
								if (reflectionVariable._binding._bindingPoint >= descSetSigBindings.size() || (descSetSigBindings[reflectionVariable._binding._bindingPoint]._type != DescriptorType::ConstantBuffer && descSetSigBindings[reflectionVariable._binding._bindingPoint]._type != DescriptorType::UnorderedAccessBuffer))
									Throw(std::runtime_error(""));

								auto inputSlot = std::distance(_looseUniforms->_immediateDataBindings.begin(), cb);
								AddLooseUniformBinding(
									UniformStreamType::ImmediateData,
									reflectionVariable._binding._descriptorSet, reflectionVariable._binding._bindingPoint,
									inputSlot, shaderStageMask);

							} else {
								auto ss = std::find(_looseUniforms->_samplerBindings.begin(), _looseUniforms->_samplerBindings.end(), hashName);
								if (ss != _looseUniforms->_samplerBindings.end()) {
									if (reflectionVariable._binding._bindingPoint >= descSetSigBindings.size() || descSetSigBindings[reflectionVariable._binding._bindingPoint]._type != DescriptorType::Sampler)
										Throw(std::runtime_error(""));

									if (reflectionVariable._slotType != DescriptorType::Sampler)
										Throw(std::runtime_error(""));

									auto inputSlot = std::distance(_looseUniforms->_samplerBindings.begin(), ss);
									AddLooseUniformBinding(
										UniformStreamType::Sampler,
										reflectionVariable._binding._descriptorSet, reflectionVariable._binding._bindingPoint,
										inputSlot, shaderStageMask);

								} else {
									// no binding found -- just mark it as an input variable we need, it will get filled in with a default binding
									AddLooseUniformBinding(
										UniformStreamType::Dummy,
										reflectionVariable._binding._descriptorSet, reflectionVariable._binding._bindingPoint,
										~0u, shaderStageMask);
								}
							}
						}
					} else {

						// There is a fixed descriptor set assigned that covers this input
						// Compare the slot within the fixed descriptor set to what the shader wants as input

						if (fixedDescSet->second.second) {
							if (reflectionVariable._binding._bindingPoint >= fixedDescSet->second.second->_slots.size())
								Throw(std::runtime_error(""));
							
							auto& descSetSlot = fixedDescSet->second.second->_slots[reflectionVariable._binding._bindingPoint];
							if (reflectionVariable._slotType != descSetSlot._type)
								Throw(std::runtime_error(""));		// types should agree
						}

						auto inputSlot = fixedDescSet->second.first;
						auto existing = std::find_if(
							_fixedDescriptorSetRules.begin(), _fixedDescriptorSetRules.end(),
							[inputSlot](const auto& c) { return c._inputSlot == inputSlot; });
						if (existing != _fixedDescriptorSetRules.end()) {
							if (existing->_outputSlot != reflectionVariable._binding._descriptorSet)
								Throw(std::runtime_error(""));		// attempting the bind the same fixed descriptor set to multiple output slots
							existing->_shaderStageMask |= shaderStageMask;
						} else {
							_fixedDescriptorSetRules.push_back(
								FixedDescriptorSetBindingRules {
									inputSlot, reflectionVariable._binding._descriptorSet, shaderStageMask
								});
						}
					}
				} else if (reflectionVariable._storageType == SPIRVReflection::StorageType::PushConstant) {

					assert(!reflectionVariable._name.IsEmpty());
					unsigned pipelineLayoutIdx = 0;
					for (; pipelineLayoutIdx<_pipelineLayout->GetPushConstantsBindingNames().size(); ++pipelineLayoutIdx) {
						if (_pipelineLayout->GetPushConstantsBindingNames()[pipelineLayoutIdx] != hashName) continue;
						if ((_pipelineLayout->GetPushConstantsRange(pipelineLayoutIdx).stageFlags & shaderStageMask) != shaderStageMask) continue;
						break;
					}
					if (pipelineLayoutIdx >= _pipelineLayout->GetPushConstantsBindingNames().size())
						Throw(std::runtime_error(""));		// can't find this push constants in the pipeline layout (for the name and shader stage configuration)

					// push constants must from the "loose uniforms" -- we can't extract them
					// from a prebuilt descriptor set. Furthermore, they must be a "immediateData"
					// type of input

					auto cb = std::find(_looseUniforms->_immediateDataBindings.begin(), _looseUniforms->_immediateDataBindings.end(), hashName);
					if (cb != _looseUniforms->_immediateDataBindings.end()) {

						auto existing = std::find_if(
							_pushConstantsRules.begin(), _pushConstantsRules.end(),
							[shaderStageMask](const auto& c) { return c._shaderStageBind == shaderStageMask; });
						if (existing != _pushConstantsRules.end())
							Throw(std::runtime_error(""));		// we can only have one push constants per shader stage
						auto inputSlot = (unsigned)std::distance(_looseUniforms->_immediateDataBindings.begin(), cb);
						auto& pipelineRange = _pipelineLayout->GetPushConstantsRange(pipelineLayoutIdx);
						_pushConstantsRules.push_back({shaderStageMask, pipelineRange.offset, pipelineRange.size, inputSlot});

					} else {
						Throw(std::runtime_error(""));		// missing push constants input
					}
				}
			}
		}
	};

	void BoundUniforms::UnbindLooseUniforms(DeviceContext& context, GraphicsEncoder& encoder)
	{
		assert(0);		// todo -- unimplemented
	}

	namespace Internal
	{
		VkShaderStageFlags_ AsVkShaderStageFlags(ShaderStage input)
		{
			switch (input) {
			case ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
			case ShaderStage::Pixel: return VK_SHADER_STAGE_FRAGMENT_BIT;
			case ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
			case ShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;

			case ShaderStage::Hull:
			case ShaderStage::Domain:
				// VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
				// not supported on Vulkan yet
				assert(0);
				return 0;

			case ShaderStage::Null:
			default:
				return 0;
			}
		}
	}

	BoundUniforms::BoundUniforms(
		const ShaderProgram& shader,
		const UniformsStreamInterface& interf)
	{
		_pipelineType = PipelineType::Graphics;

		// We need to map on the input descriptor set bindings to the slots understood
		// by the shader's pipeline layout
		auto& pipelineLayout = shader.GetPipelineLayout();
		ConstructionHelper helper;
		helper._looseUniforms = &interf;
		helper._pipelineLayout = &pipelineLayout;
		
		for (unsigned dIdx=0; dIdx<interf._fixedDescriptorSetBindings.size(); ++dIdx) {
			const auto& d = interf._fixedDescriptorSetBindings[dIdx];
			bool foundMapping = false;
			for (unsigned c=0; c<pipelineLayout.GetDescriptorSetCount(); ++c) {
				auto hashName = pipelineLayout.GetDescriptorSetBindingNames()[c];
				if (hashName == d._hashName) {
					// todo -- we should check compatibility between the given descriptor set and the pipeline layout
					helper._fixedDescriptorSets.insert({c, std::make_pair(d._inputSlot, d._signature)});
					foundMapping = true;
					break;
				}
			}
			#if defined(_DEBUG)
				if (!foundMapping) {
					std::cout << "Could not find descriptor set in pipeline layout (hash code: " << std::hex << d._hashName << std::dec << "). Ignoring" << std::endl;
				}
			#endif
		}

		for (unsigned stage=0; stage<ShaderProgram::s_maxShaderStages; ++stage) {
			const auto& compiledCode = shader.GetCompiledCode((ShaderStage)stage);
			if (compiledCode.GetByteCode().size()) {
				helper.BindReflection(SPIRVReflection(compiledCode.GetByteCode()), Internal::AsVkShaderStageFlags((ShaderStage)stage));
			}
		}

		_adaptiveSetRules = std::move(helper._adaptiveSetRules);
		_fixedDescriptorSetRules = std::move(helper._fixedDescriptorSetRules);
		_pushConstantsRules = std::move(helper._pushConstantsRules);
		_boundLooseUniformBuffers = helper._boundLooseUniformBuffers;
		_boundLooseResources = helper._boundLooseResources;
		_boundLooseSamplerStates = helper._boundLooseSamplerStates;
	}

	BoundUniforms::BoundUniforms(
		const GraphicsPipeline& pipeline,
		const UniformsStreamInterface& interf)
	: BoundUniforms(pipeline._shader, interf) {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class BoundUniforms::BindingHelper
	{
	public:
		static uint64_t WriteImmediateDataBindings(
			ProgressiveDescriptorSetBuilder& builder,
			TemporaryBufferSpace& temporaryBufferSpace,
			bool& requiresTemporaryBufferBarrier,
			ObjectFactory& factory,
			IteratorRange<const UniformsStream::ImmediateData*> pkts,
			IteratorRange<const LooseUniformBind*> bindingIndicies)
		{
			uint64_t bindingsWrittenTo = 0u;

			for (auto bind:bindingIndicies) {
				assert(!(bindingsWrittenTo & (1ull<<uint64_t(bind._descSetSlot))));
				
				auto& pkt = pkts[bind._inputUniformStreamIdx];
				// We must either allocate some memory from a temporary pool, or 
				// (or we could use push constants)
				auto tempSpace = temporaryBufferSpace.AllocateBuffer(pkt);
				if (!tempSpace.buffer) {
					Log(Warning) << "Failed to allocate temporary buffer space. Falling back to new buffer." << std::endl;
					Resource cb{
						factory, 
						CreateDesc(BindFlag::ConstantBuffer, 0, GPUAccess::Read, LinearBufferDesc::Create(unsigned(pkt.size())), "overflow-buf"), 
						SubResourceInitData{pkt}};
					builder.Bind(bind._descSetSlot, { cb.GetBuffer(), 0, VK_WHOLE_SIZE } VULKAN_VERBOSE_DEBUG_ONLY(, "temporary buffer"));
				} else {
					builder.Bind(bind._descSetSlot, tempSpace VULKAN_VERBOSE_DEBUG_ONLY(, "temporary buffer"));
					requiresTemporaryBufferBarrier |= true;
				}

				bindingsWrittenTo |= (1ull << uint64_t(bind._descSetSlot));
			}

			return bindingsWrittenTo;
		}

		static uint64_t WriteResourceViewBindings(
			ProgressiveDescriptorSetBuilder& builder,
			IteratorRange<const IResourceView*const*> srvs,
			IteratorRange<const LooseUniformBind*> bindingIndicies)
		{
			uint64_t bindingsWrittenTo = 0u;

			for (auto bind:bindingIndicies) {
				assert(bind._inputUniformStreamIdx < srvs.size());
				auto* srv = srvs[bind._inputUniformStreamIdx];

				assert(!(bindingsWrittenTo & (1ull<<uint64_t(bind._descSetSlot))));

				builder.Bind(bind._descSetSlot, *checked_cast<const ResourceView*>(srv));

				bindingsWrittenTo |= (1ull << uint64_t(bind._descSetSlot));
			}

			return bindingsWrittenTo;
		}

		static uint64_t WriteSamplerStateBindings(
			ProgressiveDescriptorSetBuilder& builder,
			IteratorRange<const SamplerState*const*> samplerStates,
			IteratorRange<const LooseUniformBind*> bindingIndicies)
		{
			uint64_t bindingsWrittenTo = 0u;

			for (auto bind:bindingIndicies) {
				assert(bind._inputUniformStreamIdx < samplerStates.size());
				auto& samplerState = samplerStates[bind._inputUniformStreamIdx];

				assert(!(bindingsWrittenTo & (1ull<<uint64_t(bind._descSetSlot))));

				builder.Bind(bind._descSetSlot, samplerState->GetUnderlying());

				bindingsWrittenTo |= (1ull << uint64_t(bind._descSetSlot));
			}

			return bindingsWrittenTo;
		}
	};

	static std::string s_looseUniforms = "loose-uniforms";

	void BoundUniforms::ApplyLooseUniforms(
		DeviceContext& context,
		GraphicsEncoder& encoder,
		const UniformsStream& stream) const
	{
		for (const auto& adaptiveSet:_adaptiveSetRules) {

			// Descriptor sets can't be written to again after they've been bound to a command buffer (unless we're
			// sure that all of the commands have already been completed).
			//
			// So, in effect writing a new descriptor set will always be a allocate operation. We may have a pool
			// of prebuild sets that we can reuse; or we can just allocate and free every time.
			//
			// Because each uniform stream can be set independantly, and at different rates, we'll use a separate
			// descriptor set for each uniform stream. 
			//
			// In this call, we could attempt to reuse another descriptor set that was created from exactly the same
			// inputs and already used earlier this frame...? But that may not be worth it. It seems like it will
			// make more sense to just create and set a full descriptor set for every call to this function.

			auto& globalPools = context.GetGlobalPools();
			auto descriptorSet = globalPools._mainDescriptorPool.Allocate(adaptiveSet._layout->GetUnderlying());
			#if defined(VULKAN_VERBOSE_DEBUG)
				DescriptorSetDebugInfo verboseDescription;
				verboseDescription._descriptorSetInfo = s_looseUniforms;
			#endif

			bool requiresTemporaryBufferBarrier = false;

			// -------- write descriptor set --------
			ProgressiveDescriptorSetBuilder builder { MakeIteratorRange(adaptiveSet._sig) };
			auto cbBindingFlag = BindingHelper::WriteImmediateDataBindings(
				builder,
				context.GetTemporaryBufferSpace(),
				requiresTemporaryBufferBarrier,
				context.GetFactory(),
				stream._immediateData,
				MakeIteratorRange(adaptiveSet._immediateDataBinds));

			auto srvBindingFlag = BindingHelper::WriteResourceViewBindings(
				builder,
				stream._resourceViews,
				MakeIteratorRange(adaptiveSet._resourceViewBinds));

			auto ssBindingFlag = BindingHelper::WriteSamplerStateBindings(
				builder,
				MakeIteratorRange((const SamplerState*const*)stream._samplers.begin(), (const SamplerState*const*)stream._samplers.end()),
				MakeIteratorRange(adaptiveSet._samplerBinds));

			// Any locations referenced by the descriptor layout, by not written by the values in
			// the streams must now be filled in with the defaults.
			// Vulkan doesn't seem to have well defined behaviour for descriptor set entries that
			// are part of the layout, but never written.
			// We can do this with "write" operations, or with "copy" operations. It seems like copy
			// might be inefficient on many platforms, so we'll prefer "write"
			//
			// In the most common case, there should be no dummy descriptors to fill in here... So we'll 
			// optimise for that case.
			uint64_t dummyDescWriteMask = (~(cbBindingFlag|srvBindingFlag|ssBindingFlag)) & adaptiveSet._shaderUsageMask;
			uint64_t dummyDescWritten = 0;
			if (dummyDescWriteMask != 0)
				dummyDescWritten = builder.BindDummyDescriptors(context.GetGlobalPools(), dummyDescWriteMask);

			// note --  vkUpdateDescriptorSets happens immediately, regardless of command list progress.
			//          Ideally we don't really want to have to update these constantly... Once they are 
			//          set, maybe we can just reuse them?
			if (cbBindingFlag | srvBindingFlag | ssBindingFlag | dummyDescWriteMask) {
				std::vector<uint64_t> resourceVisibilityList;
				builder.FlushChanges(context.GetUnderlyingDevice(), descriptorSet.get(), nullptr, 0, resourceVisibilityList VULKAN_VERBOSE_DEBUG_ONLY(, verboseDescription));
				context.RequireResourceVisbility(MakeIteratorRange(resourceVisibilityList));
			}
		
			encoder.BindDescriptorSet(
				adaptiveSet._descriptorSetIdx, descriptorSet.get()
				VULKAN_VERBOSE_DEBUG_ONLY(, std::move(verboseDescription)));

			if (requiresTemporaryBufferBarrier)
				context.GetTemporaryBufferSpace().WriteBarrier(context);
		}

		for (const auto&pushConstants:_pushConstantsRules) {
			auto cb = stream._immediateData[pushConstants._inputCBSlot];
			assert(cb.size() == pushConstants._size);
			encoder.PushConstants(pushConstants._shaderStageBind, pushConstants._offset, cb);
		}
	}

	void BoundUniforms::ApplyDescriptorSets(
		DeviceContext& context,
		GraphicsEncoder& encoder,
		IteratorRange<const IDescriptorSet* const*> descriptorSets)
	{
		for (const auto& fixedSet:_fixedDescriptorSetRules) {
			auto* descSet = checked_cast<const CompiledDescriptorSet*>(descriptorSets[fixedSet._inputSlot]);
			encoder.BindDescriptorSet(
				fixedSet._outputSlot, descSet->GetUnderlying()
				VULKAN_VERBOSE_DEBUG_ONLY(, DescriptorSetDebugInfo{descSet->GetDescription()} ));
		}
	}

	BoundUniforms::BoundUniforms() 
	{
		_pipelineType = PipelineType::Graphics;
	}
	BoundUniforms::~BoundUniforms() {}

}}

