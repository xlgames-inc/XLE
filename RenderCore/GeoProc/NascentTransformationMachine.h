// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/TransformationCommands.h"
#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/Mixins.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Core/Types.h"
#include <vector>

namespace RenderCore { namespace Assets { namespace GeoProc
{
    using AnimationParameterId = uint32;

    class NascentTransformationMachine : noncopyable
    {
    public:
        unsigned        PushTransformation(const Float4x4& localToParent);
        void            Pop                     (   unsigned popCount);
        unsigned        GetOutputMatrixMarker   ();
        void            MakeOutputMatrixMarker  (unsigned marker);
        unsigned        GetOutputMatrixCount    () const        { return _outputMatrixCount; }
        bool            IsEmpty                 () const        { return _commandStream.empty(); }

        const TransformationParameterSet&       GetDefaultParameters() const { return _defaultParameters; }

        std::pair<AnimSamplerType, uint32>  GetParameterIndex(AnimationParameterId parameterName) const;
        AnimationParameterId                GetParameterName(AnimSamplerType type, uint32 index) const;

        void    RegisterJointName(const std::string& name, const Float4x4& inverseBindMatrix, unsigned outputMatrixIndex);
        bool    TryRegisterJointName(const std::string& name, const Float4x4& inverseBindMatrix, unsigned outputMatrixIndex);
        
        std::string                 HashedIdToStringId     (AnimationParameterId colladaId) const;
        AnimationParameterId        StringIdToHashedId     (const std::string& stringId) const;

        template<typename Serializer>
            void    Serialize(Serializer& outputSerializer) const;

        std::unique_ptr<Float4x4[]>
            GenerateOutputTransforms(const TransformationParameterSet&   parameterSet) const;

        const std::vector<uint32>&  GetCommandStream() const { return _commandStream; }
        std::pair<std::vector<uint64>, std::vector<Float4x4>> GetOutputInterface() const;

        friend std::ostream& StreamOperator(std::ostream& stream, const NascentTransformationMachine& transMachine);

        template<typename Type> bool TryAddParameter(uint32& paramIndex, Type defaultValue, AnimationParameterId HashedColladaUniqueId, const char nodeName[]);

        void    PushCommand(uint32 cmd);
        void    PushCommand(TransformStackCommand cmd);
        void    PushCommand(const void* ptr, size_t size);
        void    ResolvePendingPops();

        void    Optimize(ITransformationMachineOptimizer& optimizer);

        NascentTransformationMachine();
        NascentTransformationMachine(NascentTransformationMachine&& machine);
        NascentTransformationMachine& operator=(NascentTransformationMachine&& moveFrom);
        ~NascentTransformationMachine();

    protected:
        std::vector<uint32>         _commandStream;
        unsigned                    _outputMatrixCount;
        TransformationParameterSet  _defaultParameters;

            // ... parameters required only during construction ... 
        std::vector<std::pair<AnimationParameterId, uint32>>    _float1ParameterNames;
        std::vector<std::pair<AnimationParameterId, uint32>>    _float3ParameterNames;
        std::vector<std::pair<AnimationParameterId, uint32>>    _float4ParameterNames;
        std::vector<std::pair<AnimationParameterId, uint32>>    _float4x4ParameterNames;
        int                                                     _pendingPops;

        std::vector<std::pair<std::string, AnimationParameterId>>  _stringNameMapping;

        class Joint;
        std::vector<Joint> _jointTags;

        unsigned _lastReturnedOutputMatrixMarker;

        template<typename Type>
			std::vector<std::pair<AnimationParameterId, uint32>>& GetTables();
    };


        ////////////////// template implementation //////////////////

    template<typename Type>
        bool NascentTransformationMachine::TryAddParameter( 
            uint32& paramIndex,
            Type defaultValue, 
            AnimationParameterId parameterHash, 
            const char nodeName[])
    {
        auto& tables = GetTables<Type>();
        auto i = LowerBound(tables, parameterHash);
        if (i!=tables.end() && i->first == parameterHash) {
            LogWarning << "Duplicate animation parameter name in node " << nodeName << ". Only the first will work. The rest will be static.";
            return false;
        }

        _stringNameMapping.push_back(std::make_pair(std::string(nodeName), parameterHash));
        paramIndex = (uint32)_defaultParameters.AddParameter(defaultValue);
        tables.insert(i, std::make_pair(parameterHash, paramIndex));
        return true;
    }

}}}



