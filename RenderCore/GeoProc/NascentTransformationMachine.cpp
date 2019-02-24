// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentTransformationMachine.h"
#include "../Assets/TransformationCommands.h"
#include "../RenderUtils.h"
#include "../../Assets/Assets.h"
#include "../../Assets/BlockSerializer.h"
#include "../../ConsoleRig/OutputStream.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/Streams/Serialization.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
	std::unique_ptr<Float4x4[]>           NascentSkeletonMachine::GenerateOutputTransforms(
		const Assets::TransformationParameterSet&   parameterSet) const
	{
		std::unique_ptr<Float4x4[]> result = std::make_unique<Float4x4[]>(size_t(_outputMatrixCount));
		RenderCore::Assets::GenerateOutputTransforms(
			MakeIteratorRange(result.get(), result.get() + _outputMatrixCount),
			&parameterSet, 
			MakeIteratorRange(_commandStream));
		return result;
	}

    template <>
        void    NascentSkeletonMachine::Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        outputSerializer.SerializeSubBlock(MakeIteratorRange(_commandStream));
        outputSerializer.SerializeValue(_commandStream.size());
        outputSerializer.SerializeValue(_outputMatrixCount);
	}

	void        NascentSkeletonMachine::WriteOutputMarker(unsigned marker)
	{
		_outputMatrixCount = std::max(_outputMatrixCount, marker+1);
		_commandStream.push_back((uint32)Assets::TransformStackCommand::WriteOutputMatrix);
		_commandStream.push_back(marker);
	}

	void		NascentSkeletonMachine::Pop(unsigned popCount)
	{
		_pendingPops += popCount;
	}

	static unsigned int FloatBits(float input)
	{
		// (or just use a reinterpret cast)
		union Converter { float f; unsigned int i; };
		Converter c; c.f = input; 
		return c.i;
	}

	unsigned    NascentSkeletonMachine::PushTransformation(const Float4x4& localToParent)
	{
		ResolvePendingPops();

		// push a basic, unanimatable transform
		//  see also NascentTransformationMachine_Collada::PushTransformations for a complex
		//  version of this
		_commandStream.push_back((uint32)Assets::TransformStackCommand::PushLocalToWorld);
		_commandStream.push_back((uint32)Assets::TransformStackCommand::TransformFloat4x4_Static);
		_commandStream.push_back(FloatBits(localToParent(0, 0)));
		_commandStream.push_back(FloatBits(localToParent(0, 1)));
		_commandStream.push_back(FloatBits(localToParent(0, 2)));
		_commandStream.push_back(FloatBits(localToParent(0, 3)));

		_commandStream.push_back(FloatBits(localToParent(1, 0)));
		_commandStream.push_back(FloatBits(localToParent(1, 1)));
		_commandStream.push_back(FloatBits(localToParent(1, 2)));
		_commandStream.push_back(FloatBits(localToParent(1, 3)));

		_commandStream.push_back(FloatBits(localToParent(2, 0)));
		_commandStream.push_back(FloatBits(localToParent(2, 1)));
		_commandStream.push_back(FloatBits(localToParent(2, 2)));
		_commandStream.push_back(FloatBits(localToParent(2, 3)));

		_commandStream.push_back(FloatBits(localToParent(3, 0)));
		_commandStream.push_back(FloatBits(localToParent(3, 1)));
		_commandStream.push_back(FloatBits(localToParent(3, 2)));
		_commandStream.push_back(FloatBits(localToParent(3, 3)));
		return 1;
	}

	void NascentSkeletonMachine::PushCommand(uint32 cmd)
	{
		_commandStream.push_back(cmd);
	}

	void NascentSkeletonMachine::PushCommand(TransformStackCommand cmd)
	{
		_commandStream.push_back((uint32)cmd);
	}

	void NascentSkeletonMachine::PushCommand(const void* ptr, size_t size)
	{
		assert((size % sizeof(uint32)) == 0);
		_commandStream.insert(_commandStream.end(), (const uint32*)ptr, (const uint32*)PtrAdd(ptr, size));
	}

	void NascentSkeletonMachine::ResolvePendingPops()
	{
		if (_pendingPops) {
			_commandStream.push_back((uint32)Assets::TransformStackCommand::PopLocalToWorld);
			_commandStream.push_back(_pendingPops);
			_pendingPops = 0;
		}
	}

	void NascentSkeletonMachine::Optimize(ITransformationMachineOptimizer& optimizer)
	{
		ResolvePendingPops();
		auto optimized = OptimizeTransformationMachine(MakeIteratorRange(_commandStream), optimizer);
		_commandStream = std::move(optimized);
	}

	void	NascentSkeletonMachine::RemapOutputMatrices(IteratorRange<const unsigned*> outputMatrixMapping)
	{
		ResolvePendingPops();
		_commandStream = RenderCore::Assets::RemapOutputMatrices(
			MakeIteratorRange(_commandStream), 
			outputMatrixMapping);
	}

    NascentSkeletonMachine::NascentSkeletonMachine() : _pendingPops(0), _outputMatrixCount(0) {}
    NascentSkeletonMachine::~NascentSkeletonMachine() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	/*class NascentSkeletonInterface::CompareJointName
	{
	public:
		bool operator()(const Joint& lhs, const Joint& rhs) { return lhs._name < rhs._name; }
		bool operator()(const Joint& lhs, const std::string& rhs) { return lhs._name < rhs; }
		bool operator()(const std::string& lhs, const Joint& rhs) { return lhs < rhs._name; }
	};*/

	NascentSkeletonInterface::NascentSkeletonInterface() {}
	NascentSkeletonInterface::~NascentSkeletonInterface() {}

    template<> auto NascentSkeletonInterface::GetTables<float>()    -> std::vector<AnimationParameterHashName>& { return _float1ParameterNames; }
    template<> auto NascentSkeletonInterface::GetTables<Float3>()   -> std::vector<AnimationParameterHashName>& { return _float3ParameterNames; }
    template<> auto NascentSkeletonInterface::GetTables<Float4>()   -> std::vector<AnimationParameterHashName>& { return _float4ParameterNames; }
    template<> auto NascentSkeletonInterface::GetTables<Float4x4>() -> std::vector<AnimationParameterHashName>& { return _float4x4ParameterNames; }

	template <>
		void    NascentSkeletonInterface::Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
	{
		//
		//      We have to write out both an input and an output interface
		//      First input interface...
		//
#pragma pack(push)
#pragma pack(1)
		struct Param    /* must match SkeletonMachine::InputInterface::Parameter */
		{
			uint64				_name;
			uint32				_index;
			AnimSamplerType		_type;

			void    Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
			{
				::Serialize(outputSerializer, _name);
				::Serialize(outputSerializer, _index);
				::Serialize(outputSerializer, unsigned(_type));
			}
		};
#pragma pack(pop)

		std::vector<Param> runTimeInputInterface;
		typedef std::vector<AnimationParameterHashName> T;
		std::pair<const T*, AnimSamplerType> tables[] = {
			std::make_pair(&_float1ParameterNames,      AnimSamplerType::Float1),
			std::make_pair(&_float3ParameterNames,      AnimSamplerType::Float3),
			std::make_pair(&_float4ParameterNames,      AnimSamplerType::Float4),
			std::make_pair(&_float4x4ParameterNames,    AnimSamplerType::Float4x4)
		};

		ConsoleRig::DebuggerOnlyWarning("Transformation Machine input interface:\n");
		for (unsigned t=0; t<dimof(tables); ++t) {
			for (auto i=tables[t].first->begin(); i!=tables[t].first->end(); ++i) {
				auto n = HashedIdToStringId(*i);
				if (!n.empty()) {
					Param p;
					p._type     = tables[t].second;
					p._index    = uint32(std::distance(tables[t].first->begin(), i));
					p._name     = Hash64(AsPointer(n.begin()), AsPointer(n.end()));
					runTimeInputInterface.push_back(p);

					ConsoleRig::DebuggerOnlyWarning("  (%s, %i) -- %s\n", AsString(p._type), p._index, n.c_str());
				}
			}
		}
		outputSerializer.SerializeSubBlock(MakeIteratorRange(runTimeInputInterface));
		outputSerializer.SerializeValue(runTimeInputInterface.size());

		//
		//      Now, output interface...
		//
		auto jointHashNames = BuildHashedOutputInterface();
		outputSerializer.SerializeSubBlock(MakeIteratorRange(jointHashNames));
		outputSerializer.SerializeValue(jointHashNames.size());
	}

	void NascentSkeletonInterface::SetOutputInterface(IteratorRange<const JointTag*> jointNames)
	{
		_jointTags.clear();
		_jointTags.insert(_jointTags.end(), jointNames.begin(), jointNames.end());
	}

	std::vector<uint64_t> NascentSkeletonInterface::BuildHashedOutputInterface() const
	{
		std::vector<uint64_t> hashedInterface;
		hashedInterface.reserve(_jointTags.size());
		for (const auto&j:_jointTags) hashedInterface.push_back(HashCombine(Hash64(j.first), Hash64(j.second)));
		return hashedInterface;
	}

	std::ostream& StreamOperator(
		std::ostream& stream, 
		const NascentSkeletonMachine& transMachine, 
		const NascentSkeletonInterface& interf,
		const TransformationParameterSet& defaultParameters)
	{
		stream << "Output matrices: " << interf._jointTags.size() << std::endl;
		stream << "Command stream size: " << transMachine._commandStream.size() * sizeof(uint32) << std::endl;

		stream << " --- Animation parameters (" << interf._dehashTable.size() << ") :" << std::endl;
		for (const auto& p:interf._dehashTable) {
			stream << "[" << p.second << "] (0x" << std::hex << p.first << std::dec << ") ";
			auto paramIndex = interf.GetParameterIndex(p.first);
			switch (paramIndex.first) {
			case AnimSamplerType::Float1:
				stream << "Float1[" << paramIndex.second << "], default: " << defaultParameters.GetFloat1Parameters()[paramIndex.second];
				break;
			case AnimSamplerType::Float3:
			{
				auto& f3 = defaultParameters.GetFloat3Parameters()[paramIndex.second];
				stream << "Float3[" << paramIndex.second << "], default: " << f3[0] << ", " << f3[1] << ", " << f3[2];
			}
			break;
			case AnimSamplerType::Float4:
			{
				auto& f4 = defaultParameters.GetFloat4Parameters()[paramIndex.second];
				stream << "Float4[" << paramIndex.second << "], default: " << f4[0] << ", " << f4[1] << ", " << f4[2] << ", " << f4[3];
			}
			break;
			case AnimSamplerType::Quaternion:
			{
				auto& f4 = defaultParameters.GetFloat4Parameters()[paramIndex.second];
				stream << "Quaternion[" << paramIndex.second << "], default: " << f4[0] << ", " << f4[1] << ", " << f4[2] << ", " << f4[3];
			}
			break;
			case AnimSamplerType::Float4x4:
			{
				auto& f4x4 = defaultParameters.GetFloat4x4Parameters()[paramIndex.second];
				stream << "Float4x4[" << paramIndex.second << "], default diag: " << f4x4(0,0) << ", " << f4x4(1,1) << ", " << f4x4(2,2) << ", " << f4x4(3,3);
			}
			break;
			default:
				stream << "unknown type";
			}
			stream << std::endl;
		}

		stream << " --- Output interface:" << std::endl;
		for (auto i=interf._jointTags.begin(); i!=interf._jointTags.end(); ++i)
			stream << "  [" << std::distance(interf._jointTags.begin(), i) << "] " << i->first << " : " << i->second << ", Output transform index: (" << std::distance(interf._jointTags.begin(), i) << ")" << std::endl;

		stream << " --- Command stream:" << std::endl;
		const auto& cmds = transMachine._commandStream;
		TraceTransformationMachine(
			stream, MakeIteratorRange(cmds),
			[&interf](unsigned outputMatrixIndex) -> std::string
			{
				if (outputMatrixIndex < interf._jointTags.size())
					return interf._jointTags[outputMatrixIndex].first + " : " + interf._jointTags[outputMatrixIndex].second;
				return std::string();
			},
			[&interf](AnimSamplerType samplerType, unsigned parameterIndex) -> std::string
			{
				using T = std::vector<AnimationParameterHashName>;
				std::pair<const T*, AnimSamplerType> tables[] = {
					std::make_pair(&interf._float1ParameterNames,		AnimSamplerType::Float1),
					std::make_pair(&interf._float3ParameterNames,		AnimSamplerType::Float3),
					std::make_pair(&interf._float4ParameterNames,		AnimSamplerType::Float4),
					std::make_pair(&interf._float4x4ParameterNames,		AnimSamplerType::Float4x4),
					std::make_pair(&interf._float4ParameterNames,		AnimSamplerType::Quaternion)
				};

				auto& names = *tables[unsigned(samplerType)].first;
				if (parameterIndex < names.size()) return interf.HashedIdToStringId(names[parameterIndex]);
				else return std::string();
			});

		return stream;
	}

    auto NascentSkeletonInterface::GetParameterIndex(AnimationParameterHashName parameterName) const -> std::pair<AnimSamplerType, uint32>
    {
        {
            auto i = std::find(_float1ParameterNames.begin(), _float1ParameterNames.end(), parameterName);
            if (i!=_float1ParameterNames.end()) {
                return std::make_pair(AnimSamplerType::Float1, (uint32)std::distance(_float1ParameterNames.begin(), i));
            }
        }
        {
            auto i = std::find(_float3ParameterNames.begin(), _float3ParameterNames.end(), parameterName);
            if (i!=_float3ParameterNames.end()) {
                return std::make_pair(AnimSamplerType::Float3, (uint32)std::distance(_float3ParameterNames.begin(), i));
            }
        }
        {
            auto i = std::find(_float4ParameterNames.begin(), _float4ParameterNames.end(), parameterName);
            if (i!=_float4ParameterNames.end()) {
                return std::make_pair(AnimSamplerType::Float4, (uint32)std::distance(_float4ParameterNames.begin(), i));
            }
        }
        {
            auto i = std::find(_float4x4ParameterNames.begin(), _float4x4ParameterNames.end(), parameterName);
            if (i!=_float4x4ParameterNames.end()) {
                return std::make_pair(AnimSamplerType::Float4x4, (uint32)std::distance(_float4x4ParameterNames.begin(), i));
            }
        }

        return std::make_pair(AnimSamplerType::Float1, ~uint32(0x0));
    }

    auto   NascentSkeletonInterface::GetParameterName(AnimSamplerType type, uint32 index) const -> AnimationParameterHashName
    {
        typedef std::pair<AnimationParameterHashName, uint32> P;
        if (type == AnimSamplerType::Float4x4 && index < _float4x4ParameterNames.size()) return _float4x4ParameterNames[index];
        else if (type == AnimSamplerType::Float4 && index < _float4ParameterNames.size()) return _float4ParameterNames[index];
        else if (type == AnimSamplerType::Float3 && index < _float3ParameterNames.size()) return _float3ParameterNames[index];
        else if (type == AnimSamplerType::Float1 && index < _float1ParameterNames.size()) return _float1ParameterNames[index];
        return ~AnimationParameterHashName(0x0);
    }

    std::string NascentSkeletonInterface::HashedIdToStringId     (AnimationParameterHashName colladaId) const
    {
		auto i = LowerBound(_dehashTable, colladaId);
		if (i != _dehashTable.end() && i->first == colladaId)
			return i->second;
        return std::string();
    }

    auto       NascentSkeletonInterface::StringIdToHashedId     (const std::string& stringId) const -> AnimationParameterHashName
    {
        for (auto i=_dehashTable.cbegin(); i!=_dehashTable.end(); ++i)
            if (i->second == stringId)
                return i->first;
        return ~AnimationParameterHashName(0x0);
    }

    bool    NascentSkeletonInterface::TryRegisterJointName(uint32& outputMarker, StringSection<> skeletonName, StringSection<> jointName)
    {
		outputMarker = (uint32)_jointTags.size();
		_jointTags.push_back({skeletonName.AsString(), jointName.AsString()});	// (note -- not checking for duplicates)
        return true;
    }

}}}


