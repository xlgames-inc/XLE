// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShaderService.h"
#include "../Utility/IteratorUtils.h"
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

		struct Flags
		{
			enum Bits { CompileInBackground = 1<<0 };
			using BitField = unsigned;
		};

        MinimalShaderSource(const std::shared_ptr<ILowLevelCompiler>& compiler, Flags::BitField flags = 0);
        ~MinimalShaderSource();

    protected:
        std::shared_ptr<ILowLevelCompiler> _compiler;
		unsigned _flags;

        std::shared_ptr<::Assets::ArtifactFuture> Compile(
            IteratorRange<const void*> shaderInMemory,
            const ILowLevelCompiler::ResId& resId,
			StringSection<::Assets::ResChar> definesTable) const;
    };
}



