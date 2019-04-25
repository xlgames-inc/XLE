// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Services.h"
#include "LocalCompiledShaderSource.h"
#include "MaterialCompiler.h"
#include "MaterialScaffold.h"   // just for MaterialScaffold::CompileProcessType
#include "SkinDeformer.h"
#include "ShaderPatchCollection.h"
#include "../IDevice.h"
#include "../Init.h"
#include "../ShaderService.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/GeneralCompiler.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include "../../BufferUploads/IBufferUploads.h"

namespace RenderCore { namespace Assets
{
    Services* Services::s_instance = nullptr;

    Services::Services(const std::shared_ptr<RenderCore::IDevice>& device)
    : _device(device)
    {
        _shaderService = std::make_unique<ShaderService>();

        auto shaderSource = std::make_shared<LocalCompiledShaderSource>(
            device->CreateShaderCompiler(),
			nullptr,
            device->GetDesc());
        _shaderService->AddShaderSource(shaderSource);

        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        asyncMan.GetIntermediateCompilers().AddCompiler(shaderSource);

        if (device) {
            BufferUploads::AttachLibrary(ConsoleRig::CrossModule::GetInstance());
            _bufferUploads = BufferUploads::CreateManager(*device);
        }

        // The technique config search directories are used to search for
        // technique configuration files. These are the files that point to
        // shaders used by rendering models. Each material can reference one
        // of these configuration files. But we can add some flexibility to the
        // engine by searching for these files in multiple directories. 
        _techConfDirs.AddSearchDirectory("xleres/techniques");

            // Setup required compilers.
            //  * material scaffold compiler
        asyncMan.GetIntermediateCompilers().AddCompiler(std::make_shared<RenderCore::Assets::MaterialScaffoldCompiler>());

		_deformOpsFactory = std::make_unique<RenderCore::Assets::DeformOperationFactory>();
		RenderCore::Assets::SkinDeformer::Register();

		_compiledShaderPatchCollectionRegistry = std::make_unique<ShaderPatchCollectionRegistry>();
    }

    Services::~Services()
    {
            // attempt to flush out all background operations current being performed
        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        asyncMan.GetIntermediateCompilers().StallOnPendingOperations(true);
		ShutdownModelCompilers();

        if (_bufferUploads) {
            _bufferUploads.reset();
            BufferUploads::DetachLibrary();
        }
    }

    void Services::InitModelCompilers()
    {
		if (_modelCompilers) return;		// (already loaded)

            // attach the collada compilers to the assert services
            // this is optional -- not all applications will need these compilers
		auto compileOps = ::Assets::DiscoverCompileOperations("*Conversion.dll");
		if (compileOps.empty()) return;

		_modelCompilers = std::make_shared<::Assets::GeneralCompiler>(
			MakeIteratorRange(compileOps),
			::Assets::Services::GetAsyncMan().GetIntermediateStore());
		::Assets::Services::GetAsyncMan().GetIntermediateCompilers().AddCompiler(_modelCompilers);
    }

	void Services::ShutdownModelCompilers()
	{
		if (_modelCompilers) {
			::Assets::Services::GetAsyncMan().GetIntermediateCompilers().RemoveCompiler(*_modelCompilers);
			_modelCompilers.reset();
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

