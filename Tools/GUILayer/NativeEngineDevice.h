// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EngineForward.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include <memory>

namespace RenderCore { namespace Assets { class Services; } }
namespace ToolsRig { class DivergentAssetManager; }
namespace ConsoleRig { class GlobalServices; class CrossModule; }

namespace GUILayer
{
    class NativeEngineDevice
    {
    public:
        const std::shared_ptr<RenderCore::IDevice>&        GetRenderDevice() { return _renderDevice; }
        BufferUploads::IManager*    GetBufferUploads();
        ::Assets::Services*         GetAssetServices() { return _assetServices.get(); }
        void                        AttachDefaultCompilers();
        RenderCore::IThreadContext* GetImmediateContext();
        ConsoleRig::GlobalServices* GetGlobalServices() { return _services.get(); }
		ConsoleRig::CrossModule*	GetCrossModule() { return _crossModule; }
        int                         GetCreationThreadId() { return _creationThreadId; }
        RenderCore::Assets::Services* GetRenderAssetServices() { return _renderAssetsServices.get(); }

        NativeEngineDevice();
        ~NativeEngineDevice();

    protected:
        std::shared_ptr<RenderCore::IDevice> _renderDevice;
        std::shared_ptr<RenderCore::IThreadContext> _immediateContext;
        ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;
        std::unique_ptr<ConsoleRig::Console> _console;
        ConsoleRig::AttachablePtr<RenderCore::Assets::Services> _renderAssetsServices;
        ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _services;
		std::unique_ptr<ToolsRig::DivergentAssetManager> _divAssets;
		ConsoleRig::CrossModule* _crossModule;
        int _creationThreadId;
    };

	class RenderTargetWrapper
	{
	public:
		std::shared_ptr<RenderCore::IResource> _renderTarget;
	};
}
