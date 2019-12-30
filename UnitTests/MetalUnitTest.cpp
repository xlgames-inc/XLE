// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MetalUnitTest.h"
#include "UnitTestHelper.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../Assets/AssetServices.h"
#include "../Assets/IArtifact.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Utility/StringFormat.h"
#include <sstream>

namespace UnitTests
{
    
    RenderCore::CompiledShaderByteCode MetalTestHelper::MakeShader(StringSection<> shader, StringSection<> shaderModel, StringSection<> defines)
    {
        auto future = _shaderSource->CompileFromMemory(shader, "main", shaderModel, defines);
        auto state = future->GetAssetState();
        if (state == ::Assets::AssetState::Invalid) {
            std::stringstream str;
            str << "Shader (" << shader << ") failed to compile. Message follows:" << std::endl;
            str << ::Assets::AsString(::Assets::GetErrorMessage(*future));
            Throw(std::runtime_error(str.str()));
        }
        assert(!future->GetArtifacts().empty());
        return RenderCore::CompiledShaderByteCode {
            future->GetArtifacts()[0].second->GetBlob(),
            future->GetArtifacts()[0].second->GetDependencyValidation(),
            future->GetArtifacts()[0].second->GetRequestParameters()
        };
    }

    MetalTestHelper::MetalTestHelper(RenderCore::UnderlyingAPI api)
    {
        UnitTest_SetWorkingDirectory();
        _globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
        _assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);

        _device = RenderCore::CreateDevice(api);
        // RenderCore::Techniques::SetThreadContext(_device->GetImmediateContext());

        _shaderService = std::make_unique<RenderCore::ShaderService>();
        _shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(_device->CreateShaderCompiler());
        _shaderService->AddShaderSource(_shaderSource);
    }

	MetalTestHelper::MetalTestHelper(const std::shared_ptr<RenderCore::IDevice>& device)
	{
		_device = device;

		_shaderService = std::make_unique<RenderCore::ShaderService>();
        _shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(_device->CreateShaderCompiler());
        _shaderService->AddShaderSource(_shaderSource);
	}

    MetalTestHelper::~MetalTestHelper()
    {
        // RenderCore::Techniques::SetThreadContext(nullptr);
        _shaderSource.reset();
        _shaderService.reset();
        _device.reset();
        _globalServices.reset();
    }

}

