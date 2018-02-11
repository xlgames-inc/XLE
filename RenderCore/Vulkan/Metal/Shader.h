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

		void Apply(DeviceContext&) const;
		void Apply(DeviceContext&, const BoundClassInterfaces&) const;

		const CompiledShaderByteCode&       GetCompiledCode(ShaderStage stage) const;

        bool DynamicLinkingEnabled() const;

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _validationCallback; }

		ShaderProgram(ShaderProgram&&) never_throws;
        ShaderProgram& operator=(ShaderProgram&&) never_throws;

    protected:
		CompiledShaderByteCode _compiledCode[ShaderStage::Max];
		VulkanSharedPtr<VkShaderModule> _modules[ShaderStage::Max];

        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ComputeShader
    {
    public:
        explicit ComputeShader(const CompiledShaderByteCode& byteCode);
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