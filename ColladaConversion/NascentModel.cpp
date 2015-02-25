// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS   // warning C4996: 'std::_Copy_impl': Function call with parameters that may be unsafe

#pragma warning(push)
#pragma warning(disable:4244)   // warning C4244: '=' : conversion from 'const double' to 'float', possible loss of data
#include <algorithm>
#pragma warning(pop)

#include "NascentModel.h"
#include "TableOfObjects.h"
#include "RawGeometry.h"
#include "ConversionObjects.h"
#include "ColladaUtils.h"
#include "MaterialSettingsFile.h"

#include "../RenderCore/Assets/ModelRunTime.h"
#include "../RenderCore/Assets/RawAnimationCurve.h"
#include "../RenderCore/Assets/AssetUtils.h"            // for MaterialParameters
#include "../RenderCore/Techniques/TechniqueUtils.h"    // for CameraDesc

#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../Assets/BlockSerializer.h"
#include "../Assets/AssetUtils.h"

#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/OutputStream.h"

#include "../Utility/Streams/FileUtils.h"        // (for materials stuff)
#include "../Utility/Streams/Data.h"             // (for materials stuff)
#include "../Utility/Streams/Stream.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/StringFormat.h"

#pragma warning(push)
#pragma warning(disable:4201)       // nonstandard extension used : nameless struct/union
#pragma warning(disable:4245)       // conversion from 'int' to 'const COLLADAFW::SamplerID', signed/unsigned mismatch
#pragma warning(disable:4512)       // assignment operator could not be generated
    #include <COLLADAFWIWriter.h>
    #include <COLLADAFWImage.h>
    #include <COLLADAFWGeometry.h>
    #include <COLLADAFWEffect.h>
    #include <COLLADAFWMaterial.h>
    #include <COLLADAFWVisualScene.h>
    #include <COLLADAFWLibraryNodes.h>
    #include <COLLADAFWAnimation.h>
    #include <COLLADAFWAnimationCurve.h>
    #include <COLLADAFWAnimationList.h>
    #include <COLLADAFWSkinController.h>
    #include <COLLADAFWSkinControllerData.h>
    #include <COLLADAFWMorphController.h>
    #include <COLLADAFWRoot.h>

    #include <COLLADASaxFWLLoader.h>
    #include <COLLADASaxFWLIErrorHandler.h>
    #include <COLLADASaxFWLIError.h>
    #include <COLLADASaxFWLSaxParserError.h>
    #include <COLLADASaxFWLSaxFWLError.h>
    #include <COLLADASaxFWLInputUnshared.h>

    #include <COLLADASaxFWLIExtraDataCallbackHandler.h>
    #include <generated14\COLLADASaxFWLColladaParserAutoGen14Attributes.h>
    #include <generated15\COLLADASaxFWLColladaParserAutoGen15Attributes.h>

    #include <GeneratedSaxParserParserError.h>
#pragma warning(pop)

#include <regex>

#pragma warning(disable:4127)       // conditional expression is constant

namespace COLLADAFW
{
	class Formulas;
	class Formula;
}

namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

    class ImportConfiguration
    {
    public:
        uint64 AsNativeBindingHash(const std::string& input) const;
        const ::Assets::DependencyValidation& GetDependencyValidation() const { return *_depVal; }

        ImportConfiguration(const ResChar filename[]);
        ~ImportConfiguration();
    private:
        std::vector<std::pair<std::string, uint64>> _exportNameToBindingHash;
        std::shared_ptr<::Assets::DependencyValidation> _depVal;
    };

    class Writer : public COLLADAFW::IWriter
    {
    public:
	    Writer(const ResChar baseName[], const ::Assets::DirectorySearchRules& searchRules) 
            : _importConfig("colladaimport.cfg")  
        {
            ResChar resolvedFile[MaxPath];
            searchRules.ResolveFile(resolvedFile, dimof(resolvedFile), StringMeld<MaxPath>() << baseName << ".material");
            _matSettingsFile = MaterialSettingsFile(resolvedFile);
        }

	    virtual ~Writer(){}

	    /** This method will be called if an error in the loading process occurred and the loader cannot
	    continue to to load. The writer should undo all operations that have been performed.
	    @param errorMessage A message containing informations about the error that occurred.
	    */
	    virtual void cancel(const COLLADAFW::String& errorMessage)
        {
            LogAlwaysError << "Got error while parsing Collada: " << errorMessage;
        }

	    /** This is the method called. The writer hast to prepare to receive data.*/
	    virtual void start(){}

	    /** This method is called after the last write* method. No other methods will be called after this.*/
	    virtual void finish(){}

	    /** When this method is called, the writer must write the global document asset.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeGlobalAsset ( const COLLADAFW::FileInfo* asset )
	    {
		    return true;
	    }

	    /** When this method is called, the writer must write the entire visual scene.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeScene ( const COLLADAFW::Scene* scene )
	    {
		    return true;
	    }

	    /** When this method is called, the writer must write the entire visual scene.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeVisualScene ( const COLLADAFW::VisualScene* visualScene );

	    /** When this method is called, the writer must handle all nodes contained in the 
	    library nodes.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeLibraryNodes ( const COLLADAFW::LibraryNodes* libraryNodes );

	    /** When this method is called, the writer must write the geometry.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeGeometry ( const COLLADAFW::Geometry* geometry );

	    /** When this method is called, the writer must write the material.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeMaterial( const COLLADAFW::Material* material );

	    /** When this method is called, the writer must write the effect.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeEffect( const COLLADAFW::Effect* effect );

	    /** When this method is called, the writer must write the camera.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeCamera( const COLLADAFW::Camera* camera )
	    {
		    return true;
	    }

	    /** When this method is called, the writer must write the image.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeImage( const COLLADAFW::Image* image );

	    /** When this method is called, the writer must write the light.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeLight( const COLLADAFW::Light* light )
	    {
		    return true;
	    }

	    /** Writes the animation.
	    @return True on succeeded, false otherwise.*/
	    virtual bool writeAnimation( const COLLADAFW::Animation* animation );

	    /** When this method is called, the writer must write the AnimationList.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeAnimationList( const COLLADAFW::AnimationList* animationList );

	    /** When this method is called, the writer must write the skin controller data.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeSkinControllerData( const COLLADAFW::SkinControllerData* skinControllerData );

	    /** When this method is called, the writer must write the controller.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeController( const COLLADAFW::Controller* controller );

	    /** When this method is called, the writer must write the formula.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeFormulas( const COLLADAFW::Formulas* formulas )
	    { 
		    return true; 
	    }

	    /** When this method is called, the writer must write the formula.
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeFormula( const COLLADAFW::Formula* formulas )
	    { 
		    return true; 
	    }

	    /** When this method is called, the writer must write the kinematics scene. 
	    @return The writer should return true, if writing succeeded, false otherwise.*/
	    virtual bool writeKinematicsScene( const COLLADAFW::KinematicsScene* kinematicsScene )		
	    { 
		    return true; 
	    }

        ColladaConversion::TableOfObjects               _objects;
        ColladaConversion::NascentModelCommandStream    _visualScene;
        ColladaConversion::NascentAnimationSet          _animationSet;
        ColladaConversion::NascentSkeleton              _skeleton;

        class AnimationLink
        {
        public:
            ColladaConversion::HashedColladaUniqueId    _animationListName;
            COLLADAFW::UniqueId                         _animationId;

            unsigned        _samplerWidth;
            unsigned        _samplerOffset;

            AnimationLink(  
                ColladaConversion::HashedColladaUniqueId animationListName, 
                const COLLADAFW::UniqueId& animationId, 
                unsigned samplerWidth, unsigned samplerOffset)
            : _animationListName(animationListName), _animationId(animationId)
            , _samplerWidth(samplerWidth), _samplerOffset(samplerOffset) {}
        };

        std::vector<AnimationLink>  _animationLinks;

        void CompleteProcessing();

	    // private function declarations
    private:
    /** Disable default copy ctor. */
	    Writer( const Writer& pre );
    /** Disable default assignment operator. */
	    const Writer& operator= ( const Writer& pre );

        void HandleFormatError(const FormatError& error);

        ImportConfiguration _importConfig;
        MaterialSettingsFile _matSettingsFile;
    };

    void Writer::HandleFormatError(const FormatError& error)
    {
        Warning("Supressing format error on Collada import. See description below:\n");
        Warning("%s\n", error.what());
    }

    bool Writer::writeImage(const COLLADAFW::Image* image)
    {
        TRY {
            ColladaConversion::ReferencedTexture texture = ColladaConversion::Convert(image);
            _objects.Add(   image->getOriginalId(),
                            image->getName(),
                            image->getUniqueId(),
                            std::move(texture));
            return true;
        } CATCH(const FormatError& error) {
            HandleFormatError(error);
        } CATCH_END

        return true;
    }

    bool Writer::writeGeometry(const COLLADAFW::Geometry* geometry)
	{
        TRY {
            ColladaConversion::NascentRawGeometry geo = ColladaConversion::Convert(geometry);
            _objects.Add(   geometry->getOriginalId(),
                            geometry->getName(),
                            geometry->getUniqueId(),
                            std::move(geo));
            return true;
        } CATCH(const FormatError& error) {
            HandleFormatError(error);
        } CATCH_END

        return false;
	}

    static const auto DefaultDiffuseTextureBindingHash = Hash64("DiffuseTexture");

    static void AddBoundTexture( 
        const COLLADAFW::Effect* effect, unsigned commonEffectIndex,
        const TableOfObjects& objects,
        RenderCore::Assets::MaterialParameters::ResourceBindingSet& bindings,
        uint64 bindingHash,
        COLLADAFW::SamplerID samplerId)
    {
        auto i = std::find_if(bindings.cbegin(), bindings.cend(),
            [=](const Assets::MaterialParameters::ResourceBinding&b) 
                { return b._bindHash == bindingHash; });
        if (i==bindings.cend()) {

                //
                //      Note that Collada has a method for associating texture coords
                //      with the bound textures. We're ignoring this currently,
                //      it only really useful if we want different texture coordinates
                //      for diffuse and specular parameters map (which we don't normally)
                //
                //      Only really need the diffuse map texture from here. Collada
                //      isn't good for passing the other parameters from Max. Some of
                //      the specular parameters come through -- but it isn't great.
                //

            auto& effectCommons = effect->getCommonEffects();
            if (commonEffectIndex >= effectCommons.getCount() || !effectCommons[commonEffectIndex]) { return; }

            auto& ec = *effectCommons[commonEffectIndex];
            if (    samplerId < ec.getSamplerPointerArray().getCount()
                &&  ec.getSamplerPointerArray()[samplerId]) {

                const auto& sampler = *ec.getSamplerPointerArray()[samplerId];
                const ReferencedTexture* refTexture = nullptr;
                ObjectId tableId = objects.Get<ReferencedTexture>(sampler.getSourceImage());
                if (tableId != ObjectId_Invalid) {
                    refTexture = objects.GetFromObjectId<ReferencedTexture>(tableId);
                }
                if (refTexture) {
                    bindings.push_back(
                        Assets::MaterialParameters::ResourceBinding(
                            bindingHash, refTexture->_resourceName));
                }
            }
        }
    }

    ImportConfiguration::ImportConfiguration(const ResChar filename[])
    {
        Data data;
        size_t sourceFileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &sourceFileSize);

        _depVal = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(_depVal, filename);
        
        if (sourceFile && sourceFileSize) {
            Data data;
            data.Load((const char*)sourceFile.get(), (int)sourceFileSize);
            auto* bindingRenames = data.ChildWithValue("BindingRenames");
            if (bindingRenames) {
                auto* child = bindingRenames->child;
                for (; child; child = child->next) {
                    if (child->child) {
                        std::string exportName = child->StrValue();
                        auto bindingHash = Hash64(child->child->StrValue());
                        _exportNameToBindingHash.push_back(std::make_pair(exportName, bindingHash));
                    }
                }
            }
        }
    }

    ImportConfiguration::~ImportConfiguration()
    {}

    uint64 ImportConfiguration::AsNativeBindingHash(const std::string& input) const
    {
            //  we need to define a mapping between the names used by the max exporter
            //  and the native XLE shader names. The meaning might not match perfectly
            //  but let's try to get as close as possible
        auto i = std::find_if(
            _exportNameToBindingHash.cbegin(), _exportNameToBindingHash.cend(),
            [=](const std::pair<std::string, uint64>& e) { return e.first == input; });
        if (i == _exportNameToBindingHash.cend()) {
            return 0;
        }
        return i->second;
    }

////////////////////////////////////////////////////////////////////////

    bool Writer::writeEffect( const COLLADAFW::Effect* effect )
    {
        TRY {

                //
                //      When we encounter an effect, let's read some extra
                //      information about that effect from a script file
                //
                //      Most of the material information is in the effect
                //      object. The "material" just instantiates the effect
                //      and can add some parameters. It seems that most
                //      collada exporters just use the material object to
                //      reference the effect, and put all of the real information
                //      in the effect.
                //
                //      Note; how should we associate the material to the script
                //      file? From the material name, the id or the effect id?
                //
            MaterialSettingsFile::MaterialDesc matSettings;

            {
                    //  Look for material settings with the same name as the 
                    //  effect. If we can't find it, look for a default settings
                    //  entry (just called "*")
                    //  Note that we could modify this so that all materials inherit
                    //  from the default, or have the default only apply if there are
                    //  no perfect matches. Either way has advantages... But in this
                    //  version, the default will only apply if we can't find an
                    //  exact match
                auto hashName = Hash64(effect->getName());
                auto i = LowerBound(_matSettingsFile._materials, hashName);
                if (i != _matSettingsFile._materials.end() && i->first == hashName) {
                    matSettings = i->second;
                } else {
                    static const auto defHash = Hash64("*");
                    i = LowerBound(_matSettingsFile._materials, defHash);
                    if (i != _matSettingsFile._materials.end() && i->first == defHash) {
                        matSettings = i->second;
                    }
                }
            }

                //  Any settings from the Collada file should override what we read
                //  in the material settings file. This means that we have 
                //  clear out the settings in the Collada file if we want the .material
                //  file to show through. This pattern works best if we can use 
                //  the .material files to specify the default settings, and then use
                //  the collada data to specialise the settings of specific parts of geometry.

            using namespace COLLADAFW;
            using namespace ColladaConversion;

                // bind the texture in the "common" effects part
            const CommonEffectPointerArray& commonEffects = effect->getCommonEffects();
            for (unsigned c=0; c<commonEffects.getCount(); ++c) {
                auto& diffuse = commonEffects[c]->getDiffuse();
                if (diffuse.getType() == ColorOrTexture::TEXTURE) {
                    AddBoundTexture(
                        effect, c, _objects, matSettings._resourceBindings,
                        DefaultDiffuseTextureBindingHash, 
                        diffuse.getTexture().getSamplerId());
                }
            }

                //  Also bind the "extra textures" part. Note that the
                //  way OpenCollada works, there must be at least one profile_COMMON part
                //  Let's assume the sampler indices refer to the samplers stored in the 
                //  first profile_COMMON object.

            auto& xts = const_cast<COLLADAFW::Effect*>(effect)->getExtraTextures();
            for (unsigned c=0; c<xts.getCount(); ++c) {
                auto xt = xts[c];
                if (!xt) { continue; }

                    // texture coords set and binding point are encoded together as a single string.
                std::regex decode("<([^>]*)><([^>]*)>");
                std::smatch match;
                auto r = std::regex_match(xt->texCoord, match, decode);
                if (r && match.size() >= 2) {
                    auto bindPoint = _importConfig.AsNativeBindingHash(match[2]);
                    if (bindPoint != 0) {
                        AddBoundTexture(
                            effect, 0, _objects, matSettings._resourceBindings,
                            bindPoint, xt->samplerId);
                    }
                }
            }

            Assets::MaterialParameters result;
            result._bindings = std::move(matSettings._resourceBindings);
            result._matParams = std::move(matSettings._matParamBox);
            result._stateSet = matSettings._stateSet;
            result._constants = std::move(matSettings._constants);

            _objects.Add(
                effect->getOriginalId(),
                effect->getName(),
                effect->getUniqueId(),
                std::move(result));
            return true;

        } CATCH(const FormatError& error) {
            HandleFormatError(error);
        } CATCH_END

        return true;
    }

    bool Writer::writeMaterial(const COLLADAFW::Material* material)
    {
        TRY {

            using namespace COLLADAFW;
            using namespace ColladaConversion;

            const UniqueId& effect = material->getInstantiatedEffect();
            _objects.Add(
                material->getOriginalId(),
                material->getName(),
                material->getUniqueId(),
                ReferencedMaterial(effect));
            /*const MaterialParameters* convertedMaterial = nullptr;
            ObjectId tableId = _objects.Get<MaterialParameters>(effect);
            if (tableId!=ObjectId_Invalid) {
                convertedMaterial = _objects.Get<MaterialParameters>(tableId);
            }

            if (convertedMaterial) {

                    //
                    //      No further changes to this material.
                    //      Just copy it back in under the new instantiated
                    //      id

                _objects.Add(
                    material->getOriginalId(),
                    material->getName(),
                    material->getUniqueId(),
                    MaterialParameters(*convertedMaterial));
            }*/
            return true;

        } CATCH(const FormatError& error) {
            HandleFormatError(error);
        } CATCH_END

        return true;
    }

    void Writer::CompleteProcessing()
    {
            //
            //      Any parameters in the transformation machine that don't have 
            //      animation drivers associated with them should get constant
            //      drivers. This is required because we still need to apply constant
            //      transforms in the skeleton information when applying this
            //      animation to an object.
            //
            //      Basically we're just shifting information from the skeleton into
            //      the animation set
            //
        auto defaultParameters = _skeleton.GetTransformationMachine().GetDefaultParameters();
        struct ParameterType { size_t count; AnimSamplerType samplerType; const void* arrayStart; };
        ParameterType parameterTypes[] = 
        {
            { defaultParameters.GetFloat4x4ParametersCount(),   Assets::TransformationParameterSet::Type::Float4x4, defaultParameters.GetFloat4x4Parameters() },
            { defaultParameters.GetFloat4ParametersCount(),     Assets::TransformationParameterSet::Type::Float4,   defaultParameters.GetFloat4Parameters() },
            { defaultParameters.GetFloat3ParametersCount(),     Assets::TransformationParameterSet::Type::Float3,   defaultParameters.GetFloat3Parameters() },
            { defaultParameters.GetFloat1ParametersCount(),     Assets::TransformationParameterSet::Type::Float1,   defaultParameters.GetFloat1Parameters() },
        };
        for (size_t t=0; t<dimof(parameterTypes); ++t) {
            for (size_t c=0; c<parameterTypes[t].count; ++c) {
                HashedColladaUniqueId name  = _skeleton.GetTransformationMachine().GetParameterName(parameterTypes[t].samplerType, (uint32)c);
                std::string stringName      = _skeleton.GetTransformationMachine().HashedIdToStringId(name);
                if (!_animationSet.HasAnimationDriver(stringName)) {
                    _animationSet.AddConstantDriver(
                        stringName, PtrAdd(parameterTypes[t].arrayStart, c*SamplerSize(parameterTypes[t].samplerType)),
                        parameterTypes[t].samplerType, 0);
                }
            }
        }
    }

    bool Writer::writeVisualScene(const COLLADAFW::VisualScene* visualScene)
    {
        TRY {
                //
                //      Here is a scene of nodes. Let's convert this into a format
                //      that's more useful to use.
                //          (Also remove any nodes that useless to us)
                //
            using namespace COLLADAFW;
            ColladaConversion::NascentModelCommandStream    commandStream;

            size_t rootNodeCount = visualScene->getRootNodes().getCount();
            JointReferences jointRefs;
            for (size_t c=0; c<rootNodeCount; ++c) {
                    //      are we really going to instantiate every skin controller we find?)
                    //      If we decide we don't need all of them, we can filter out some here.
                FindInstancedSkinControllers(*visualScene->getRootNodes()[c], _objects, jointRefs);
            }

            for (size_t c=0; c<rootNodeCount; ++c) {
                const Node* node = visualScene->getRootNodes()[c];
                if (ColladaConversion::IsUseful(*node, _objects, jointRefs)) {
                    _skeleton.PushNode(*node, _objects, jointRefs);
                    commandStream.PushNode(*node, _objects, jointRefs);
                }
            }

                //
                //      Second pass -- find and instantiate all of the referenced controllers
                //
            for (size_t c=0; c<rootNodeCount; ++c) {
                const Node* node = visualScene->getRootNodes()[c];
                commandStream.InstantiateControllers(*node, _objects, _objects);
            }

                //
                //      Now, read the animation links and add "AnimationDriver" objects as required
                //
            for (auto i=_animationLinks.begin(); i!=_animationLinks.end(); ++i) {
                ColladaConversion::ObjectId tableId = _objects.Get<Assets::RawAnimationCurve>(i->_animationId);
                if (tableId != ColladaConversion::ObjectId_Invalid) {
                    _animationSet.AddAnimationDriver(
                        _skeleton.GetTransformationMachine().HashedIdToStringId(i->_animationListName),
                        tableId, SamplerWidthToType(i->_samplerWidth), i->_samplerOffset);
                }
            }

            if (!commandStream.IsEmpty()) {
                _visualScene = std::move(commandStream);
            }
            return true;

        } CATCH(const FormatError& error) {
            HandleFormatError(error);
        } CATCH_END

        return true;
    }

    bool Writer::writeLibraryNodes ( const COLLADAFW::LibraryNodes* libraryNodes )
    {
        TRY {

            using namespace COLLADAFW;

            size_t rootNodeCount = libraryNodes->getNodes().getCount();

            JointReferences instancedSkinControllers;
            for (size_t c=0; c<rootNodeCount; ++c) {
                    //      are we really going to instantiate every skin controller we find?)
                    //      If we decide we don't need all of them, we can filter out some here.
                FindInstancedSkinControllers(*libraryNodes->getNodes()[c], _objects, instancedSkinControllers);
            }

            for (size_t c=0; c<rootNodeCount; ++c) {
                const Node* node = libraryNodes->getNodes()[c];
                if (IsUseful(*node, _objects, instancedSkinControllers)) {
                    ColladaConversion::NascentModelCommandStream    commandStream;
                    commandStream.PushNode(*node, _objects, instancedSkinControllers);

                    _objects.Add(   node->getOriginalId(),
                                    node->getName(),
                                    node->getUniqueId(),
                                    std::move(commandStream));
                }
            }

        } CATCH(const FormatError& error) {
            HandleFormatError(error);
        } CATCH_END

        return true;
    }

    bool Writer::writeSkinControllerData( const COLLADAFW::SkinControllerData* skinControllerData )
    {
        TRY {

            ColladaConversion::UnboundSkinController unboundController = 
                ColladaConversion::Convert(skinControllerData);

            _objects.Add(   skinControllerData->getOriginalId(),
                            skinControllerData->getName(),
                            skinControllerData->getUniqueId(),
                            std::move(unboundController));

            return true;

        } CATCH(const FormatError& error) {
            HandleFormatError(error);
        } CATCH_END

        return true;
    }

    bool Writer::writeController( const COLLADAFW::Controller* controller )
    {
        TRY {

                //
                //      A controller is a binding between some raw geometry and
                //      an animation. 
                //
                //      In the skinning case, 
                //          We need to create a list of joints & their names
                //          We also need to build vertex buffer information for
                //          skinning weights.
                //
                //      Let's put the skinning weights in a vertex buffer separate
                //      from the name vertex buffer. This will allow us to (theoretically
                //      attach multiple skeletons to the same basic geometry)
                //          -- and also render some geometry without skinning enabled.
                //

            using namespace COLLADAFW;
            if (controller->getControllerType() == Controller::CONTROLLER_TYPE_SKIN) {

                const COLLADAFW::SkinController* skinController = objectSafeCast<const COLLADAFW::SkinController>(controller);
                COLLADAFW::UniqueId id = skinController->getSkinControllerData();

                    //  Find this id within our table of objects
                ColladaConversion::ObjectId tableId = _objects.Get<ColladaConversion::UnboundSkinController>(id);
                const ColladaConversion::UnboundSkinController* obj = nullptr;
                if (tableId != ColladaConversion::ObjectId_Invalid) {
                    obj = _objects.GetFromObjectId<ColladaConversion::UnboundSkinController>(tableId);
                }

                if (obj==nullptr) {
                    Warning("Missing skin controller data for skin controller. There must have been a failure while processing this object!\n");
                    return false;
                }

                    //
                    //      The "controller" now has a list of joint ids to attach to the 
                    //      unbound controller data. 
                    //
                    //      This is an extra step only required for OpenCollada... The basic
                    //      Collada standard has all the joint ids already in the original skin object.
                    //      (but maybe it's similar to the "skeleton" attribute of the instance_controller...?
                    //
                    //      For our purposes, we need to match the joint ids up with output matrices
                    //      (ie, the index of the matrix written by the transformation machine)
                    //
                using namespace ColladaConversion;
                UnboundSkinControllerAndAttachedSkeleton result;
                result._unboundControllerId = tableId;
                result._jointIds.reserve(skinController->getJoints().getCount());
                result._source = skinController->getSource();
                for (size_t c=0; c<skinController->getJoints().getCount(); ++c)
                    result._jointIds.push_back(AsHashedColladaUniqueId(skinController->getJoints()[c]));

                _objects.Add("UnnamedSkinController", "UnnamedSkinController", skinController->getUniqueId(), std::move(result));
                return true;

            } else if (controller->getControllerType() == Controller::CONTROLLER_TYPE_MORPH) {

                    //
                    //      Sometimes we get simple morph controllers with just a single target. In this case,
                    //      we can just treat it like static geometry
                    //
                const COLLADAFW::MorphController* morphController = objectSafeCast<const COLLADAFW::MorphController>(controller);
                if (morphController->getMorphTargets().getCount() > 0) {
                    UnboundMorphController newController;
                    newController._source = morphController->getMorphTargets()[0];
                    _objects.Add("UnnamedMorphController", "UnnamedMorphController", morphController->getUniqueId(), std::move(newController));
                    return true;
                }

            } else {
                ThrowException(FormatError(
                    "Unknown controller type found (%s). This isn't currently supported!", controller->getUniqueId().toAscii().c_str()));
            }

            return true;

        } CATCH(const FormatError& error) {
            HandleFormatError(error);
        } CATCH_END

        return true;
    }

    bool Writer::writeAnimation(const COLLADAFW::Animation* animation)
    {
        TRY {

            using namespace ColladaConversion;
            Assets::RawAnimationCurve convertedCurve = Convert(*animation);
            _objects.Add(   animation->getOriginalId(),
                            animation->getName(),
                            animation->getUniqueId(),
                            std::move(convertedCurve));
            return true;

        } CATCH (const FormatError& error) {
            HandleFormatError(error);
        } CATCH_END

        return true;
    }

    std::pair<int, int> AsInterpolatorType(COLLADAFW::AnimationList::AnimationClass input)
    {
        switch (input) {
        case COLLADAFW::AnimationList::TIME:
        case COLLADAFW::AnimationList::ANGLE:
        case COLLADAFW::AnimationList::FLOAT:               return std::make_pair(1, 0);
        case COLLADAFW::AnimationList::COLOR_R:             return std::make_pair(1, 0);
		case COLLADAFW::AnimationList::COLOR_G:             return std::make_pair(1, 1);
		case COLLADAFW::AnimationList::COLOR_B:             return std::make_pair(1, 2);
		case COLLADAFW::AnimationList::COLOR_A:             return std::make_pair(1, 3);
        case COLLADAFW::AnimationList::POSITION_X:          return std::make_pair(1, 0);
		case COLLADAFW::AnimationList::POSITION_Y:          return std::make_pair(1, 1);
		case COLLADAFW::AnimationList::POSITION_Z:          return std::make_pair(1, 2);
        
        case COLLADAFW::AnimationList::POSITION_XYZ:  
        case COLLADAFW::AnimationList::COLOR_RGB:           return std::make_pair(3, 0);

        case COLLADAFW::AnimationList::COLOR_RGBA:    
        case COLLADAFW::AnimationList::AXISANGLE:           return std::make_pair(4, 0);
		
        case COLLADAFW::AnimationList::MATRIX4X4:           return std::make_pair(16, 0);
		
        default:
        case COLLADAFW::AnimationList::UNKNOWN_CLASS:
        case COLLADAFW::AnimationList::ARRAY_ELEMENT_1D:
        case COLLADAFW::AnimationList::ARRAY_ELEMENT_2D:    return std::make_pair(0, 0);
        }
    }

	bool Writer::writeAnimationList(const COLLADAFW::AnimationList* animationList)
    {
        TRY  {

                    //
                    //      We can use this to attach an animation curve to a specific 
                    //      parameter in the command stream. Each parameter in the 
                    //      command stream has a parameter has value that represents
                    //      the unique id of the animation list. So there should be
                    //      a fairly direct attachment.
                    //          (but does this become before or after we write
                    //          the node tree?)
                    //

            for (unsigned c=0; c<animationList->getAnimationBindings().getCount(); ++c) {
                const COLLADAFW::AnimationList::AnimationBinding& sourceBinding = animationList->getAnimationBindings()[c];
                auto interpolatorType = AsInterpolatorType(sourceBinding.animationClass);
                AnimationLink animationLink(
                    ColladaConversion::AsHashedColladaUniqueId(animationList->getUniqueId()),
                    sourceBinding.animation, 
                    interpolatorType.first, interpolatorType.second);

                        //      If we've build the visual scene already, hook up the animation driver
                ColladaConversion::ObjectId tableId = _objects.Get<Assets::RawAnimationCurve>(animationLink._animationId);
                if (tableId != ColladaConversion::ObjectId_Invalid) {
                    std::string stringId = _skeleton.GetTransformationMachine().HashedIdToStringId(animationLink._animationListName);
                    if (stringId.empty()) {
                        Warning("Couldn't bind animation driver. Sometimes this happens when there is an unsupported animation type (eg, animated material parameters)\n");
                    } else {
                        if (XlFindStringI(stringId.c_str(), "rope")) {
                            continue;   // ignore everything applying to "rope" nodes
                        }
                        _animationSet.AddAnimationDriver(
                            stringId, tableId, SamplerWidthToType(animationLink._samplerWidth), animationLink._samplerOffset);
                    }
                }

                _animationLinks.push_back(animationLink);
            }

            return true;

        } CATCH (const FormatError& error) {
            HandleFormatError(error);
        } CATCH_END

        return true;
    }

    class ValidationErrorHandler : public COLLADASaxFWL::IErrorHandler
    {
    public:
	    ValidationErrorHandler();
	    virtual ~ValidationErrorHandler();

	    bool    handleError(const COLLADASaxFWL::IError* error);
	    bool    hasHandledSaxParserError() const    { return mHasHandledSaxParserError; }
	    bool    hasHandledSaxFWLError() const       { return mHasHandledSaxFWLError; }
	    bool    getFileNotFound() const             { return mFileNotFound; }

    private:
	    /** Disable default copy ctor. */
	    ValidationErrorHandler( const ValidationErrorHandler& pre );
	    /** Disable default assignment operator. */
	    const ValidationErrorHandler& operator= ( const ValidationErrorHandler& pre );

        bool mHasHandledSaxParserError;
	    bool mHasHandledSaxFWLError;
	    bool mFileNotFound;
    };

    //--------------------------------------------------------------------
    ValidationErrorHandler::ValidationErrorHandler()
	: mHasHandledSaxParserError(false)
	, mHasHandledSaxFWLError(false)
	, mFileNotFound(false)
    {
    }

    //--------------------------------------------------------------------
    ValidationErrorHandler::~ValidationErrorHandler()
    {
    }

    //--------------------------------------------------------------------
    bool ValidationErrorHandler::handleError( const COLLADASaxFWL::IError* error )
    {
        if (error->getSeverity() != COLLADASaxFWL::IError::SEVERITY_ERROR_NONCRITICAL) {
            int t = 0;
            (void)t;
        }

	    if ( error->getErrorClass() == COLLADASaxFWL::IError::ERROR_SAXPARSER )
	    {
		    COLLADASaxFWL::SaxParserError* saxParserError = (COLLADASaxFWL::SaxParserError*) error;
		    const GeneratedSaxParser::ParserError& parserError = saxParserError->getError();

		    // Workaround to avoid wrong error
		    if ( parserError.getErrorType() == GeneratedSaxParser::ParserError::ERROR_VALIDATION_MIN_OCCURS_UNMATCHED)
		    {
			    if ( strcmp(parserError.getElement(), "effect") == 0 )
			    {
				    return false;
			    }
		    }
		    if ( parserError.getErrorType() == GeneratedSaxParser::ParserError::ERROR_VALIDATION_SEQUENCE_PREVIOUS_SIBLING_NOT_PRESENT)
		    {
			    if (    (strcmp(parserError.getElement(), "extra") == 0) 
				    &&  (strcmp(parserError.getAdditionalText().c_str(), "sibling: fx_profile_abstract") == 0)) 
			    {

				    return false;
			    }
		    }

		    if ( parserError.getErrorType() == GeneratedSaxParser::ParserError::ERROR_COULD_NOT_OPEN_FILE)
		    {
			    mFileNotFound = true;
		    }

		    Warning("Schema validation error: (%s)\n", parserError.getErrorMessage().c_str());
		    mHasHandledSaxParserError = true;
	    }
	    else if ( error->getErrorClass() == COLLADASaxFWL::IError::ERROR_SAXFWL )
	    {
		    COLLADASaxFWL::SaxFWLError* saxFWLError = (COLLADASaxFWL::SaxFWLError*) error;
		    Warning("Sax FWL Error: (%s)\n", saxFWLError->getErrorMessage().c_str());
		    mHasHandledSaxFWLError = true;
	    }
	    return false;
    }

    template<typename Type>
        Float4x4 AsFloat4x4(const Type& type)
        {
            Float4x4 result = Identity<Float4x4>();
            Combine_InPlace(type, result);
            return result;
        }

    class ExtraDataCallback : public COLLADASaxFWL::IExtraDataCallbackHandler
    {
    public:
        typedef COLLADASaxFWL::ParserChar ParserChar;
        typedef COLLADASaxFWL::StringHash StringHash;

        bool elementBegin(const ParserChar* elementName, const GeneratedSaxParser::xmlChar** attributes);
        bool elementEnd(const ParserChar* elementName);
        bool textData(const ParserChar* text, size_t textLength);
        bool parseElement(
            const ParserChar* profileName, 
            const StringHash& elementHash, 
            const COLLADAFW::UniqueId& uniqueId,
			COLLADAFW::Object* object);

        ExtraDataCallback();
		~ExtraDataCallback();

        class ExtraTexture
        {
        public:
            COLLADAFW::UniqueId     _attached;
            std::string             _bindingPoint;
            std::string             _samplerName;
            std::string             _textureCoordsChannel;
        };
        std::vector<ExtraTexture> _extraTextures;

    protected:
        std::stack<std::string>     _stackOfElements;
        COLLADAFW::UniqueId         _attachedObject;
        COLLADAFW::Effect*          _attachedObjectPtr;
    };

    bool ExtraDataCallback::elementBegin(const ParserChar* elementName, const GeneratedSaxParser::xmlChar** attributes) 
    { 
        if (!XlCompareStringI(elementName, "texture")) {
                //  this is an extra texture definition
                //  we should get a "texture" attribute with a reference to a sampler
            ExtraTexture tex;
            tex._attached = _attachedObject;
            if (!_stackOfElements.empty()) { tex._bindingPoint = _stackOfElements.top(); }
            for (auto a = attributes; *a && *(a+1); a+=2) {
                if (!XlCompareStringI(*a, "texture")) {
                    tex._samplerName = *(a+1);
                }
                if (!XlCompareStringI(*a, "texcoord")) {
                    tex._textureCoordsChannel = *(a+1);
                }
            }
            _extraTextures.push_back(tex);

            if (_attachedObjectPtr) {
                auto* extraTex = _attachedObjectPtr->createExtraTextureAttributes();
                if (extraTex) {
                    extraTex->textureMapId = 0;     // gets filled in by handleExtraEffectTextures, based on texCoord string (getTextureMapIdBySematic(tex._parameterName);)
                    extraTex->samplerId = 0;        // gets filled in by handleExtraEffectTextures
                    extraTex->textureSampler = tex._samplerName;
                    extraTex->texCoord = "<" + tex._textureCoordsChannel + "><" + tex._bindingPoint + ">";
                }
            }
        }

        _stackOfElements.push(elementName);
        return true; 
    }
    
    bool ExtraDataCallback::elementEnd(const ParserChar* elementName) 
    {
        assert(!XlCompareStringI(_stackOfElements.top().c_str(), elementName));
        _stackOfElements.pop();
        return true; 
    }

    bool ExtraDataCallback::textData(const ParserChar* text, size_t textLength) 
    { 
        return false; 
    }

    bool ExtraDataCallback::parseElement(
        const ParserChar* profileName, 
        const StringHash& elementHash, 
        const COLLADAFW::UniqueId& uniqueId,
        COLLADAFW::Object* object)
    {
        assert(_stackOfElements.empty());
        _attachedObjectPtr = nullptr;
        _attachedObject = COLLADAFW::UniqueId::INVALID;

            //  We can use "elementHash" to determine the type
            //  of the parent element. We get two common cases:
            //      HASH_ELEMENT_TECHNIQUE
            //      HASH_ELEMENT_EFFECT
        if (    elementHash == COLLADASaxFWL14::HASH_ELEMENT_TECHNIQUE
            ||  elementHash == COLLADASaxFWL15::HASH_ELEMENT_TECHNIQUE) {

            if (uniqueId.getClassId() == COLLADAFW::COLLADA_TYPE::EFFECT) {

                    //  "OpenCOLLADA3dsMax" element inside <technique> in an effect will
                    //  be a list of extra textures we need.
                if (!XlCompareStringI(profileName, "OpenCOLLADA3dsMax")) {
                    _attachedObject = uniqueId;
                    // assert(dynamic_cast<COLLADAFW::Effect*>(object) == static_cast<COLLADAFW::Effect*>(object));
                    _attachedObjectPtr = static_cast<COLLADAFW::Effect*>(object);    // have to do a cast here
                    return true;
                }
            }
        }

        return false;
    }

    ExtraDataCallback::ExtraDataCallback() 
    {
        _attachedObjectPtr = nullptr;
        _attachedObject = COLLADAFW::UniqueId::INVALID;
    }

	ExtraDataCallback::~ExtraDataCallback() {}

    NascentModel::NascentModel(const ResChar identifier[])
    {
            ////////////////////
        #if 0   // (some basic checks for transform math functions)
            {
                Float4x4 transform = Identity<Float4x4>();
                Combine_InPlace(Float3(0.18012f, 0.62315f, 0.33130f),                   transform);
                Combine_InPlace(RotationX(-90.0f),                                      transform);
                Combine_InPlace(RotationY(0.0f),                                        transform);
                Combine_InPlace(RotationZ(-82.46461f),                                  transform);
                Combine_InPlace(ArbitraryScale(Float3(0.11485f, 0.11485f, 0.15507f)),   transform);

                const Float3 demoPoint(2.f, 1.5f, 3.f);
                Float4 R1 = transform * Expand(demoPoint, 1.f);

                Float4 R2 = AsFloat4x4(ArbitraryScale(Float3(0.11485f, 0.11485f, 0.15507f))) * Expand(demoPoint, 1.f);
                R2 = AsFloat4x4(RotationZ(-82.46461f)) * R2;
                R2 = AsFloat4x4(RotationY(0.0f)) * R2;
                R2 = AsFloat4x4(RotationX(-90.0f)) * R2;
                R2 = AsFloat4x4(Float3(0.18012f, 0.62315f, 0.33130f)) * R2;

                    // [4x1] = [4x4][4x4][4x4][4x1]

                Float4x4 rotX1 = Identity<Float4x4>(); Combine_InPlace(RotationX(-82.46461f), rotX1);
                Float4x4 rotX2 = Combine(MakeRotationMatrix(Float3(1,0,0), -82.46461f),         Identity<Float4x4>());
            
                Float4x4 rotY1 = Identity<Float4x4>(); Combine_InPlace(RotationY(-82.46461f), rotY1);
                Float4x4 rotY2 = Combine(MakeRotationMatrix(Float3(0,1,0), -82.46461f),         Identity<Float4x4>());
            
                Float4x4 rotZ1 = Identity<Float4x4>(); Combine_InPlace(RotationZ(-82.46461f), rotZ1);
                Float4x4 rotZ2 = Combine(MakeRotationMatrix(Float3(0,0,1), -82.46461f),         Identity<Float4x4>());

                Float4x4 cameraToWorld = Identity<Float4x4>();
                Combine_InPlace(Float3(2.22403f, -1.63879f, 1.98092f),  cameraToWorld);
                Combine_InPlace(RotationZ(-44.29106f),                  cameraToWorld);
                Combine_InPlace(RotationY(-2.38208f),                   cameraToWorld);
                Combine_InPlace(RotationX(60.80816f),                   cameraToWorld);
                Float4x4 worldToCamera = InvertOrthonormalTransform(cameraToWorld);

                Float4x4 shouldBeIdentity = Combine(cameraToWorld, worldToCamera);

                #pragma warning(disable:4189)
                int c = 0;
            }
        #endif

        auto extraDataCallback = std::make_unique<ExtraDataCallback>();

            ////////////////////
        if (identifier) {
            ValidationErrorHandler  errorHandler;
            COLLADASaxFWL::Loader   loader(&errorHandler);
	        loader.registerExtraDataCallbackHandler(extraDataCallback.get());

            auto searchRules = ::Assets::DefaultDirectorySearchRules(identifier);

            ResChar baseName[MaxPath];
            XlBasename(baseName, dimof(baseName), identifier);
            XlChopExtension(baseName);
            
            Writer writer(baseName, searchRules);
	        COLLADAFW::Root root(&loader, &writer);
            
	        root.loadDocument(identifier);
            writer.CompleteProcessing();

            _objects        = std::move(writer._objects);
            _visualScene    = std::move(writer._visualScene);
            _animationSet   = std::move(writer._animationSet);
            _skeleton       = std::move(writer._skeleton);

            ConsoleRig::GetWarningStream().Flush();
        }

        if (identifier) {
            _name = identifier;
        }
        _extraDataCallback = std::move(extraDataCallback);
    }

    NascentModel::~NascentModel()
    {
    }

    std::pair<Float3, Float3>       NascentModel::CalculateBoundingBox() const
    {
            //
            //      For all the parts of the model, calculate the bounding box.
            //      We just have to go through each vertex in the model, and
            //      transform it into model space, and calculate the min and max values
            //      found;
            //
        using namespace ColladaConversion;
        auto result = InvalidBoundingBox();
        const auto finalMatrices = 
            _skeleton.GetTransformationMachine().GenerateOutputTransforms(
                _animationSet.BuildTransformationParameterSet(0.f, nullptr, _skeleton, _objects));

            //
            //      Do the unskinned geometry first
            //

        for (auto i=_visualScene._geometryInstances.cbegin(); i!=_visualScene._geometryInstances.cend(); ++i) {
            const NascentModelCommandStream::GeometryInstance& inst = *i;

            const NascentRawGeometry*  geo = _objects.GetFromObjectId<NascentRawGeometry>(inst._id);
            if (!geo) continue;

            const Float4x4&     localToWorld = finalMatrices[inst._localToWorldId];
            const void*         vertexBuffer = geo->_vertices.get();
            const unsigned      vertexStride = geo->_mainDrawInputAssembly._vertexStride;

            Metal::InputElementDesc positionDesc = FindPositionElement(
                AsPointer(geo->_mainDrawInputAssembly._vertexInputLayout.begin()),
                geo->_mainDrawInputAssembly._vertexInputLayout.size());

            if (positionDesc._nativeFormat != Metal::NativeFormat::Unknown && vertexStride) {
                AddToBoundingBox(
                    result, vertexBuffer, vertexStride, 
                    geo->_vertices.size() / vertexStride, positionDesc, localToWorld);
            }
        }

            //
            //      Now also do the skinned geometry. But use the default pose for
            //      skinned geometry (ie, don't apply the skinning transforms to the bones).
            //      Obvious this won't give the correct result post-animation.
            //

        for (auto i=_visualScene._skinControllerInstances.cbegin(); i!=_visualScene._skinControllerInstances.cend(); ++i) {
            const NascentModelCommandStream::SkinControllerInstance& inst = *i;

            const NascentBoundSkinnedGeometry* controller = _objects.GetFromObjectId<NascentBoundSkinnedGeometry>(inst._id);
            if (!controller) continue;

            const Float4x4& localToWorld = finalMatrices[inst._localToWorldId];

                //  We can't get the vertex position data directly from the vertex buffer, because
                //  the "bound" object is already using an opaque hardware object. However, we can
                //  transform the local space bounding box and use that.

            const unsigned indices[][3] = 
            {
                {0,0,0}, {0,1,0}, {1,0,0}, {1,1,0},
                {0,0,1}, {0,1,1}, {1,0,1}, {1,1,1}
            };

            const Float3* A = (const Float3*)&controller->_localBoundingBox.first;
            for (unsigned c=0; c<dimof(indices); ++c) {
                Float3 position(A[indices[c][0]][0], A[indices[c][1]][1], A[indices[c][2]][2]);
                AddToBoundingBox(result, position, localToWorld);
            }
        }

        return result;
    }

    unsigned    NascentModel::CameraCount() const
    {
        return (unsigned)_visualScene._cameraInstances.size();
    }

    auto        NascentModel::Camera(unsigned index) const -> Techniques::CameraDesc
    {
            //
            //      We have to run the transformations machine to calculate
            //      the current state of the camera.
            //      But we don't have all of the animation parameters, etc...
            //
        if (index >= _visualScene._cameraInstances.size()) return Techniques::CameraDesc();
        auto finalMatrices = _skeleton.GetTransformationMachine().GenerateOutputTransforms(
            _skeleton.GetTransformationMachine().GetDefaultParameters());
        Techniques::CameraDesc result;
        result._cameraToWorld       = finalMatrices[_visualScene._cameraInstances[index]._localToWorldId];
        result._nearClip            = 0.1f;
        result._farClip             = 1000.f;
        result._verticalFieldOfView = Deg2Rad(34.8246f);
        return result;
    }

    static void DestroyChunkArray(const void* chunkArray) { delete[] (const NascentChunk*)chunkArray; }

    CONVERSION_API NascentChunkArray NascentModel::SerializeSkin() const
    {
        Serialization::NascentBlockSerializer serializer;
        std::vector<uint8> largeResourcesBlock;

        auto i = _skeleton.GetTransformationMachine().GetCommandStream();
        Assets::TraceTransformationMachine(ConsoleRig::GetWarningStream(), AsPointer(i.begin()), AsPointer(i.end()));
        ConsoleRig::GetWarningStream().Flush();

        Serialization::Serialize(serializer, _visualScene);
        _objects.SerializeSkin(serializer, largeResourcesBlock);

        auto boundingBox = CalculateBoundingBox();
        Serialization::Serialize(serializer, boundingBox.first);
        Serialization::Serialize(serializer, boundingBox.second);
        ConsoleRig::GetWarningStream().Flush();

        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_ModelScaffold, 0, _name.c_str(), size);
        Serialization::ChunkFile::ChunkHeader largeBlockChunk(
            RenderCore::Assets::ChunkType_ModelScaffoldLargeBlocks, 0, _name.c_str(), largeResourcesBlock.size());

        NascentChunkArray result(
            std::unique_ptr<NascentChunk[], Internal::CrossDLLDeletor>(
                new NascentChunk[2], Internal::CrossDLLDeletor(&DestroyChunkArray)),
            2);        
        result.first[0] = NascentChunk(scaffoldChunk, std::vector<uint8>(block.get(), PtrAdd(block.get(), size)));
        result.first[1] = NascentChunk(largeBlockChunk, std::move(largeResourcesBlock));
        return std::move(result);
    }

    CONVERSION_API NascentChunkArray NascentModel::SerializeAnimationSet() const
    {
        Serialization::NascentBlockSerializer serializer;

        Serialization::Serialize(serializer, _animationSet);
        _objects.SerializeAnimationSet(serializer);
        ConsoleRig::GetWarningStream().Flush();

        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_AnimationSet, 0, _name.c_str(), size);

        NascentChunkArray result(
            std::unique_ptr<NascentChunk[], Internal::CrossDLLDeletor>(
                new NascentChunk[1], Internal::CrossDLLDeletor(&DestroyChunkArray)),
            1);
        result.first[0] = NascentChunk(scaffoldChunk, std::vector<uint8>(block.get(), PtrAdd(block.get(), size)));
        return std::move(result);
    }

    CONVERSION_API NascentChunkArray NascentModel::SerializeSkeleton() const
    {
        Serialization::NascentBlockSerializer serializer;

        Serialization::Serialize(serializer, _skeleton);
        ConsoleRig::GetWarningStream().Flush();

        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_Skeleton, 0, _name.c_str(), size);

        NascentChunkArray result(
            std::unique_ptr<NascentChunk[], Internal::CrossDLLDeletor>(
                new NascentChunk[1], Internal::CrossDLLDeletor(&DestroyChunkArray)),
            1);
        result.first[0] = NascentChunk(scaffoldChunk, std::vector<uint8>(block.get(), PtrAdd(block.get(), size)));
        return std::move(result);
    }

    CONVERSION_API void         NascentModel::MergeAnimationData(const NascentModel& source, const char animationName[])
    {
        _animationSet.MergeAnimation(source._animationSet, animationName, source._objects, _objects);
    }

    static void DestroyModel(const void* model) { delete (NascentModel*)model; }

    CONVERSION_API std::unique_ptr<NascentModel, Internal::CrossDLLDeletor>    CreateModel(const ResChar identifier[])
    {
        return std::unique_ptr<NascentModel, Internal::CrossDLLDeletor>(
            std::make_unique<NascentModel>(identifier).release(), 
            Internal::CrossDLLDeletor(&DestroyModel));
    }

    extern char VersionString[];
    extern char BuildDateString[];

    CONVERSION_API std::pair<const char*, const char*>    GetVersionInformation()
    {
        return std::make_pair(VersionString, BuildDateString);
    }
}}

