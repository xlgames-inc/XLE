// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/StringUtils.h"
#include "../../Core/Types.h"
#include <vector>

namespace RenderCore { namespace Assets
{
	enum class TransformStackCommand : uint32;
	enum class AnimSamplerType;
	class ITransformationMachineOptimizer;
	class TransformationParameterSet;
}}

namespace RenderCore { namespace Assets { namespace GeoProc
{
	class NascentSkeletonInterface;

    class NascentSkeletonMachine
    {
    public:
        unsigned        PushTransformation(const Float4x4& localToParent);
        void            Pop                     (   unsigned popCount);
        unsigned        GetOutputMatrixCount    () const        { return _outputMatrixCount; }
        bool            IsEmpty                 () const        { return _commandStream.empty(); }

        template<typename Serializer>
            void    Serialize(Serializer& outputSerializer) const;

        std::unique_ptr<Float4x4[]>
            GenerateOutputTransforms(const TransformationParameterSet&   parameterSet) const;

        const std::vector<uint32>&  GetCommandStream() const { return _commandStream; }

        friend std::ostream& StreamOperator(
			std::ostream& stream, 
			const NascentSkeletonMachine& transMachine, 
			const NascentSkeletonInterface& interf,
			const TransformationParameterSet& defaultParameters);

        void    PushCommand(uint32 cmd);
        void    PushCommand(TransformStackCommand cmd);
        void    PushCommand(const void* ptr, size_t size);
		void	WriteOutputMarker(unsigned marker);
        void    ResolvePendingPops();

        void    Optimize(ITransformationMachineOptimizer& optimizer);

        NascentSkeletonMachine();
        NascentSkeletonMachine(NascentSkeletonMachine&& machine) = default;
        NascentSkeletonMachine& operator=(NascentSkeletonMachine&& moveFrom) = default;
        ~NascentSkeletonMachine();

    protected:
        std::vector<uint32>     _commandStream;
        unsigned                _outputMatrixCount;
        int						_pendingPops; // Only required during construction
    };

	using AnimationParameterHashName = uint32;

	class NascentSkeletonInterface
	{
	public:
		T1(Type) bool	TryAddParameter(uint32& paramIndex, StringSection<char> paramName, AnimationParameterHashName hashName);
		bool			TryRegisterJointName(uint32& outputMarker, StringSection<char> name, const Float4x4& inverseBindMatrix);

		template<typename Serializer>
			void    Serialize(Serializer& outputSerializer) const;

		friend std::ostream& StreamOperator(
			std::ostream& stream, 
			const NascentSkeletonMachine& transMachine, 
			const NascentSkeletonInterface& interf,
			const TransformationParameterSet& defaultParameters);

		NascentSkeletonInterface();
		NascentSkeletonInterface(NascentSkeletonInterface&& machine) = default;
		NascentSkeletonInterface& operator=(NascentSkeletonInterface&& moveFrom) = default;
		~NascentSkeletonInterface();

	private:
		std::vector<AnimationParameterHashName>		_float1ParameterNames;
		std::vector<AnimationParameterHashName>		_float3ParameterNames;
		std::vector<AnimationParameterHashName>		_float4ParameterNames;
		std::vector<AnimationParameterHashName>		_float4x4ParameterNames;

		std::vector<std::pair<AnimationParameterHashName, std::string>>  _dehashTable;

		class Joint;
		std::vector<Joint> _jointTags;

		unsigned _lastReturnedOutputMatrixMarker;

		template<typename Type>
			std::vector<AnimationParameterHashName>& GetTables();

		std::pair<std::vector<uint64>, std::vector<Float4x4>> GetOutputInterface() const;
		std::pair<AnimSamplerType, uint32>  GetParameterIndex(AnimationParameterHashName parameterName) const;
		AnimationParameterHashName			GetParameterName(AnimSamplerType type, uint32 index) const;

		std::string                 HashedIdToStringId     (AnimationParameterHashName colladaId) const;
		AnimationParameterHashName	StringIdToHashedId     (const std::string& stringId) const;
	};

        ////////////////// template implementation //////////////////

    template<typename Type>
        bool NascentSkeletonInterface::TryAddParameter( 
            uint32& paramIndex,
			StringSection<char> paramName,
			AnimationParameterHashName hashName)
    {
        auto& tables = GetTables<Type>();
        auto i = std::find(tables.begin(), tables.end(), hashName);
        if (i!=tables.end()) {
            LogWarning << "Duplicate animation parameter name in node " << paramName.AsString() << ". Only the first will work. The rest will be static.";
            return false;
        }

		_dehashTable.push_back(std::make_pair(hashName, paramName.AsString()));
        paramIndex = (uint32)tables.size();
        tables[paramIndex] = hashName;
        return true;
    }

}}}



