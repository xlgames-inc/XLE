// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicScene.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/Drawables.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../FixedFunctionModel/ModelRunTime.h"
#include "../../FixedFunctionModel/SharedStateSet.h"
#include "../../RenderCore/Assets/ModelScaffold.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Assets/SimpleModelRenderer.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/IAnnotator.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/Tonemap.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/AssetFuture.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../ConsoleRig/Console.h"
#include "../../Math/Transformations.h"
#include "../../Utility/Streams/PathUtils.h"

#include "../../RenderCore/Techniques/BasicDelegates.h"

namespace Sample
{
    const float SunDirectionAngle = .33f;

    class BasicSceneParser::Model
    {
    public:
        void RenderOpaque(
            RenderCore::IThreadContext& context, 
            RenderCore::Techniques::ParsingContext&, 
            unsigned techniqueIndex);

		void RenderOpaque_SimpleModelRenderer(
            SceneEngine::SceneExecuteContext& executeContext);

        Model();
        ~Model();
    protected:
        std::unique_ptr<FixedFunctionModel::SharedStateSet> _sharedStateSet;
        mutable std::unique_ptr<FixedFunctionModel::ModelRenderer> _modelRenderer;

		::Assets::FuturePtr<RenderCore::Assets::SimpleModelRenderer> _simpleModelRenderer;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    void BasicSceneParser::ExecuteScene(
        RenderCore::IThreadContext& context, 
		SceneEngine::SceneExecuteContext& executeContext) const
    {
        _model->RenderOpaque_SimpleModelRenderer(executeContext);
    }

	BasicSceneParser::BasicSceneParser()
	{
		_model = std::make_unique<Model>();
	}

	BasicSceneParser::~BasicSceneParser()
	{}

///////////////////////////////////////////////////////////////////////////////////////////////////

    BasicSceneParser::Model::Model()
    {
        _sharedStateSet = std::make_unique<FixedFunctionModel::SharedStateSet>(
            RenderCore::Assets::Services::GetTechniqueConfigDirs());

		_simpleModelRenderer = ::Assets::MakeAsset<RenderCore::Assets::SimpleModelRenderer>(
			"game/model/galleon/galleon.dae");
    }

    BasicSceneParser::Model::~Model()
    {}

    void BasicSceneParser::Model::RenderOpaque(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext, 
        unsigned techniqueIndex)
    {
            //  This class shows a simple method for rendering an object
            //  using the ModelRenderer class.
            //
            //  There are a few steps involved. The ModelRenderer/ModelScaffold
            //  classes are very flexible and efficient. But the flexibility
            //  brings with it a little bit of complexity.
            //
            //  Basically, I think of a "model" is an discrete single exporter from Max 
            //  (or Maya, etc).
            //  It might come via a Collada file or some intermediate format. But at
            //  run-time we want some highly optimized format that can be loaded quickly,
            //  and that also allows for a certain amount of flexibility while rendering.
            //
            //  There are 3 important classes:
            //      ModelRenderer is the class that can actually render a model file. It's
            //      mostly a static class, however. Once it's constructed, we can't change
            //      it (other than by setting animation parameters). ModelRenderer manages
            //      the low-level graphics API objects required for rendering.
            //
            //      ModelScaffold is a light weight representation of what it contained in
            //      the model file. It never uses the low level graphics API, and we can't
            //      render from it directly.
            //
            //      SharedStateSet contains state related information (from the low level
            //      graphics API) for rendering one or more models. Normally we want multiple
            //      models to share the same SharedStateSet. When multiple models use the
            //      same SharedStateSet, we can reorder rendering operations between all those
            //      models. But we can choose the granularity at which this occurs.
            //      So, for example if we have a city made from many models, we can created a
            //      single SharedStateSet for that city. That would means we can perform
            //      draw command sorting on the city as a whole (rather than just within a 
            //      single model)
            //
            //  First, we need to load the ModelScaffold object. We have to load this before
            //  we can construct the ModelRenderer.
        using namespace RenderCore::Assets;
        if (!_modelRenderer) {

                //  We're going to use the Assets::GetAssetComp<> function to initialize
                //  our ModelScaffold. These are other ways to create a ModelScaffold, 
                //  though (eg, Assets::GetAsset<>, or just using the constructor directly)
                //
                //  In this case, we use GetAssetComp to cause the system to execute a
                //  Collada compile when required. This will compile the input Collada
                //  file into our run-time format in a file in the intermediate store.
                //
                //  The compile can occur in a background thread. When this happens,
                //  we will get thrown a Assets::Exceptions::PendingAsset exception
                //  until the compile is finished. We aware that some assets that are
                //  compiled or loaded in the background can throw PendingAsset when
                //  they are not ready!
                //
                //  We can also get a Assets::Exceptions::InvalidAsset if asset can
                //  never be correctly loaded (eg, missing file or something)
            const char sampleAsset[] = "game/model/galleon/galleon.dae";
            const char sampleMaterial[] = "game/model/galleon/galleon.material";
            auto& scaffold = Assets::GetAssetComp<ModelScaffold>(sampleAsset);
            auto& matScaffold = Assets::GetAssetComp<MaterialScaffold>(sampleMaterial, sampleAsset);

                //  We want to create a Assets::DirectorySearchRules object before we
                //  make the ModelRenderer. This is used when we need to find the 
                //  dependent assets (like textures). In this case, we just need to
                //  add the directory that contains the dae file as a search path
                //  (though if we needed, we could add other paths as well).
            auto searchRules = Assets::DefaultDirectorySearchRules(sampleAsset);

                //  Now, finally we can construct the model render.
                //
                //  Each model renderer is associated with a single level of detail
                //  (though the ModelScaffold could contain information for multiple
                //  levels of detail)
                //
                //  During the constructor of ModelRenderer, all of the low level 
                //  graphics API resources will be constructed. So it can be expensive
                //  in some cases.
                //
                //  Also note that if we get an allocation failure while making a 
                //  low level resource (like a vertex buffer), it will throw an
                //  exception.
            const unsigned levelOfDetail = 0;
            _modelRenderer = std::unique_ptr<FixedFunctionModel::ModelRenderer>(
                new FixedFunctionModel::ModelRenderer(
                    scaffold, matScaffold, FixedFunctionModel::ModelRenderer::Supplements(),
                    *_sharedStateSet, &searchRules, levelOfDetail));
        }

		RenderCore::GPUProfilerBlock profileBlock(context, "RenderModel");

            //  Before using SharedStateSet for the first time, we need to capture the device 
            //  context state. If we were rendering multiple models with the same shared state, we would 
            //  capture once and render multiple times with the same capture.
        auto captureMarker = _sharedStateSet->CaptureState(
			context, 
			parserContext.GetRenderStateDelegate(), parserContext.GetRenderStateDelegateParameters());

            //  Finally, we can render the object!
        _modelRenderer->Render(
            FixedFunctionModel::ModelRendererContext(context, parserContext, techniqueIndex),
            *_sharedStateSet, Identity<Float4x4>());
    }

	RenderCore::Techniques::SequencerTechnique MakeSequencerTechnique(
		RenderCore::Techniques::ParsingContext& parserContext)
	{
		RenderCore::Techniques::SequencerTechnique result;
		result._techniqueDelegate = std::make_shared<RenderCore::Techniques::TechniqueDelegate_Basic>();
		result._materialDelegate = std::make_shared<RenderCore::Techniques::MaterialDelegate_Basic>();
		result._renderStateDelegate = parserContext.GetRenderStateDelegate();

		auto& techUSI = RenderCore::Techniques::TechniqueContext::GetGlobalUniformsStreamInterface();
		for (unsigned c=0; c<techUSI._cbBindings.size(); ++c)
			result._sequencerUniforms.emplace_back(std::make_pair(techUSI._cbBindings[c]._hashName, std::make_shared<RenderCore::Techniques::GlobalCBDelegate>(c)));
		return result;
	}

	void BasicSceneParser::Model::RenderOpaque_SimpleModelRenderer(
        SceneEngine::SceneExecuteContext& executeContext)
    {
		auto renderer = _simpleModelRenderer->TryActualize();
		if (!renderer) return;

		// renderer->BuildDrawables(packet);
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    RenderCore::Techniques::CameraDesc SampleLightingDelegate::GetCameraDesc() const 
    { 
            //  The scene parser provides some global rendering properties
            //  to the lighting parser. 
            //
            //  The simplest example of this is the camera properties.
            //  This function just returns the properties the main camera
            //  that should be used to render this scene.
        RenderCore::Techniques::CameraDesc result;
        static const auto camDist = 50.f;
        const auto camHeight = 7.5f;
        const auto secondsPerRotation = 40.f;
        const auto rotationSpeed = -gPI * 2.f / secondsPerRotation;
        Float3 cameraForward(XlCos(_time * rotationSpeed), XlSin(_time * rotationSpeed), 0.f);
        Float3 cameraPosition = -camDist * cameraForward + Float3(0.f, 0.f, camHeight);
        result._cameraToWorld = MakeCameraToWorld(cameraForward, Float3(0.f, 0.f, 1.f), cameraPosition);
        result._farClip = 1000.f;
        return result;
    }

    unsigned SampleLightingDelegate::GetLightCount() const 
    {
        return 1; 
    }

    auto SampleLightingDelegate::GetLightDesc(unsigned index) const -> const SceneEngine::LightDesc&
    { 
            //  This method just returns a properties for the lights in the scene. 
            //
            //  The lighting parser will take care of the actual lighting calculations 
            //  required. All we have to do is return the properties of the lights
            //  we want.
        static SceneEngine::LightDesc dummy;
        dummy._cutoffRange = 10000.f;
        dummy._shape = SceneEngine::LightDesc::Directional;

            // sun direction based on angle in the sky
        Float2 sunDirectionOfMovement = Normalize(Float2(1.f, 0.33f));
        Float2 sunRotationAxis(-sunDirectionOfMovement[1], sunDirectionOfMovement[0]);
        dummy._position = 
            Normalize(TransformDirectionVector(
                MakeRotationMatrix(Expand(sunRotationAxis, 0.f), SunDirectionAngle), Float3(0.f, 0.f, 1.f)));

        return dummy;
    }

    auto SampleLightingDelegate::GetGlobalLightingDesc() const -> SceneEngine::GlobalLightingDesc
    { 
            //  There are some "global" lighting parameters that apply to
            //  the entire rendered scene 
            //      (or, at least, to one area of the scene -- eg, indoors/outdoors)
            //  Here, we can fill in these properties.
            //
            //  Note that the scene parser "desc" functions can be called multiple
            //  times in a single frame. Generally the properties can be animated in
            //  any way, but they should stay constant over the course of a single frame.
        SceneEngine::GlobalLightingDesc result;
        result._ambientLight = Float3(0.f, 0.f, 0.f);
        XlCopyString(result._skyTexture, "xleres/defaultresources/sky/samplesky2.dds");
        XlCopyString(result._diffuseIBL, "xleres/defaultresources/sky/samplesky2_diffuse.dds");
        XlCopyString(result._specularIBL, "xleres/defaultresources/sky/samplesky2_specular.dds");
        result._skyTextureType = SceneEngine::GlobalLightingDesc::SkyTextureType::Cube;
        result._skyReflectionScale = 1.f;
        return result;
    }

    auto SampleLightingDelegate::GetToneMapSettings() const -> SceneEngine::ToneMapSettings
    {
        SceneEngine::ToneMapSettings toneMapSettings;
        toneMapSettings._bloomRampingFactor = 0.f;
        toneMapSettings._sceneKey = 0.16f;
        toneMapSettings._luminanceMin = 0.f;
        toneMapSettings._luminanceMax = 20.f;
        toneMapSettings._whitepoint = 3.f;
        toneMapSettings._bloomThreshold = 5.f;
        toneMapSettings._bloomColor = Float3(1.f,1.f,1.f);
        return toneMapSettings;
    }

    unsigned SampleLightingDelegate::GetShadowProjectionCount() const
    {
        return 1; 
    }

    auto SampleLightingDelegate::GetShadowProjectionDesc(
		unsigned index, 
		const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
        -> SceneEngine::ShadowProjectionDesc
    {
            //  Shadowing lights can have a ShadowProjectionDesc object associated.
            //  This object determines the shadow "projections" or "cascades" we use 
            //  for calculating shadows.
            //
            //  Normally, we want multiple shadow cascades per light. There are a few
            //  different methods for deciding on the cascades for a scene.
            //
            //  In this case, we're just using a default implementation -- this
            //  implementation is very basic. The results are ok, but not optimal.
            //  Specialised scenes may some specialised algorithm for calculating shadow
            //  cascades.
        if (index >= GetShadowProjectionCount()) {
            throw ::Exceptions::BasicLabel("Bad shadow frustum index");
        }

        return PlatformRig::CalculateDefaultShadowCascades(
            GetLightDesc(index), index, mainSceneProjectionDesc,
            PlatformRig::DefaultShadowFrustumSettings());
    }

    float SampleLightingDelegate::GetTimeValue() const      
    { 
            //  The scene parser can also provide a time value, in seconds.
            //  This is used to control rendering effects, such as wind
            //  and waves.
        return _time; 
    }

    void SampleLightingDelegate::Update(float deltaTime)    { _time += deltaTime; }

    SampleLightingDelegate::SampleLightingDelegate()
    {
        _time = 0.f;
    }

    SampleLightingDelegate::~SampleLightingDelegate()
    {}


}

