// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentModel2.h"
#include "Scaffold.h"
#include "ModelCommandStream.h"
#include "SRawGeometry.h"
#include "SEffect.h"
#include "SCommandStream.h"
#include "ConversionUtil.h"
#include "ParsingUtil.h"    // for AsString
#include "../RenderCore/Assets/TransformationCommands.h"
#include "../RenderCore/Assets/ModelRunTime.h"
#include "../RenderCore/Assets/ModelRunTimeInternal.h"
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

    class NascentModel2
    {
    public:
        std::shared_ptr<DocumentScaffold> _doc;

        TableOfObjects _objects;
        
        NascentSkeleton _skeleton;
        NascentModelCommandStream _visualScene;

        NodeReferences _jointRefs;
        std::string _name;

        ImportConfiguration _cfg;
        std::unique_ptr<uint8[]> _fileData;
    };

    static void DestroyModel(const void* model) { delete (NascentModel2*)model; }

    CrossDLLPtr<NascentModel2> CreateModel2(const ::Assets::ResChar identifier[])
    {
        CrossDLLPtr<NascentModel2> result(
            new NascentModel2,
            Internal::CrossDLLDeletor2(&DestroyModel));

        result->_cfg = ImportConfiguration("colladaimport.cfg");

        size_t size;
        result->_fileData = LoadFileAsMemoryBlock(identifier, &size);
        XmlInputStreamFormatter<utf8> formatter(
            MemoryMappedInputStream(result->_fileData.get(), PtrAdd(result->_fileData.get(), size)));

        result->_name = identifier;
        result->_doc = std::make_shared<ColladaConversion::DocumentScaffold>();
        result->_doc->Parse(formatter);

        ::ColladaConversion::URIResolveContext resolveContext(result->_doc);

        const auto& effects = result->_doc->_effects;
        for (auto i=effects.cbegin(); i!=effects.cend(); ++i) {
            TRY
            {
                auto obj = Convert(*i, resolveContext, result->_cfg);
                result->_objects.Add(
                    i->GetId().GetHash(),
                    AsString(i->GetName()), AsString(i->GetId().GetOriginal()),
                    std::move(obj));
            } CATCH (...) {
            } CATCH_END
        }

        const auto& geos = result->_doc->_geometries;
        for (auto i=geos.cbegin(); i!=geos.cend(); ++i) {
            TRY
            {
                auto obj = Convert(*i, resolveContext);
                result->_objects.Add(
                    i->GetId().GetHash(),
                    AsString(i->GetName()), AsString(i->GetId().GetOriginal()),
                    std::move(obj));
            } CATCH (...) {
            } CATCH_END
        }

        const auto* scene = result->_doc->FindVisualScene(
            GuidReference(result->_doc->_visualScene)._id);
        if (!scene)
            Throw(::Assets::Exceptions::FormatError("No visual scene found"));

        FindImportantNodes(result->_jointRefs, *scene);

        RenderCore::ColladaConversion::BuildSkeleton(
            result->_skeleton, 
            scene->GetRootNode(), 
            result->_jointRefs);

        for (unsigned c=0; c<scene->GetInstanceGeometryCount(); ++c)
            RenderCore::ColladaConversion::InstantiateGeometry(
                result->_visualScene,
                scene->GetInstanceGeometry(c),
                scene->GetInstanceGeometry_Attach(c),
                resolveContext,
                result->_objects, result->_jointRefs);
        
        return std::move(result);
    }

    
    
    static void DestroyChunkArray(const void* chunkArray) { delete[] (const NascentChunk2*)chunkArray; }

    NascentChunkArray2 SerializeSkin2(const NascentModel2& model)
    {
        Serialization::NascentBlockSerializer serializer;
        std::vector<uint8> largeResourcesBlock;

        auto i = model._skeleton.GetTransformationMachine().GetCommandStream();
        Assets::TraceTransformationMachine(
            ConsoleRig::GetWarningStream(), 
            AsPointer(i.begin()), AsPointer(i.end()));
        ConsoleRig::GetWarningStream().Flush();

        Serialization::Serialize(serializer, model._visualScene);
        model._objects.SerializeSkin(serializer, largeResourcesBlock);

        Serialization::Serialize(serializer, model._skeleton);

            // Generate the default transforms and serialize them out
            // unfortunately this requires we use the run-time types to
            // work out the transforms.
            // And that requires a bit of hack to get pointers to those 
            // run-time types
        {
            auto tempBlock = serializer.AsMemoryBlock();
            using namespace RenderCore::Assets;

            Serialization::Block_Initialize(tempBlock.get());
            auto* immData = (const ModelImmutableData*)Serialization::Block_GetFirstObject(tempBlock.get());

            const auto& transMachine = immData->_embeddedSkeleton;
            auto defTransformCount = transMachine.GetOutputMatrixCount();
            auto skeletonOutput = std::make_unique<Float4x4[]>(defTransformCount);
            transMachine.GenerateOutputTransforms(
                skeletonOutput.get(), defTransformCount,
                &transMachine.GetDefaultParameters());

            RenderCore::Assets::SkeletonBinding skelBinding(
                transMachine.GetOutputInterface(), 
                immData->_visualScene.GetInputInterface());

            auto finalMatrixCount = immData->_visualScene.GetInputInterface()._jointCount;
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

            auto boundingBox = CalculateBoundingBox(
                model._visualScene, model._objects, reordered.get(), &reordered[finalMatrixCount]);
            Serialization::Serialize(serializer, boundingBox.first);
            Serialization::Serialize(serializer, boundingBox.second);
            
            immData->~ModelImmutableData();
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

    NascentChunkArray2 SerializeSkeleton2(const NascentModel2& model)
    {
        Serialization::NascentBlockSerializer serializer;

        Serialization::Serialize(serializer, model._skeleton);
        ConsoleRig::GetWarningStream().Flush();

        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_Skeleton, 0, model._name.c_str(), unsigned(size));

        NascentChunkArray2 result(
            std::unique_ptr<NascentChunk2[], Internal::CrossDLLDeletor2>(
                new NascentChunk2[1], Internal::CrossDLLDeletor2(&DestroyChunkArray)),
            1);
        result.first[0] = NascentChunk2(scaffoldChunk, std::vector<uint8>(block.get(), PtrAdd(block.get(), size)));
        return std::move(result);
    }

    NascentChunkArray2 SerializeMaterials2(const NascentModel2& model)  
    { 
        std::string matSettingsFile;
        {
            ::Assets::ResChar settingsName[MaxPath];
            XlBasename(settingsName, dimof(settingsName), model._name.c_str());
            XlChopExtension(settingsName);
            XlCatString(settingsName, dimof(settingsName), ".material");
            matSettingsFile = settingsName;
        }

        // auto table = model._objects.SerializeMaterial(matSettingsFile);
        ::ColladaConversion::URIResolveContext resolveContext(model._doc);

        std::vector<std::unique_ptr<Data>> table;
        const auto& mats = model._doc->_materials;
        for (auto m=mats.cbegin(); m!=mats.cend(); ++m) {
            GuidReference effect(m->_effectReference);
            auto* rawMat = model._objects.Get<RenderCore::Assets::RawMaterial>(
                ObjectGuid(effect._id, effect._fileHash));
            if (rawMat) {
                auto newBlock = rawMat->SerializeAsData();
                newBlock->SetValue(AsString(m->_name).c_str());
                table.push_back(std::move(newBlock));
            }
        }

        MemoryOutputStream<uint8> strm;
        auto root = std::make_unique<Data>();
        for (auto i=table.begin(); i!=table.end(); ++i) {
            root->Add(i->release());
        }
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

    void         MergeAnimationData(NascentModel2& dest, const NascentModel2& source, const char animationName[])
    {
        // dest._animationSet.MergeAnimation(source._animationSet, animationName, source._objects, dest._objects);
    }
}}

