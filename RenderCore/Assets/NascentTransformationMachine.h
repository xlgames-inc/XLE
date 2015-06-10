// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TransformationCommands.h"
#include "../../Core/Types.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/Mixins.h"
#include "../../Utility/IteratorUtils.h"
#include <vector>

namespace RenderCore { namespace Assets
{
    typedef Assets::TransformationParameterSet::Type::Enum AnimSamplerType;

    using AnimationParameterId = uint32;

    class NascentTransformationMachine : noncopyable
    {
    public:
        unsigned        PushTransformation(const Float4x4& localToParent, const char nodeName[]);
        void            Pop                     (   unsigned popCount);
        unsigned        GetOutputMatrixMarker   ();
        unsigned        GetOutputMatrixCount    () const        { return _outputMatrixCount; }
        bool            IsEmpty                 () const        { return _commandStream.empty(); }

        const Assets::TransformationParameterSet&       GetDefaultParameters() const { return _defaultParameters; }

        std::pair<AnimSamplerType, uint32>  GetParameterIndex(AnimationParameterId parameterName) const;
        AnimationParameterId                GetParameterName(AnimSamplerType type, uint32 index) const;

        void                        RegisterJointName(AnimationParameterId colladaId, const std::string& name, const Float4x4& inverseBindMatrix, unsigned outputMatrixIndex);
        
        std::string                 HashedIdToStringId     (AnimationParameterId colladaId) const;
        AnimationParameterId        StringIdToHashedId     (const std::string& stringId) const;

        void    Serialize(Serialization::NascentBlockSerializer& outputSerializer) const;

        std::unique_ptr<Float4x4[]>
            GenerateOutputTransforms(const Assets::TransformationParameterSet&   parameterSet) const;

        const std::vector<uint32>&  GetCommandStream() const { return _commandStream; }

        NascentTransformationMachine();
        NascentTransformationMachine(NascentTransformationMachine&& machine);
        NascentTransformationMachine& operator=(NascentTransformationMachine&& moveFrom);
        ~NascentTransformationMachine();

    protected:
        std::vector<uint32>                     _commandStream;
        unsigned                                _outputMatrixCount;
        Assets::TransformationParameterSet      _defaultParameters;

            // ... parameters required only during construction ... 
        std::vector<std::pair<AnimationParameterId, uint32>>    _float1ParameterNames;
        std::vector<std::pair<AnimationParameterId, uint32>>    _float3ParameterNames;
        std::vector<std::pair<AnimationParameterId, uint32>>    _float4ParameterNames;
        std::vector<std::pair<AnimationParameterId, uint32>>    _float4x4ParameterNames;
        int                                                     _pendingPops;

        std::vector<std::pair<std::string, AnimationParameterId>>  _stringNameMapping;

        class Joint;
        std::vector<Joint> _jointTags;

        template<typename Type>
            uint32      AddParameter(Type defaultValue, AnimationParameterId HashedColladaUniqueId, const char nodeName[]);
        template<typename Type>
            std::pair<
                std::vector<std::pair<AnimationParameterId, uint32>>&,
                std::vector<Type, Serialization::BlockSerializerAllocator<Type>>& >    
                GetTables();
    };


        ////////////////// template implementation //////////////////

    template<typename Type>
        uint32      NascentTransformationMachine::AddParameter( 
            Type defaultValue, 
            AnimationParameterId parameterHash, 
            const char nodeName[])
    {
        auto tables = GetTables<Type>();
        auto i      = std::lower_bound( tables.first.begin(), tables.first.end(), 
                                        parameterHash, CompareFirst<AnimationParameterId, uint32>());
        if (i!=tables.first.end() && i->first == parameterHash) {
            ThrowException(::Assets::Exceptions::FormatError("Non-unique parameter hash found for animatable property in node (%s)", nodeName));
        }

        _stringNameMapping.push_back(std::make_pair(std::string(nodeName), parameterHash));

        size_t parameterIndex = tables.second.size();
        tables.second.push_back(defaultValue);
        tables.first.insert(i, std::make_pair(parameterHash, (uint32)parameterIndex));
        return (uint32)parameterIndex;
    }
}}



