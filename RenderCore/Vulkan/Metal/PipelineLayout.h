// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanForward.h"
#include "../../../Assets/AssetsCore.h"
#include "../../../Assets/AssetUtils.h"
#include <memory>
#include <vector>
#include <string>

namespace RenderCore { namespace Metal_Vulkan
{
    class ObjectFactory;
    class RootSignature;
	class DescriptorSetSignature;

    class PipelineLayout
    {
    public:
        VkDescriptorSetLayout			GetDescriptorSetLayout(unsigned index);
		const DescriptorSetSignature&	GetDescriptorSetSignature(unsigned index);
		unsigned						GetDescriptorSetCount();

        VkPipelineLayout		GetUnderlying();

        const std::shared_ptr<RootSignature>& GetRootSignature();
        void RebuildLayout(const ObjectFactory& objectFactory);

        PipelineLayout(
            const ObjectFactory& objectFactory,
            StringSection<::Assets::ResChar> rootSignatureCfg,
            VkShaderStageFlags stageFlags);
        ~PipelineLayout();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
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

    class DescriptorSetSignature
    {
    public:
		enum class Type { Adaptive, Numeric, Unknown };
        std::string						_name;
		Type							_type = Type::Unknown;
		unsigned						_uniformStream;
        std::vector<DescriptorType>		_bindings;
    };

	class LegacyRegisterBinding
	{
	public:
		enum class RegisterType { Sampler, ShaderResource, ConstantBuffer, UnorderedAccess, Unknown };
		enum class RegisterQualifier { Texture, Buffer, None };

		struct Entry
		{
			unsigned		_begin = 0, _end = 0;
			unsigned		_targetDescriptorSet = ~0u;
			unsigned		_targetBegin = 0, _targetEnd = 0;
		};
		std::vector<Entry> _samplerRegisters;
		std::vector<Entry> _constantBufferRegisters;
		std::vector<Entry> _srvRegisters;
		std::vector<Entry> _uavRegisters;
		std::vector<Entry> _srvRegisters_boundToBuffer;
		std::vector<Entry> _uavRegisters_boundToBuffer;

		IteratorRange<const Entry*>	GetEntries(RegisterType type, RegisterQualifier qualifier) const;
	};

    class PushConstantsRangeSigniture
    {
    public:
        std::string     _name;
        unsigned        _rangeStart;
        unsigned        _rangeSize;
        unsigned        _stages;
    };
        
    class RootSignature
    {
    public:
        std::vector<DescriptorSetSignature> _descriptorSets;
        std::vector<PushConstantsRangeSigniture> _pushConstantRanges;
		LegacyRegisterBinding _legacyBinding;

        const ::Assets::DependentFileState& GetDependentFileState() const { return _dependentFileState; };
        const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

        RootSignature(StringSection<::Assets::ResChar> filename);
        ~RootSignature();
    private:
        ::Assets::DependentFileState _dependentFileState;
        ::Assets::DepValPtr _depVal;
    };

    VkDescriptorType AsVkDescriptorType(DescriptorType type);
	const char* AsString(DescriptorType type);
	char GetRegisterPrefix(LegacyRegisterBinding::RegisterType regType);
}}

