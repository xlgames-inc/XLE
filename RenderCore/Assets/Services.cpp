// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Services.h"
#include "LocalCompiledShaderSource.h"
#include "../Metal/Shader.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/IntermediateResources.h"
#include "../../ConsoleRig/AttachableInternal.h"
#include "../../BufferUploads/IBufferUploads.h"

namespace RenderCore { namespace Assets
{
    Services* Services::s_instance = nullptr;

    Services::Services(RenderCore::IDevice& device)
    {
        _shaderService = std::make_unique<Metal::ShaderService>();
        _shaderService->SetLowLevelCompiler(Metal::CreateLowLevelShaderCompiler());

        auto shaderSource = std::make_shared<LocalCompiledShaderSource>();
        _shaderService->AddShaderSource(shaderSource);

        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        asyncMan.GetIntermediateCompilers().AddCompiler(
            Metal::CompiledShaderByteCode::CompileProcessType, shaderSource);

        BufferUploads::AttachLibrary(ConsoleRig::GlobalServices::GetInstance());
        _bufferUploads = BufferUploads::CreateManager(&device);

        ConsoleRig::GlobalServices::GetCrossModule().Publish(*this);
    }

    Services::~Services()
    {
            // attempt to flush out all background operations current being performed
        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        asyncMan.GetIntermediateCompilers().StallOnPendingOperations(true);

        _bufferUploads.reset();
        BufferUploads::DetachLibrary();
        ConsoleRig::GlobalServices::GetCrossModule().Withhold(*this);
    }

    void Services::AttachCurrentModule()
    {
        assert(s_instance==nullptr);
        s_instance = this;
        Metal::ShaderService::SetInstance(_shaderService.get());
    }

    void Services::DetachCurrentModule()
    {
        assert(s_instance==this);
        Metal::ShaderService::SetInstance(nullptr);
        s_instance = nullptr;
    }

}}

