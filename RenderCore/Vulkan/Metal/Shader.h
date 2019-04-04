// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "PipelineLayout.h"
#include "../../Types.h"
#include "../../ShaderService.h"
#include "../../../Assets/AssetsCore.h"

namespace RenderCore { class CompiledShaderByteCode; class IDevice; }

namespace RenderCore { namespace Metal_Vulkan
{
	class ObjectFactory;
	class BoundClassInterfaces;
	class GraphicsPipelineBuilder;
	class DescriptorSetSignatureFile;
	class BoundPipelineLayout;
	class LegacyRegisterBinding;

	class PipelineLayoutShaderConfig
	{
	public:
		class DescriptorSet
		{
		public:
			BoundPipelineLayout::DescriptorSet			_bound;
			std::shared_ptr<DescriptorSetSignature>		_signature;
			unsigned									_pipelineLayoutBindingIndex;
			RootSignature::DescriptorSetType			_type;
			unsigned									_uniformStream;
			std::string									_name;
		};
		std::vector<DescriptorSet>					_descriptorSets;
		std::vector<PushConstantsRangeSigniture>	_pushConstants;
		std::shared_ptr<LegacyRegisterBinding>		_legacyRegisterBinding;

		mutable VulkanUniquePtr<VkPipelineLayout>	_cachedPipelineLayout;
		mutable unsigned							_cachedPipelineLayoutId = 0;
		mutable unsigned							_cachedDescriptorSetCount = 0;

		PipelineLayoutShaderConfig();
		PipelineLayoutShaderConfig(ObjectFactory& factory, const DescriptorSetSignatureFile& signatureFile, VkShaderStageFlags stageFlags);
		~PipelineLayoutShaderConfig();

		PipelineLayoutShaderConfig& operator=(PipelineLayoutShaderConfig&& moveFrom) = default;
		PipelineLayoutShaderConfig(PipelineLayoutShaderConfig&& moveFrom) = default;
	};

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ShaderProgram
    {
    public:
        ShaderProgram(	ObjectFactory& factory,
						const CompiledShaderByteCode& vs,
						const CompiledShaderByteCode& ps);
        
        ShaderProgram(	ObjectFactory& factory, 
						const CompiledShaderByteCode& vs,
						const CompiledShaderByteCode& gs,
						const CompiledShaderByteCode& ps,
						StreamOutputInitializers so = {});

		ShaderProgram(	ObjectFactory& factory, 
						const CompiledShaderByteCode& vs,
						const CompiledShaderByteCode& gs,
						const CompiledShaderByteCode& ps,
						const CompiledShaderByteCode& hs,
						const CompiledShaderByteCode& ds,
						StreamOutputInitializers so = {});

		ShaderProgram();
        ~ShaderProgram();

		const CompiledShaderByteCode&				GetCompiledCode(ShaderStage stage) const	{ assert(unsigned(stage) < dimof(_compiledCode)); return _compiledCode[(unsigned)stage]; }
		const VulkanSharedPtr<VkShaderModule>&		GetModule(ShaderStage stage) const			{ assert(unsigned(stage) < dimof(_modules)); return _modules[(unsigned)stage]; }
		static const unsigned s_maxShaderStages = 5;

        bool DynamicLinkingEnabled() const;

		void Apply(GraphicsPipelineBuilder& pipeline) const;
		void Apply(GraphicsPipelineBuilder& pipeline, const BoundClassInterfaces&) const;

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _validationCallback; }

		ShaderProgram(ShaderProgram&&) = default;
        ShaderProgram& operator=(ShaderProgram&&) = default;

		// Legacy asset based API --
		static void ConstructToFuture(
			::Assets::AssetFuture<ShaderProgram>&,
			StringSection<::Assets::ResChar> vsName,
			StringSection<::Assets::ResChar> psName,
			StringSection<::Assets::ResChar> definesTable = {});

		static void ConstructToFuture(
			::Assets::AssetFuture<ShaderProgram>&,
			StringSection<::Assets::ResChar> vsName,
			StringSection<::Assets::ResChar> gsName,
			StringSection<::Assets::ResChar> psName,
			StringSection<::Assets::ResChar> definesTable);

		static void ConstructToFuture(
			::Assets::AssetFuture<ShaderProgram>&,
			StringSection<::Assets::ResChar> vsName,
			StringSection<::Assets::ResChar> gsName,
			StringSection<::Assets::ResChar> psName,
			StringSection<::Assets::ResChar> hsName,
			StringSection<::Assets::ResChar> dsName,
			StringSection<::Assets::ResChar> definesTable);

		std::shared_ptr<PipelineLayoutShaderConfig> _pipelineLayoutHelper;

    protected:
		CompiledShaderByteCode _compiledCode[s_maxShaderStages];
		VulkanSharedPtr<VkShaderModule> _modules[s_maxShaderStages];
		std::shared_ptr<DescriptorSetSignatureFile> _descriptorSetSignatureFile;
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

	using DeepShaderProgram = ShaderProgram;

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ComputeShader
    {
    public:
		const VulkanSharedPtr<VkShaderModule>&	GetModule() const { return _module; }
		const CompiledShaderByteCode& GetCompiledShaderByteCode() const { return _compiledCode; }

        ComputeShader(ObjectFactory& factory, const CompiledShaderByteCode& byteCode);
        ComputeShader();
        ~ComputeShader();

		ComputeShader& operator=(ComputeShader&& moveFrom) = default;
		ComputeShader(ComputeShader&& moveFrom) = default;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     { return _validationCallback; }

		// Legacy asset based API --
		static void ConstructToFuture(
			::Assets::AssetFuture<ComputeShader>&,
			StringSection<::Assets::ResChar> codeName,
			StringSection<::Assets::ResChar> definesTable = {});

		std::shared_ptr<PipelineLayoutShaderConfig> _pipelineLayoutHelper;

    private:
        std::shared_ptr<::Assets::DependencyValidation>		_validationCallback;
		VulkanSharedPtr<VkShaderModule>						_module;
		CompiledShaderByteCode								_compiledCode;
		std::shared_ptr<DescriptorSetSignatureFile>			_descriptorSetSignatureFile;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device);
}}