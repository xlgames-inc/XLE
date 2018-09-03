// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Sky.h"
#include "SceneParser.h"
#include "LightDesc.h"
#include "MetalStubs.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../RenderCore/Assets/AssetUtils.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Utility/StringUtils.h"

#pragma warning(disable:4702)       // warning C4702: unreachable code

namespace SceneEngine
{
    using namespace RenderCore;

    static void    RenderHalfCubeGeometry(      RenderCore::Metal::DeviceContext& context, 
                                                const SkyTextureParts& parts,
                                                const RenderCore::Metal::ShaderProgram& shader)
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

        Metal::BoundInputLayout inputLayout(GlobalInputLayouts::PT, shader);
        auto temporaryVB = RenderCore::Assets::CreateStaticVertexBuffer(MakeIteratorRange(vertices));
		VertexBufferView vbvs[] = {VertexBufferView{temporaryVB}};
		inputLayout.Apply(context, MakeIteratorRange(vbvs));

            //  render 2 faces at a time, switching the texture as we go
            //  this would be more efficient if we could combine the shader
            //  resources into 1 texture array... but we don't have support
            //  for that currently.
        context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(parts._faces12->GetShaderResource()));
        context.Draw(6*2);
        context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(parts._faces34->GetShaderResource()));
        context.Draw(6*2, 6*2);
        context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(parts._face5->GetShaderResource()));
        context.Draw(  6, 6*2*2);
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

        const RenderCore::Metal::ShaderProgram* _shader;
        const RenderCore::Metal::ShaderProgram* _postFogShader;

        RenderCore::Metal::BoundUniforms _uniforms;
        RenderCore::Metal::BoundUniforms _postfogUniforms;

        SkyShaderRes(const Desc& desc);

        const ::Assets::DepValPtr& GetDependencyValidation() const   { return _validationCallback; }
    private:
        ::Assets::DepValPtr _validationCallback;
    };

    SkyShaderRes::SkyShaderRes(const Desc& desc)
    {
        char definesBuffer[128];

        if (desc._geoType == Plane) {
            _snprintf_s(definesBuffer, _TRUNCATE, "SKY_PROJECTION=%i;BLEND_FOG=%i", desc._projectionType, int(desc._blendFog));
            _shader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
                "xleres/basic2D.vsh:fullscreen_viewfrustumvector_deep:vs_*",
                "xleres/effects/sky.psh:main:ps_*",
                definesBuffer);
        } else {
            assert(desc._geoType == HalfCube);
            _snprintf_s(definesBuffer, _TRUNCATE, "GEO_HAS_TEXCOORD=1;OUTPUT_WORLD_POSITION=1;SKY_PROJECTION=2;BLEND_FOG=%i", int(desc._blendFog));
            _shader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
                "xleres/effects/sky.psh:vs_main:vs_*",
                "xleres/effects/sky.psh:ps_HalfCube:ps_*",
                definesBuffer);
        }

        if (desc._geoType == Plane) {
            // _snprintf_s(definesBuffer, _TRUNCATE, "SKY_PROJECTION=%i", desc._projectionType);
            // _postFogShader = &Assets::GetAssetDep<Metal::ShaderProgram>(
            //     "xleres/basic2D.vsh:fullscreen_viewfrustumvector_deep:vs_*",
            //     "game/xleres_cry/effects/skypostfog.psh:ps_HalfCube_PostFogPass:ps_*",
            //     definesBuffer);
            _postFogShader = nullptr;
        } else {
            assert(desc._geoType == HalfCube);
            _postFogShader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
                "xleres/effects/sky.psh:vs_main:vs_*",
                "game/xleres_cry/effects/skypostfog.psh:ps_HalfCube_PostFogPass:ps_*",
                "GEO_HAS_TEXCOORD=1;OUTPUT_WORLD_POSITION=1;SKY_PROJECTION=2");
        }

		UniformsStreamInterface usi;
		usi.BindConstantBuffer(0, {Hash64("SkySettings")});

        RenderCore::Metal::BoundUniforms uniforms(
			*_shader,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			usi);
        RenderCore::Metal::BoundUniforms postFogUniforms;
        if (_postFogShader) {
            postFogUniforms = RenderCore::Metal::BoundUniforms(
				*_postFogShader,
				Metal::PipelineLayoutConfig{},
				Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
				usi);
        }

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, _shader->GetDependencyValidation());
        if (_postFogShader) {
            ::Assets::RegisterAssetDependency(validationCallback, _postFogShader->GetDependencyValidation());
        }

        _uniforms = std::move(uniforms);
        _postfogUniforms = std::move(postFogUniforms);
        _validationCallback = std::move(validationCallback);
    }

    void    Sky_Render(RenderCore::IThreadContext& threadContext, Techniques::ParsingContext& parserContext, const GlobalLightingDesc& globalLightingDesc, bool blendFog)
    {
		SkyTextureParts textureParts = globalLightingDesc;
		if (!textureParts.IsGood()) return;

		auto& context = *RenderCore::Metal::DeviceContext::Get(threadContext);

        CATCH_ASSETS_BEGIN
            auto& res = ConsoleRig::FindCachedBoxDep2<SkyShaderRes>(textureParts._projectionType, blendFog, CurrentSkyGeometryType);

            struct SkyRenderSettings { float _brightness; unsigned _dummy[3]; } settings = { globalLightingDesc._skyBrightness };
            ConstantBufferView cbvs[] = { MakeSharedPkt(settings) };

            res._uniforms.Apply(context, 0, parserContext.GetGlobalUniformsStream());
			res._uniforms.Apply(context, 1, UniformsStream{MakeIteratorRange(cbvs)});
            context.Bind(*res._shader);
            context.Bind(Techniques::CommonResources()._blendOpaque);

            textureParts.BindPS(context, 0);

            if (CurrentSkyGeometryType == Plane) {
                context.Bind(RenderCore::Topology::TriangleStrip);
                context.UnbindInputLayout();

                context.Draw(4);
            } else {
                context.Bind(Topology::TriangleList);
                RenderHalfCubeGeometry(context, textureParts, *res._shader);
            }
        CATCH_ASSETS_END(parserContext)

        context.Bind(RenderCore::Topology::TriangleList);
    }

    void    Sky_RenderPostFog(  RenderCore::IThreadContext& threadContext, 
                                Techniques::ParsingContext& parserContext,
								const GlobalLightingDesc& globalLightingDesc)
    {
		SkyTextureParts textureParts = globalLightingDesc;
		if (!textureParts.IsGood()) return;

		auto& context = *RenderCore::Metal::DeviceContext::Get(threadContext);

        CATCH_ASSETS_BEGIN
            auto& res = ConsoleRig::FindCachedBoxDep2<SkyShaderRes>(
                textureParts._projectionType, false, CurrentSkyGeometryType);
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
            auto settingsConstants = MakeSharedPkt(settings);
            ConstantBufferView cbvs[] = { settingsConstants };

            res._postfogUniforms.Apply(context, 0, parserContext.GetGlobalUniformsStream());
			res._postfogUniforms.Apply(context, 1, UniformsStream{MakeIteratorRange(cbvs)});

            context.Bind(Techniques::CommonResources()._blendStraightAlpha);
            context.Bind(*res._postFogShader);

            if (CurrentSkyGeometryType == Plane) {
                context.Bind(Topology::TriangleStrip);
                context.UnbindInputLayout();

                context.Draw(4);
            } else {
                context.Bind(Topology::TriangleList);
                RenderHalfCubeGeometry(context, textureParts, *res._postFogShader);
            }
        CATCH_ASSETS_END(parserContext)

        context.Bind(RenderCore::Topology::TriangleList);
    }

    

    SkyTextureParts::SkyTextureParts(
        const ::Assets::ResChar skyTextureName[], 
        GlobalLightingDesc::SkyTextureType resourceType)
    {
        if (skyTextureName && skyTextureName[0]) {
            if (resourceType == GlobalLightingDesc::SkyTextureType::HemiCube) {
                auto* halfCubePart = XlFindString(skyTextureName, "_*");
                if (!halfCubePart)
                    halfCubePart = XlStringEnd(skyTextureName);

                _projectionType = 1;

                    //  This is a half-cube projection style (like Archeage).
                    //  We need to extract the names of 3 separate textures by
                    //  replacing the "_*" with "_12", "_34" & "_5"
                    //
                    // \todo -- ideally these source resources would be collapsed into a single
                    //          texture array
                char nameBuffer[MaxPath];
                size_t beforePart = halfCubePart-skyTextureName;
                XlCopyNString(nameBuffer, skyTextureName, beforePart);
                XlCopyString(&nameBuffer[beforePart], MaxPath-beforePart, "_12");
                if (*halfCubePart)
                    XlCopyString(&nameBuffer[beforePart+3], MaxPath-beforePart-3, &skyTextureName[beforePart+3]);
                _faces12 = &::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>(nameBuffer);

                XlCopyString(&nameBuffer[beforePart], MaxPath-beforePart, "_34");
                if (*halfCubePart)
                    XlCopyString(&nameBuffer[beforePart+3], MaxPath-beforePart-3, &skyTextureName[beforePart+3]);
                _faces34 = &::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>(nameBuffer);

                XlCopyString(&nameBuffer[beforePart], MaxPath-beforePart, "_5");
                if (*halfCubePart)
                    XlCopyString(&nameBuffer[beforePart+2], MaxPath-beforePart-2, &skyTextureName[beforePart+3]);
                _face5 = &::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>(nameBuffer);
            } else {
                switch (resourceType) {
                case GlobalLightingDesc::SkyTextureType::Cube:                  _projectionType = 5; break;
                case GlobalLightingDesc::SkyTextureType::HemiEquirectangular:   _projectionType = 4; break;
                default:
                case GlobalLightingDesc::SkyTextureType::Equirectangular:       _projectionType = 3; break;
                }
                _face5 = &::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>(skyTextureName);
            }
        } else {
            _faces12 = _faces34 = _face5 = nullptr;
            _projectionType = -1;
        }
    }

    SkyTextureParts::SkyTextureParts(const GlobalLightingDesc& globalDesc)
        : SkyTextureParts(globalDesc._skyTexture, globalDesc._skyTextureType) {}

    unsigned    SkyTextureParts::BindPS(  
        RenderCore::Metal::DeviceContext& context, 
        int bindSlot) const
    {
        if (!IsGood()) return ~0u;

        if (_projectionType==1) {
			if (_faces12->TryResolve() != ::Assets::AssetState::Ready
				|| _faces34->TryResolve() != ::Assets::AssetState::Ready
				|| _face5->TryResolve() != ::Assets::AssetState::Ready)
				return ~0u;

            context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(
                bindSlot, _faces12->GetShaderResource(), _faces34->GetShaderResource(), _face5->GetShaderResource()));
        } else {
			if (_face5->TryResolve() != ::Assets::AssetState::Ready)
				return ~0u;

            context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(
                bindSlot,
                _face5->GetShaderResource()));
		}

        return _projectionType;
    }

	unsigned    SkyTextureParts::BindPS_G(  
        RenderCore::Metal::DeviceContext& context, 
        int bindSlot) const
    {
        if (!IsGood()) return ~0u;

        if (_projectionType==1) {
			if (_faces12->TryResolve() != ::Assets::AssetState::Ready
				|| _faces34->TryResolve() != ::Assets::AssetState::Ready
				|| _face5->TryResolve() != ::Assets::AssetState::Ready)
				return ~0u;

            MetalStubs::GetGlobalNumericUniforms(context, ShaderStage::Pixel).Bind(MakeResourceList(
                bindSlot, _faces12->GetShaderResource(), _faces34->GetShaderResource(), _face5->GetShaderResource()));
        } else {
			if (_face5->TryResolve() != ::Assets::AssetState::Ready)
				return ~0u;

            MetalStubs::GetGlobalNumericUniforms(context, ShaderStage::Pixel).Bind(MakeResourceList(
                bindSlot,
                _face5->GetShaderResource()));
		}

        return _projectionType;
    }
}


