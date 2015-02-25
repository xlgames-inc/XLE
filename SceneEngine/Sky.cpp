// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Sky.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "LightDesc.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"

#pragma warning(disable:4702)       // warning C4702: unreachable code

namespace SceneEngine
{
    static void    RenderHalfCubeGeometry(      RenderCore::Metal::DeviceContext* context, 
                                                const SkyTextureParts& parts,
                                                const RenderCore::Metal::ShaderProgram& shader)
    {
        using namespace RenderCore;

        TRY 
        {
            class Vertex
            {
            public:
                Float3      _position;
                Float2      _texCoord;
                Vertex(const Float3& position, const Float2& texCoord) : _position(position), _texCoord(texCoord) {}
            };

            const float scale       = 100.f;
            const float halfScale   =    .5f * scale;
            Vertex vertices[]       = 
            {
                    // ------- face 1 -------
                Vertex(Float3( scale, -scale, 0.f),          Float2(1.f, 0.5f)),
                Vertex(Float3( scale, -scale, halfScale),    Float2(1.f, 1.0f)),
                Vertex(Float3( scale,  scale, 0.f),          Float2(0.f, 0.5f)),
            
                Vertex(Float3( scale,  scale, 0.f),          Float2(0.f, 0.5f)),
                Vertex(Float3( scale, -scale, halfScale),    Float2(1.f, 1.0f)),
                Vertex(Float3( scale,  scale, halfScale),    Float2(0.f, 1.0f)),

                    // ------- face 2 -------
                Vertex(Float3( scale,  scale, 0.f),          Float2(1.f, 0.5f)),
                Vertex(Float3( scale,  scale, halfScale),    Float2(1.f, 0.0f)),
                Vertex(Float3(-scale,  scale, 0.f),          Float2(0.f, 0.5f)),

                Vertex(Float3(-scale,  scale, 0.f),          Float2(0.f, 0.5f)),
                Vertex(Float3( scale,  scale, halfScale),    Float2(1.f, 0.0f)),
                Vertex(Float3(-scale,  scale, halfScale),    Float2(0.f, 0.0f)),

                    // ------- face 3 -------
                Vertex(Float3(-scale,  scale, 0.f),          Float2(1.f, 0.5f)),
                Vertex(Float3(-scale,  scale, halfScale),    Float2(1.f, 1.0f)),
                Vertex(Float3(-scale, -scale, 0.f),          Float2(0.f, 0.5f)),
                
                Vertex(Float3(-scale, -scale, 0.f),          Float2(0.f, 0.5f)),
                Vertex(Float3(-scale,  scale, halfScale),    Float2(1.f, 1.0f)),
                Vertex(Float3(-scale, -scale, halfScale),    Float2(0.f, 1.0f)),

                    // ------- face 4 -------
                Vertex(Float3( scale, -scale, 0.f),          Float2(0.f, 0.5f)),
                Vertex(Float3(-scale, -scale, 0.f),          Float2(1.f, 0.5f)),
                Vertex(Float3( scale, -scale, halfScale),    Float2(0.f, 0.0f)),

                Vertex(Float3( scale, -scale, halfScale),    Float2(0.f, 0.0f)),
                Vertex(Float3(-scale, -scale, 0.f),          Float2(1.f, 0.5f)),
                Vertex(Float3(-scale, -scale, halfScale),    Float2(1.f, 0.0f)),

                    // ------- face 5 -------
                Vertex(Float3(-scale, -scale, halfScale),    Float2(0.f, 0.f)),
                Vertex(Float3(-scale,  scale, halfScale),    Float2(0.f, 1.f)),
                Vertex(Float3( scale, -scale, halfScale),    Float2(1.f, 0.f)),

                Vertex(Float3( scale, -scale, halfScale),    Float2(1.f, 0.f)),
                Vertex(Float3(-scale,  scale, halfScale),    Float2(0.f, 1.f)),
                Vertex(Float3( scale,  scale, halfScale),    Float2(1.f, 1.f))
            };

            Metal::BoundInputLayout inputLayout(Metal::GlobalInputLayouts::PT, shader);
            context->Bind(inputLayout);

            Metal::VertexBuffer temporaryVB(&vertices, sizeof(vertices));
            context->Bind(MakeResourceList(temporaryVB), sizeof(Vertex), 0);

                //  render 2 faces at a time, switching the texture as we go
                //  this would be more efficient if we could combine the shader
                //  resources into 1 texture array... but we don't have support
                //  for that currently.
            context->BindPS(MakeResourceList(parts._faces12->GetShaderResource()));
            context->Draw(6*2);
            context->BindPS(MakeResourceList(parts._faces34->GetShaderResource()));
            context->Draw(6*2, 6*2);
            context->BindPS(MakeResourceList(parts._face5->GetShaderResource()));
            context->Draw(  6, 6*2*2);

        } CATCH(...) {
        } CATCH_END
    }

    enum SkyGeometryType { Plane, HalfCube };
    static SkyGeometryType CurrentSkyGeometryType = Plane; // HalfCube;

    class SkyShaderRes
    {
    public:
        class Desc
        {
        public:
            unsigned        _projectionType;
            bool            _blendFog;
            SkyGeometryType _geoType;
            Desc(unsigned projectionType, bool blendFog, SkyGeometryType geoType) : _projectionType(projectionType), _blendFog(blendFog), _geoType(geoType) {}
        };

        RenderCore::Metal::ShaderProgram* _shader;
        RenderCore::Metal::ShaderProgram* _postFogShader;

        RenderCore::Metal::BoundUniforms _uniforms;
        RenderCore::Metal::BoundUniforms _postfogUniforms;

        SkyShaderRes(const Desc& desc);

        const Assets::DependencyValidation& GetDependencyValidation() const   { return *_validationCallback; }
    private:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };

    SkyShaderRes::SkyShaderRes(const Desc& desc)
    {
        using namespace RenderCore;
        char definesBuffer[128];

        if (desc._geoType == Plane) {
            _snprintf_s(definesBuffer, _TRUNCATE, "MAT_SKY_PROJECTION=%i;BLEND_FOG=%i", desc._projectionType, int(desc._blendFog));
            _shader = &Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector_deep:vs_*",
                "game/xleres/effects/sky.psh:main:ps_*",
                definesBuffer);
        } else {
            assert(desc._geoType == HalfCube);
            _snprintf_s(definesBuffer, _TRUNCATE, "GEO_HAS_TEXCOORD=1;OUTPUT_WORLD_POSITION=1;MAT_SKY_PROJECTION=2;BLEND_FOG=%i", int(desc._blendFog));
            _shader = &Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/effects/sky.psh:vs_main:vs_*",
                "game/xleres/effects/sky.psh:ps_HalfCube:ps_*",
                definesBuffer);
        }

        if (desc._geoType == Plane) {
            // _snprintf_s(definesBuffer, _TRUNCATE, "MAT_SKY_PROJECTION=%i", desc._projectionType);
            // _postFogShader = &Assets::GetAssetDep<Metal::ShaderProgram>(
            //     "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector_deep:vs_*",
            //     "game/xleres_cry/effects/skypostfog.psh:ps_HalfCube_PostFogPass:ps_*",
            //     definesBuffer);
            _postFogShader = nullptr;
        } else {
            assert(desc._geoType == HalfCube);
            _postFogShader = &Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/effects/sky.psh:vs_main:vs_*",
                "game/xleres_cry/effects/skypostfog.psh:ps_HalfCube_PostFogPass:ps_*",
                "GEO_HAS_TEXCOORD=1;OUTPUT_WORLD_POSITION=1;MAT_SKY_PROJECTION=2");
        }

        RenderCore::Metal::BoundUniforms uniforms(*_shader);
        RenderCore::Metal::BoundUniforms postFogUniforms;
        if (_postFogShader) {
            postFogUniforms = RenderCore::Metal::BoundUniforms(*_postFogShader);
        }
        Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
        Techniques::TechniqueContext::BindGlobalUniforms(postFogUniforms);
        postFogUniforms.BindConstantBuffer(Hash64("SkySettings"), 0, 1);

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, &_shader->GetDependencyValidation());
        if (_postFogShader) {
            ::Assets::RegisterAssetDependency(validationCallback, &_postFogShader->GetDependencyValidation());
        }

        _uniforms = std::move(uniforms);
        _postfogUniforms = std::move(postFogUniforms);
        _validationCallback = std::move(validationCallback);
    }

    void    Sky_Render( RenderCore::Metal::DeviceContext* context, 
                        LightingParserContext& parserContext,
                        bool blendFog)
    {
        TRY 
        {
            using namespace RenderCore;
            auto skyTextureName = parserContext.GetSceneParser()->GetGlobalLightingDesc()._skyTexture;
            if (!skyTextureName) return;

            SkyTextureParts textureParts(skyTextureName);

            auto& res = Techniques::FindCachedBoxDep<SkyShaderRes>(SkyShaderRes::Desc(textureParts._projectionType, blendFog, CurrentSkyGeometryType));

            res._uniforms.Apply(*context, parserContext.GetGlobalUniformsStream(), Metal::UniformsStream());
            context->Bind(*res._shader);
            context->Bind(Techniques::CommonResources()._blendOpaque);

            SkyTexture_BindPS(context, parserContext, textureParts, 0);

            if (CurrentSkyGeometryType == Plane) {
                context->Bind(RenderCore::Metal::Topology::TriangleStrip);
                context->Unbind<RenderCore::Metal::VertexBuffer>();
                context->Unbind<RenderCore::Metal::BoundInputLayout>();

                context->Draw(4);
            } else {
                context->Bind(Metal::Topology::TriangleList);
                RenderHalfCubeGeometry(context, textureParts, *res._shader);
            }
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END

        context->Bind(RenderCore::Metal::Topology::TriangleList);
    }

    void    Sky_RenderPostFog(  RenderCore::Metal::DeviceContext* context, 
                                LightingParserContext& parserContext)
    {
        TRY 
        {
            using namespace RenderCore;
            auto skyTextureName = parserContext.GetSceneParser()->GetGlobalLightingDesc()._skyTexture;
            if (!skyTextureName) return;

            SkyTextureParts textureParts(skyTextureName);

            auto& res = Techniques::FindCachedBoxDep<SkyShaderRes>(SkyShaderRes::Desc(textureParts._projectionType, false, CurrentSkyGeometryType));
            if (!res._postFogShader)
                return;

            struct Settings
            {
                Float3 _shadeColorFromSky; float _dummy0;
                Float3 _shadeColorFromSun; float _dummy1;
            } 
            settings = 
            {
                Float3(1.f, 1.f, 1.f), 0.f,
                Float3(1.f, 1.f, 1.f), 0.f,
            };
            Metal::ConstantBuffer settingsConstants(&settings, sizeof(settings));
            const Metal::ConstantBuffer* constants[] = { &settingsConstants };

            res._postfogUniforms.Apply(
                *context, 
                parserContext.GetGlobalUniformsStream(),
                Metal::UniformsStream(nullptr, constants, 1));

            context->Bind(Techniques::CommonResources()._blendStraightAlpha);
            context->Bind(*res._postFogShader);

            if (CurrentSkyGeometryType == Plane) {
                context->Bind(Metal::Topology::TriangleStrip);
                context->Unbind<RenderCore::Metal::VertexBuffer>();
                context->Unbind<RenderCore::Metal::BoundInputLayout>();

                context->Draw(4);
            } else {
                context->Bind(Metal::Topology::TriangleList);
                RenderHalfCubeGeometry(context, textureParts, *res._postFogShader);
            }
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END

        context->Bind(RenderCore::Metal::Topology::TriangleList);
    }

    

    SkyTextureParts::SkyTextureParts(const char skyTextureName[])
    {
        using namespace RenderCore;

        auto* halfCubePart = strstr(skyTextureName, "_XX");
        if (halfCubePart) {
            _projectionType = 1;

                //  This is a half-cube projection style (like Archeage).
                //  We need to extract the names of 3 separate textures by
                //  replacing the "_XX" with "_12", "_34" & "_5"
            char nameBuffer[MaxPath];
            size_t beforePart = halfCubePart-skyTextureName;
            XlCopyNString(nameBuffer, skyTextureName, beforePart);
            XlCopyString(&nameBuffer[beforePart], MaxPath-beforePart, "_12");
            XlCopyString(&nameBuffer[beforePart+3], MaxPath-beforePart-3, &skyTextureName[beforePart+3]);
            _faces12 = &Assets::GetAssetDep<Metal::DeferredShaderResource>(nameBuffer, Metal::DeferredShaderResource::LinearSpace);

            XlCopyString(&nameBuffer[beforePart], MaxPath-beforePart, "_34");
            XlCopyString(&nameBuffer[beforePart+3], MaxPath-beforePart-3, &skyTextureName[beforePart+3]);
            _faces34 = &Assets::GetAssetDep<Metal::DeferredShaderResource>(nameBuffer, Metal::DeferredShaderResource::LinearSpace);

            XlCopyString(&nameBuffer[beforePart], MaxPath-beforePart, "_5");
            XlCopyString(&nameBuffer[beforePart+2], MaxPath-beforePart-2, &skyTextureName[beforePart+3]);
            _face5 = &Assets::GetAssetDep<Metal::DeferredShaderResource>(nameBuffer, Metal::DeferredShaderResource::LinearSpace);
        } else {
            _projectionType = 3;
            _face5 = &Assets::GetAssetDep<Metal::DeferredShaderResource>(skyTextureName, Metal::DeferredShaderResource::LinearSpace);
        }
    }

    unsigned    SkyTexture_BindPS(  RenderCore::Metal::DeviceContext* context, 
                                    LightingParserContext& parserContext,
                                    const SkyTextureParts& parts,
                                    int bindSlot)
    {
        using namespace RenderCore;

        if (parts._projectionType==1) {

            context->BindPS(MakeResourceList(
                bindSlot, parts._faces12->GetShaderResource(), parts._faces34->GetShaderResource(), parts._face5->GetShaderResource()));

            return 1;

        } else {

            context->BindPS(MakeResourceList(
                bindSlot,
                parts._face5->GetShaderResource()));
            return 3;

        }
    }
}


