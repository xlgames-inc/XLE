// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "SceneEngineUtility.h"

#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/InputLayout.h"

#include "../BufferUploads/IBufferUploads.h"

namespace SceneEngine
{
    class MainTargetsBox
    {
    public:
        class Desc
        {
        public:
            Desc(   unsigned width, unsigned height, 
                    const FormatStack& diffuseFormat, const FormatStack& normalFormat, 
                    const FormatStack& parametersFormat, const FormatStack& depthFormat,
                    const BufferUploads::TextureSamples& sampling);

            unsigned _width, _height;
            FormatStack _diffuseFormat, _normalFormat;
            FormatStack _parametersFormat, _depthFormat;
            BufferUploads::TextureSamples _sampling;
        };

        MainTargetsBox(const Desc& desc);
        ~MainTargetsBox();

        Desc _desc;
        ResourcePtr  _gbufferTextures[3];
        ResourcePtr  _msaaDepthBufferTexture;
        ResourcePtr  _secondaryDepthBufferTexture;

        typedef RenderCore::Metal::RenderTargetView RTV;
        typedef RenderCore::Metal::DepthStencilView DSV;
        typedef RenderCore::Metal::ShaderResourceView SRV;

        RTV _gbufferRTVs[3];

        DSV _msaaDepthBuffer;
        DSV _secondaryDepthBuffer;

        SRV _gbufferRTVsSRV[3];
        SRV _msaaDepthBufferSRV;
        SRV _secondaryDepthBufferSRV;
    };

    class ForwardTargetsBox
    {
    public:
        class Desc
        {
        public:
            Desc(   unsigned width, unsigned height, 
                    const FormatStack& depthFormat,
                    const BufferUploads::TextureSamples& sampling);

            unsigned _width, _height;
            FormatStack _depthFormat;
            BufferUploads::TextureSamples _sampling;
        };

        ForwardTargetsBox(const Desc& desc);
        ~ForwardTargetsBox();

        Desc _desc;
        ResourcePtr  _msaaDepthBufferTexture;
        ResourcePtr  _secondaryDepthBufferTexture;

        typedef RenderCore::Metal::DepthStencilView DSV;
        typedef RenderCore::Metal::ShaderResourceView SRV;

        DSV _msaaDepthBuffer;
        DSV _secondaryDepthBuffer;

        SRV _msaaDepthBufferSRV;
        SRV _secondaryDepthBufferSRV;
    };

    class LightingResolveTextureBox
    {
    public:
        class Desc
        {
        public:
            Desc(   unsigned width, unsigned height, 
                    const FormatStack& lightingResolveFormat,
                    const BufferUploads::TextureSamples& sampling);

            unsigned _width, _height;
            FormatStack _lightingResolveFormat;
            BufferUploads::TextureSamples _sampling;
        };

        typedef RenderCore::Metal::RenderTargetView RTV;
        typedef RenderCore::Metal::ShaderResourceView SRV;

        ResourcePtr     _lightingResolveTexture;
        RTV             _lightingResolveRTV;
        SRV             _lightingResolveSRV;

        ResourcePtr     _lightingResolveCopy;
        SRV             _lightingResolveCopySRV;

        LightingResolveTextureBox(const Desc& desc);
        ~LightingResolveTextureBox();
    };


    class LightingResolveShaders
    {
    public:
        class Desc
        {
        public:
            unsigned _msaaSampleCount;
            bool _msaaSamplers, _flipDirection;
            Desc(unsigned msaaSampleCount, bool msaaSamplers, bool flipDirection) 
            {
                    //  we have to "memset" this -- because padding adds random values in 
                    //  profile mode
                std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), 0);
                _msaaSampleCount = msaaSampleCount;
                _msaaSamplers = msaaSamplers;
                _flipDirection = flipDirection;
            }
        };

        typedef RenderCore::Metal::ShaderProgram ShaderProgram;
        typedef RenderCore::Metal::BoundUniforms BoundUniforms;

        ShaderProgram*  _shadowedDirectionalLight;
        ShaderProgram*  _shadowedDirectionalOrthoLight;
        ShaderProgram*  _shadowedPointLight;
        ShaderProgram*  _unshadowedPointLight;
        ShaderProgram*  _unshadowedDirectionalLight;

        std::unique_ptr<BoundUniforms>  _shadowedDirectionalLightUniforms;
        std::unique_ptr<BoundUniforms>  _shadowedDirectionalOrthoLightUniforms;
        std::unique_ptr<BoundUniforms>  _shadowedPointLightUniforms;
        std::unique_ptr<BoundUniforms>  _unshadowedPointLightUniforms;
        std::unique_ptr<BoundUniforms>  _unshadowedDirectionalLightUniforms;

        const Assets::DependencyValidation& GetDependancyValidation() const   { return *_validationCallback; }

        LightingResolveShaders(const Desc& desc);
        ~LightingResolveShaders();

    private:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };


    class AmbientResolveShaders
    {
    public:
        class Desc
        {
        public:
            unsigned _msaaSampleCount;
            bool _msaaSamplers;
            bool _flipDirection;
            bool _hasAO, _hasTiledLighting, _hasSRR;
            unsigned _skyProjectionType;

            Desc(   unsigned msaaSampleCount, bool msaaSamplers, bool flipDirection,
                    bool hasAO, bool hasTiledLighting, bool hasSSR, unsigned skyProjectionType)
            {
                    //  we have to "memset" this -- because padding adds random values in 
                    //  profile mode
                std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), 0);
                _msaaSampleCount = msaaSampleCount;
                _msaaSamplers = msaaSamplers;
                _flipDirection = flipDirection;
                _hasAO = hasAO; _hasSRR = hasSSR; _hasTiledLighting = hasTiledLighting;
                _skyProjectionType = skyProjectionType;
            }
        };

        RenderCore::Metal::ShaderProgram*   _ambientLight;
        std::unique_ptr<RenderCore::Metal::BoundUniforms>   _ambientLightUniforms;

        const Assets::DependencyValidation& GetDependancyValidation() const   { return *_validationCallback; }

        AmbientResolveShaders(const Desc& desc);
        ~AmbientResolveShaders();

    private:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };


 
    #if defined(_DEBUG)
        void SaveGBuffer(RenderCore::Metal::DeviceContext* context, MainTargetsBox& mainTargets);
    #endif

    class LightingParserContext;
    void Deferred_DrawDebugging(RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, MainTargetsBox& mainTargets);

}

