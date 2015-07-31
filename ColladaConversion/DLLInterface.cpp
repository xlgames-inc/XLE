// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DLLInterface.h"
#include "NascentCommandStream.h"
#include "NascentRawGeometry.h"
#include "NascentAnimController.h"

#include "Scaffold.h"
#include "ScaffoldParsingUtil.h"    // for AsString
#include "SkeletonRegistry.h"

#include "SEffect.h"
#include "SCommandStream.h"
#include "SAnimation.h"

#include "ConversionUtil.h"

#include "../RenderCore/Assets/ModelRunTimeInternal.h"      // just for RenderCore::Assets::SkeletonBinding
#include "../RenderCore/Assets/AssetUtils.h"

#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/XmlStreamFormatter.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/Streams/StreamTypes.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/PtrUtils.h"
#include "../ConsoleRig/OutputStream.h"
#include <memory>

namespace RenderCore { namespace ColladaConversion
{
    using namespace ::ColladaConversion;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ColladaScaffold
    {
    public:
        std::string _name;
        ImportConfiguration _cfg;
        MemoryMappedFile _fileData;
        std::shared_ptr<DocumentScaffold> _doc;
        ::ColladaConversion::URIResolveContext _resolveContext;
    };

    static void DestroyModel(const void* model) { delete (ColladaScaffold*)model; }
    static void DestroyChunkArray(const void* chunkArray) { delete (std::vector<NascentChunk>*)chunkArray; }

    std::shared_ptr<ColladaScaffold> CreateColladaScaffold(const ::Assets::ResChar identifier[])
    {
        std::shared_ptr<ColladaScaffold> result(new ColladaScaffold, &DestroyModel);

        result->_cfg = ImportConfiguration("colladaimport.cfg");
        result->_fileData = MemoryMappedFile(identifier, 0, MemoryMappedFile::Access::Read, BasicFile::ShareMode::Read);
        if (!result->_fileData.IsValid())
            Throw(::Exceptions::BasicLabel("Error opening file for read (%s)", identifier));

        XmlInputStreamFormatter<utf8> formatter(
            MemoryMappedInputStream(
                result->_fileData.GetData(), 
                PtrAdd(result->_fileData.GetData(), result->_fileData.GetSize())));

        result->_name = identifier;
        result->_doc = std::make_shared<ColladaConversion::DocumentScaffold>();
        result->_doc->Parse(formatter);

        result->_resolveContext = ::ColladaConversion::URIResolveContext(result->_doc);
        
        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PreparedSkinFile
    {
    public:
        NascentModelCommandStream _cmdStream;
        NascentGeometryObjects _geoObjects;
        NascentSkeleton _skeleton;

        PreparedSkinFile(const ColladaScaffold&);
    };

    PreparedSkinFile::PreparedSkinFile(const ColladaScaffold& input)
    {
        SkeletonRegistry jointRefs;

        const auto* scene = input._doc->FindVisualScene(
            GuidReference(input._doc->_visualScene)._id);
        if (!scene)
            Throw(::Assets::Exceptions::FormatError("No visual scene found"));

        for (unsigned c=0; c<scene->GetInstanceGeometryCount(); ++c) {
            TRY {
                _cmdStream.Add(
                    RenderCore::ColladaConversion::InstantiateGeometry(
                        scene->GetInstanceGeometry(c),
                        scene->GetInstanceGeometry_Attach(c),
                        input._resolveContext, _geoObjects, jointRefs,
                        input._cfg));
            } CATCH(...) {
            } CATCH_END
        }

        for (unsigned c=0; c<scene->GetInstanceControllerCount(); ++c) {
            bool skinSuccessful = false;
            TRY {
                _cmdStream.Add(
                    RenderCore::ColladaConversion::InstantiateController(
                        scene->GetInstanceController(c),
                        scene->GetInstanceController_Attach(c),
                        input._resolveContext, _geoObjects, jointRefs,
                        input._cfg));
                skinSuccessful = true;
            } CATCH(...) {
            } CATCH_END

            if (!skinSuccessful) {
                    // if we failed to instantiate this object as a skinned controller,
                    // we can try to fall back to a static geometry object. This fallback
                    // can be required for some controller objects that use rigid animation
                    //  -- they can have a skin controller with no joints (meaning at the 
                    //      only transform that can affect them is the parent node -- or maybe the skeleton root?)
                LogWarning << "Could not instantiate controller as a skinned object. Falling back to rigid object.";
                TRY {
                    _cmdStream.Add(
                        RenderCore::ColladaConversion::InstantiateGeometry(
                            scene->GetInstanceController(c),
                            scene->GetInstanceController_Attach(c),
                            input._resolveContext, _geoObjects, jointRefs,
                            input._cfg));
                } CATCH(...) {
                } CATCH_END
            }
        }

        using namespace RenderCore::ColladaConversion;
        BuildMinimalSkeleton(_skeleton, scene->GetRootNode(), jointRefs);
        RegisterNodeBindingNames(_skeleton, jointRefs);
        RegisterNodeBindingNames(_cmdStream, jointRefs);
    }

    static void SerializeSkin(
        Serialization::NascentBlockSerializer& serializer, 
        std::vector<uint8>& largeResourcesBlock,
        NascentGeometryObjects& objs)
    {
        {
            Serialization::NascentBlockSerializer tempBlock;
            for (auto i = objs._rawGeos.begin(); i!=objs._rawGeos.end(); ++i) {
                i->second.Serialize(tempBlock, largeResourcesBlock);
            }
            serializer.SerializeSubBlock(tempBlock);
            ::Serialize(serializer, objs._rawGeos.size());
        }

        {
            Serialization::NascentBlockSerializer tempBlock;
            for (auto i = objs._skinnedGeos.begin(); i!=objs._skinnedGeos.end(); ++i) {
                i->second.Serialize(tempBlock, largeResourcesBlock);
            }
            serializer.SerializeSubBlock(tempBlock);
            ::Serialize(serializer, objs._skinnedGeos.size());
        }
    }
  
    NascentChunkArray SerializeSkin(const ColladaScaffold& model)
    {
        Serialization::NascentBlockSerializer serializer;
        std::vector<uint8> largeResourcesBlock;

        PreparedSkinFile skinFile(model);

        auto i = skinFile._skeleton.GetTransformationMachine().GetCommandStream();
        Assets::TraceTransformationMachine(
            ConsoleRig::GetWarningStream(), 
            AsPointer(i.begin()), AsPointer(i.end()));
        ConsoleRig::GetWarningStream().Flush();

        ::Serialize(serializer, skinFile._cmdStream);
        SerializeSkin(serializer, largeResourcesBlock, skinFile._geoObjects);

        ::Serialize(serializer, skinFile._skeleton);

            // Generate the default transforms and serialize them out
            // unfortunately this requires we use the run-time types to
            // work out the transforms.
            // And that requires a bit of hack to get pointers to those 
            // run-time types
        {
            const auto& transMachine = skinFile._skeleton.GetTransformationMachine();
            auto skeletonOutput = transMachine.GenerateOutputTransforms(
                transMachine.GetDefaultParameters());

            auto skelOutputInterface = transMachine.GetOutputInterface();
            auto streamInputInterface = skinFile._cmdStream.GetInputInterface();
            RenderCore::Assets::SkeletonBinding skelBinding(
                RenderCore::Assets::TransformationMachine::OutputInterface
                    {AsPointer(skelOutputInterface.first.begin()), AsPointer(skelOutputInterface.second.begin()), skelOutputInterface.first.size()},
                RenderCore::Assets::ModelCommandStream::InputInterface
                    {AsPointer(streamInputInterface.begin()), streamInputInterface.size()});

            auto finalMatrixCount = streamInputInterface.size(); // immData->_visualScene.GetInputInterface()._jointCount;
            auto reordered = std::make_unique<Float4x4[]>(finalMatrixCount);
            for (size_t c = 0; c < finalMatrixCount; ++c) {
                auto machineOutputIndex = skelBinding._modelJointIndexToMachineOutput[c];
                if (machineOutputIndex == ~unsigned(0x0)) {
                    reordered[c] = Identity<Float4x4>();
                } else {
                    reordered[c] = skeletonOutput[machineOutputIndex];
                }
            }

                // if we have any non-identity internal transforms, then we should 
                // write a default set of transformations. But many models don't have any
                // internal transforms -- in this case all of the generated transforms
                // will be identity. If we find this case, they we should write zero
                // default transforms.
            bool hasNonIdentity = false;
            const float tolerance = 1e-6f;
            for (unsigned c=0; c<finalMatrixCount; ++c)
                hasNonIdentity |= !Equivalent(reordered[c], Identity<Float4x4>(), tolerance);
            if (!hasNonIdentity)
                finalMatrixCount = 0;

            serializer.SerializeSubBlock(reordered.get(), &reordered[finalMatrixCount]);
            serializer.SerializeValue(finalMatrixCount);

            auto boundingBox = skinFile._geoObjects.CalculateBoundingBox(
                skinFile._cmdStream, reordered.get(), &reordered[finalMatrixCount]);
            ::Serialize(serializer, boundingBox.first);
            ::Serialize(serializer, boundingBox.second);
            
            // immData->~ModelImmutableData();
        }

        ConsoleRig::GetWarningStream().Flush();

        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_ModelScaffold, 0, model._name.c_str(), unsigned(size));
        Serialization::ChunkFile::ChunkHeader largeBlockChunk(
            RenderCore::Assets::ChunkType_ModelScaffoldLargeBlocks, 0, model._name.c_str(), (unsigned)largeResourcesBlock.size());

        NascentChunkArray result(new std::vector<NascentChunk>, &DestroyChunkArray);
        result->push_back(NascentChunk(scaffoldChunk, std::vector<uint8>(block.get(), PtrAdd(block.get(), size))));
        result->push_back(NascentChunk(largeBlockChunk, std::move(largeResourcesBlock)));

        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PreparedSkeletonFile
    {
    public:
        NascentSkeleton _skeleton;

        PreparedSkeletonFile(const ColladaScaffold&);
    };

    PreparedSkeletonFile::PreparedSkeletonFile(const ColladaScaffold& input)
    {
        SkeletonRegistry jointRefs;

        const auto* scene = input._doc->FindVisualScene(
            GuidReference(input._doc->_visualScene)._id);
        if (!scene)
            Throw(::Assets::Exceptions::FormatError("No visual scene found"));

        using namespace RenderCore::ColladaConversion;
        BuildFullSkeleton(_skeleton, scene->GetRootNode(), jointRefs);
        RegisterNodeBindingNames(_skeleton, jointRefs);
    }

    NascentChunkArray SerializeSkeleton(const ColladaScaffold& model)
    {
        Serialization::NascentBlockSerializer serializer;

        PreparedSkeletonFile skeleFile(model);

        ::Serialize(serializer, skeleFile._skeleton);
        ConsoleRig::GetWarningStream().Flush();

        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_Skeleton, 0, 
            model._name.c_str(), unsigned(size));

        NascentChunkArray result(new std::vector<NascentChunk>, &DestroyChunkArray);
        result->push_back(NascentChunk(scaffoldChunk, std::vector<uint8>(block.get(), PtrAdd(block.get(), size))));
        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void SerializeMatTable(OutputStream& stream, const ColladaScaffold& model)
    {
        OutputStreamFormatter formatter(stream);

        std::vector<std::pair<ObjectGuid, Assets::RawMaterial>> compiledEffects;

        const auto& effects = model._doc->_effects;
        for (auto i=effects.cbegin(); i!=effects.cend(); ++i) {
            TRY
            {
                ObjectGuid id = i->GetId().GetHash();
                compiledEffects.insert(
                    LowerBound(compiledEffects, id), 
                    std::make_pair(id, Convert(*i, model._resolveContext, model._cfg)));
            } CATCH (...) {
            } CATCH_END
        }

        const auto& mats = model._doc->_materials;
        for (auto m=mats.cbegin(); m!=mats.cend(); ++m) {
            GuidReference effect(m->_effectReference);
            auto i = LowerBound(compiledEffects, ObjectGuid(effect._id, effect._fileHash));
            if (i == compiledEffects.end() || !(i->first == ObjectGuid(effect._id, effect._fileHash)))
                continue;

            auto ele = formatter.BeginElement(
                Conversion::Convert<std::basic_string<utf8>>(AsString(m->_name)).c_str());
            i->second.Serialize(formatter);
            formatter.EndElement(ele);
        }
    }

    NascentChunkArray SerializeMaterials(const ColladaScaffold& model)  
    { 
        std::string matSettingsFile;
        {
            ::Assets::ResChar settingsName[MaxPath];
            XlBasename(settingsName, dimof(settingsName), model._name.c_str());
            XlChopExtension(settingsName);
            XlCatString(settingsName, dimof(settingsName), ".material");
            matSettingsFile = settingsName;
        }

        MemoryOutputStream<uint8> strm;
        SerializeMatTable(strm, model);

            // convert into a chunk...

        auto finalSize = size_t(strm.GetBuffer().End()) - size_t(strm.GetBuffer().Begin());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_RawMat, 0, 
            model._name.c_str(), Serialization::ChunkFile::SizeType(finalSize));

        NascentChunkArray result(new std::vector<NascentChunk>(), &DestroyChunkArray);
        result->push_back(NascentChunk(
            scaffoldChunk, 
            std::vector<uint8>(strm.GetBuffer().Begin(), strm.GetBuffer().End())));
        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PreparedAnimationFile
    {
    public:
        NascentAnimationSet _animationSet;
        std::vector<Assets::RawAnimationCurve> _curves;

        PreparedAnimationFile(const ColladaScaffold&);
    };

    PreparedAnimationFile::PreparedAnimationFile(const ColladaScaffold& input)
    {
        SkeletonRegistry jointRefs;

        std::vector<UnboundAnimation> anims;
        const auto& animations = input._doc->_animations;
        for (auto i=animations.cbegin(); i!=animations.cend(); ++i) {
            TRY {
                auto anim = Convert(*i, input._resolveContext, jointRefs); 

                for (auto c=anim._curves.begin(); c!=anim._curves.end(); ++c) {
                    _curves.emplace_back(std::move(c->_curve));
                    _animationSet.AddAnimationDriver(
                        c->_parameterName, unsigned(_curves.size()-1),
                        c->_samplerType, c->_samplerOffset);
                }
            } CATCH (...) {
            } CATCH_END
        }
    }

    class WorkingAnimationSet
    {
    public:
        NascentAnimationSet _animationSet;
        std::vector<Assets::RawAnimationCurve> _curves;
        std::string _name;

        WorkingAnimationSet(const char name[]) : _name(name) {}
    };

    static void DestroyWorkingAnimSet(const void* model) { delete (WorkingAnimationSet*)model; }

    std::shared_ptr<WorkingAnimationSet> CreateAnimationSet(const char name[])
    {
        return std::shared_ptr<WorkingAnimationSet>(
            new WorkingAnimationSet(name),
            &DestroyWorkingAnimSet);
    }

    NascentChunkArray SerializeAnimationSet(const WorkingAnimationSet& animSet)
    {
        Serialization::NascentBlockSerializer serializer;

        ::Serialize(serializer, animSet._animationSet);
        serializer.SerializeSubBlock(AsPointer(animSet._curves.begin()), AsPointer(animSet._curves.end()));
        serializer.SerializeValue(animSet._curves.size());

        ConsoleRig::GetWarningStream().Flush();

        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_AnimationSet, 0, animSet._name.c_str(), unsigned(size));

        NascentChunkArray result(new std::vector<NascentChunk>(), &DestroyChunkArray);
        result->push_back(NascentChunk(scaffoldChunk, std::vector<uint8>(block.get(), PtrAdd(block.get(), size))));
        return std::move(result);
    }

    void ExtractAnimations(WorkingAnimationSet& dest, const ColladaScaffold& source, const char animationName[])
    {
        PreparedAnimationFile animFile(source);
        dest._animationSet.MergeAnimation(
            animFile._animationSet, animationName, 
            animFile._curves, dest._curves);
    }
}}

namespace RenderCore { namespace ColladaConversion
{
    extern char VersionString[];
    extern char BuildDateString[];
}}

ConsoleRig::LibVersionDesc GetVersionInformation()
{
    ConsoleRig::LibVersionDesc result;
    result._versionString = RenderCore::ColladaConversion::VersionString;
    result._buildDateString = RenderCore::ColladaConversion::BuildDateString;
    return result;
}

static ConsoleRig::AttachRef<ConsoleRig::GlobalServices> s_attachRef;

void AttachLibrary(ConsoleRig::GlobalServices& services)
{
    s_attachRef = services.Attach();
    LogInfo << "Attached Collada Compiler DLL: {" << RenderCore::ColladaConversion::VersionString << "} -- {" << RenderCore::ColladaConversion::BuildDateString << "}";
}

void DetachLibrary()
{
    s_attachRef.Detach();
    TerminateFileSystemMonitoring();
}

