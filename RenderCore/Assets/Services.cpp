// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Services.h"
#include "LocalCompiledShaderSource.h"
#include "MaterialCompiler.h"
#include "PipelineConfigurationUtils.h"
#include "../MinimalShaderSource.h"
#include "../IDevice.h"
#include "../Vulkan/IDeviceVulkan.h"
#include "../ShaderService.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/IntermediateCompilers.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/IntermediateCompilers.h"
#include "../../Assets/CompilerLibrary.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/SerializationUtils.h"

namespace RenderCore { namespace Assets
{
    Services* Services::s_instance = nullptr;

    static std::shared_ptr<RenderCore::ILowLevelCompiler> CreateDefaultShaderCompiler(RenderCore::IDevice& device)
	{
		auto* vulkanDevice  = (RenderCore::IDeviceVulkan*)device.QueryInterface(typeid(RenderCore::IDeviceVulkan).hash_code());
		if (vulkanDevice) {
			// Vulkan allows for multiple ways for compiling shaders. The tests currently use a HLSL to GLSL to SPIRV 
			// cross compilation approach
			RenderCore::VulkanCompilerConfiguration cfg;
			cfg._shaderMode = RenderCore::VulkanShaderMode::HLSLCrossCompiled;
			cfg._legacyBindings = CreateDefaultLegacyRegisterBindingDesc();
		 	return vulkanDevice->CreateShaderCompiler(cfg);
		} else {
			return device.CreateShaderCompiler();
		}
	}

    Services::Services(const std::shared_ptr<RenderCore::IDevice>& device)
    : _device(device)
    {
        _modelCompilersLoaded = false;
        _shaderService = std::make_unique<ShaderService>();

        auto shaderCompiler = CreateDefaultShaderCompiler(*_device);
        auto shaderSource = std::make_shared<MinimalShaderSource>(shaderCompiler);
        _shaderService->SetShaderSource(shaderSource);

        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        _shaderCompilerRegistration = RegisterShaderCompiler(shaderSource, asyncMan.GetIntermediateCompilers())._registrationId;

        // The technique config search directories are used to search for
        // technique configuration files. These are the files that point to
        // shaders used by rendering models. Each material can reference one
        // of these configuration files. But we can add some flexibility to the
        // engine by searching for these files in multiple directories. 
        // _techConfDirs.AddSearchDirectory("xleres/techniques");
    }

    Services::~Services()
    {
            // attempt to flush out all background operations current being performed
        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        asyncMan.GetIntermediateCompilers().StallOnPendingOperations(true);
        asyncMan.GetIntermediateCompilers().DeregisterCompiler(_shaderCompilerRegistration);
        for (const auto&m:_modelCompilers)
            asyncMan.GetIntermediateCompilers().DeregisterCompiler(m);
		ShutdownModelCompilers();
    }

    void Services::InitModelCompilers()
    {
		if (!_modelCompilersLoaded) return;		// (already loaded)
        assert(_modelCompilers.empty());

        auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
		_modelCompilers = ::Assets::DiscoverCompileOperations(compilers, "*Conversion.dll");
        _modelCompilersLoaded = true;
    }

	void Services::ShutdownModelCompilers()
	{
		if (_modelCompilersLoaded) {
			auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
            for (const auto&m:_modelCompilers)
                compilers.DeregisterCompiler(m);
            _modelCompilers.clear();
            _modelCompilersLoaded = false;
		}
	}

    void Services::AttachCurrentModule()
    {
        assert(s_instance==nullptr);
        s_instance = this;
        ShaderService::SetInstance(_shaderService.get());
    }

    void Services::DetachCurrentModule()
    {
        assert(s_instance==this);
        ShaderService::SetInstance(nullptr);
        s_instance = nullptr;
    }

}}

