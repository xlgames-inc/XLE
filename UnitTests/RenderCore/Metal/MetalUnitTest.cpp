// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MetalUnitTest.h"
// #include "UnitTestHelper.h"
#include "../../../RenderCore/Techniques/Techniques.h"
#include "../../../RenderCore/OpenGLES/IDeviceOpenGLES.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/IArtifact.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../Assets/OSFileSystem.h"
#include "../../../Assets/AssetUtils.h"
#include "../../../Utility/StringFormat.h"
#include <sstream>

namespace UnitTests
{
    
    RenderCore::CompiledShaderByteCode MetalTestHelper::MakeShader(StringSection<> shader, StringSection<> shaderModel, StringSection<> defines)
    {
        auto codeBlob = _shaderSource->CompileFromMemory(shader, "main", shaderModel, defines);
        return RenderCore::CompiledShaderByteCode {
            codeBlob._payload,
            ::Assets::AsDepVal(MakeIteratorRange(codeBlob._deps)),
            {}
        };
    }

    MetalTestHelper::MetalTestHelper(RenderCore::UnderlyingAPI api)
    {
        // UnitTest_SetWorkingDirectory();
        // _globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
        // _assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);

        _device = RenderCore::CreateDevice(api);
        // RenderCore::Techniques::SetThreadContext(_device->GetImmediateContext());

        // For GLES, we must initialize the root context to something. Since we're not going to be
        // rendering to window for unit tests, we will never create a PresentationChain (during which the
        // device implicitly initializes the root context in the normal flow)
        auto* glesDevice = (RenderCore::IDeviceOpenGLES*)_device->QueryInterface(typeid(RenderCore::IDeviceOpenGLES).hash_code());
        if (glesDevice)
            glesDevice->InitializeRootContextHeadless();

        _shaderService = std::make_unique<RenderCore::ShaderService>();
        _shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(_device->CreateShaderCompiler());
        _shaderService->SetShaderSource(_shaderSource);
    }

	MetalTestHelper::MetalTestHelper(const std::shared_ptr<RenderCore::IDevice>& device)
	{
		_device = device;

		_shaderService = std::make_unique<RenderCore::ShaderService>();
        _shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(_device->CreateShaderCompiler());
        _shaderService->SetShaderSource(_shaderSource);
	}

    MetalTestHelper::~MetalTestHelper()
    {
        // RenderCore::Techniques::SetThreadContext(nullptr);
        _shaderSource.reset();
        _shaderService.reset();
        _device.reset();
        // _globalServices.reset();
    }

}

