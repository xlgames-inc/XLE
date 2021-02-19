// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Types.h"		// for ShaderStage
#include "Assets/PredefinedDescriptorSetLayout.h"
#include "../Utility/IteratorUtils.h"
#include <vector>

namespace RenderCore 
{
	class MiniInputElementDesc;
	class ConstantBufferView;
	enum class Format;

	class UniformsStream
	{
	public:
		// todo -- is there any way to shift ShaderResourceView down to RenderCore layer?
		IteratorRange<const ConstantBufferView*> _bufferViews = {};
		IteratorRange<const void*const*> _textureViews = {};			// Metal::TextureView
		IteratorRange<const void*const*> _samplers = {};				// Metal::SamplerState

		template<typename Type>
			static IteratorRange<const void*const*> MakeResources(const Type& input)
			{
				return IteratorRange<const void*const*>((const void*const*)input.begin(), (const void*const*)input.end());
			}
	};

	class ConstantBufferElementDesc
	{
	public:
		uint64_t    _semanticHash = 0ull;
		Format      _nativeFormat = Format(0);
		unsigned    _offset = 0u;
		unsigned    _arrayElementCount = 0u;            // set to zero if the element is not actually an array (ie, use std::max(1u, _arrayElementCount) in most cases)
	};

	unsigned CalculateSize(IteratorRange<const ConstantBufferElementDesc*> elements);

	class UniformsStreamInterface
	{
	public:
		struct CBBinding
		{
		public:
			uint64_t _hashName;
			IteratorRange<const ConstantBufferElementDesc*> _elements = {};
		};

		void BindBufferView(unsigned slot, const CBBinding& binding);
		void BindBufferView(unsigned slot, uint64_t hashName);
		void BindTextureView(unsigned slot, uint64_t hashName);
		void BindSampler(unsigned slot, uint64_t hashName);

		uint64_t GetHash() const;

		UniformsStreamInterface();
		~UniformsStreamInterface();

	////////////////////////////////////////////////////////////////////////
		struct RetainedCBBinding
		{
		public:
			uint64_t _hashName = 0ull;
			std::vector<ConstantBufferElementDesc> _elements = {};
		};
		std::vector<RetainedCBBinding> _cbBindings;
		std::vector<uint64_t> _srvBindings;
		std::vector<uint64_t> _samplerBindings;

	private:
		mutable uint64_t _hash;
	};
	
	enum class DescriptorType
	{
		Sampler,
		Texture,
		ConstantBuffer,
		UnorderedAccessTexture,
		UnorderedAccessBuffer,
		Unknown
	};

	const char* AsString(DescriptorType type);
	DescriptorType AsDescriptorType(StringSection<> type);

	struct DescriptorSlot
	{
		DescriptorType _type = DescriptorType::Unknown;
		unsigned _count = 1;
	};

	class DescriptorSetSignature
	{
	public:
		std::vector<DescriptorSlot> _slots;
		uint64_t GetHash() const;		// hash of content, not including name

		DescriptorSetSignature() {}
		DescriptorSetSignature(std::initializer_list<DescriptorSlot> init) : _slots(std::move(init)) {}
	};

	class PipelineLayoutDesc
	{
	public:
		struct DescriptorSetBinding
		{
			std::string _name;
			DescriptorSetSignature _signature;
		};

		struct PushConstantsBinding
		{
			std::string _name;
			unsigned _cbSize = 0;
			ShaderStage _shaderStage;
			std::vector<ConstantBufferElementDesc> _cbElements;
		};

		void AppendDescriptorSet(
			const std::string& name,
			const DescriptorSetSignature& signature);

		void AppendPushConstants(
			const std::string& name,
			IteratorRange<const ConstantBufferElementDesc*> elements,
			ShaderStage shaderStage);

		void AppendPushConstants(
			const std::string& name,
			size_t bufferSize,
			ShaderStage shaderStage);

		IteratorRange<const DescriptorSetBinding*> GetDescriptorSets() const { return MakeIteratorRange(_descriptorSets); }
		IteratorRange<const PushConstantsBinding*> GetPushConstants() const { return MakeIteratorRange(_pushConstants); }

		PipelineLayoutDesc();
		~PipelineLayoutDesc();
	private:
		std::vector<DescriptorSetBinding> _descriptorSets;
		std::vector<PushConstantsBinding> _pushConstants;
	};

	class LegacyRegisterBindingDesc
	{
	public:
		enum class RegisterType { Sampler, ShaderResource, ConstantBuffer, UnorderedAccess, Unknown };
		enum class RegisterQualifier { Texture, Buffer, None };

		struct Entry
		{
			unsigned		_begin = 0, _end = 0;
			uint64_t		_targetDescriptorSetBindingName = 0ull;
			unsigned		_targetDescriptorSetIdx = 0;
			unsigned		_targetBegin = 0, _targetEnd = 0;
		};

		void AppendEntry(
			RegisterType type, RegisterQualifier qualifier,
			const Entry& entry);

		IteratorRange<const Entry*>	GetEntries(RegisterType type, RegisterQualifier qualifier = RegisterQualifier::None) const;

		LegacyRegisterBindingDesc();
		~LegacyRegisterBindingDesc();
	private:
		std::vector<Entry> _samplerRegisters;
		std::vector<Entry> _constantBufferRegisters;
		std::vector<Entry> _srvRegisters;
		std::vector<Entry> _uavRegisters;
		std::vector<Entry> _srvRegisters_boundToBuffer;
		std::vector<Entry> _uavRegisters_boundToBuffer;
	};
}
