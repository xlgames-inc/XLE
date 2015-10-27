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
#include "SCommandStream.h"

#include "ConversionUtil.h"

#include "../RenderCore/Assets/ModelImmutableData.h"      // just for RenderCore::Assets::SkeletonBinding
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

    static NascentChunkArray MakeNascentChunkArray(
        const std::initializer_list<NascentChunk>& inits)
    {
        return NascentChunkArray(
            new std::vector<NascentChunk>(inits),
            &DestroyChunkArray);
    }

    std::vector<uint8> AsVector(const Serialization::NascentBlockSerializer& serializer)
    {
        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());
        return std::vector<uint8>(block.get(), PtrAdd(block.get(), size));
    }

    // static std::vector<uint8> AsVector(MemoryOutputStream<>& stream)
    // {
    //     auto& buffer = stream.GetBuffer();
    //     return std::vector<uint8>((const uint8*)buffer.Begin(), (const uint8*)buffer.End());
    // }

    template<typename Char>
        static std::vector<uint8> AsVector(std::basic_stringstream<Char>& stream)
    {
        auto str = stream.str();
        return std::vector<uint8>((const uint8*)AsPointer(str.begin()), (const uint8*)AsPointer(str.end()));
    }

    template<typename Type>
        static std::vector<uint8> SerializeToVector(const Type& obj)
    {
        Serialization::NascentBlockSerializer serializer;
        ::Serialize(serializer, obj);
        return AsVector(serializer);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static Float4x4 BuildCoordinateTransform(const AssetDesc& assetDesc)
    {
        // When the "up" vector, or the units specified in the collada header
        // don't match our expectations, we can apply an extra transform.
        // This should transform some given input coordinates into our expected
        // final output.
        // Here, we will convert to 1.f unit == 1.f meter, with +Z being up.
        float scale = assetDesc._metersPerUnit;
        Float3x3 axisTransform;
        switch (assetDesc._upAxis) {
        case AssetDesc::UpAxis::X:
                // -Y --> +X
                // +Z --> -Y
                // +X --> +Z
            axisTransform = Float3x3(
                 0.f, -1.f,  0.f,
                 0.f,  0.f, -1.f,
                 1.f,  0.f,  0.f);
            break;

        case AssetDesc::UpAxis::Y:
                // +X --> +X
                // +Z --> -Y
                // +Y --> +Z
                //  hmm... winding flip...?
            axisTransform = Float3x3(
                 1.f,  0.f,   0.f,
                 0.f,  0.f,  -1.f,
                 0.f,  1.f,   0.f);
            break;

        default:
        case AssetDesc::UpAxis::Z:
            axisTransform = Identity<Float3x3>();
            break;
        }

        return AsFloat4x4(Float3x3(scale * axisTransform));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class TransMachineOptimizer : public Assets::ITransformationMachineOptimizer
    {
    public:
        bool CanMergeIntoOutputMatrix(unsigned outputMatrixIndex) const;
        void MergeIntoOutputMatrix(unsigned outputMatrixIndex, const Float4x4& transform);
        Float4x4 GetMergedOutputMatrix(unsigned outputMatrixIndex) const;

        TransMachineOptimizer(ReferencedGeometries& refGeos, unsigned outputMatrixCount);
        TransMachineOptimizer();
        ~TransMachineOptimizer();
    protected:
        std::vector<bool>       _canMergeIntoTransform;
        std::vector<Float4x4>   _mergedTransforms;
    };

    bool TransMachineOptimizer::CanMergeIntoOutputMatrix(unsigned outputMatrixIndex) const
    {
        if (outputMatrixIndex < unsigned(_canMergeIntoTransform.size()))
            return _canMergeIntoTransform[outputMatrixIndex];
        return false;
    }

    void TransMachineOptimizer::MergeIntoOutputMatrix(unsigned outputMatrixIndex, const Float4x4& transform)
    {
        assert(CanMergeIntoOutputMatrix(outputMatrixIndex));
        _mergedTransforms[outputMatrixIndex] = Combine(
            _mergedTransforms[outputMatrixIndex], transform);
    }

    Float4x4 TransMachineOptimizer::GetMergedOutputMatrix(unsigned outputMatrixIndex) const
    {
        if (outputMatrixIndex < unsigned(_mergedTransforms.size()))
            return _mergedTransforms[outputMatrixIndex];
        return Identity<Float4x4>();
    }

    TransMachineOptimizer::TransMachineOptimizer(ReferencedGeometries& refGeos, unsigned outputMatrixCount)
    {
        _canMergeIntoTransform.resize(outputMatrixCount, false);
        _mergedTransforms.resize(outputMatrixCount, Identity<Float4x4>());

        for (unsigned c=0; c<outputMatrixCount; ++c) {
                // if we've got a skin controller attached, we can't do any merging
            auto skinI = std::find_if(
                refGeos._skinControllers.cbegin(), refGeos._skinControllers.cend(),
                [c](const ReferencedGeometries::AttachedObject& obj) { return obj._outputMatrixIndex == c; });
            if (skinI != refGeos._skinControllers.cend()) continue;

                // check to see if we have at least one mesh attached...
            auto geoI = std::find_if(
                refGeos._meshes.cbegin(), refGeos._meshes.cend(),
                [c](const ReferencedGeometries::AttachedObject& obj) { return obj._outputMatrixIndex == c; });
            if (geoI == refGeos._meshes.cend()) continue;

                // find all of the meshes attached, and check if any are attached in
                // multiple places
            bool doublyAttachedObject = false;
            for (auto i=refGeos._meshes.cbegin(); i!=refGeos._meshes.cend() && !doublyAttachedObject; ++i)
                if (i->_outputMatrixIndex == c) {
                    for (auto i2=refGeos._meshes.cbegin(); i2!=refGeos._meshes.cend(); ++i2) {
                        if (i2->_objectIndex == i->_objectIndex && i2->_outputMatrixIndex != i->_outputMatrixIndex) {
                            doublyAttachedObject = true;
                            break;
                        }
                    }
                }

            _canMergeIntoTransform[c] = !doublyAttachedObject;
        }
    }

    TransMachineOptimizer::TransMachineOptimizer() {}

    TransMachineOptimizer::~TransMachineOptimizer()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PreparedSkinFile
    {
    public:
        NascentModelCommandStream _cmdStream;
        NascentGeometryObjects _geoObjects;
        NascentSkeleton _skeleton;

        PreparedSkinFile(const ColladaScaffold&, const VisualScene&, const Node&);
    };

    PreparedSkinFile::PreparedSkinFile(const ColladaScaffold& input, const VisualScene& scene, const Node& rootNode)
    {
        using namespace RenderCore::ColladaConversion;

        SkeletonRegistry jointRefs;

        ReferencedGeometries refGeos;
        refGeos.Gather(rootNode, jointRefs);

            // We can now build the skeleton (because ReferencedGeometries::Gather 
            // has initialised jointRefs.
            // Note that when we build the skeleton, it won't contain the joints 
            // referenced by skin controllers. This is because we haven't built the
            // skin controllers yet (so the skin controller joints haven't been registered)
            // This means that the transformation machine built into the skin file will only
            // output transforms between the skin call root and model space.

        unsigned topLevelPops = 0;
        auto coordinateTransform = BuildCoordinateTransform(input._doc->GetAssetDesc());
        if (!Equivalent(coordinateTransform, Identity<Float4x4>(), 1e-5f)) {
                // Push on the coordinate transform (if there is one)
                // This should be optimised into other matrices (or even into
                // the geometry) when we perform the skeleton optimisation steps.
            topLevelPops = _skeleton.GetTransformationMachine().PushTransformation(
                coordinateTransform);
        }

            // When extracting an internal node, we ignore the transform 
            // on that internal node
        BuildSkeleton(_skeleton, rootNode, jointRefs, (rootNode == scene.GetRootNode())?0:1, false);
        _skeleton.GetTransformationMachine().Pop(topLevelPops);

            // For each output matrix, we want to know if we can merge a transformation into it.
            // We can only do this if (unskinned) geometry instances are attached -- and those
            // geometry instances must be attached in only one place. If the output transform does
            // not have a geometry instance attached, or if any of the geometry instances are
            // attached to more than one matrix, or if something other than a geometry instance is
            // attached, then we cannot do any merging.
        
        TransMachineOptimizer optimizer(refGeos, _skeleton.GetTransformationMachine().GetOutputMatrixCount());
        _skeleton.GetTransformationMachine().Optimize(optimizer);

            // We can try to optimise the skeleton here. We should collect the list
            // of meshes that we can optimise transforms into (ie, meshes that aren't
            // used in multiple places, and that aren't skinned).
            // We need to collect that list of transforms before we actually instantiate
            // the geometry -- so that merging in the changes can be done in the instantiate
            // step.

        for (auto c:refGeos._meshes) {
            TRY {
                _cmdStream.Add(
                    RenderCore::ColladaConversion::InstantiateGeometry(
                        scene.GetInstanceGeometry(c._objectIndex),
                        c._outputMatrixIndex, optimizer.GetMergedOutputMatrix(c._outputMatrixIndex),
                        input._resolveContext, _geoObjects, jointRefs,
                        input._cfg));
            } CATCH(...) {
            } CATCH_END
        }

        for (auto c:refGeos._skinControllers) {
            bool skinSuccessful = false;
            TRY {
                _cmdStream.Add(
                    RenderCore::ColladaConversion::InstantiateController(
                        scene.GetInstanceController(c._objectIndex),
                        c._outputMatrixIndex,
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
                            scene.GetInstanceController(c._objectIndex),
                            c._outputMatrixIndex, Identity<Float4x4>(),
                            input._resolveContext, _geoObjects, jointRefs,
                            input._cfg));
                } CATCH(...) {
                } CATCH_END
            }
        }

            // register the names so the skeleton and command stream can be bound together
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

    class DefaultPoseData
    {
    public:
        std::vector<Float4x4>       _defaultTransforms;
        std::pair<Float3, Float3>   _boundingBox;
    };

    static DefaultPoseData CalculateDefaultPoseData(
        const NascentSkeleton::NascentTransformationMachine& transMachine,
        const NascentModelCommandStream& cmdStream,
        const NascentGeometryObjects& geoObjects)
    {
        DefaultPoseData result;

        auto skeletonOutput = transMachine.GenerateOutputTransforms(
            transMachine.GetDefaultParameters());

        auto skelOutputInterface = transMachine.GetOutputInterface();
        auto streamInputInterface = cmdStream.GetInputInterface();
        RenderCore::Assets::SkeletonBinding skelBinding(
            RenderCore::Assets::TransformationMachine::OutputInterface
                {AsPointer(skelOutputInterface.first.begin()), AsPointer(skelOutputInterface.second.begin()), skelOutputInterface.first.size()},
            RenderCore::Assets::ModelCommandStream::InputInterface
                {AsPointer(streamInputInterface.begin()), streamInputInterface.size()});

        auto finalMatrixCount = (unsigned)streamInputInterface.size(); // immData->_visualScene.GetInputInterface()._jointCount;
        result._defaultTransforms.resize(finalMatrixCount);
        for (unsigned c=0; c<finalMatrixCount; ++c) {
            auto machineOutputIndex = skelBinding.ModelJointToMachineOutput(c);
            if (machineOutputIndex == ~unsigned(0x0)) {
                result._defaultTransforms[c] = Identity<Float4x4>();
            } else {
                result._defaultTransforms[c] = skeletonOutput[machineOutputIndex];
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
            hasNonIdentity |= !Equivalent(result._defaultTransforms[c], Identity<Float4x4>(), tolerance);
        if (!hasNonIdentity) {
            finalMatrixCount = 0u;
            result._defaultTransforms.clear();
        }

        result._boundingBox = geoObjects.CalculateBoundingBox(
            cmdStream, MakeIteratorRange(result._defaultTransforms));

        return result;
    }

    static void TraceMetrics(std::ostream& stream, const PreparedSkinFile& skinFile)
    {
        stream << "============== Geometry Objects ==============" << std::endl;
        stream << skinFile._geoObjects;
        stream << std::endl;
        stream << "============== Command stream ==============" << std::endl;
        stream << skinFile._cmdStream;
        stream << std::endl;
        stream << "============== Transformation Machine ==============" << std::endl;
        StreamOperator(stream, skinFile._skeleton.GetTransformationMachine());
    }
      
    NascentChunkArray SerializeSkin(const ColladaScaffold& model, const char startingNode[])
    {
        Serialization::NascentBlockSerializer serializer;
        std::vector<uint8> largeResourcesBlock;

        const auto* scene = model._doc->FindVisualScene(
            GuidReference(model._doc->_visualScene)._id);
        if (!scene)
            Throw(::Assets::Exceptions::FormatError("No visual scene found"));

        auto rootNode = scene->GetRootNode();
        if (startingNode && *startingNode) {
                // Search for the given node, and use that as a root node.
                // if it doesn't exist, we have to throw an exception
            rootNode = rootNode.FindBreadthFirst(
                [startingNode](const Node& node) { return XlEqString(node.GetName(), (const utf8*)startingNode); });
            if (!rootNode)
                Throw(::Assets::Exceptions::FormatError("Could not find root node: %s", startingNode));
        }

        PreparedSkinFile skinFile(model, *scene, rootNode);

            // Serialize the prepared skin file data to a BlockSerializer

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
            const auto& cmdStream = skinFile._cmdStream;
            const auto& geoObjects = skinFile._geoObjects;
            
            auto defaultPoseData = CalculateDefaultPoseData(transMachine, cmdStream, geoObjects);
            serializer.SerializeSubBlock(
                AsPointer(defaultPoseData._defaultTransforms.cbegin()), 
                AsPointer(defaultPoseData._defaultTransforms.cend()));
            serializer.SerializeValue(size_t(defaultPoseData._defaultTransforms.size()));
            ::Serialize(serializer, defaultPoseData._boundingBox.first);
            ::Serialize(serializer, defaultPoseData._boundingBox.second);

            // immData->~ModelImmutableData();
        }

            // Serialize human-readable metrics information
        std::stringstream metricsStream;
        TraceMetrics(metricsStream, skinFile);

        auto scaffoldBlock = AsVector(serializer);
        auto metricsBlock = AsVector(metricsStream);

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_ModelScaffold, 0, model._name.c_str(), unsigned(scaffoldBlock.size()));
        Serialization::ChunkFile::ChunkHeader largeBlockChunk(
            RenderCore::Assets::ChunkType_ModelScaffoldLargeBlocks, 0, model._name.c_str(), (unsigned)largeResourcesBlock.size());
        Serialization::ChunkFile::ChunkHeader metricsChunk(
            RenderCore::Assets::ChunkType_Metrics, 0, "metrics", (unsigned)metricsBlock.size());

        return MakeNascentChunkArray(
            {
                NascentChunk(scaffoldChunk, std::move(scaffoldBlock)),
                NascentChunk(largeBlockChunk, std::move(largeResourcesBlock)),
                NascentChunk(metricsChunk, std::move(metricsBlock))
            });
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
        const auto* scene = input._doc->FindVisualScene(
            GuidReference(input._doc->_visualScene)._id);
        if (!scene)
            Throw(::Assets::Exceptions::FormatError("No visual scene found"));

        using namespace RenderCore::ColladaConversion;
        SkeletonRegistry jointRefs;
        BuildSkeleton(_skeleton, scene->GetRootNode(), jointRefs, 0, true);
        RegisterNodeBindingNames(_skeleton, jointRefs);
        TransMachineOptimizer optimizer;
        _skeleton.GetTransformationMachine().Optimize(optimizer);
    }

    static void TraceMetrics(std::ostream& stream, const PreparedSkeletonFile& file)
    {
        StreamOperator(stream, file._skeleton.GetTransformationMachine());
    }

    NascentChunkArray SerializeSkeleton(const ColladaScaffold& model, const char[])
    {
        PreparedSkeletonFile skeleFile(model);
        auto block = SerializeToVector(skeleFile._skeleton);

        std::stringstream metricsStream;
        TraceMetrics(metricsStream, skeleFile);
        auto metricsBlock = AsVector(metricsStream);

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_Skeleton, 0, model._name.c_str(), unsigned(block.size()));
        Serialization::ChunkFile::ChunkHeader metricsChunk(
            RenderCore::Assets::ChunkType_Metrics, 0, "metrics", (unsigned)metricsBlock.size());

        return MakeNascentChunkArray({
            NascentChunk(scaffoldChunk, std::move(block)),
            NascentChunk(metricsChunk, std::move(metricsBlock))
            });
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

    NascentChunkArray SerializeMaterials(const ColladaScaffold& model, const char[])  
    { 
        // std::string matSettingsFile;
        // {
        //     ::Assets::ResChar settingsName[MaxPath];
        //     XlBasename(settingsName, dimof(settingsName), model._name.c_str());
        //     XlChopExtension(settingsName);
        //     XlCatString(settingsName, dimof(settingsName), ".material");
        //     matSettingsFile = settingsName;
        // }

        MemoryOutputStream<uint8> strm;
        SerializeMatTable(strm, model);

            // convert into a chunk...

        auto finalSize = size_t(strm.GetBuffer().End()) - size_t(strm.GetBuffer().Begin());

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_RawMat, 0, 
            model._name.c_str(), Serialization::ChunkFile::SizeType(finalSize));

        return MakeNascentChunkArray({
            NascentChunk(
                scaffoldChunk, 
                std::vector<uint8>(strm.GetBuffer().Begin(), strm.GetBuffer().End()))
            });
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

        auto block = AsVector(serializer);

        Serialization::ChunkFile::ChunkHeader scaffoldChunk(
            RenderCore::Assets::ChunkType_AnimationSet, 0, animSet._name.c_str(), unsigned(block.size()));

        return MakeNascentChunkArray({NascentChunk(scaffoldChunk, std::move(block))});
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

