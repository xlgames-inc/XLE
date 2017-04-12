// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../ShaderService.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Utility/Threading/ThreadingUtils.h"
#include <vector>
#include <memory>
#include <mutex>

namespace RenderCore { class DeviceDesc; }

namespace RenderCore { namespace Assets 
{
    class ShaderCacheSet;
    class ShaderCompileMarker;

    class LocalCompiledShaderSource 
        : public ::Assets::IntermediateAssets::IAssetCompiler
        , public ShaderService::IShaderSource
        , public std::enable_shared_from_this<LocalCompiledShaderSource>
    {
    public:
        std::shared_ptr<::Assets::ICompileMarker> PrepareAsset(
            uint64 typeCode, const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount,
            const ::Assets::IntermediateAssets::Store& destinationStore);

        std::shared_ptr<::Assets::PendingCompileMarker> CompileFromFile(
            StringSection<::Assets::ResChar> resId, 
            StringSection<::Assets::ResChar> definesTable) const;
            
        std::shared_ptr<::Assets::PendingCompileMarker> CompileFromMemory(
            StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
            StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable) const;

        void StallOnPendingOperations(bool cancelAll);

        ShaderCacheSet& GetCacheSet() { return *_shaderCacheSet; }

		void AddCompileOperation(const std::shared_ptr<ShaderCompileMarker>& marker);
		void RemoveCompileOperation(ShaderCompileMarker& marker);

        LocalCompiledShaderSource(std::shared_ptr<ShaderService::ILowLevelCompiler> compiler, const DeviceDesc& devDesc);
        ~LocalCompiledShaderSource();
    protected:
        std::unique_ptr<ShaderCacheSet> _shaderCacheSet;
        std::vector<std::shared_ptr<ShaderCompileMarker>> _activeCompileOperations;
        mutable Interlocked::Value _activeCompileCount;
        Threading::Mutex _activeCompileOperationsLock;
        std::shared_ptr<ShaderService::ILowLevelCompiler> _compiler;

        class Marker;
    };
}}

