// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetsCore.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/ResourceDesc.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/FrameBuffer.h"
#include "../RenderCore/Metal/Forward.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../Math/Vector.h"

namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace SceneEngine
{
    class SceneTechniqueDesc;

    class LightingResolveShaders
    {
    public:
        class Desc
        {
        public:
            unsigned    _msaaSampleCount;
            bool        _msaaSamplers, _flipDirection;
            unsigned    _gbufferType;
            unsigned    _dynamicLinking;
            bool        _debugging;

            Desc(unsigned gbufferType, unsigned msaaSampleCount, bool msaaSamplers, bool flipDirection, unsigned dynamicLinking, bool debugging)
            {
                    //  we have to "memset" this -- because padding adds
                    //  random values in profile mode
                std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), '\0');
                _gbufferType = gbufferType;
                _msaaSampleCount = msaaSampleCount;
                _msaaSamplers = msaaSamplers;
                _flipDirection = flipDirection;
                _dynamicLinking = dynamicLinking;
                _debugging = debugging;
            }
        };

        using ShaderProgram = RenderCore::Metal::ShaderProgram;
        using BoundUniforms = RenderCore::Metal::BoundUniforms;
        using BoundClassInterfaces = RenderCore::Metal::BoundClassInterfaces;

        enum Shape : uint8 { Directional, Sphere, Tube, Rectangle, Disc };
        enum Shadowing : uint8 { NoShadows, PerspectiveShadows, OrthShadows, OrthShadowsNearCascade, OrthHybridShadows };
            
        class LightShaderType
        {
        public:
            Shape       _shape;
            Shadowing   _shadows;
            uint8       _diffuseModel;
            uint8       _shadowResolveModel;
            bool        _hasScreenSpaceAO;

            unsigned AsIndex() const;
            static unsigned ReservedIndexCount();

            LightShaderType(
                Shape shape, Shadowing shadows, 
                uint8 diffuseModel, uint8 shadowResolveModel, bool hasScreenSpaceAO)
                : _shape(shape), _shadows(shadows), _diffuseModel(diffuseModel), _shadowResolveModel(shadowResolveModel)
                , _hasScreenSpaceAO(hasScreenSpaceAO) {}
            LightShaderType()
                : _shape(Directional), _shadows(NoShadows), _diffuseModel(0), _shadowResolveModel(0), _hasScreenSpaceAO(false) {}
        };

        class LightShader
        {
        public:
            const ShaderProgram*    _shader;

            BoundUniforms           _uniforms;
            BoundClassInterfaces    _boundClassInterfaces;
            bool                    _hasBeenResolved;
            bool                    _dynamicLinking;

            LightShader() : _shader(nullptr), _dynamicLinking(false), _hasBeenResolved(false) {}
        };

        struct CB
        {
            enum 
            {
                ArbitraryShadowProjection,
                LightBuffer,
                ShadowFilteringTable,
                ScreenToShadowProjection,
                OrthogonalShadowProjection,
                ShadowResolveParameters,
                ScreenToRTShadow,
                Debugging,
                Max
            };
        };

        struct SR
        {
            enum
            {
                DMShadow,
                RTShadow_ListHead,
                RTShadow_LinkedLists,
                RTShadow_Triangles,
                Max
            };
        };

        const LightShader* GetShader(const LightShaderType& type);

        const ::Assets::DependencyValidation& GetDependencyValidation() const   { return _validationCallback; }

        LightingResolveShaders(const Desc& desc);
        ~LightingResolveShaders();
    private:
        ::Assets::DependencyValidation _validationCallback;
        std::vector<LightShader> _shaders;
        unsigned _dynamicLinking;

        void BuildShader(const Desc& desc, const LightShaderType& type);
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
            bool _rangeFog;
            unsigned _skyProjectionType;
            bool _hasIBL;
            unsigned _gbufferType;
            bool _referenceShaders;

            Desc(   unsigned gbufferType,
                    unsigned msaaSampleCount, bool msaaSamplers, bool flipDirection,
                    bool hasAO, bool hasTiledLighting, bool hasSSR, 
                    unsigned skyProjectionType, bool hasIBL,
                    bool rangeFog, bool referenceShaders)
            {
                    //  we have to "memset" this -- because padding adds random values in 
                    //  profile mode
                std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), '\0');
                _gbufferType = gbufferType;
                _msaaSampleCount = msaaSampleCount;
                _msaaSamplers = msaaSamplers;
                _flipDirection = flipDirection;
                _hasAO = hasAO; _hasSRR = hasSSR; _hasTiledLighting = hasTiledLighting;
                _skyProjectionType = skyProjectionType;
                _hasIBL = hasIBL;
                _rangeFog = rangeFog;
                _referenceShaders = referenceShaders;
            }
        };

        const RenderCore::Metal::ShaderProgram*             _ambientLight;
        const RenderCore::Metal::ShaderProgram*             _ambientLightRangeFog;
        std::unique_ptr<RenderCore::Metal::BoundUniforms>   _ambientLightUniforms;

        const ::Assets::DependencyValidation& GetDependencyValidation() const   { return _validationCallback; }

        AmbientResolveShaders(const Desc& desc);
        ~AmbientResolveShaders();

    private:
        ::Assets::DependencyValidation  _validationCallback
    };

	class MainTargets;

    #if defined(_DEBUG)
        void SaveGBuffer(RenderCore::Metal::DeviceContext& context, RenderCore::Techniques::ParsingContext& parserContext, const MainTargets& mainTargets);
    #endif

    void Deferred_DrawDebugging(
		RenderCore::Metal::DeviceContext& context, RenderCore::Techniques::ParsingContext& parserContext,
		MainTargets mainTargets,
		bool useMsaaSamplers, unsigned debuggingType);

	void ShadowGen_DrawDebugging(
		RenderCore::Metal::DeviceContext& context, RenderCore::Techniques::ParsingContext& parserContext,
		RenderCore::Metal::ShaderResourceView srv);

}

