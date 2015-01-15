// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS   // warning C4996: 'std::_Copy_impl': Function call with parameters that may be unsafe

#include "ModelCommandStream.h"
#include "RawGeometry.h"
#include "ConversionObjects.h"
#include "ColladaUtils.h"
#include "../RenderCore/Assets/RawAnimationCurve.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../Assets/BlockSerializer.h"
#include "../ConsoleRig/OutputStream.h"

#pragma warning(push)
#pragma warning(disable:4201)       // nonstandard extension used : nameless struct/union
#pragma warning(disable:4245)       // conversion from 'int' to 'const COLLADAFW::SamplerID', signed/unsigned mismatch
#pragma warning(disable:4512)       // assignment operator could not be generated
    #include <COLLADAFWTranslate.h>
    #include <COLLADAFWMatrix.h>
    #include <COLLADAFWRotate.h>
    #include <COLLADAFWScale.h>

    #include <COLLADAFWMaterialBinding.h>
    #include <COLLADAFWNode.h>
#pragma warning(pop)

#pragma warning(disable:4127)       // C4127: conditional expression is constant

namespace RenderCore { namespace ColladaConversion
{ 
    using ::Assets::Exceptions::FormatError;

    static std::string GetNodeStringID(const COLLADAFW::Node& node)
    {
        return node.getOriginalId();
    }

    class NascentAnimationSet::Animation
    {
    public:
        std::string     _name;
        unsigned        _begin, _end;
        unsigned        _constantBegin, _constantEnd;
        float           _startTime, _endTime;
        Animation() : _begin(0), _end(0), _constantBegin(0), _constantEnd(0), _startTime(0.f), _endTime(0.f) {}
        Animation(const std::string& name, unsigned begin, unsigned end, unsigned constantBegin, unsigned constantEnd, float startTime, float endTime) 
            : _name(name), _begin(begin), _end(end), _constantBegin(constantBegin), _constantEnd(constantEnd), _startTime(startTime), _endTime(endTime) {}
    };

    Assets::TransformationParameterSet      NascentAnimationSet::BuildTransformationParameterSet(
        float time, 
        const char animationName[],
        const NascentSkeleton& skeleton,
        const TableOfObjects& accessableObjects) const
    {
        Assets::TransformationParameterSet result(skeleton.GetTransformationMachine().GetDefaultParameters());
        float* float1s      = result.GetFloat1Parameters();
        Float3* float3s     = result.GetFloat3Parameters();
        Float4* float4s     = result.GetFloat4Parameters();
        Float4x4* float4x4s = result.GetFloat4x4Parameters();

        size_t driverStartIndex = 0, driverEndIndex = _animationDrivers.size();
        size_t constantDriverStartIndex = 0, constantDriverEndIndex = _constantDrivers.size();
        if (animationName) {
            for (auto i=_animations.begin(); i!=_animations.end(); ++i) {
                if (i->_name == animationName) {
                    driverStartIndex = i->_begin;
                    driverEndIndex = i->_end;
                    constantDriverStartIndex = i->_constantBegin;
                    constantDriverEndIndex = i->_constantEnd;
                    time += i->_startTime;
                }
            }
        }

        for (   auto i=_animationDrivers.begin() + driverStartIndex;
                i!=_animationDrivers.begin() + driverEndIndex; ++i) {
            auto colladaId      = skeleton.GetTransformationMachine().StringIdToHashedId(_parameterInterfaceDefinition[i->_parameterIndex]);
            auto typeAndIndex   = skeleton.GetTransformationMachine().GetParameterIndex(colladaId);
            if (i->_samplerType == Assets::TransformationParameterSet::Type::Float4x4) {
                const Assets::RawAnimationCurve* curve = accessableObjects.GetFromObjectId<Assets::RawAnimationCurve>(i->_curveId);
                if (curve) {
                    assert(typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4x4);
                    // assert(i->_index < float4x4s.size());
                    float4x4s[typeAndIndex.second] = curve->Calculate<Float4x4>(time);
                }
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float4) {
                const Assets::RawAnimationCurve* curve = accessableObjects.GetFromObjectId<Assets::RawAnimationCurve>(i->_curveId);
                if (curve) {
                    if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4) {
                        float4s[typeAndIndex.second] = curve->Calculate<Float4>(time);
                    } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3) {
                        float3s[typeAndIndex.second] = Truncate(curve->Calculate<Float4>(time));
                    }
                }
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float3) {
                const Assets::RawAnimationCurve* curve = accessableObjects.GetFromObjectId<Assets::RawAnimationCurve>(i->_curveId);
                if (curve) {
                    assert(typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3);
                    // assert(i->_index < float3s.size());
                    float3s[typeAndIndex.second] = curve->Calculate<Float3>(time);
                }
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float1) {
                const Assets::RawAnimationCurve* curve = accessableObjects.GetFromObjectId<Assets::RawAnimationCurve>(i->_curveId);
                if (curve) {
                    float result = curve->Calculate<float>(time);
                    if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float1) {
                        // assert(i->_index < float1s.size());
                        float1s[typeAndIndex.second] = result;
                    } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3) {
                        assert(i->_samplerOffset < 3);
                        float3s[typeAndIndex.second][i->_samplerOffset] = result;
                    } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4) {
                        assert(i->_samplerOffset < 4);
                        float4s[typeAndIndex.second][i->_samplerOffset] = result;
                    }
                }
            }
        }

        for (   auto i=_constantDrivers.begin() + constantDriverStartIndex;
                i!=_constantDrivers.begin() + constantDriverEndIndex; ++i) {
            auto colladaId      = skeleton.GetTransformationMachine().StringIdToHashedId(_parameterInterfaceDefinition[i->_parameterIndex]);
            auto typeAndIndex   = skeleton.GetTransformationMachine().GetParameterIndex(colladaId);
            const void* data    = PtrAdd(AsPointer(_constantData.begin()), i->_dataOffset);
            if (i->_samplerType == Assets::TransformationParameterSet::Type::Float4x4) {
                assert(typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4x4);
                float4x4s[typeAndIndex.second] = *(const Float4x4*)data;
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float4) {
                if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4) {
                    float4s[typeAndIndex.second] = *(const Float4*)data;
                } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3) {
                    float3s[typeAndIndex.second] = Truncate(*(const Float4*)data);
                }
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float3) {
                assert(typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3);
                float3s[typeAndIndex.second] = *(Float3*)data;
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float1) {
                if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float1) {
                    float1s[typeAndIndex.second] = *(float*)data;
                } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3) {
                    assert(i->_samplerOffset < 3);
                    float3s[typeAndIndex.second][i->_samplerOffset] = *(const float*)data;
                } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4) {
                    assert(i->_samplerOffset < 4);
                    float4s[typeAndIndex.second][i->_samplerOffset] = *(const float*)data;
                }
            }
        }

        return result;
    }

    AnimSamplerType      SamplerWidthToType(unsigned samplerWidth)
    {
        Assets::TransformationParameterSet::Type::Enum samplerType = Assets::TransformationParameterSet::Type::Float1;
        switch (samplerWidth) {
        case  1:    samplerType = Assets::TransformationParameterSet::Type::Float1;     break;
        case  3:    samplerType = Assets::TransformationParameterSet::Type::Float3;     break;
        case  4:    samplerType = Assets::TransformationParameterSet::Type::Float4;     break;
        case 16:    samplerType = Assets::TransformationParameterSet::Type::Float4x4;   break;
        default: 
            ThrowException(
                FormatError(
                    "Strange sampler width encountered when adding animation driver!"));
        }

        return samplerType;
    }

    size_t      SamplerSize(AnimSamplerType samplerType)
    {
        switch (samplerType) {
        case Assets::TransformationParameterSet::Type::Float1:      return sizeof(float);
        case Assets::TransformationParameterSet::Type::Float3:      return sizeof(Float3);
        case Assets::TransformationParameterSet::Type::Float4:      return sizeof(Float4);
        case Assets::TransformationParameterSet::Type::Float4x4:    return sizeof(Float4x4);
        }
        return 0;
    }

    void    NascentAnimationSet::AddConstantDriver( 
                                    const std::string&  parameterName, 
                                    const void*         constantValue, 
                                    AnimSamplerType     samplerType, 
                                    unsigned            samplerOffset)
    {
        size_t parameterIndex = _parameterInterfaceDefinition.size();
        auto i = std::find( _parameterInterfaceDefinition.cbegin(), 
                            _parameterInterfaceDefinition.cend(), parameterName);
        if (i!=_parameterInterfaceDefinition.end()) {
            parameterIndex = (unsigned)std::distance(_parameterInterfaceDefinition.cbegin(), i);
        } else {
            _parameterInterfaceDefinition.push_back(parameterName);
        }

        unsigned dataOffset = unsigned(_constantData.size());
        std::copy(
            (uint8*)constantValue, PtrAdd((uint8*)constantValue, SamplerSize(samplerType)),
            std::back_inserter(_constantData));

        _constantDrivers.push_back(
            ConstantDriver(dataOffset, (unsigned)parameterIndex, samplerType, samplerOffset));
    }

    void    NascentAnimationSet::AddAnimationDriver( const std::string& parameterName, 
                                    ObjectId curveId, 
                                    AnimSamplerType samplerType, unsigned samplerOffset)
    {
        size_t parameterIndex = _parameterInterfaceDefinition.size();
        auto i = std::find( _parameterInterfaceDefinition.cbegin(), 
                            _parameterInterfaceDefinition.cend(), parameterName);
        if (i!=_parameterInterfaceDefinition.end()) {
            parameterIndex = (unsigned)std::distance(_parameterInterfaceDefinition.cbegin(), i);
        } else {
            _parameterInterfaceDefinition.push_back(parameterName);
        }

        _animationDrivers.push_back(
            AnimationDriver(curveId, (unsigned)parameterIndex, samplerType, samplerOffset));
    }

    bool    NascentAnimationSet::HasAnimationDriver(const std::string&  parameterName) const
    {
        auto i = std::find( _parameterInterfaceDefinition.cbegin(), 
                            _parameterInterfaceDefinition.cend(), parameterName);
        if (i==_parameterInterfaceDefinition.end()) 
            return false;

        auto parameterIndex = (unsigned)std::distance(_parameterInterfaceDefinition.cbegin(), i);

        for (auto i=_animationDrivers.begin(); i!=_animationDrivers.end(); ++i) {
            if (i->_parameterIndex == parameterIndex)
                return true;
        }

        for (auto i=_constantDrivers.begin(); i!=_constantDrivers.end(); ++i) {
            if (i->_parameterIndex == parameterIndex)
                return true;
        }
        return false;
    }

    void    NascentAnimationSet::MergeAnimation(
                const NascentAnimationSet& animation, const char name[],
                const TableOfObjects& sourceObjects, TableOfObjects& destinationObjects)
    {
            //
            //      Merge the animation drivers in the given input animation, and give 
            //      them the supplied name
            //
        uint32 fileHash = Hash32(name, &name[XlStringLen(name)]);
        float minTime = FLT_MAX, maxTime = -FLT_MAX;
        size_t startIndex = _animationDrivers.size();
        size_t constantStartIndex = _constantDrivers.size();
        for (auto i=animation._animationDrivers.cbegin(); i!=animation._animationDrivers.end(); ++i) {
            const Assets::RawAnimationCurve* animCurve = sourceObjects.GetFromObjectId<Assets::RawAnimationCurve>(i->_curveId);
            if (animCurve) {
                float curveStart = animCurve->StartTime();
                float curveEnd = animCurve->EndTime();
                minTime = std::min(minTime, curveStart);
                maxTime = std::max(maxTime, curveEnd);

                auto desc = sourceObjects.GetDesc<Assets::RawAnimationCurve>(i->_curveId);
                const COLLADAFW::UniqueId& sourceId(std::get<2>(desc));
                
                const std::string& name = animation._parameterInterfaceDefinition[i->_parameterIndex];
                Assets::RawAnimationCurve duplicate(*animCurve);
                ObjectId curveId = destinationObjects.Add(std::get<0>(desc), std::get<1>(desc), 
                    COLLADAFW::UniqueId(sourceId.getClassId(), sourceId.getObjectId(), fileHash), std::move(duplicate));
                AddAnimationDriver(name, curveId, i->_samplerType, i->_samplerOffset);
            }
        }

        for (auto i=animation._constantDrivers.cbegin(); i!=animation._constantDrivers.end(); ++i) {
            const std::string& name = animation._parameterInterfaceDefinition[i->_parameterIndex];
            AddConstantDriver(name, PtrAdd(AsPointer(animation._constantData.begin()), i->_dataOffset), i->_samplerType, i->_samplerOffset);
        }

        _animations.push_back(Animation(name, 
            (unsigned)startIndex, (unsigned)_animationDrivers.size(), 
            (unsigned)constantStartIndex, (unsigned)_constantDrivers.size(),
            minTime, maxTime));
    }

    void NascentAnimationSet::AnimationDriver::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, unsigned(_curveId));
        Serialization::Serialize(serializer, _parameterIndex);
        Serialization::Serialize(serializer, _samplerOffset);
        Serialization::Serialize(serializer, unsigned(_samplerType));
    }

    void NascentAnimationSet::ConstantDriver::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _dataOffset);
        Serialization::Serialize(serializer, _parameterIndex);
        Serialization::Serialize(serializer, _samplerOffset);
        Serialization::Serialize(serializer, unsigned(_samplerType));
    }

    struct AnimationDesc        // matches AnimationSet::Animation
    {
        uint64      _name;
        unsigned    _beginDriver, _endDriver;
        unsigned    _beginConstantDriver, _endConstantDriver;
        float       _beginTime, _endTime; 

        AnimationDesc() {}
        void Serialize(Serialization::NascentBlockSerializer& serializer) const
        {
            Serialization::Serialize(serializer, _name);
            Serialization::Serialize(serializer, _beginDriver);
            Serialization::Serialize(serializer, _endDriver);
            Serialization::Serialize(serializer, _beginConstantDriver);
            Serialization::Serialize(serializer, _endConstantDriver);
            Serialization::Serialize(serializer, _beginTime);
            Serialization::Serialize(serializer, _endTime);
        }
    };

    struct CompareAnimationName
    {
        bool operator()(const AnimationDesc& lhs, const AnimationDesc& rhs) const   { return lhs._name < rhs._name; }
        bool operator()(const AnimationDesc& lhs, uint64 rhs) const                 { return lhs._name < rhs; }
        bool operator()(uint64 lhs, const AnimationDesc& rhs) const                 { return lhs < rhs._name; }
    };

    void NascentAnimationSet::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        serializer.SerializeSubBlock(AsPointer(_animationDrivers.begin()), AsPointer(_animationDrivers.end()));
        serializer.SerializeValue(_animationDrivers.size());
        serializer.SerializeSubBlock(AsPointer(_constantDrivers.begin()), AsPointer(_constantDrivers.end()));
        serializer.SerializeValue(_constantDrivers.size());
        serializer.SerializeSubBlock(AsPointer(_constantData.begin()), AsPointer(_constantData.end()));

            //      List of animations...

        auto outputAnimations = std::make_unique<AnimationDesc[]>(_animations.size());
        for (size_t c=0; c<_animations.size(); ++c) {
            AnimationDesc&o = outputAnimations[c];
            const Animation&i = _animations[c];
            o._name = Hash64(AsPointer(i._name.begin()), AsPointer(i._name.end()));
            o._beginDriver = i._begin; o._endDriver = i._end;
            o._beginConstantDriver = i._constantBegin; o._endConstantDriver = i._constantEnd;
            o._beginTime = i._startTime; o._endTime = i._endTime;
        }
        std::sort(outputAnimations.get(), &outputAnimations[_animations.size()], CompareAnimationName());
        serializer.SerializeSubBlock(outputAnimations.get(), &outputAnimations[_animations.size()]);
        serializer.SerializeValue(_animations.size());

            //      Output interface...

        ConsoleRig::DebuggerOnlyWarning("Animation set output interface:\n");
        auto parameterNameHashes = std::make_unique<uint64[]>(_parameterInterfaceDefinition.size());
        for (size_t c=0; c<_parameterInterfaceDefinition.size(); ++c) {
            ConsoleRig::DebuggerOnlyWarning("  [%i] %s\n", c, _parameterInterfaceDefinition[c].c_str());
            parameterNameHashes[c] = Hash64(AsPointer(_parameterInterfaceDefinition[c].begin()), AsPointer(_parameterInterfaceDefinition[c].end()));
        }
        serializer.SerializeSubBlock(parameterNameHashes.get(), &parameterNameHashes[_parameterInterfaceDefinition.size()]);
        serializer.SerializeValue(_parameterInterfaceDefinition.size());
    }

    NascentAnimationSet::NascentAnimationSet() {}
    NascentAnimationSet::~NascentAnimationSet() {}
    NascentAnimationSet::NascentAnimationSet(NascentAnimationSet&& moveFrom)
    :   _animationDrivers(std::move(moveFrom._animationDrivers))
    ,   _constantDrivers(std::move(moveFrom._constantDrivers))
    ,   _animations(std::move(moveFrom._animations))
    ,   _parameterInterfaceDefinition(std::move(moveFrom._parameterInterfaceDefinition))
    ,   _constantData(std::move(moveFrom._constantData))
    {}
    NascentAnimationSet& NascentAnimationSet::operator=(NascentAnimationSet&& moveFrom)
    {
        _animationDrivers = std::move(moveFrom._animationDrivers);
        _constantDrivers = std::move(moveFrom._constantDrivers);
        _animations = std::move(moveFrom._animations);
        _parameterInterfaceDefinition = std::move(moveFrom._parameterInterfaceDefinition);
        _constantData = std::move(moveFrom._constantData);
        return *this;
    }



    static std::tuple<bool,bool> NeedOutputMatrix(  const COLLADAFW::Node& node, const TableOfObjects& accessableObjects,
                                                    const JointReferences& skeletonReferences)
    {
        using namespace COLLADAFW;
        bool needAnOutputMatrix = false;
        for (size_t c=0; c<node.getInstanceGeometries().getCount(); ++c) {

                //
                //      Ignore material bindings for the moment...!
                //

            const UniqueId& id = node.getInstanceGeometries()[c]->getInstanciatedObjectId();
            ObjectId tableId = accessableObjects.Get<NascentRawGeometry>(id);
            if (tableId==ObjectId_Invalid) {
                Warning( "Warning -- bad instanced geometry link found in node (%s)\n", GetNodeStringID(node).c_str());
            } else {
                needAnOutputMatrix = true;
            }
        }

        for (size_t c=0; c<node.getInstanceNodes().getCount(); ++c) {
            const UniqueId& id = node.getInstanceNodes()[c]->getInstanciatedObjectId();
            ObjectId tableId = accessableObjects.Get<NascentModelCommandStream>(id);
            if (tableId==ObjectId_Invalid) {
                Warning("Warning -- bad instanced geometry link found in node (%s)\n", GetNodeStringID(node).c_str());
            } else {
                needAnOutputMatrix = true;
            }
        }

        for (size_t c=0; c<node.getInstanceControllers().getCount(); ++c) {
            const UniqueId& id = node.getInstanceControllers()[c]->getInstanciatedObjectId();
            ObjectId tableId = accessableObjects.Get<UnboundSkinControllerAndAttachedSkeleton>(id);
            if (tableId==ObjectId_Invalid) {
                tableId = accessableObjects.Get<UnboundMorphController>(id);
            }
            if (tableId==ObjectId_Invalid) {
                Warning("Warning -- bad instanced controller link found in node (%s)\n", GetNodeStringID(node).c_str());
            } else {
                needAnOutputMatrix = true;
            }
        }

        needAnOutputMatrix |= ImportCameras && !node.getInstanceCameras().empty();

            //
            //      Check to see if this node is referenced by any instance controllers
            //      if it is, it's probably part of a skeleton (and we need an output
            //      matrix and a joint tag)
            //

        bool isReferencedJoint = false;
        if (skeletonReferences.HasJoint(AsHashedColladaUniqueId(node.getUniqueId()))) {
            isReferencedJoint = needAnOutputMatrix = true;
        }

        return std::make_tuple(needAnOutputMatrix, isReferencedJoint);
    }

    void        NascentSkeleton::PushNode(      const COLLADAFW::Node& node, const TableOfObjects& accessableObjects,
                                                const JointReferences& skeletonReferences)
    {
        using namespace COLLADAFW;
        COLLADABU::Math::Matrix4 matrix;
        node.getTransformationMatrix(matrix);
        unsigned pushCount = _transformationMachine.PushTransformations(node.getTransformations(), GetNodeStringID(node).c_str());

            //
            //      We have to assume we need an output matrix. We don't really know
            //      which nodes need output matrices at this point (because we haven't 
            //      got all the downstream skinning data). So, let's just assume it's needed.
            //
        bool needAnOutputMatrix, isReferencedJoint;
        std::tie(needAnOutputMatrix, isReferencedJoint) = NeedOutputMatrix(node, accessableObjects, skeletonReferences);
            // DavidJ -- hack! -- When writing a "skeleton" we need to include all nodes, even those that aren't
            //              referenced within the same file. This is because the node might become an output-interface
            //              node... Maybe there is a better way to do this. Perhaps we could identify which nodes are
            //              output interface transforms / bones... Or maybe we could just include everything when
            //              compiling a skeleton...?
        needAnOutputMatrix = isReferencedJoint = true;
        if (needAnOutputMatrix) {
            unsigned thisOutputMatrix = _transformationMachine.GetOutputMatrixMarker();

                //
                //      (We can't instantiate the skin controllers yet, because we can't be sure
                //          we've already parsed the skeleton nodes)
                //      But we can write a tag to find the output matrix later
                //          (we also need a tag for all nodes with instance controllers in them)
                //

            if (isReferencedJoint || node.getInstanceControllers().getCount() || node.getInstanceGeometries().getCount()) {
                HashedColladaUniqueId hashedColladaId = AsHashedColladaUniqueId(node.getUniqueId());
                Float4x4 inverseBindMatrix = Identity<Float4x4>();
                for (auto i=skeletonReferences._references.begin(); i!=skeletonReferences._references.end(); ++i) {
                    if (i->_joint == hashedColladaId) {
                            // note -- it could be bound multiple times!
                        inverseBindMatrix = i->_inverseBindMatrix;
                        break;
                    }
                }

                    // note -- there may be problems here, because the "name" of the node isn't necessarily
                    //          unique. There are unique ids in collada, however. We some some unique identifier
                    //          can can be seen in Max, and can be used to associate different files with shared
                    //          references (eg, animations, skeletons and skins in separate files)
                _transformationMachine.RegisterJointName(hashedColladaId, GetNodeStringID(node), inverseBindMatrix, thisOutputMatrix);
            }
        }

        const NodePointerArray& childNodes = node.getChildNodes();
        for (size_t c=0; c<childNodes.getCount(); ++c) {
            PushNode(*childNodes[c], accessableObjects, skeletonReferences);
        }

        _transformationMachine.Pop(pushCount);
    }

    void NascentSkeleton::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _transformationMachine);
    }

    NascentSkeleton::NascentSkeleton() {}
    NascentSkeleton::~NascentSkeleton() {}
    NascentSkeleton::NascentSkeleton(NascentSkeleton&& moveFrom)
    :   _transformationMachine(std::move(moveFrom._transformationMachine))
    {}
    NascentSkeleton& NascentSkeleton::operator=(NascentSkeleton&& moveFrom)
    {
        _transformationMachine = std::move(moveFrom._transformationMachine);
        return *this;
    }








    std::vector<ObjectId>       NascentModelCommandStream::BuildMaterialTable(
        const COLLADAFW::MaterialBindingArray& bindingArray, 
        const std::vector<uint64>& geometryMaterialOrdering,
        const TableOfObjects& accessableObjects)
    {
                        //
                        //        For each material referenced in the raw geometry, try to 
                        //        match it with a material we've built during collada processing
                        //            We have to map it via the binding table in the InstanceGeometry
                        //
                        
        std::vector<ObjectId> materials;
        materials.resize(geometryMaterialOrdering.size(), ObjectId_Invalid);

        for (unsigned c=0; c<bindingArray.getCount(); ++c)
            for (auto i=geometryMaterialOrdering.cbegin(); i!=geometryMaterialOrdering.cend(); ++i)
                if (*i == bindingArray[c].getMaterialId()) {
                    assert(materials[std::distance(geometryMaterialOrdering.cbegin(), i)] == ObjectId_Invalid);

                    ObjectId refMaterialTableId = accessableObjects.Get<ReferencedMaterial>(bindingArray[c].getReferencedMaterial());
                    const ReferencedMaterial* matRef = accessableObjects.GetFromObjectId<ReferencedMaterial>(refMaterialTableId);

                        //      Redirect from ReferencedMaterial -> MaterialParameters
                    ObjectId finalMaterialId = ObjectId_Invalid;
                    if (matRef) {
                        finalMaterialId = accessableObjects.Get<Assets::MaterialParameters>(matRef->_effectId.AsColladaId());
                    }
                    materials[std::distance(geometryMaterialOrdering.cbegin(), i)] = finalMaterialId;
                    break;
                }

        return std::move(materials);
    }
    
    class NascentModelCommandStream::TransformationMachineOutput
    {
    public:
        HashedColladaUniqueId _colladaId;
        std::string _name;
    };

    void NascentModelCommandStream::PushNode(   const COLLADAFW::Node& node, const TableOfObjects& accessableObjects,
                                                const JointReferences& skeletonReferences)
    {
        if (!IsUseful(node, accessableObjects, skeletonReferences)) {
            return;
        }

        using namespace COLLADAFW;

            //
            //      If we have any "instances" attached to this node... Execute those draw calls
            //      and thingamabob's
            //

        bool needAnOutputMatrix, isReferencedJoint;
        std::tie(needAnOutputMatrix, isReferencedJoint) = NeedOutputMatrix(node, accessableObjects, skeletonReferences);
        if (needAnOutputMatrix) {
            const size_t thisOutputMatrix = _transformationMachineOutputs.size();
            TransformationMachineOutput transInput;
            transInput._name        = GetNodeStringID(node);
            transInput._colladaId   = AsHashedColladaUniqueId(node.getUniqueId());
            _transformationMachineOutputs.push_back(transInput);
            
            for (size_t c=0; c<node.getInstanceGeometries().getCount(); ++c) {
                const InstanceGeometry& instanceGeo = *node.getInstanceGeometries()[c];
                const UniqueId& id  = instanceGeo.getInstanciatedObjectId();
                ObjectId tableId    = accessableObjects.Get<NascentRawGeometry>(id);
                if (tableId!=ObjectId_Invalid) {
                    const NascentRawGeometry* inputGeometry = 
                        accessableObjects.GetFromObjectId<NascentRawGeometry>(tableId);
                    std::vector<ObjectId> materials = BuildMaterialTable(
                        instanceGeo.getMaterialBindings(), inputGeometry->_materials, accessableObjects);
                    _geometryInstances.push_back(GeometryInstance(tableId, (unsigned)thisOutputMatrix, std::move(materials), 0));
                }
            }

            for (size_t c=0; c<node.getInstanceNodes().getCount(); ++c) {
                const UniqueId& id  = node.getInstanceNodes()[c]->getInstanciatedObjectId();
                ObjectId tableId    = accessableObjects.Get<NascentModelCommandStream>(id);
                if (tableId!=ObjectId_Invalid) {
                    _modelInstances.push_back(ModelInstance(tableId, (unsigned)thisOutputMatrix));
                }
            }

            for (size_t c=0; c<node.getInstanceCameras().getCount(); ++c) {

                    //
                    //      Ignore camera parameters for the moment
                    //          (they should come from another node in the <library_cameras> part
                    //  

                _cameraInstances.push_back(CameraInstance((unsigned)thisOutputMatrix));
            }
        }

            //
            //      There's also instanced nodes, lights, cameras, controllers
            //
            //      Now traverse to child nodes.
            //

        const NodePointerArray& childNodes = node.getChildNodes();
        for (size_t c=0; c<childNodes.getCount(); ++c) {
            PushNode(*childNodes[c], accessableObjects, skeletonReferences);
        }
    }

    static void CopyVertexElements(     void* destinationBuffer,            size_t destinationVertexStride,
                                        const void* sourceBuffer,           size_t sourceVertexStride,
                                        const Metal::InputElementDesc* destinationLayoutBegin,  const Metal::InputElementDesc* destinationLayoutEnd,
                                        const Metal::InputElementDesc* sourceLayoutBegin,       const Metal::InputElementDesc* sourceLayoutEnd,
                                        const uint16* reorderingBegin,      const uint16* reorderingEnd )
    {
        uint32      elementReordering[32];
        signed      maxSourceLayout = -1;
        for (auto source=sourceLayoutBegin; source!=sourceLayoutEnd; ++source) {
                //      look for the same element in the destination layout (or put ~uint16(0x0) if it's not there)
            elementReordering[source-sourceLayoutBegin] = ~uint32(0x0);
            for (auto destination=destinationLayoutBegin; destination!=destinationLayoutEnd; ++destination) {
                if (    destination->_semanticName   == source->_semanticName 
                    &&  destination->_semanticIndex  == source->_semanticIndex
                    &&  destination->_nativeFormat   == source->_nativeFormat) {

                    elementReordering[source-sourceLayoutBegin] = uint32(destination-destinationLayoutBegin);
                    maxSourceLayout = std::max(maxSourceLayout, signed(source-sourceLayoutBegin));
                    break;
                }
            }
        }

        if (maxSourceLayout<0) return;

        size_t vertexCount = reorderingEnd - reorderingBegin; (void)vertexCount;

        #if defined(_DEBUG)
                    //  fill in some dummy values
            std::fill((uint8*)destinationBuffer, (uint8*)PtrAdd(destinationBuffer, vertexCount*destinationVertexStride), 0xaf);
        #endif

            ////////////////     copy each vertex (slowly) piece by piece       ////////////////
        for (auto reordering = reorderingBegin; reordering!=reorderingEnd; ++reordering) {
            size_t sourceIndex               = reordering-reorderingBegin, destinationIndex = *reordering;
            void* destinationVertexStart     = PtrAdd(destinationBuffer, destinationIndex*destinationVertexStride);
            const void* sourceVertexStart    = PtrAdd(sourceBuffer, sourceIndex*sourceVertexStride);
            for (unsigned c=0; c<=(unsigned)maxSourceLayout; ++c) {
                if (elementReordering[c] != ~uint16(0x0)) {
                    const Metal::InputElementDesc& destinationElement = destinationLayoutBegin[elementReordering[c]]; assert(&destinationElement < destinationLayoutEnd);
                    const Metal::InputElementDesc& sourceElement = sourceLayoutBegin[c]; assert(&sourceElement < sourceLayoutEnd);
                    size_t elementSize = Metal::BitsPerPixel(destinationElement._nativeFormat)/8;
                    assert(elementSize == Metal::BitsPerPixel(sourceElement._nativeFormat)/8);
                    assert(destinationElement._alignedByteOffset + elementSize <= destinationVertexStride);
                    assert(sourceElement._alignedByteOffset + elementSize <= sourceVertexStride);
                    assert(PtrAdd(destinationVertexStart, destinationElement._alignedByteOffset+elementSize) <= PtrAdd(destinationVertexStart, vertexCount*destinationVertexStride));
                    assert(PtrAdd(sourceVertexStart, sourceElement._alignedByteOffset+elementSize) <= PtrAdd(sourceVertexStart, vertexCount*sourceVertexStride));

                    XlCopyMemory(
                        PtrAdd(destinationVertexStart, destinationElement._alignedByteOffset),
                        PtrAdd(sourceVertexStart, sourceElement._alignedByteOffset),
                        elementSize);
                }
            }
        }
    }

    static unsigned CalculateVertexSize(const Metal::InputElementDesc* layoutBegin,  const Metal::InputElementDesc* layoutEnd)
    {
        unsigned result = 0;
        for (auto l=layoutBegin; l!=layoutEnd; ++l) {
            result += Metal::BitsPerPixel(l->_nativeFormat)/8;
        }
        return result;
    }

    static const bool SkinNormals = true;

    unsigned    NascentModelCommandStream::FindTransformationMachineOutput(HashedColladaUniqueId nodeId) const
    {
        auto i2=_transformationMachineOutputs.cbegin();
        for (;i2!=_transformationMachineOutputs.cend(); ++i2) {
            if (i2->_colladaId == nodeId) {
                return (unsigned)std::distance(_transformationMachineOutputs.cbegin(), i2);
            }
        }
        return ~unsigned(0x0);
    }

    void NascentModelCommandStream::InstantiateControllers(const COLLADAFW::Node& node, const TableOfObjects& accessableObjects, TableOfObjects& destinationForNewObjects)
    {
            //
            //      This is usually a second pass through the node hierarchy (after PushNode)
            //      Look for all of the skin controllers and instantiate a proper bound controllers
            //      for each!
            //
            //      We have to do a second pass, because this must happen after we've pushed in all of the 
            //      joint nodes from the skeleton (which we can't be sure has happened until after PushNode
            //      has gone through the entire tree.
            //
        using namespace COLLADAFW;

        std::vector<std::string> elementsToBeSkinned;
        elementsToBeSkinned.push_back("POSITION");
        if (SkinNormals) {
            elementsToBeSkinned.push_back("NORMAL");
        }

        for (size_t instanceController=0; instanceController<node.getInstanceControllers().getCount(); ++instanceController) {
            const UniqueId& id = node.getInstanceControllers()[instanceController]->getInstanciatedObjectId();
            const UnboundSkinControllerAndAttachedSkeleton* controllerAndSkeleton = nullptr;

            ObjectId tableId = accessableObjects.Get<UnboundSkinControllerAndAttachedSkeleton>(id);
            if (tableId != ObjectId_Invalid) {
                controllerAndSkeleton = accessableObjects.GetFromObjectId<UnboundSkinControllerAndAttachedSkeleton>(tableId);
            }

            if (controllerAndSkeleton) {
                const UnboundSkinController* controller = 
                    accessableObjects.GetFromObjectId<UnboundSkinController>(
                        controllerAndSkeleton->_unboundControllerId);
                if (controller) {

                        //
                        //      Assume the "source" of this controller is a geometry. Collada can 
                        //      support cascading controllers so that the output of one controller 
                        //      is used as input to the next controller. This can be useful for 
                        //      combining skinning and morph targets on the same geometry.
                        //

                    const NascentRawGeometry* source = nullptr;
                    ObjectId sourceTableId = accessableObjects.Get<NascentRawGeometry>(
                        controllerAndSkeleton->_source.AsColladaId());
                    if (sourceTableId != ObjectId_Invalid) {
                        source = accessableObjects.GetFromObjectId<NascentRawGeometry>(sourceTableId);
                    } else {
                        sourceTableId = accessableObjects.Get<UnboundMorphController>(
                            controllerAndSkeleton->_source.AsColladaId());
                        if (sourceTableId != ObjectId_Invalid) {
                            const UnboundMorphController* morphController = 
                                accessableObjects.GetFromObjectId<UnboundMorphController>(sourceTableId);
                            if (morphController) {
                                sourceTableId = accessableObjects.Get<NascentRawGeometry>(morphController->_source.AsColladaId());
                                if (sourceTableId != ObjectId_Invalid) {
                                    source = accessableObjects.GetFromObjectId<NascentRawGeometry>(sourceTableId);
                                }
                            }
                        }
                    }
                    if (source) {

                            //
                            //      Our instantiation of this geometry needs to be slightly different
                            //      (but still similar) to the basic raw geometry case.
                            //
                            //      Basic geometry:
                            //          vertex buffer
                            //          index buffer
                            //          input assembly setup
                            //          draw calls
                            //
                            //      Skinned Geometry:
                            //              (this part is mostly the same, except we've reordered the
                            //              vertex buffers, and removed the part of the vertex buffer 
                            //              that will be animated)
                            //          unanimated vertex buffer
                            //          index buffer
                            //          input assembly setup (final draw calls)
                            //          draw calls (final draw calls)
                            //
                            //              (this part is new)
                            //          animated vertex buffer
                            //          input assembly setup (skinning calculation pass)
                            //          draw calls (skinning calculation pass)
                            //
                            //      Note that we need to massage the vertex buffers slightly. So the
                            //      raw geometry input must be in a format that allows us to read from
                            //      the vertex and index buffers.
                            //
                            
                        size_t unifiedVertexCount = source->_unifiedVertexIndexToPositionIndex.size();

                        std::vector<std::pair<uint16,uint32>> unifiedVertexIndexToBucketIndex;
                        unifiedVertexIndexToBucketIndex.reserve(unifiedVertexCount);

                        for (uint16 c=0; c<unifiedVertexCount; ++c) {
                            uint32 positionIndex = source->_unifiedVertexIndexToPositionIndex[c];
                            uint32 bucketIndex   = controller->_positionIndexToBucketIndex[positionIndex];
                            unifiedVertexIndexToBucketIndex.push_back(std::make_pair(c, bucketIndex));
                        }

                            //
                            //      Resort by bucket index...
                            //

                        std::sort(unifiedVertexIndexToBucketIndex.begin(), unifiedVertexIndexToBucketIndex.end(), CompareSecond<uint16, uint32>());

                        std::vector<uint16> unifiedVertexReordering;       // unifiedVertexReordering[oldIndex] = newIndex;
                        std::vector<uint16> newUnifiedVertexIndexToPositionIndex;
                        unifiedVertexReordering.resize(unifiedVertexCount, (uint16)~uint16(0x0));
                        newUnifiedVertexIndexToPositionIndex.resize(unifiedVertexCount, (uint16)~uint16(0x0));

                            //
                            //      \todo --    it would better if we tried to maintain the vertex ordering within
                            //                  the bucket. That is, the relative positions of vertices within the
                            //                  bucket should be the same as the relative positions of those vertices
                            //                  as they were in the original
                            //

                        uint16 indexAccumulator = 0;
                        const size_t bucketCount = dimof(((UnboundSkinController*)nullptr)->_bucket);
                        uint16 bucketStart  [bucketCount];
                        uint16 bucketEnd    [bucketCount];
                        uint16 currentBucket = 0; bucketStart[0] = 0;
                        for (auto i=unifiedVertexIndexToBucketIndex.cbegin(); i!=unifiedVertexIndexToBucketIndex.cend(); ++i) {
                            if ((i->second >> 16)!=currentBucket) {
                                bucketEnd[currentBucket] = indexAccumulator;
                                bucketStart[++currentBucket] = indexAccumulator;
                            }
                            uint16 newIndex = indexAccumulator++;
                            uint16 oldIndex = i->first;
                            unifiedVertexReordering[oldIndex] = newIndex;
                            newUnifiedVertexIndexToPositionIndex[newIndex] = (uint16)source->_unifiedVertexIndexToPositionIndex[oldIndex];
                        }
                        bucketEnd[currentBucket] = indexAccumulator;
                        for (unsigned b=currentBucket+1; b<bucketCount; ++b) {
                            bucketStart[b] = bucketEnd[b] = indexAccumulator;
                        }
                        if (indexAccumulator != unifiedVertexCount) {
                            ThrowException(FormatError("Vertex count mismatch in node (%s)", GetNodeStringID(node).c_str()));
                        }

                            //
                            //      Move vertex data for vertex elements that will be skinned into a separate vertex buffer
                            //      Note that we don't really know which elements will be skinned. We can assume that at
                            //      least "POSITION" will be skinned. But actually this is defined by the particular
                            //      shader. We could wait until binding with the material to make this decision...?
                            //
                        std::vector<Metal::InputElementDesc> unanimatedVertexLayout = source->_mainDrawInputAssembly._vertexInputLayout;
                        std::vector<Metal::InputElementDesc> animatedVertexLayout;

                        for (auto i=unanimatedVertexLayout.begin(); i!=unanimatedVertexLayout.end();) {
                            const bool mustBeSkinned = 
                                std::find_if(   elementsToBeSkinned.begin(), elementsToBeSkinned.end(), 
                                                [&](const std::string& s){ return !XlCompareStringI(i->_semanticName.c_str(), s.c_str()); }) 
                                        != elementsToBeSkinned.end();
                            if (mustBeSkinned) {
                                animatedVertexLayout.push_back(*i);
                                i=unanimatedVertexLayout.erase(i);
                            } else ++i;
                        }

                        {
                            unsigned elementOffset = 0;     // reset the _alignedByteOffset members in the vertex layout
                            for (auto i=unanimatedVertexLayout.begin(); i!=unanimatedVertexLayout.end();++i) {
                                i->_alignedByteOffset = elementOffset;
                                elementOffset += Metal::BitsPerPixel(i->_nativeFormat)/8;
                            }
                        }

                        unsigned unanimatedVertexStride  = CalculateVertexSize(AsPointer(unanimatedVertexLayout.begin()), AsPointer(unanimatedVertexLayout.end()));
                        unsigned animatedVertexStride    = CalculateVertexSize(AsPointer(animatedVertexLayout.begin()), AsPointer(animatedVertexLayout.end()));

                        if (!animatedVertexStride) {
                            ThrowException(FormatError("Could not find any animated vertex elements in skinning controller in node (%s). There must be a problem with vertex input semantics.", GetNodeStringID(node).c_str()));
                        }
                            
                            //      Copy out those parts of the vertex buffer that are unanimated and animated
                            //      (we also do the vertex reordering here)
                        std::unique_ptr<uint8[]> unanimatedVertexBuffer  = std::make_unique<uint8[]>(unanimatedVertexStride*unifiedVertexCount);
                        std::unique_ptr<uint8[]> animatedVertexBuffer    = std::make_unique<uint8[]>(animatedVertexStride*unifiedVertexCount);
                        CopyVertexElements( unanimatedVertexBuffer.get(),                   unanimatedVertexStride, 
                                            source->_vertices.get(),                        source->_mainDrawInputAssembly._vertexStride,
                                            AsPointer(unanimatedVertexLayout.begin()),      AsPointer(unanimatedVertexLayout.end()),
                                            AsPointer(source->_mainDrawInputAssembly._vertexInputLayout.begin()), AsPointer(source->_mainDrawInputAssembly._vertexInputLayout.end()),
                                            AsPointer(unifiedVertexReordering.begin()),     AsPointer(unifiedVertexReordering.end()));

                        CopyVertexElements( animatedVertexBuffer.get(),                     animatedVertexStride,
                                            source->_vertices.get(),                        source->_mainDrawInputAssembly._vertexStride,
                                            AsPointer(animatedVertexLayout.begin()),        AsPointer(animatedVertexLayout.end()),
                                            AsPointer(source->_mainDrawInputAssembly._vertexInputLayout.begin()), AsPointer(source->_mainDrawInputAssembly._vertexInputLayout.end()),
                                            AsPointer(unifiedVertexReordering.begin()),     AsPointer(unifiedVertexReordering.end()));

                            //      We have to remap the index buffer, also.
                        std::unique_ptr<uint8[]> newIndexBuffer = std::make_unique<uint8[]>(source->_indices.size());
                        if (source->_indexFormat == Metal::NativeFormat::R16_UINT) {
                            std::transform(
                                (const uint16*)source->_indices.begin(), (const uint16*)source->_indices.end(),
                                (uint16*)newIndexBuffer.get(),
                                [&unifiedVertexReordering](uint16 inputIndex){return unifiedVertexReordering[inputIndex];});
                        } else if (source->_indexFormat == Metal::NativeFormat::R8_UINT) {
                            std::transform(
                                (const uint8*)source->_indices.begin(), (const uint8*)source->_indices.end(),
                                (uint8*)newIndexBuffer.get(),
                                [&unifiedVertexReordering](uint8 inputIndex){return unifiedVertexReordering[inputIndex];});
                        } else {
                            ThrowException(FormatError("Unrecognised index format when instantiating skin controller in node (%s).", GetNodeStringID(node).c_str()));
                        }
                                
                            //      We have to define the draw calls that perform the pre-skinning step

                        std::vector<NascentDrawCallDesc> preskinningDrawCalls;
                        if (bucketEnd[0] > bucketStart[0]) {
                            preskinningDrawCalls.push_back(NascentDrawCallDesc(
                                ~unsigned(0x0), bucketEnd[0] - bucketStart[0], bucketStart[0],
                                4, Metal::Topology::PointList));
                        }
                        if (bucketEnd[1] > bucketStart[1]) {
                            preskinningDrawCalls.push_back(NascentDrawCallDesc(
                                ~unsigned(0x0), bucketEnd[1] - bucketStart[1], bucketStart[1],
                                2, Metal::Topology::PointList));
                        }
                        if (bucketEnd[2] > bucketStart[2]) {
                            preskinningDrawCalls.push_back(NascentDrawCallDesc(
                                ~unsigned(0x0), bucketEnd[2] - bucketStart[2], bucketStart[2],
                                1, Metal::Topology::PointList));
                        }

                        assert(bucketEnd[2] <= unifiedVertexCount);

                            //      Build the final vertex weights buffer (our weights are currently stored
                            //      per vertex-position. So we need to expand to per-unified vertex -- blaggh!)
                            //      This means the output weights vertex buffer is going to be larger than input ones combined.

                        assert(newUnifiedVertexIndexToPositionIndex.size()==unifiedVertexCount);
                        size_t destinationWeightVertexStride = 0;
                        const std::vector<Metal::InputElementDesc>* finalWeightBufferFormat = nullptr;

                        unsigned bucketVertexSizes[bucketCount];
                        for (unsigned b=0; b<bucketCount; ++b) {
                            bucketVertexSizes[b] = CalculateVertexSize(     
                                AsPointer(controller->_bucket[b]._vertexInputLayout.begin()), 
                                AsPointer(controller->_bucket[b]._vertexInputLayout.end()));

                            if (controller->_bucket[b]._vertexBufferSize) {
                                if (bucketVertexSizes[b] > destinationWeightVertexStride) {
                                    destinationWeightVertexStride = bucketVertexSizes[b];
                                    finalWeightBufferFormat = &controller->_bucket[b]._vertexInputLayout;
                                }
                            }
                        }

                        unsigned alignedDestinationWeightVertexStride = (unsigned)std::max(destinationWeightVertexStride, size_t(4));
                        if (alignedDestinationWeightVertexStride != destinationWeightVertexStride) {
                            Warning("Warning -- vertex buffer had to be expanded for vertex alignment restrictions in node (%s). This will leave some wasted space in the vertex buffer. This can be caused when using skinning when only 1 weight is really required.\n", GetNodeStringID(node).c_str());
                            destinationWeightVertexStride = alignedDestinationWeightVertexStride;
                        }

                        std::unique_ptr<uint8[]> skeletonBindingVertices;
                        if (destinationWeightVertexStride && finalWeightBufferFormat) {
                            skeletonBindingVertices = std::make_unique<uint8[]>(destinationWeightVertexStride*unifiedVertexCount);
                            XlSetMemory(skeletonBindingVertices.get(), 0, destinationWeightVertexStride*unifiedVertexCount);

                            for (auto i=newUnifiedVertexIndexToPositionIndex.begin(); i!=newUnifiedVertexIndexToPositionIndex.end(); ++i) {
                                const size_t destinationVertexIndex = i-newUnifiedVertexIndexToPositionIndex.begin();
                                unsigned sourceVertexPositionIndex = *i;
                                
                                    //
                                    //      We actually need to find the source position vertex from one of the buckets.
                                    //      We can make a guess from the ordering, but it's safest to find it again
                                    //      This lookup could get quite expensive for large meshes!
                                    //
                                for (unsigned b=0; b<bucketCount; ++b) {
                                    auto i = std::find( controller->_bucket[b]._vertexBindings.begin(), 
                                                        controller->_bucket[b]._vertexBindings.end(), 
                                                        sourceVertexPositionIndex);
                                    if (i!=controller->_bucket[b]._vertexBindings.end()) {

                                            //
                                            //      Note that sometimes we'll be expanding the vertex format in this process
                                            //      If some buckets are using R8G8, and others are R8G8B8A8 (for example)
                                            //      then they will all be expanded to the largest size
                                            //

                                        auto sourceVertexStride = bucketVertexSizes[b];
                                        size_t sourceVertexInThisBucket = std::distance(controller->_bucket[b]._vertexBindings.begin(), i);
                                        void* destinationVertex = PtrAdd(skeletonBindingVertices.get(), destinationVertexIndex*destinationWeightVertexStride);
                                        assert((sourceVertexInThisBucket+1)*sourceVertexStride <= controller->_bucket[b]._vertexBufferSize);
                                        const void* sourceVertex = PtrAdd(controller->_bucket[b]._vertexBufferData.get(), sourceVertexInThisBucket*sourceVertexStride);

                                        if (sourceVertexStride == destinationWeightVertexStride) {
                                            XlCopyMemory(destinationVertex, sourceVertex, sourceVertexStride);
                                        } else {
                                            const Metal::InputElementDesc* dstElement = AsPointer(finalWeightBufferFormat->cbegin());
                                            for (   auto srcElement=controller->_bucket[b]._vertexInputLayout.cbegin(); 
                                                    srcElement!=controller->_bucket[b]._vertexInputLayout.cend(); ++srcElement, ++dstElement) {
                                                unsigned elementSize = std::min(Metal::BitsPerPixel(srcElement->_nativeFormat)/8, Metal::BitsPerPixel(dstElement->_nativeFormat)/8);
                                                assert(PtrAdd(destinationVertex, dstElement->_alignedByteOffset+elementSize) <= PtrAdd(skeletonBindingVertices.get(), destinationWeightVertexStride*unifiedVertexCount));
                                                assert(PtrAdd(sourceVertex, srcElement->_alignedByteOffset+elementSize) <= PtrAdd(controller->_bucket[b]._vertexBufferData.get(), controller->_bucket[b]._vertexBufferSize));
                                                XlCopyMemory(   PtrAdd(destinationVertex, dstElement->_alignedByteOffset), 
                                                                PtrAdd(sourceVertex, srcElement->_alignedByteOffset), 
                                                                elementSize);   // (todo -- precalculate this min of element sizes)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                            //  Double check that weights are normalized in the binding buffer
                        #if 0 // defined(_DEBUG)

                            {
                                unsigned weightsOffset = 0;
                                Metal::NativeFormat::Enum weightsFormat = Metal::NativeFormat::Unknown;
                                for (auto i=finalWeightBufferFormat->cbegin(); i!=finalWeightBufferFormat->cend(); ++i) {
                                    if (!XlCompareStringI(i->_semanticName.c_str(), "WEIGHTS") && i->_semanticIndex == 0) {
                                        weightsOffset = i->_alignedByteOffset;
                                        weightsFormat = i->_nativeFormat;
                                        break;
                                    }
                                }

                                size_t stride = destinationWeightVertexStride;
                                if (weightsFormat == Metal::NativeFormat::R8G8_UNORM) {
                                    for (unsigned c=0; c<unifiedVertexCount; ++c) {
                                        const void* p = PtrAdd(skeletonBindingVertices.get(), c*stride+weightsOffset);
                                        unsigned char zero   = ((unsigned char*)p)[0];
                                        unsigned char one    = ((unsigned char*)p)[1];
                                        assert((zero+one) >= 0xfd);
                                    }
                                } else if (weightsFormat == Metal::NativeFormat::R8G8B8A8_UNORM) {
                                    for (unsigned c=0; c<unifiedVertexCount; ++c) {
                                        const void* p = PtrAdd(skeletonBindingVertices.get(), c*stride+weightsOffset);
                                        unsigned char zero   = ((unsigned char*)p)[0];
                                        unsigned char one    = ((unsigned char*)p)[1];
                                        unsigned char two    = ((unsigned char*)p)[2];
                                        unsigned char three  = ((unsigned char*)p)[3];
                                        assert((zero+one+two+three) >= 0xfd);
                                    }
                                } else {
                                    assert(weightsFormat == Metal::NativeFormat::R8_UNORM);
                                }
                            }
                                
                        #endif

                            //      We need to map from from our joint indices to output matrix index
                        const size_t jointCount = controllerAndSkeleton->_jointIds.size();
                        std::unique_ptr<uint16[]> jointMatrices = std::make_unique<uint16[]>(jointCount);
                        for (auto i = controllerAndSkeleton->_jointIds.cbegin(); i!=controllerAndSkeleton->_jointIds.cend(); ++i) {
                            jointMatrices[std::distance(controllerAndSkeleton->_jointIds.cbegin(), i)] = 
                                (uint16)FindTransformationMachineOutput(*i);
                        }

                            //      Calculate the local space bounding box for the input vertex buffer
                            //      (assuming the position will appear in the animated vertex buffer)
                        auto boundingBox = InvalidBoundingBox();
                        Metal::InputElementDesc positionDesc = FindPositionElement(
                            AsPointer(animatedVertexLayout.begin()),
                            animatedVertexLayout.size());
                        if (positionDesc._nativeFormat != Metal::NativeFormat::Unknown) {
                            AddToBoundingBox(
                                boundingBox,
                                animatedVertexBuffer.get(), animatedVertexStride, unifiedVertexCount,
                                positionDesc, Identity<Float4x4>());
                        }

                            //      Build the final "BoundSkinnedGeometry" object
                        NascentBoundSkinnedGeometry result(
                            DynamicArray<uint8>(std::move(unanimatedVertexBuffer), unanimatedVertexStride*unifiedVertexCount),
                            DynamicArray<uint8>(std::move(animatedVertexBuffer), animatedVertexStride*unifiedVertexCount),
                            DynamicArray<uint8>(std::move(skeletonBindingVertices), destinationWeightVertexStride*unifiedVertexCount),
                            DynamicArray<uint8>(std::move(newIndexBuffer), source->_indices.size()));

                        result._skeletonBindingVertexStride = (unsigned)destinationWeightVertexStride;
                        result._animatedVertexBufferSize = (unsigned)(animatedVertexStride*unifiedVertexCount);

                        result._inverseBindMatrices = DynamicArray<Float4x4>::Copy(controller->_inverseBindMatrices);
                        result._jointMatrices = DynamicArray<uint16>(std::move(jointMatrices), jointCount);
                        result._bindShapeMatrix = controller->_bindShapeMatrix;

                        result._mainDrawCalls = source->_mainDrawCalls;
                        result._mainDrawUnanimatedIA._vertexStride = unanimatedVertexStride;
                        result._mainDrawUnanimatedIA._vertexInputLayout = std::move(unanimatedVertexLayout);
                        result._indexFormat = source->_indexFormat;

                        result._mainDrawAnimatedIA._vertexStride = animatedVertexStride;
                        result._mainDrawAnimatedIA._vertexInputLayout = std::move(animatedVertexLayout);

                        result._preskinningDrawCalls = preskinningDrawCalls;

                        if (finalWeightBufferFormat) {
                            result._preskinningIA._vertexInputLayout = *finalWeightBufferFormat;
                            result._preskinningIA._vertexStride = destinationWeightVertexStride;
                        }

                        result._localBoundingBox = boundingBox;

                        std::tuple<std::string, std::string, COLLADAFW::UniqueId> desc = 
                            accessableObjects.GetDesc<UnboundSkinController>(controllerAndSkeleton->_unboundControllerId);
                        ObjectId finalObjectTableId = destinationForNewObjects.Add(
                            std::get<0>(desc), std::get<1>(desc), std::get<2>(desc),
                            std::move(result));
                                
                            //
                            //  Have to build the material bindings, as well..
                            //
                        std::vector<ObjectId> materials = BuildMaterialTable(
                            node.getInstanceControllers()[instanceController]->getMaterialBindings(), source->_materials, accessableObjects);

                        SkinControllerInstance newInstance(finalObjectTableId, FindTransformationMachineOutput(AsHashedColladaUniqueId(node.getUniqueId())), std::move(materials), 0);
                        _skinControllerInstances.push_back(newInstance);

                    } else {
                        Warning("Warning -- skin controller attached to bad source object in node (%s). Note that skin controllers must be attached directly to geometry. We don't support cascading controllers.\n", GetNodeStringID(node).c_str());
                    }

                } else {
                    Warning("Warning -- skin controller with attached skeleton points to invalid skin controller in node (%s)\n", GetNodeStringID(node).c_str());
                }

            }
        }
            
        const NodePointerArray& childNodes = node.getChildNodes();
        for (size_t c=0; c<childNodes.getCount(); ++c) {
            InstantiateControllers(*childNodes[c], accessableObjects, destinationForNewObjects);
        }
    }

    NascentModelCommandStream::NascentModelCommandStream()
    {
    }

    NascentModelCommandStream::~NascentModelCommandStream()
    {

    }

    NascentModelCommandStream::NascentModelCommandStream(NascentModelCommandStream&& moveFrom)
    :       _transformationMachineOutputs(std::move(moveFrom._transformationMachineOutputs))
    ,       _geometryInstances(std::move(moveFrom._geometryInstances))
    ,       _modelInstances(std::move(moveFrom._modelInstances))
    ,       _cameraInstances(std::move(moveFrom._cameraInstances))
    ,       _skinControllerInstances(std::move(moveFrom._skinControllerInstances))
    {
    }

    NascentModelCommandStream& NascentModelCommandStream::operator=(NascentModelCommandStream&& moveFrom) never_throws
    {
        _transformationMachineOutputs = std::move(moveFrom._transformationMachineOutputs);
        _geometryInstances = std::move(moveFrom._geometryInstances);
        _modelInstances = std::move(moveFrom._modelInstances);
        _cameraInstances = std::move(moveFrom._cameraInstances);
        _skinControllerInstances = std::move(moveFrom._skinControllerInstances);
        return *this;
    }

    void NascentModelCommandStream::GeometryInstance::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _id);
        Serialization::Serialize(serializer, _localToWorldId);
        serializer.SerializeSubBlock(AsPointer(_materials.begin()), AsPointer(_materials.end()));
        Serialization::Serialize(serializer, _materials.size());
        Serialization::Serialize(serializer, _levelOfDetail);
    }

    void NascentModelCommandStream::SkinControllerInstance::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _id);
        Serialization::Serialize(serializer, _localToWorldId);
        serializer.SerializeSubBlock(AsPointer(_materials.begin()), AsPointer(_materials.end()));
        Serialization::Serialize(serializer, _materials.size());
        Serialization::Serialize(serializer, _levelOfDetail);
    }

    void NascentModelCommandStream::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        serializer.SerializeSubBlock(AsPointer(_geometryInstances.begin()), AsPointer(_geometryInstances.end()));
        serializer.SerializeValue(_geometryInstances.size());
        serializer.SerializeSubBlock(AsPointer(_skinControllerInstances.begin()), AsPointer(_skinControllerInstances.end()));
        serializer.SerializeValue(_skinControllerInstances.size());
            
            //
            //      Turn our list of input matrices into hash values, and write out the
            //      run-time input interface definition...
            //
        ConsoleRig::DebuggerOnlyWarning("Command stream input interface:\n");
        auto inputInterface = std::make_unique<uint64[]>(_transformationMachineOutputs.size());
        unsigned c=0;
        for (auto i=_transformationMachineOutputs.begin(); i!=_transformationMachineOutputs.end(); ++i, ++c) {
            ConsoleRig::DebuggerOnlyWarning("  [%i] %s\n", std::distance(_transformationMachineOutputs.begin(), i), i->_name.c_str());
            inputInterface[c] = Hash64(AsPointer(i->_name.begin()), AsPointer(i->_name.end()));
        }
        serializer.SerializeSubBlock(inputInterface.get(), &inputInterface[_transformationMachineOutputs.size()]);
        serializer.SerializeValue(_transformationMachineOutputs.size());
    }

    



    bool IsUseful(  const COLLADAFW::Node& node, const TableOfObjects& objects,
                    const JointReferences& skeletonReferences)
    {
            //
            //      Traverse all of the nodes in the hierarchy
            //      and look for something that is useful for us.
            //
            //          Many node types we just ignore (like lights, etc)
            //          Let's strip them off first.
            //
            //      This node is useful if there is an instantiation of 
            //      something useful, or if a child node is useful.
            //
            //      But note that some nodes are just an "armature" or a
            //      set of skeleton bones. These don't have anything attached,
            //      but they are still important. 
            //
            //      We need to make sure that
            //      if we're instantiating skin instance_controller, then
            //      we consider all of the nodes in the attached "skeleton"
            //      to be important. To do this, we may need to scan to find
            //      all of the instance_controllers first, and use that to
            //      decide which pure nodes to keep.
            //
            //      It looks like there may be another problem in OpenCollada
            //      here. In pure collada, one skin controller can be attached
            //      to different skeletons in different instance_controllers. But
            //      it's not clear if that's also the case in OpenCollada (OpenCollada
            //      has a different scheme for binding instances to skeletons.
            //
        using namespace COLLADAFW;
        const InstanceGeometryPointerArray& instanceGeometrys = node.getInstanceGeometries();
        for (size_t c=0; c<instanceGeometrys.getCount(); ++c)
            if (objects.Has<NascentRawGeometry>(instanceGeometrys[c]->getInstanciatedObjectId()))
                return true;
        
        const InstanceNodePointerArray& instanceNodes = node.getInstanceNodes();
        for (size_t c=0; c<instanceNodes.getCount(); ++c)
            if (objects.Has<NascentModelCommandStream>(instanceNodes[c]->getInstanciatedObjectId()))
                return true;

        const InstanceControllerPointerArray& instanceControllers = node.getInstanceControllers();
        for (size_t c=0; c<instanceControllers.getCount(); ++c)
            if (objects.Has<UnboundSkinControllerAndAttachedSkeleton>(instanceControllers[c]->getInstanciatedObjectId()))
                return true;

        if (ImportCameras && !node.getInstanceCameras().empty())
            return true;

            // if this node is part of any of the skeletons we need, then it's "useful"
        if (skeletonReferences.HasJoint(AsHashedColladaUniqueId(node.getUniqueId())))
            return true;

        const NodePointerArray& childNodes = node.getChildNodes();
        for (size_t c=0; c<childNodes.getCount(); ++c)
            if (IsUseful(*childNodes[c], objects, skeletonReferences))
                return true;

        return false;
    }

    void FindInstancedSkinControllers(  const COLLADAFW::Node& node, const TableOfObjects& objects,
                                        JointReferences& results)
    {
        using namespace COLLADAFW;
        const InstanceControllerPointerArray& instanceControllers = node.getInstanceControllers();
        for (size_t c=0; c<instanceControllers.getCount(); ++c) {
            const UnboundSkinControllerAndAttachedSkeleton* controller = nullptr;
            ObjectId tableId = objects.Get<UnboundSkinControllerAndAttachedSkeleton>(
                instanceControllers[c]->getInstanciatedObjectId());
            if (tableId != ObjectId_Invalid) {
                controller = objects.GetFromObjectId<UnboundSkinControllerAndAttachedSkeleton>(tableId);
            }

            if (controller) {

                const UnboundSkinController* skinController = 
                    objects.GetFromObjectId<UnboundSkinController>(controller->_unboundControllerId);

                for (size_t c=0; c<controller->_jointIds.size(); ++c) {
                    JointReferences::Reference ref;
                    ref._joint = controller->_jointIds[c];
                    ref._inverseBindMatrix = Identity<Float4x4>();

                        //
                        //      look for an inverse bind matrix associated with this joint
                        //      from any attached skinning controllers
                        //      We need to know inverse bind matrices when combining skeletons
                        //      and animation data from different exports
                        //
                    if (c<skinController->_inverseBindMatrices.size()) {
                        ref._inverseBindMatrix = skinController->_inverseBindMatrices[(unsigned)c];
                    }

                    results._references.push_back(ref);
                }

            } else {
                Warning("Warning -- couldn't match skin controller in node (%s)\n", GetNodeStringID(node).c_str());
            }
        }

        const NodePointerArray& childNodes = node.getChildNodes();
        for (size_t c=0; c<childNodes.getCount(); ++c)
            FindInstancedSkinControllers(*childNodes[c], objects, results);
    }

}}

