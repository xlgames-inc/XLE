// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShaderService.h"
#include <memory>

namespace RenderCore
{
    class MinimalShaderSource : public ShaderService::IShaderSource
    {
    public:
        std::shared_ptr<::Assets::ArtifactFuture> CompileFromFile(
			StringSection<::Assets::ResChar> resId, 
			StringSection<::Assets::ResChar> definesTable) const;
            
        std::shared_ptr<::Assets::ArtifactFuture> CompileFromMemory(
			StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
			StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable) const;

		void ClearCaches();

        MinimalShaderSource(std::shared_ptr<ShaderService::ILowLevelCompiler> compiler);
        ~MinimalShaderSource();

    protected:
        std::shared_ptr<ShaderService::ILowLevelCompiler> _compiler;

        std::shared_ptr<::Assets::ArtifactFuture> Compile(
            const void* shaderInMemory, size_t size,
            const ShaderService::ResId& resId,
			StringSection<::Assets::ResChar> definesTable) const;
    };
}



