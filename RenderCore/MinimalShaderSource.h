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
        class PendingMarker : public ShaderService::IPendingMarker
        {
        public:
            const Payload& Resolve(StringSection<::Assets::ResChar> initializer, const ::Assets::DepValPtr& depVal) const; 
            ::Assets::AssetState TryResolve(Payload& result, const ::Assets::DepValPtr& depVal) const;
            Payload GetErrors() const;

            ::Assets::AssetState StallWhilePending() const;
            ShaderStage::Enum GetStage() const;

            PendingMarker(
                Payload payload, 
                std::vector<::Assets::DependentFileState> deps, ShaderStage::Enum stage);
            PendingMarker(Payload errors);
            ~PendingMarker();
            PendingMarker(const PendingMarker&) = delete;
            const PendingMarker& operator=(const PendingMarker&) = delete;
        private:
            Payload _payload;
            std::vector<::Assets::DependentFileState> _deps;
            ShaderStage::Enum _stage;
            Payload _errors;
        };

        using IPendingMarker = ShaderService::IPendingMarker;
        std::shared_ptr<IPendingMarker> CompileFromFile(
            const ::Assets::ResChar resId[], 
            const ::Assets::ResChar definesTable[]) const;
            
        std::shared_ptr<IPendingMarker> CompileFromMemory(
            const char shaderInMemory[], const char entryPoint[], 
            const char shaderModel[], const ::Assets::ResChar definesTable[]) const;

        MinimalShaderSource(std::shared_ptr<ShaderService::ILowLevelCompiler> compiler);
        ~MinimalShaderSource();

    protected:
        std::shared_ptr<ShaderService::ILowLevelCompiler> _compiler;

        std::shared_ptr<IPendingMarker> Compile(
            const void* shaderInMemory, size_t size,
            const ShaderService::ResId& resId,
            const ::Assets::ResChar definesTable[]) const;
    };
}



