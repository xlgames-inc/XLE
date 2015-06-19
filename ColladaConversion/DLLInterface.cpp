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
#include "../Utility/Streams/Data.h"
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
        std::unique_ptr<uint8[]> _fileData;
        std::shared_ptr<DocumentScaffold> _doc;
        ::ColladaConversion::URIResolveContext _resolveContext;
    };

    static void DestroyModel(const void* model) { delete (ColladaScaffold*)model; }
    static void DestroyChunkArray(const void* chunkArray) { delete[] (const NascentChunk2*)chunkArray; }

    CrossDLLPtr<ColladaScaffold> CreateColladaScaffold(const ::Assets::ResChar identifier[])
    {
        CrossDLLPtr<ColladaScaffold> result(
            new ColladaScaffold,
            Internal::CrossDLLDeletor2(&DestroyModel));

        result->_cfg = ImportConfiguration("colladaimport.cfg");

        if (!identifier) return std::move(result);

        size_t size;
        result->_fileData = LoadFileAsMemoryBlock(identifier, &size);
        XmlInputStreamFormatter<utf8> formatter(
            MemoryMappedInputStream(result->_fileData.get(), PtrAdd(result->_fileData.get(), size)));

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
            TRY {
                _cmdStream.Add(
                    RenderCore::ColladaConversion::InstantiateController(
                        scene->GetInstanceController(c),
                        scene->GetInstanceController_Attach(c),
                        input._resolveContext, _geoObjects, jointRefs,
                        input._cfg));
            } CATCH(...) {
            } CATCH_END
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
            Serialization::Serialize(serializer, objs._rawGeos.size());
        }

        {
            Serialization::NascentBlockSerializer tempBlock;
            for (auto i = objs._skinnedGeos.begin(); i!=objs._skinnedGeos.end(); ++i) {
                i->second.Serialize(tempBlock, largeResourcesBlock);
            }
            serializer.SerializeSubBlock(tempBlock);
            Serialization::Serialize(serializer, objs._skinnedGeos.size());
        }
    }
  
    NascentChunkArray2 SerializeSkin2(const ColladaScaffold& model)
    {
        Serialization::NascentBlockSerializer serializer;
        std::vector<uint8> largeResourcesBlock;

        PreparedSkinFile skinFile(model);

        auto i = skinFile._skeleton.GetTransformationMachine().GetCommandStream();
        Assets::TraceTransformationMachine(
            ConsoleRig::GetWarningStream(), 
            AsPointer(i.begin()), AsPointer(i.end()));
        ConsoleRig::GetWarningStream().Flush();

        Serialization::Serialize(serializer, skinFile._cmdStream);
        SerializeSkin(serializer, largeResourcesBlock, skinFile._geoObjects);

        Serialization::Serialize(serializer, skinFile._skeleton);

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
            Serialization::Serialize(serializer, boundingBox.first);
            Serialization::Serialize(serializer, boundingBox.second);
            
            // immData->~ModelImmutableData();
        }

        ConsoleRig::GetWarningStream().Flush();

        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_ModelScaffold, 0, model._name.c_str(), unsigned(size));
        Serialization::ChunkFile::ChunkHeader largeBlockChunk(
            RenderCore::Assets::ChunkType_ModelScaffoldLargeBlocks, 0, model._name.c_str(), (unsigned)largeResourcesBlock.size());

        NascentChunkArray2 result(
            std::unique_ptr<NascentChunk2[], Internal::CrossDLLDeletor2>(
                new NascentChunk2[2], Internal::CrossDLLDeletor2(&DestroyChunkArray)),
            2);        
        result.first[0] = NascentChunk2(scaffoldChunk, std::vector<uint8>(block.get(), PtrAdd(block.get(), size)));
        result.first[1] = NascentChunk2(largeBlockChunk, std::move(largeResourcesBlock));

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

    NascentChunkArray2 SerializeSkeleton2(const ColladaScaffold& model)
    {
        Serialization::NascentBlockSerializer serializer;

        PreparedSkeletonFile skeleFile(model);

        Serialization::Serialize(serializer, skeleFile._skeleton);
        ConsoleRig::GetWarningStream().Flush();

        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_Skeleton, 0, 
            model._name.c_str(), unsigned(size));

        NascentChunkArray2 result(
            std::unique_ptr<NascentChunk2[], Internal::CrossDLLDeletor2>(
                new NascentChunk2[1], Internal::CrossDLLDeletor2(&DestroyChunkArray)),
            1);
        result.first[0] = NascentChunk2(scaffoldChunk, std::vector<uint8>(block.get(), PtrAdd(block.get(), size)));
        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PreparedMaterialFile
    {
    public:
        std::vector<std::unique_ptr<Data>> _table;

        PreparedMaterialFile(const ColladaScaffold& model);
    };

    PreparedMaterialFile::PreparedMaterialFile(const ColladaScaffold& model)
    {
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

            auto newBlock = i->second.SerializeAsData();
            newBlock->SetValue(AsString(m->_name).c_str());
            _table.push_back(std::move(newBlock));
        }
    }

    NascentChunkArray2 SerializeMaterials2(const ColladaScaffold& model)  
    { 
        std::string matSettingsFile;
        {
            ::Assets::ResChar settingsName[MaxPath];
            XlBasename(settingsName, dimof(settingsName), model._name.c_str());
            XlChopExtension(settingsName);
            XlCatString(settingsName, dimof(settingsName), ".material");
            matSettingsFile = settingsName;
        }

        PreparedMaterialFile preparedFile(model);

        MemoryOutputStream<uint8> strm;
        auto root = std::make_unique<Data>();
        for (auto i=preparedFile._table.begin(); i!=preparedFile._table.end(); ++i)
            root->Add(i->release());
        root->SaveToOutputStream(strm);

            // convert into a chunk...

        auto finalSize = size_t(strm.GetBuffer().End()) - size_t(strm.GetBuffer().Begin());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_RawMat, 0, 
            model._name.c_str(), Serialization::ChunkFile::SizeType(finalSize));

        NascentChunkArray2 result(
            std::unique_ptr<NascentChunk2[], Internal::CrossDLLDeletor2>(
                new NascentChunk2[1], Internal::CrossDLLDeletor2(&DestroyChunkArray)),
            1);
        result.first[0] = NascentChunk2(
            scaffoldChunk, 
            std::vector<uint8>(strm.GetBuffer().Begin(), strm.GetBuffer().End()));
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

    CrossDLLPtr<WorkingAnimationSet> CreateAnimationSet(const char name[])
    {
        return CrossDLLPtr<WorkingAnimationSet>(
            new WorkingAnimationSet(name),
            Internal::CrossDLLDeletor2(&DestroyWorkingAnimSet));
    }

    NascentChunkArray2 SerializeAnimationSet2(const WorkingAnimationSet& animSet)
    {
        Serialization::NascentBlockSerializer serializer;

        Serialization::Serialize(serializer, animSet._animationSet);
        serializer.SerializeSubBlock(AsPointer(animSet._curves.begin()), AsPointer(animSet._curves.end()));
        serializer.SerializeValue(animSet._curves.size());

        ConsoleRig::GetWarningStream().Flush();

        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_AnimationSet, 0, animSet._name.c_str(), unsigned(size));

        NascentChunkArray2 result(
            std::unique_ptr<NascentChunk2[], Internal::CrossDLLDeletor2>(
                new NascentChunk2[1], Internal::CrossDLLDeletor2(&DestroyChunkArray)),
            1);
        result.first[0] = NascentChunk2(scaffoldChunk, std::vector<uint8>(block.get(), PtrAdd(block.get(), size)));
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

