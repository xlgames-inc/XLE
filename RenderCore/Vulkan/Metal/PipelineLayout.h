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

    class PipelineLayout
    {
    public:
        VkDescriptorSetLayout       GetDescriptorSetLayout(unsigned index);
        unsigned                    GetDescriptorSetCount();
        VkPipelineLayout            GetUnderlying();

        std::shared_ptr<RootSignature> ShareRootSignature();
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

    class DescriptorSetBindingSignature
    {
    public:
        enum class Type 
        { 
            Sampler, 
            Texture,
            TextureAsBuffer,
            ConstantBuffer, 
            UnorderedAccess, 
            UnorderedAccessAsBuffer, 
            Unknown 
        };
        Type        _type;
        unsigned    _hlslBindingIndex;  // this is the binding number as it appears in the HLSL code
    };

    class DescriptorSetSignature
    {
    public:
        std::string             _name;
        std::vector<DescriptorSetBindingSignature>    _bindings;
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

        const ::Assets::DependentFileState& GetDependentFileState() const { return _dependentFileState; };
        const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

        RootSignature(StringSection<::Assets::ResChar> filename);
        ~RootSignature();
    private:
        ::Assets::DependentFileState _dependentFileState;
        ::Assets::DepValPtr _depVal;
    };

    VkDescriptorType AsDescriptorType(DescriptorSetBindingSignature::Type type);
}}

