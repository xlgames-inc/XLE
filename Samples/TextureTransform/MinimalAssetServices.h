// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../BufferUploads/IBufferUploads_Forward.h"
#include "../../RenderCore/IDevice.h"

namespace RenderCore { class ShaderService; }

namespace Samples
{
    /// <summary>Startup and manage a minimal set of asset services</summary>
    /// This is similar to RenderCore::Assets::Services, but provides reduced
    /// functionality for special-case command line programs.
    /// This works much better in cases where we don't want to cache the compiled
    /// shaders to disk.
    class MinimalAssetServices
    {
    public:
        static BufferUploads::IManager& GetBufferUploads() { return *s_instance->_bufferUploads; }

        MinimalAssetServices(RenderCore::IDevice* device);
        ~MinimalAssetServices();

        void AttachCurrentModule();
        void DetachCurrentModule();

        MinimalAssetServices(const MinimalAssetServices&) = delete;
        const MinimalAssetServices& operator=(const MinimalAssetServices&) = delete;
    protected:
        std::unique_ptr<BufferUploads::IManager> _bufferUploads;
        std::unique_ptr<RenderCore::ShaderService> _shaderService;
        static MinimalAssetServices* s_instance;
    };
}

