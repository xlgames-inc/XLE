// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicScene.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Assets/SharedStateSet.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Tonemap.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../ConsoleRig/Console.h"
#include "../../Math/Transformations.h"
#include "../../Utility/Streams/PathUtils.h"

namespace Sample
{
    const float SunDirectionAngle = .33f;

    class BasicSceneParser::Model
    {
    public:
        void RenderOpaque(
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext, 
            unsigned techniqueIndex);

        Model();
        ~Model();
    protected:
        std::unique_ptr<RenderCore::Assets::SharedStateSet> _sharedStateSet;
        mutable std::unique_ptr<RenderCore::Assets::ModelRenderer> _modelRenderer;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    void BasicSceneParser::PrepareFrame(RenderCore::Metal::DeviceContext* context) 
    {
        //  Some effects need a "prepare" step before the main render begins... It's often handy to 
        //  add a PrepareFrame() method to the scene parser.
        //  In this example, though... Nothing happens!
    }

    void BasicSceneParser::ExecuteScene(   
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned techniqueIndex) const
    {
            //  "ExecuteScene" is the main entry point for rendering the scene geometry.
            //
            //  The lighting parser will have already set up the environment for rendering
            //  (eg, setting render targets, etc).
            //  
            //  The scene parser should now go through all objects in the scene and
            //  render them as appropriate.
            //
            //  Note that the parseSettings objects contains a number of filters for
            //  enabling and disabling certain types of geometry. The scene parser
            //  should be careful to check the flags and only render the correct geometry.
        if (    parseSettings._batchFilter == SceneParseSettings::BatchFilter::General
            ||  parseSettings._batchFilter == SceneParseSettings::BatchFilter::Depth) {

                //  Our scene is just a single model. So it's really simple here.
                //
                //  More complex scenes might have some hierarchical culling structure
                //  (like a quad-tree or some occlusion system)
                //  In those cases, we could start to navigate through those structures
                //  here.
                //
                //  Normally the scene parser would do some camera frustum culling.
                //  But this example just ignores that.
                //
                //  Also note that order is often very important when parsing the scene!
                //  It can sometimes have an impact on rendering performance, and is
                //  particularly important when translucent objects are involved.
                //  It can also have an effect on rendering quality (eg, in MSAA modes)
                //
                //  The scene parser is responsible for getting the ordering right. But
                //  the best ordering depends on the type of scene you want to render.
            if (parseSettings._toggles & SceneParseSettings::Toggles::NonTerrain) {
                _model->RenderOpaque(context, parserContext, techniqueIndex);
            }

        }
    }

    void BasicSceneParser::ExecuteShadowScene( 
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned frustumIndex, unsigned techniqueIndex) const 
    {
            //  "ExecuteShadowScene" is very similar to ExecuteScene,
            //  except that is used while rendering the shadows. 
            //
            //  In this particularly case, we just reuse ExecuteScene,
            //  A more complex implementation of ExecuteShadowScene should
            //  perform culling against all of the shadow frustums, as
            //  appropriate.
        ExecuteScene(context, parserContext, parseSettings, techniqueIndex);
    }

    RenderCore::Techniques::CameraDesc BasicSceneParser::GetCameraDesc() const 
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

    unsigned BasicSceneParser::GetLightCount() const 
    {
        return 1; 
    }

    auto BasicSceneParser::GetLightDesc(unsigned index) const -> const LightDesc&
    { 
            //  This method just returns a properties for the lights in the scene. 
            //
            //  The lighting parser will take care of the actual lighting calculations 
            //  required. All we have to do is return the properties of the lights
            //  we want.
        static LightDesc dummy;
        dummy._radius = 10000.f;
        dummy._type = LightDesc::Directional;

            // sun direction based on angle in the sky
        Float2 sunDirectionOfMovement = Normalize(Float2(1.f, 0.33f));
        Float2 sunRotationAxis(-sunDirectionOfMovement[1], sunDirectionOfMovement[0]);
        dummy._negativeLightDirection = 
            Normalize(TransformDirectionVector(
                MakeRotationMatrix(Expand(sunRotationAxis, 0.f), SunDirectionAngle), Float3(0.f, 0.f, 1.f)));

        return dummy;
    }

    auto BasicSceneParser::GetGlobalLightingDesc() const -> GlobalLightingDesc
    { 
            //  There are some "global" lighting parameters that apply to
            //  the entire rendered scene 
            //      (or, at least, to one area of the scene -- eg, indoors/outdoors)
            //  Here, we can fill in these properties.
            //
            //  Note that the scene parser "desc" functions can be called multiple
            //  times in a single frame. Generally the properties can be animated in
            //  any way, but they should stay constant over the course of a single frame.
        GlobalLightingDesc result;
        auto ambientScale = Tweakable("AmbientScale", .5f * 0.075f);
        result._ambientLight = Float3(.65f * ambientScale, .7f * ambientScale, 1.f * ambientScale);
        XlCopyString(result._skyTexture, "game/xleres/defaultresources/sky/samplesky.dds");
        return result;
    }

    auto BasicSceneParser::GetToneMapSettings() const -> ToneMapSettings
    {
        return SceneEngine::DefaultToneMapSettings();
    }

    unsigned BasicSceneParser::GetShadowProjectionCount() const
    {
        return 1; 
    }

    auto BasicSceneParser::GetShadowProjectionDesc(unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
        -> ShadowProjectionDesc
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
            throw Exceptions::BasicLabel("Bad shadow frustum index");
        }

        return PlatformRig::CalculateDefaultShadowCascades(
            GetLightDesc(index), mainSceneProjectionDesc,
            PlatformRig::DefaultShadowFrustumSettings());
    }

    float BasicSceneParser::GetTimeValue() const      
    { 
            //  The scene parser can also provide a time value, in seconds.
            //  This is used to control rendering effects, such as wind
            //  and waves.
        return _time; 
    }

    void BasicSceneParser::Update(float deltaTime)    { _time += deltaTime; }

    BasicSceneParser::BasicSceneParser()
    {
        _time = 0.f;
        _model = std::make_unique<Model>();
    }

    BasicSceneParser::~BasicSceneParser()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    BasicSceneParser::Model::Model()
    {
        _sharedStateSet = std::make_unique<RenderCore::Assets::SharedStateSet>();
    }

    BasicSceneParser::Model::~Model()
    {}

    void BasicSceneParser::Model::RenderOpaque(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, 
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
            _modelRenderer = std::make_unique<ModelRenderer>(
                std::ref(scaffold), std::ref(matScaffold), std::ref(*_sharedStateSet), 
                &searchRules, levelOfDetail);
        }

            //  Before using SharedStateSet for the first time, we need to capture the device 
            //  context state. If we were rendering multiple models with the same shared state, we would 
            //  capture once and render multiple times with the same capture.
        _sharedStateSet->CaptureState(context);

            //  Finally, we can render the object!
        const float x2ScaleFactor = 100.f;
        _modelRenderer->Render(
            RenderCore::Assets::ModelRendererContext(context, parserContext, techniqueIndex),
            *_sharedStateSet, AsFloat4x4(UniformScale(1.f/x2ScaleFactor)));

        _sharedStateSet->ReleaseState(context);
    }

}

