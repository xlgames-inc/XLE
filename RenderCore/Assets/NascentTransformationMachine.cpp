// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentTransformationMachine.h"
#include "../RenderUtils.h"
#include "../../Assets/Assets.h"
#include "../../Assets/BlockSerializer.h"
#include "../../ConsoleRig/OutputStream.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/Streams/Serialization.h"

namespace RenderCore { namespace Assets
{

    class NascentTransformationMachine::Joint
    {
    public:
        std::string     _name;
        uint32          _outputMatrixIndex;
        Float4x4        _inverseBindMatrix;

        class CompareColladaId
        {
        public:
            bool operator()(const Joint& lhs, const Joint& rhs) { return lhs._name < rhs._name; }
            bool operator()(const Joint& lhs, const std::string& rhs) { return lhs._name < rhs; }
            bool operator()(const std::string& lhs, const Joint& rhs) { return lhs < rhs._name; }
        };
    };

    static const char* AsString(Assets::TransformationParameterSet::Type::Enum value)
    {
        switch (value) {
        case Assets::TransformationParameterSet::Type::Float1: return "Float1";
        case Assets::TransformationParameterSet::Type::Float3: return "Float3";
        case Assets::TransformationParameterSet::Type::Float4: return "Float4";
        case Assets::TransformationParameterSet::Type::Float4x4: return "Float4x4";
        }
        return "<<unknown>>";
    }

    template <>
        void    NascentTransformationMachine::Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        outputSerializer.SerializeSubBlock(AsPointer(_commandStream.begin()), AsPointer(_commandStream.end()));
        outputSerializer.SerializeValue(_commandStream.size());
        outputSerializer.SerializeValue(_outputMatrixCount);
        _defaultParameters.Serialize(outputSerializer);

            //
            //      We have to write out both an input and an output interface
            //      First input interface...
            //
        #pragma pack(push)
        #pragma pack(1)
            struct Param    /* must match TransformationMachine::InputInterface::Parameter */
            {
                uint64      _name;
                uint32      _index;
                Assets::TransformationParameterSet::Type::Enum  _type;

                void    Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
                {
                    ::Serialize(outputSerializer, _name);
                    ::Serialize(outputSerializer, _index);
                    ::Serialize(outputSerializer, unsigned(_type));
                }
            };
        #pragma pack(pop)

        std::vector<Param> runTimeInputInterface;
        typedef std::vector<std::pair<AnimationParameterId, uint32>> T;
        std::pair<const T*, Assets::TransformationParameterSet::Type::Enum> tables[] = {
            std::make_pair(&_float1ParameterNames,      Assets::TransformationParameterSet::Type::Float1),
            std::make_pair(&_float3ParameterNames,      Assets::TransformationParameterSet::Type::Float3),
            std::make_pair(&_float4ParameterNames,      Assets::TransformationParameterSet::Type::Float4),
            std::make_pair(&_float4x4ParameterNames,    Assets::TransformationParameterSet::Type::Float4x4)
        };

        ConsoleRig::DebuggerOnlyWarning("Transformation Machine input interface:\n");
        for (unsigned t=0; t<dimof(tables); ++t) {
            for (auto i=tables[t].first->begin(); i!=tables[t].first->end(); ++i) {
                std::string n = HashedIdToStringId(i->first);
                if (!n.empty()) {
                    Param p;
                    p._type     = tables[t].second;
                    p._index    = i->second;
                    p._name     = Hash64(AsPointer(n.begin()), AsPointer(n.end()));
                    runTimeInputInterface.push_back(p);

                    ConsoleRig::DebuggerOnlyWarning("  (%s, %i) -- %s\n", AsString(p._type), p._index, n.c_str());
                }
            }
        }
        outputSerializer.SerializeSubBlock(AsPointer(runTimeInputInterface.begin()), AsPointer(runTimeInputInterface.end()));
        outputSerializer.SerializeValue(runTimeInputInterface.size());

            //
            //      Now, output interface...
            //
        std::vector<uint64> jointHashNames;
        std::vector<Float4x4> jointInverseBindMatrices;
        std::tie(jointHashNames, jointInverseBindMatrices) = GetOutputInterface();
        outputSerializer.SerializeSubBlock(AsPointer(jointHashNames.cbegin()), AsPointer(jointHashNames.cend()));
        outputSerializer.SerializeSubBlock(AsPointer(jointInverseBindMatrices.cbegin()), AsPointer(jointInverseBindMatrices.cend()));
        outputSerializer.SerializeValue(size_t(_outputMatrixCount));
    }

    std::pair<std::vector<uint64>, std::vector<Float4x4>> NascentTransformationMachine::GetOutputInterface() const
    {
        ConsoleRig::DebuggerOnlyWarning("Transformation Machine output interface:\n");
        std::vector<uint64> jointHashNames(_outputMatrixCount, 0ull);
        std::vector<Float4x4> jointInverseBindMatrices(_outputMatrixCount, Identity<Float4x4>());

        for (auto i=_jointTags.begin(); i!=_jointTags.end(); ++i) {
            if (i->_outputMatrixIndex < _outputMatrixCount) {
                ConsoleRig::DebuggerOnlyWarning("  [%i] %s\n", std::distance(_jointTags.begin(), i), i->_name.c_str());

                assert(jointHashNames[i->_outputMatrixIndex] == 0ull);
                jointHashNames[i->_outputMatrixIndex] = 
                    Hash64(AsPointer(i->_name.begin()), AsPointer(i->_name.end()));
                jointInverseBindMatrices[i->_outputMatrixIndex] = i->_inverseBindMatrix;
            }
        }

        return std::make_pair(std::move(jointHashNames), std::move(jointInverseBindMatrices));
    }

    std::ostream& StreamOperator(std::ostream& stream, const NascentTransformationMachine& transMachine)
    {
        stream << "Output matrices: " << transMachine._outputMatrixCount << std::endl;
        stream << "Command stream size: " << transMachine._commandStream.size() * sizeof(uint32) << std::endl;

        stream << " --- Animation parameters (" << transMachine._stringNameMapping.size() << ") :" << std::endl;
        for (const auto& p:transMachine._stringNameMapping) {
            stream << "[" << p.first << "] (0x" << std::hex << p.second << std::dec << ") ";
            auto paramIndex = transMachine.GetParameterIndex(p.second);
            switch (paramIndex.first) {
            case AnimSamplerType::Float1:
                stream << "Float1[" << paramIndex.second << "], default: " << transMachine.GetDefaultParameters().GetFloat1Parameters()[paramIndex.second];
                break;
            case AnimSamplerType::Float3:
                {
                    auto& f3 = transMachine.GetDefaultParameters().GetFloat3Parameters()[paramIndex.second];
                    stream << "Float3[" << paramIndex.second << "], default: " << f3[0] << ", " << f3[1] << ", " << f3[2];
                }
                break;
            case AnimSamplerType::Float4:
                {
                    auto& f4 = transMachine.GetDefaultParameters().GetFloat4Parameters()[paramIndex.second];
                    stream << "Float4[" << paramIndex.second << "], default: " << f4[0] << ", " << f4[1] << ", " << f4[2] << ", " << f4[3];
                }
                break;
            case AnimSamplerType::Float4x4:
                {
                    auto& f4x4 = transMachine.GetDefaultParameters().GetFloat4x4Parameters()[paramIndex.second];
                    stream << "Float4x4[" << paramIndex.second << "], default diag: " << f4x4(0,0) << ", " << f4x4(1,1) << ", " << f4x4(2,2) << ", " << f4x4(3,3);
                }
                break;
            default:
                stream << "unknown type";
            }
            stream << std::endl;
        }

        stream << " --- Output interface:" << std::endl;
        for (auto i=transMachine._jointTags.begin(); i!=transMachine._jointTags.end(); ++i)
            if (i->_outputMatrixIndex < transMachine._outputMatrixCount)
                stream << "  [" << std::distance(transMachine._jointTags.begin(), i) << "] " << i->_name << ", Output transform index: (" << i->_outputMatrixIndex << ")" << std::endl;

        stream << " --- Command stream:" << std::endl;
        auto cmds = transMachine._commandStream;
        TraceTransformationMachine(
            stream, AsPointer(cmds.begin()), AsPointer(cmds.end()),
            [&transMachine](unsigned outputMatrixIndex) -> std::string
            {
                auto i = std::find_if(
                    transMachine._jointTags.cbegin(), transMachine._jointTags.cend(),
                    [outputMatrixIndex](const NascentTransformationMachine::Joint& j) { return j._outputMatrixIndex == outputMatrixIndex; });
                if (i != transMachine._jointTags.cend())
                    return i->_name;
                return std::string();
            },
            [&transMachine](AnimSamplerType samplerType, unsigned parameterIndex) -> std::string
            {
                using T = std::vector<std::pair<AnimationParameterId, uint32>>;
                std::pair<const T*, Assets::TransformationParameterSet::Type::Enum> tables[] = {
                    std::make_pair(&transMachine._float1ParameterNames,      Assets::TransformationParameterSet::Type::Float1),
                    std::make_pair(&transMachine._float3ParameterNames,      Assets::TransformationParameterSet::Type::Float3),
                    std::make_pair(&transMachine._float4ParameterNames,      Assets::TransformationParameterSet::Type::Float4),
                    std::make_pair(&transMachine._float4x4ParameterNames,    Assets::TransformationParameterSet::Type::Float4x4)
                };
                
                auto& names = *tables[unsigned(samplerType)].first;
                auto i = std::find_if(names.cbegin(), names.cend(), 
                    [parameterIndex](const std::pair<AnimationParameterId, uint32>& param) { return param.second == parameterIndex; });
                if (i == names.cend()) return std::string();
                return transMachine.HashedIdToStringId(i->first);
            });

        return stream;
    }

    NascentTransformationMachine::NascentTransformationMachine()
    : _pendingPops(0), _outputMatrixCount(0), _defaultParameters()
    , _lastReturnedOutputMatrixMarker(~unsigned(0)) {}

    NascentTransformationMachine::~NascentTransformationMachine()
    {}

    NascentTransformationMachine::NascentTransformationMachine(NascentTransformationMachine&& moveFrom)
    :       _commandStream(             std::move(moveFrom._commandStream))
    ,       _float1ParameterNames(      std::move(moveFrom._float1ParameterNames))
    ,       _float3ParameterNames(      std::move(moveFrom._float3ParameterNames))
    ,       _float4ParameterNames(      std::move(moveFrom._float4ParameterNames))
    ,       _float4x4ParameterNames(    std::move(moveFrom._float4x4ParameterNames))
    ,       _defaultParameters(         std::move(moveFrom._defaultParameters))
    ,       _outputMatrixCount(         moveFrom._outputMatrixCount)
    ,       _pendingPops(               moveFrom._pendingPops)
    ,       _stringNameMapping(         std::move(moveFrom._stringNameMapping))
    ,       _jointTags(                 std::move(moveFrom._jointTags))
    ,       _lastReturnedOutputMatrixMarker(moveFrom._lastReturnedOutputMatrixMarker)
    {
    }

    NascentTransformationMachine& NascentTransformationMachine::operator=(NascentTransformationMachine&& moveFrom) never_throws
    {
        _commandStream               = std::move(moveFrom._commandStream);
        _float1ParameterNames        = std::move(moveFrom._float1ParameterNames);
        _float3ParameterNames        = std::move(moveFrom._float3ParameterNames);
        _float4ParameterNames        = std::move(moveFrom._float4ParameterNames);
        _float4x4ParameterNames      = std::move(moveFrom._float4x4ParameterNames);
        _defaultParameters           = std::move(moveFrom._defaultParameters);
        _outputMatrixCount           = moveFrom._outputMatrixCount;
        _pendingPops                 = moveFrom._pendingPops;
        _stringNameMapping           = std::move(moveFrom._stringNameMapping);
        _jointTags                   = std::move(moveFrom._jointTags);
        _lastReturnedOutputMatrixMarker = moveFrom._lastReturnedOutputMatrixMarker;
        return *this;
    }

    template<> auto NascentTransformationMachine::GetTables<float>() -> std::pair<std::vector<std::pair<AnimationParameterId, uint32>>&,SerializableVector<float>&>
    {
        return std::make_pair(std::ref(_float1ParameterNames), std::ref(_defaultParameters.GetFloat1ParametersVector()));
    }

    template<> auto NascentTransformationMachine::GetTables<Float3>() -> std::pair<std::vector<std::pair<AnimationParameterId, uint32>>&,SerializableVector<Float3>&>
    {
        return std::make_pair(std::ref(_float3ParameterNames), std::ref(_defaultParameters.GetFloat3ParametersVector()));
    }

    template<> auto NascentTransformationMachine::GetTables<Float4>() -> std::pair<std::vector<std::pair<AnimationParameterId, uint32>>&,SerializableVector<Float4>&>
    {
        return std::make_pair(std::ref(_float4ParameterNames), std::ref(_defaultParameters.GetFloat4ParametersVector()));
    }

    template<> auto NascentTransformationMachine::GetTables<Float4x4>() -> std::pair<std::vector<std::pair<AnimationParameterId, uint32>>&,SerializableVector<Float4x4>&>
    {
        return std::make_pair(std::ref(_float4x4ParameterNames), std::ref(_defaultParameters.GetFloat4x4ParametersVector())); //(note, ref is needed to make sure we get the right type of pair)
    }

    void            NascentTransformationMachine::Pop(unsigned popCount)
    {
        _pendingPops += popCount;
    }

    unsigned        NascentTransformationMachine::GetOutputMatrixMarker()
    {
        if (_lastReturnedOutputMatrixMarker != ~unsigned(0))
            return _lastReturnedOutputMatrixMarker;

        unsigned result = _outputMatrixCount++;
        _commandStream.push_back((uint32)Assets::TransformStackCommand::WriteOutputMatrix);
        _commandStream.push_back(result);
        _lastReturnedOutputMatrixMarker = result;
        return result;
    }

    void            NascentTransformationMachine::MakeOutputMatrixMarker(unsigned marker)
    {
        _commandStream.push_back((uint32)Assets::TransformStackCommand::WriteOutputMatrix);
        _commandStream.push_back(marker);
        _lastReturnedOutputMatrixMarker = marker;
        _outputMatrixCount = std::max(_outputMatrixCount, marker+1);
    }

    auto NascentTransformationMachine::GetParameterIndex(AnimationParameterId parameterName) const -> std::pair<Assets::TransformationParameterSet::Type::Enum, uint32>
    {
        {
            auto i = LowerBound(_float1ParameterNames, parameterName);
            if (i!=_float1ParameterNames.end() && i->first == parameterName) {
                return std::make_pair(Assets::TransformationParameterSet::Type::Float1, i->second);
            }
        }
        {
            auto i = LowerBound(_float3ParameterNames, parameterName);
            if (i!=_float3ParameterNames.end() && i->first == parameterName) {
                return std::make_pair(Assets::TransformationParameterSet::Type::Float3, i->second);
            }
        }
        {
            auto i = LowerBound(_float4ParameterNames, parameterName);
            if (i!=_float4ParameterNames.end() && i->first == parameterName) {
                return std::make_pair(Assets::TransformationParameterSet::Type::Float4, i->second);
            }
        }
        {
            auto i = LowerBound(_float4x4ParameterNames, parameterName);
            if (i!=_float4x4ParameterNames.end() && i->first == parameterName) {
                return std::make_pair(Assets::TransformationParameterSet::Type::Float4x4, i->second);
            }
        }

        return std::make_pair(Assets::TransformationParameterSet::Type::Float1, ~uint32(0x0));
    }

    auto   NascentTransformationMachine::GetParameterName(AnimSamplerType type, uint32 index) const -> AnimationParameterId
    {
        typedef std::pair<AnimationParameterId, uint32> P;
        if (type == Assets::TransformationParameterSet::Type::Float4x4) {
            auto i = std::find_if(_float4x4ParameterNames.begin(), _float4x4ParameterNames.end(), [=](const P&p) { return p.second == index; });
            if (i!=_float4x4ParameterNames.end()) {
                return i->first;
            }
        }
        if (type == Assets::TransformationParameterSet::Type::Float4) {
            auto i = std::find_if(_float4ParameterNames.begin(), _float4ParameterNames.end(), [=](const P&p) { return p.second == index; });
            if (i!=_float4ParameterNames.end()) {
                return i->first;
            }
        }
        if (type == Assets::TransformationParameterSet::Type::Float3) {
            auto i = std::find_if(_float3ParameterNames.begin(), _float3ParameterNames.end(), [=](const P&p) { return p.second == index; });
            if (i!=_float3ParameterNames.end()) {
                return i->first;
            }
        }
        if (type == Assets::TransformationParameterSet::Type::Float1) {
            auto i = std::find_if(_float1ParameterNames.begin(), _float1ParameterNames.end(), [=](const P&p) { return p.second == index; });
            if (i!=_float1ParameterNames.end()) {
                return i->first;
            }
        }
        return ~AnimationParameterId(0x0);
    }

    std::string                 NascentTransformationMachine::HashedIdToStringId     (AnimationParameterId colladaId) const
    {
        for (auto i=_stringNameMapping.cbegin(); i!=_stringNameMapping.end(); ++i)
            if (i->second == colladaId)
                return i->first;
        return std::string();
    }

    auto       NascentTransformationMachine::StringIdToHashedId     (const std::string& stringId) const -> AnimationParameterId
    {
        for (auto i=_stringNameMapping.cbegin(); i!=_stringNameMapping.end(); ++i)
            if (i->first == stringId)
                return i->second;
        return ~AnimationParameterId(0x0);
    }

    void        NascentTransformationMachine::RegisterJointName(
        const std::string& name, const Float4x4& inverseBindMatrix, unsigned outputMatrixIndex)
    {
        auto insertionPoint = std::lower_bound(
            _jointTags.begin(), _jointTags.end(), 
            name, Joint::CompareColladaId());
        if (insertionPoint != _jointTags.end() && insertionPoint->_name==name) {
            Throw(
                ::Assets::Exceptions::FormatError(
                    "Duplicate when inserting joint tag in node (%s)", name.c_str()));
        }

        _jointTags.insert(insertionPoint, Joint{name, outputMatrixIndex, inverseBindMatrix});
    }

    bool    NascentTransformationMachine::TryRegisterJointName(
        const std::string& name, const Float4x4& inverseBindMatrix, unsigned outputMatrixIndex)
    {
        auto insertionPoint = std::lower_bound(
            _jointTags.begin(), _jointTags.end(), 
            name, Joint::CompareColladaId());
        if (insertionPoint != _jointTags.end() && insertionPoint->_name==name)
            return false;

        _jointTags.insert(insertionPoint, Joint{name, outputMatrixIndex, inverseBindMatrix});
        return true;
    }

//     template <typename Iterator>
//         inline Iterator AdvanceTo16ByteAlignment(Iterator input)
//         {
//             input += ((16 - (AsPointer(input) & 0xf)) & 0xf) / sizeof(typename Iterator::value_type); // advance to 16 bit alignment
//             assert((AsPointer(input) & 0xf)==0);
//             return input;
//         }

    std::unique_ptr<Float4x4[]>           NascentTransformationMachine::GenerateOutputTransforms(
        const Assets::TransformationParameterSet&   parameterSet) const
    {
        std::unique_ptr<Float4x4[]> result = std::make_unique<Float4x4[]>(size_t(_outputMatrixCount));
        GenerateOutputTransformsFree(
            result.get(), _outputMatrixCount,
            &parameterSet, 
            MakeIteratorRange(_commandStream));
        return result;
    }

    static unsigned int FloatBits(float input)
    {
            // (or just use a reinterpret cast)
        union Converter { float f; unsigned int i; };
        Converter c; c.f = input; 
        return c.i;
    }

    unsigned    NascentTransformationMachine::PushTransformation(const Float4x4& localToParent)
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

        _lastReturnedOutputMatrixMarker = ~unsigned(0);
        return 1;
    }

    void NascentTransformationMachine::PushCommand(uint32 cmd)
    {
        _commandStream.push_back(cmd);
        _lastReturnedOutputMatrixMarker = ~unsigned(0);
    }

    void NascentTransformationMachine::PushCommand(TransformStackCommand cmd)
    {
        _commandStream.push_back((uint32)cmd);
        _lastReturnedOutputMatrixMarker = ~unsigned(0);
    }

    void NascentTransformationMachine::PushCommand(const void* ptr, size_t size)
    {
        assert((size % sizeof(uint32)) == 0);
        _commandStream.insert(_commandStream.end(), (const uint32*)ptr, (const uint32*)PtrAdd(ptr, size));
        _lastReturnedOutputMatrixMarker = ~unsigned(0);
    }

    void NascentTransformationMachine::ResolvePendingPops()
    {
        if (_pendingPops) {
            _commandStream.push_back((uint32)Assets::TransformStackCommand::PopLocalToWorld);
            _commandStream.push_back(_pendingPops);
            _pendingPops = 0;
            _lastReturnedOutputMatrixMarker = ~unsigned(0);
        }
    }

    void NascentTransformationMachine::Optimize()
    {
        ResolvePendingPops();
        auto optimized = OptimizeTransformationMachine(MakeIteratorRange(_commandStream));
        _commandStream = std::move(optimized);
    }

}}


