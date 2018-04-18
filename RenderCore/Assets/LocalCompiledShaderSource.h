// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../ShaderService.h"
#include "../../Assets/IAssetCompiler.h"
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
            std::vector<::Assets::DependentFileState> _dependencies;
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

        void ClearCaches();
        
        void StallOnPendingOperations(bool cancelAll);

        ShaderCacheSet& GetCacheSet() { return *_shaderCacheSet; }

        LocalCompiledShaderSource(
            std::shared_ptr<ShaderService::ILowLevelCompiler> compiler,
            std::shared_ptr<ISourceCodePreprocessor> preprocessor,
            const DeviceDesc& devDesc);
        ~LocalCompiledShaderSource();
    protected:
        std::unique_ptr<ShaderCacheSet> _shaderCacheSet;
        std::shared_ptr<ShaderService::ILowLevelCompiler> _compiler;
        std::shared_ptr<ISourceCodePreprocessor> _preprocessor;

        class Marker;
    };
}}

