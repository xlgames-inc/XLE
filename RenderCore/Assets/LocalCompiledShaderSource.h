// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../ShaderService.h"
#include "../../Assets/IAssetCompiler.h"
#include "../../Utility/Threading/ThreadingUtils.h"
#include "../../Utility/Threading/Mutex.h"
#include <vector>
#include <memory>
#include <mutex>

namespace RenderCore { class DeviceDesc; }

namespace RenderCore { namespace Assets 
{
    class ShaderCacheSet;
    class ShaderCompileMarker;

    class ISourceCodePreprocessor
    {
    public:
        struct SourceCodeWithRemapping
        {
        public:
            struct LineMarker
            {
                std::string _sourceName;
                unsigned _sourceLine;
                unsigned _processedSourceLine;
            };
            std::string _processedSource;
            std::vector<LineMarker> _lineMarkers;
        };

        virtual SourceCodeWithRemapping RunPreprocessor(const char filename[]) = 0;
    };

    class LocalCompiledShaderSource 
        : public ::Assets::IAssetCompiler
        , public ShaderService::IShaderSource
        , public std::enable_shared_from_this<LocalCompiledShaderSource>
    {
    public:
        std::shared_ptr<::Assets::ICompileMarker> PrepareAsset(
            uint64 typeCode, const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount,
            const ::Assets::IntermediateAssets::Store& destinationStore);

        std::shared_ptr<::Assets::CompileFuture> CompileFromFile(
            StringSection<::Assets::ResChar> resId, 
            StringSection<::Assets::ResChar> definesTable) const;
            
        std::shared_ptr<::Assets::CompileFuture> CompileFromMemory(
            StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
            StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable) const;

        void StallOnPendingOperations(bool cancelAll);

        ShaderCacheSet& GetCacheSet() { return *_shaderCacheSet; }

		void AddCompileOperation(const std::shared_ptr<ShaderCompileMarker>& marker);
		void RemoveCompileOperation(ShaderCompileMarker& marker);

        LocalCompiledShaderSource(
            std::shared_ptr<ShaderService::ILowLevelCompiler> compiler,
            std::shared_ptr<ISourceCodePreprocessor> preprocessor,
            const DeviceDesc& devDesc);
        ~LocalCompiledShaderSource();
    protected:
        std::unique_ptr<ShaderCacheSet> _shaderCacheSet;
        std::vector<std::shared_ptr<ShaderCompileMarker>> _activeCompileOperations;
        mutable Interlocked::Value _activeCompileCount;
        Threading::Mutex _activeCompileOperationsLock;
        std::shared_ptr<ShaderService::ILowLevelCompiler> _compiler;
        std::shared_ptr<ISourceCodePreprocessor> _preprocessor;

        class Marker;
    };
}}

