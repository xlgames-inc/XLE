// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Shader.h"
#include "../../Assets/IntermediateResources.h"
#include "../../Utility/Threading/ThreadingUtils.h"
#include <vector>
#include <memory>

namespace RenderCore { namespace Assets 
{
    class ShaderCacheSet;
    class ShaderCompileMarker;

    class LocalCompiledShaderSource 
        : public ::Assets::IntermediateResources::IResourceCompiler
        , public Metal::ShaderService::IShaderSource
        , public std::enable_shared_from_this<LocalCompiledShaderSource>
    {
    public:
        std::shared_ptr<::Assets::PendingCompileMarker> PrepareResource(
            uint64 typeCode, const ::Assets::ResChar* initializers[], unsigned initializerCount,
            const ::Assets::IntermediateResources::Store& destinationStore);

        using IPendingMarker = Metal::ShaderService::IPendingMarker;
        std::shared_ptr<IPendingMarker> CompileFromFile(
            const Metal::ShaderService::ResId& resId, 
            const ::Assets::ResChar definesTable[]) const;
            
        std::shared_ptr<IPendingMarker> CompileFromMemory(
            const char shaderInMemory[], const char entryPoint[], 
            const char shaderModel[], const ::Assets::ResChar definesTable[]) const;

        void StallOnPendingOperations(bool cancelAll) const;

        ShaderCacheSet& GetCacheSet() { return *_shaderCacheSet; }

        LocalCompiledShaderSource();
        ~LocalCompiledShaderSource();
    protected:
        std::unique_ptr<ShaderCacheSet> _shaderCacheSet;
        std::vector<std::shared_ptr<ShaderCompileMarker>> _activeCompileOperations;
        mutable Interlocked::Value _activeCompileCount;
    };
}}

