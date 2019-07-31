
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IDevice.h"
#include "../RenderCore/Assets/Services.h"
#include "../RenderCore/MinimalShaderSource.h"
#include "../RenderCore/ShaderService.h"
#include "../Assets/AssetServices.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/AttachablePtr.h"
#include <memory>

namespace UnitTests
{
    class MetalTestHelper
    {
    public:
        RenderCore::CompiledShaderByteCode MakeShader(StringSection<> shader, StringSection<> shaderModel, StringSection<> defines = {});

        ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;
		std::shared_ptr<RenderCore::IDevice> _device;
		std::unique_ptr<RenderCore::ShaderService> _shaderService;
		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;

        MetalTestHelper(RenderCore::UnderlyingAPI api);
        ~MetalTestHelper();
    };
}

