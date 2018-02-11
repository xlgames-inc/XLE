// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "../../Types.h"
#include "../../ShaderService.h"
#include "../../../Assets/AssetsCore.h"

namespace RenderCore { class CompiledShaderByteCode; class IDevice; }

namespace RenderCore { namespace Metal_Vulkan
{
	class ObjectFactory;
	class BoundClassInterfaces;
	class DeviceContext;

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
						const CompiledShaderByteCode& ps);

		ShaderProgram(	ObjectFactory& factory, 
						const CompiledShaderByteCode& vs,
						const CompiledShaderByteCode& gs,
						const CompiledShaderByteCode& ps,
						const CompiledShaderByteCode& hs,
						const CompiledShaderByteCode& ds);

		ShaderProgram();
        ~ShaderProgram();

		const CompiledShaderByteCode&			GetCompiledCode(ShaderStage stage) const	{ assert(unsigned(stage) < dimof(_compiledCode)); return _compiledCode[(unsigned)stage]; }
		const VulkanSharedPtr<VkShaderModule>&	GetModule(ShaderStage stage) const			{ assert(unsigned(stage) < dimof(_modules)); return _modules[(unsigned)stage]; }

        bool DynamicLinkingEnabled() const;

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

    protected:
		CompiledShaderByteCode _compiledCode[ShaderStage::Max];
		VulkanSharedPtr<VkShaderModule> _modules[ShaderStage::Max];

        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ComputeShader
    {
    public:
		const VulkanSharedPtr<VkShaderModule>&	GetModule() const { return _module; }

        ComputeShader(ObjectFactory& factory, const CompiledShaderByteCode& byteCode);
        ComputeShader();
        ~ComputeShader();

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>		_validationCallback;
		VulkanSharedPtr<VkShaderModule>						_module;
		CompiledShaderByteCode								_compiledCode;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device);
}}