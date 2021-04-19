// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicSceneParser.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetFutureContinuation.h"
#include "../../Math/Transformations.h"
#include "../../Math/MathSerialization.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/Meta/AccessorSerialize.h"

#include "../../SceneEngine/LightingParserStandardPlugin.h"     // (for stubbing out)

namespace PlatformRig
{
    using namespace SceneEngine;

    unsigned BasicLightingParserDelegate::GetShadowProjectionCount() const 
    { 
        return (unsigned)GetEnvSettings()._shadowProj.size(); 
    }

    auto BasicLightingParserDelegate::GetShadowProjectionDesc(
        unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
        -> ShadowProjectionDesc
    {
        return PlatformRig::CalculateDefaultShadowCascades(
            GetEnvSettings()._shadowProj[index]._light, 
            GetEnvSettings()._shadowProj[index]._lightId,
            mainSceneProjectionDesc,
            GetEnvSettings()._shadowProj[index]._shadowFrustumSettings);
    }

    unsigned BasicLightingParserDelegate::GetLightCount() const 
    { 
        return (unsigned)GetEnvSettings()._lights.size(); 
    }

    auto BasicLightingParserDelegate::GetLightDesc(unsigned index) const -> const LightDesc&
    {
        return GetEnvSettings()._lights[index];
    }

    auto BasicLightingParserDelegate::GetGlobalLightingDesc() const -> GlobalLightingDesc
    {
        return GetEnvSettings()._globalLightingDesc;
    }

    ToneMapSettings BasicLightingParserDelegate::GetToneMapSettings() const
    {
        return GetEnvSettings()._toneMapSettings;
    }

	float		BasicLightingParserDelegate::GetTimeValue() const
	{
		return _timeValue;
	}

	void		BasicLightingParserDelegate::SetTimeValue(float newValue)
	{
		_timeValue = newValue;
	}

	void BasicLightingParserDelegate::ConstructToFuture(
		::Assets::AssetFuture<BasicLightingParserDelegate>& future,
		StringSection<::Assets::ResChar> envSettingFileName)
	{
		auto envSettingsFuture = ::Assets::MakeAsset<EnvironmentSettings>(envSettingFileName);
		::Assets::WhenAll(envSettingsFuture).ThenConstructToFuture(future);
	}

	BasicLightingParserDelegate::BasicLightingParserDelegate(
		const std::shared_ptr<EnvironmentSettings>& envSettings)
	: _envSettings(envSettings)
	, _timeValue(0.f)
	{
	}

	BasicLightingParserDelegate::~BasicLightingParserDelegate() {}

	const EnvironmentSettings&  BasicLightingParserDelegate::GetEnvSettings() const
	{
		return *_envSettings;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    SceneEngine::LightDesc DefaultDominantLight()
    {
        SceneEngine::LightDesc light;
        light._shape = SceneEngine::LightDesc::Directional;
        light._position = Normalize(Float3(-0.15046243f, 0.97377890f, 0.17063323f));
        light._cutoffRange = 10000.f;
        light._diffuseColor = Float3(3.2803922f, 2.2372551f, 1.9627452f);
        light._specularColor = Float3(6.7647061f, 6.4117646f, 4.7647061f);
        light._diffuseWideningMax = .9f;
        light._diffuseWideningMin = 0.2f;
        light._diffuseModel = 1;
        return light;
    }

    SceneEngine::GlobalLightingDesc DefaultGlobalLightingDesc()
    {
        SceneEngine::GlobalLightingDesc result;
        result._ambientLight = Float3(0.f, 0.f, 0.f);
        XlCopyString(result._skyTexture, "xleres/defaultresources/sky/samplesky2.dds");
        XlCopyString(result._diffuseIBL, "xleres/defaultresources/sky/samplesky2_diffuse.dds");
        XlCopyString(result._specularIBL, "xleres/defaultresources/sky/samplesky2_specular.dds");
        result._skyTextureType = GlobalLightingDesc::SkyTextureType::Cube;
        result._skyReflectionScale = 1.f;
        result._doAtmosphereBlur = false;
        return result;
    }

    EnvironmentSettings DefaultEnvironmentSettings()
    {
        EnvironmentSettings result;
        result._globalLightingDesc = DefaultGlobalLightingDesc();

        auto defLight = DefaultDominantLight();
        result._lights.push_back(defLight);

        auto frustumSettings = PlatformRig::DefaultShadowFrustumSettings();
        result._shadowProj.push_back(EnvironmentSettings::ShadowProj { defLight, 0, frustumSettings });

        if (constant_expression<false>::result()) {
            SceneEngine::LightDesc secondaryLight;
            secondaryLight._shape = SceneEngine::LightDesc::Directional;
            secondaryLight._position = Normalize(Float3(0.71622938f, 0.48972201f, -0.49717990f));
            secondaryLight._cutoffRange = 10000.f;
            secondaryLight._diffuseColor = Float3(3.2803922f, 2.2372551f, 1.9627452f);
            secondaryLight._specularColor = Float3(5.f, 5.f, 5.f);
            secondaryLight._diffuseWideningMax = 2.f;
            secondaryLight._diffuseWideningMin = 0.5f;
            secondaryLight._diffuseModel = 0;
            result._lights.push_back(secondaryLight);

            SceneEngine::LightDesc tertiaryLight;
            tertiaryLight._shape = SceneEngine::LightDesc::Directional;
            tertiaryLight._position = Normalize(Float3(-0.75507462f, -0.62672323f, 0.19256261f));
            tertiaryLight._cutoffRange = 10000.f;
            tertiaryLight._diffuseColor = Float3(0.13725491f, 0.18666667f, 0.18745099f);
            tertiaryLight._specularColor = Float3(3.5f, 3.5f, 3.5f);
            tertiaryLight._diffuseWideningMax = 2.f;
            tertiaryLight._diffuseWideningMin = 0.5f;
            tertiaryLight._diffuseModel = 0;
            result._lights.push_back(tertiaryLight);
        }

        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void ReadTransform(SceneEngine::LightDesc& light, const ParameterBox& props)
    {
        static const auto transformHash = ParameterBox::MakeParameterNameHash("Transform");
        auto transform = Transpose(props.GetParameter(transformHash, Identity<Float4x4>()));

        ScaleRotationTranslationM decomposed(transform);
        light._position = decomposed._translation;
        light._orientation = decomposed._rotation;
        light._radii = Float2(decomposed._scale[0], decomposed._scale[1]);

            // For directional lights we need to normalize the position (it will be treated as a direction)
        if (light._shape == SceneEngine::LightDesc::Shape::Directional)
            light._position = (MagnitudeSquared(light._position) > 1e-5f) ? Normalize(light._position) : Float3(0.f, 0.f, 0.f);
    }
    
    namespace EntityTypeName
    {
        static const auto* EnvSettings = (const utf8*)"EnvSettings";
        static const auto* AmbientSettings = (const utf8*)"AmbientSettings";
        static const auto* DirectionalLight = (const utf8*)"DirectionalLight";
        static const auto* AreaLight = (const utf8*)"AreaLight";
        static const auto* ToneMapSettings = (const utf8*)"ToneMapSettings";
        static const auto* ShadowFrustumSettings = (const utf8*)"ShadowFrustumSettings";

        static const auto* OceanLightingSettings = (const utf8*)"OceanLightingSettings";
        static const auto* OceanSettings = (const utf8*)"OceanSettings";
        static const auto* FogVolumeRenderer = (const utf8*)"FogVolumeRenderer";
    }
    
    namespace Attribute
    {
        static const auto AttachedLight = ParameterBox::MakeParameterNameHash("Light");
        static const auto Name = ParameterBox::MakeParameterNameHash("Name");
        static const auto Flags = ParameterBox::MakeParameterNameHash("Flags");
    }

    EnvironmentSettings::EnvironmentSettings(
        InputStreamFormatter<utf8>& formatter, 
        const ::Assets::DirectorySearchRules&,
		const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
    {
        using namespace SceneEngine;

        _globalLightingDesc = DefaultGlobalLightingDesc();

        std::vector<std::pair<uint64, DefaultShadowFrustumSettings>> shadowSettings;
        std::vector<uint64> lightNames;
        std::vector<std::pair<uint64, uint64>> lightFrustumLink;    // lightid to shadow settings map

        utf8 buffer[256];

        bool exit = false;
        StringSection<> name;
        while (formatter.TryKeyedItem(name)) {
            switch(formatter.PeekNext()) {
            case FormatterBlob::BeginElement:
                {
                    RequireBeginElement(formatter);

                    if (XlEqString(name, EntityTypeName::AmbientSettings)) {
                        _globalLightingDesc = GlobalLightingDesc(ParameterBox(formatter));
                    } else if (XlEqString(name, EntityTypeName::ToneMapSettings)) {
                        AccessorDeserialize(formatter, _toneMapSettings);
                    } else if (XlEqString(name, EntityTypeName::DirectionalLight) || XlEqString(name, EntityTypeName::AreaLight)) {

                        ParameterBox params(formatter);
                        uint64 hashName = 0ull;
                        auto paramValue = params.GetParameterAsString(Attribute::Name);
                        if (paramValue.has_value())
                            hashName = Hash64(paramValue.value());

                        SceneEngine::LightDesc lightDesc(params);
                        if (XlEqString(name, EntityTypeName::DirectionalLight))
                            lightDesc._shape = LightDesc::Shape::Directional;
                        ReadTransform(lightDesc, params);

                        _lights.push_back(lightDesc);

                        if (params.GetParameter(Attribute::Flags, 0u) & (1<<0)) {
                            lightNames.push_back(hashName);
                        } else {
                            lightNames.push_back(0);    // dummy if shadows are disabled
                        }
                        
                    } else if (XlEqString(name, EntityTypeName::ShadowFrustumSettings)) {

                        ParameterBox params(formatter);
                        uint64 hashName = 0ull;
                        auto paramValue = params.GetParameterAsString(Attribute::Name);
                        if (paramValue.has_value())
                            hashName = Hash64(paramValue.value());

                        shadowSettings.push_back(
                            std::make_pair(hashName, CreateFromParameters<PlatformRig::DefaultShadowFrustumSettings>(params)));

                        uint64 frustumLink = 0;
                        paramValue = params.GetParameterAsString(Attribute::Name);
                        if (paramValue.has_value())
                            frustumLink = Hash64(paramValue.value());
                        lightFrustumLink.push_back(std::make_pair(frustumLink, hashName));

#if 0
                    } else if (XlEqString(name, EntityTypeName::OceanLightingSettings)) {
                        _oceanLighting = OceanLightingSettings(ParameterBox(formatter));
                    } else if (XlEqString(name, EntityTypeName::OceanSettings)) {
                        _deepOceanSim = DeepOceanSimSettings(ParameterBox(formatter));
                    } else if (XlEqString(name, EntityTypeName::FogVolumeRenderer)) {
                        _volFogRenderer = VolumetricFogConfig::Renderer(formatter);
#endif
                    } else
                        SkipElement(formatter);
                    
                    RequireEndElement(formatter);
                    break;
                }

            case FormatterBlob::Value:
                RequireValue(formatter);
                break;

            default:
                Throw(FormatException("Expected value or element", formatter.GetLocation()));
            }
        }

            // bind shadow settings (mapping via the light name parameter)
        for (unsigned c=0; c<lightFrustumLink.size(); ++c) {
            auto f = std::find_if(shadowSettings.cbegin(), shadowSettings.cend(), 
                [&lightFrustumLink, c](const std::pair<uint64, DefaultShadowFrustumSettings>&i) { return i.first == lightFrustumLink[c].second; });

            auto l = std::find(lightNames.cbegin(), lightNames.cend(), lightFrustumLink[c].first);

            if (f != shadowSettings.end() && l != lightNames.end()) {
                auto lightIndex = std::distance(lightNames.cbegin(), l);
                assert(lightIndex < ptrdiff_t(_lights.size()));

                _shadowProj.push_back(
                    EnvironmentSettings::ShadowProj { _lights[lightIndex], SceneEngine::LightId(lightIndex), f->second });
            }
        }
    }

    EnvironmentSettings::EnvironmentSettings() {}
    EnvironmentSettings::~EnvironmentSettings() {}

    /*std::vector<std::pair<std::string, PlatformRig::EnvironmentSettings>> 
        DeserializeEnvSettings(InputStreamFormatter<utf8>& formatter)
    {
        std::vector<std::pair<std::string, PlatformRig::EnvironmentSettings>> result;
        for (;;) {
            switch(formatter.PeekNext()) {
            case InputStreamFormatter<utf8>::Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection name;
                    if (!formatter.TryBeginElement(name)) break;
                    auto settings = DeserializeSingleSettings(formatter);
                    if (!formatter.TryEndElement()) break;

                    result.emplace_back(std::move(settings));
                    break;
                }

            default:
                return std::move(result);
            }
        }
    }*/
}

#include "../RenderCore/Techniques/TechniqueDelegates.h"
#include "../RenderCore/Techniques/Drawables.h"
#include "../RenderCore/Techniques/Apparatuses.h"
#include "../SceneEngine/RenderStep_PrepareShadows.h"
#include "../SceneEngine/RenderStepUtils.h"
#include "../Utility/Meta/AccessorSerialize.h"
#include "../Utility/Meta/ClassAccessors.h"

namespace SceneEngine
{
    ToneMapSettings::ToneMapSettings() {}

    RenderCore::Techniques::DrawablesPacket* ViewDelegate_Shadow::GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch)
	{
		return nullptr;
	}

	void ViewDelegate_Shadow::Reset()
	{
	}

    ViewDelegate_Shadow::ViewDelegate_Shadow(ShadowProjectionDesc shadowProjection)
	: _shadowProj(shadowProjection)
	{
	}

	ViewDelegate_Shadow::~ViewDelegate_Shadow()
	{
	}

    std::shared_ptr<ICompiledShadowGenerator> CreateCompiledShadowGenerator(const ShadowGeneratorDesc&, const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>&)
	{
		return nullptr;
	}

////////////////////////////////////////////////////////////////////////////////////////////////

	class RenderStep_Direct : public IRenderStep
	{
	public:
		std::shared_ptr<IViewDelegate> CreateViewDelegate() override { return std::make_shared<BasicViewDelegate>(); }
		const RenderStepFragmentInterface& GetInterface() const override { return _direct; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
            const RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
			LightingParserContext& lightingParserContext,
			RenderStepFragmentInstance& rpi,
			IViewDelegate* viewDelegate) override;

		RenderStep_Direct(
            std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate> techniqueDelegate);
		~RenderStep_Direct();
	private:
		RenderStepFragmentInterface _direct;
	};

	void RenderStep_Direct::Execute(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parsingContext,
        const RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		LightingParserContext& lightingParserContext,
		RenderStepFragmentInstance& rpi,
		IViewDelegate* viewDelegate)
	{
		assert(viewDelegate);
		auto& executedScene = *checked_cast<BasicViewDelegate*>(viewDelegate);
        RenderCore::Techniques::SequencerContext sequencerContext;
        sequencerContext._sequencerConfig = rpi.GetSequencerConfig();
		ExecuteDrawables(
            threadContext, parsingContext, 
            pipelineAccelerators,
            sequencerContext,
			executedScene._pkt,
			"MainScene-Direct");
	}

	RenderStep_Direct::RenderStep_Direct(
        std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate> techniqueDelegate)
	: _direct(RenderCore::PipelineType::Graphics)
	{
        using namespace RenderCore;
        AttachmentDesc colorDesc =
            {   RenderCore::Format::Unknown, 1.f, 1.f, 0u,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::OutputRelativeDimensions };
		AttachmentDesc msDepthDesc =
            {   RenderCore::Format::D24_UNORM_S8_UINT, 1.f, 1.f, 0u,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::OutputRelativeDimensions };

        auto output = _direct.DefineAttachment(Techniques::AttachmentSemantics::ColorLDR, colorDesc);
		auto depth = _direct.DefineAttachment(Techniques::AttachmentSemantics::MultisampleDepth, msDepthDesc);

		SubpassDesc mainSubpass;
		mainSubpass.AppendOutput(output, LoadStore::Clear);
		mainSubpass.SetDepthStencil(depth, LoadStore::Clear_ClearStencil);

		_direct.AddSubpass(mainSubpass.SetName("MainForward"), techniqueDelegate);
	}

	RenderStep_Direct::~RenderStep_Direct() {}

	std::shared_ptr<IRenderStep> CreateRenderStep_Direct(RenderCore::Techniques::DrawingApparatus& apparatus)
	{ 
		return std::make_shared<RenderStep_Direct>(apparatus._techniqueDelegateDeferred);
	}

////////////////////////////////////////////////////////////////////////////////////////////////

    std::vector<std::shared_ptr<IRenderStep>> CreateStandardRenderSteps(LightingModel lightingModel)
    {
        return {};
    }

    void LightingParserStandardPlugin::OnPreScenePrepare(
        RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&) const {}
    void LightingParserStandardPlugin::OnLightingResolvePrepare(
        RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&,  LightingParserContext& parserContext,
        LightingResolveContext& resolveContext) const {}
    void LightingParserStandardPlugin::OnPostSceneRender(
        RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext& parserContext, 
        RenderCore::Techniques::BatchFilter filter, unsigned techniqueIndex) const {}
    void LightingParserStandardPlugin::InitBasicLightEnvironment(
        RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, 
        ShaderLightDesc::BasicEnvironment& env) const {}
}

template<> const ClassAccessors& Legacy_GetAccessors<SceneEngine::ToneMapSettings>()
{
    static ClassAccessors dummy(0);
    return dummy;
}
