// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MinimalAssetServices.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../RenderCore/ShaderService.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/MinimalShaderSource.h"
#include "../../Assets/AssetUtils.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/AttachableInternal.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/MemoryUtils.h"

namespace Samples
{
    using namespace RenderCore;

////////////////////////////////////////////////////////////////////////////////////////////////////

    MinimalAssetServices* MinimalAssetServices::s_instance = nullptr;

    MinimalAssetServices::MinimalAssetServices(RenderCore::IDevice* device)
    {
        _shaderService = std::make_unique<ShaderService>();
        auto shaderSource = std::make_shared<MinimalShaderSource>(Metal::CreateLowLevelShaderCompiler(*device));
        _shaderService->AddShaderSource(shaderSource);

        if (device) {
            BufferUploads::AttachLibrary(ConsoleRig::GlobalServices::GetInstance());
            _bufferUploads = BufferUploads::CreateManager(*device);
        }

        ConsoleRig::GlobalServices::GetCrossModule().Publish(*this);
    }

    MinimalAssetServices::~MinimalAssetServices()
    {
        if (_bufferUploads) {
            _bufferUploads.reset();
            BufferUploads::DetachLibrary();
        }

        ConsoleRig::GlobalServices::GetCrossModule().Withhold(*this);
    }

    void MinimalAssetServices::AttachCurrentModule()
    {
        assert(s_instance==nullptr);
        s_instance = this;
        ShaderService::SetInstance(_shaderService.get());
    }

    void MinimalAssetServices::DetachCurrentModule()
    {
        assert(s_instance==this);
        ShaderService::SetInstance(nullptr);
        s_instance = nullptr;
    }
}

