// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentTransformationMachine.h"
#include "../RenderUtils.h"
#include "../../Assets/BlockSerializer.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/OutputStream.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Assets
{

    class NascentTransformationMachine::Joint
    {
    public:
        ParameterNameId   _colladaId;
        std::string       _name;
        uint32            _outputMatrixIndex;
        Float4x4          _inverseBindMatrix;

        class CompareColladaId
        {
        public:
            bool operator()(const Joint& lhs, const Joint& rhs) { return lhs._colladaId < rhs._colladaId; }
            bool operator()(const Joint& lhs, ParameterNameId rhs) { return lhs._colladaId < rhs; }
            bool operator()(ParameterNameId lhs, const Joint& rhs) { return lhs < rhs._colladaId; }
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
                    Serialization::Serialize(outputSerializer, _name);
                    Serialization::Serialize(outputSerializer, _index);
                    Serialization::Serialize(outputSerializer, unsigned(_type));
                }
            };
        #pragma pack(pop)

        std::vector<Param> runTimeInputInterface;
        typedef std::vector<std::pair<ParameterNameId, uint32>> T;
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
        ConsoleRig::DebuggerOnlyWarning("Transformation Machine output interface:\n");
        auto jointHashNames             = std::make_unique<uint64[]>(size_t(_outputMatrixCount));
        auto jointInverseBindMatrices   = std::make_unique<Float4x4[]>(size_t(_outputMatrixCount));
        std::fill(jointHashNames.get(), &jointHashNames[_outputMatrixCount], 0ull);
        std::fill(jointInverseBindMatrices.get(), &jointInverseBindMatrices[_outputMatrixCount], Identity<Float4x4>());
        for (auto i=_jointTags.begin(); i!=_jointTags.end(); ++i) {
            if (i->_outputMatrixIndex < _outputMatrixCount) {
                ConsoleRig::DebuggerOnlyWarning("  [%i] %s\n", std::distance(_jointTags.begin(), i), i->_name.c_str());
                assert(jointHashNames[i->_outputMatrixIndex] == 0ull);
                jointHashNames[i->_outputMatrixIndex] = 
                    Hash64(AsPointer(i->_name.begin()), AsPointer(i->_name.end()));
                jointInverseBindMatrices[i->_outputMatrixIndex] = i->_inverseBindMatrix;
            }
        }
        outputSerializer.SerializeSubBlock(jointHashNames.get(), &jointHashNames[_outputMatrixCount]);
        outputSerializer.SerializeSubBlock(jointInverseBindMatrices.get(), &jointInverseBindMatrices[_outputMatrixCount]);
        outputSerializer.SerializeValue(size_t(_outputMatrixCount));
    }

    NascentTransformationMachine::NascentTransformationMachine()
    : _pendingPops(0), _outputMatrixCount(0), _defaultParameters() {}

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
        return *this;
    }

    template<> auto NascentTransformationMachine::GetTables<float>() -> std::pair<std::vector<std::pair<ParameterNameId, uint32>>&,std::vector<float, Serialization::BlockSerializerAllocator<float>>&>
    {
        return std::make_pair(std::ref(_float1ParameterNames), std::ref(_defaultParameters.GetFloat1ParametersVector()));
    }

    template<> auto NascentTransformationMachine::GetTables<Float3>() -> std::pair<std::vector<std::pair<ParameterNameId, uint32>>&,std::vector<Float3, Serialization::BlockSerializerAllocator<Float3>>&>
    {
        return std::make_pair(std::ref(_float3ParameterNames), std::ref(_defaultParameters.GetFloat3ParametersVector()));
    }

    template<> auto NascentTransformationMachine::GetTables<Float4>() -> std::pair<std::vector<std::pair<ParameterNameId, uint32>>&,std::vector<Float4, Serialization::BlockSerializerAllocator<Float4>>&>
    {
        return std::make_pair(std::ref(_float4ParameterNames), std::ref(_defaultParameters.GetFloat4ParametersVector()));
    }

    template<> auto NascentTransformationMachine::GetTables<Float4x4>() -> std::pair<std::vector<std::pair<ParameterNameId, uint32>>&,std::vector<Float4x4, Serialization::BlockSerializerAllocator<Float4x4>>&>
    {
        return std::make_pair(std::ref(_float4x4ParameterNames), std::ref(_defaultParameters.GetFloat4x4ParametersVector())); //(note, ref is needed to make sure we get the right type of pair)
    }

    void            NascentTransformationMachine::Pop(unsigned popCount)
    {
        _pendingPops += popCount;
    }

    unsigned        NascentTransformationMachine::GetOutputMatrixMarker()
    {
        unsigned result = _outputMatrixCount++;
        _commandStream.push_back(Assets::TransformStackCommand::WriteOutputMatrix);
        _commandStream.push_back(result);
        return result;
    }

    auto NascentTransformationMachine::GetParameterIndex(ParameterNameId parameterName) const -> std::pair<Assets::TransformationParameterSet::Type::Enum, uint32>
    {
        {
            auto i = std::lower_bound(
                _float1ParameterNames.begin(), _float1ParameterNames.end(), 
                parameterName, CompareFirst<ParameterNameId, uint32>());
            if (i!=_float1ParameterNames.end() && i->first == parameterName) {
                return std::make_pair(Assets::TransformationParameterSet::Type::Float1, i->second);
            }
        }
        {
            auto i = std::lower_bound(
                _float3ParameterNames.begin(), _float3ParameterNames.end(), 
                parameterName, CompareFirst<ParameterNameId, uint32>());
            if (i!=_float3ParameterNames.end() && i->first == parameterName) {
                return std::make_pair(Assets::TransformationParameterSet::Type::Float3, i->second);
            }
        }
        {
            auto i = std::lower_bound(
                _float4ParameterNames.begin(), _float4ParameterNames.end(), 
                parameterName, CompareFirst<ParameterNameId, uint32>());
            if (i!=_float4ParameterNames.end() && i->first == parameterName) {
                return std::make_pair(Assets::TransformationParameterSet::Type::Float4, i->second);
            }
        }
        {
            auto i = std::lower_bound(
                _float4x4ParameterNames.begin(), _float4x4ParameterNames.end(), 
                parameterName, CompareFirst<ParameterNameId, uint32>());
            if (i!=_float4x4ParameterNames.end() && i->first == parameterName) {
                return std::make_pair(Assets::TransformationParameterSet::Type::Float4x4, i->second);
            }
        }

        return std::make_pair(Assets::TransformationParameterSet::Type::Float1, ~uint32(0x0));
    }

    auto   NascentTransformationMachine::GetParameterName(AnimSamplerType type, uint32 index) const -> ParameterNameId
    {
        typedef std::pair<ParameterNameId, uint32> P;
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
        return ~ParameterNameId(0x0);
    }

    std::string                 NascentTransformationMachine::HashedIdToStringId     (ParameterNameId colladaId) const
    {
        for (auto i=_stringNameMapping.cbegin(); i!=_stringNameMapping.end(); ++i)
            if (i->second == colladaId)
                return i->first;
        return std::string();
    }

    auto       NascentTransformationMachine::StringIdToHashedId     (const std::string& stringId) const -> ParameterNameId
    {
        for (auto i=_stringNameMapping.cbegin(); i!=_stringNameMapping.end(); ++i)
            if (i->first == stringId)
                return i->second;
        return ~ParameterNameId(0x0);
    }

    void                        NascentTransformationMachine::RegisterJointName(
        ParameterNameId colladaId, const std::string& name, 
        const Float4x4& inverseBindMatrix, unsigned outputMatrixIndex)
    {
        auto insertionPoint = std::lower_bound( _jointTags.begin(), _jointTags.end(), 
                                                colladaId, Joint::CompareColladaId());
        if (insertionPoint != _jointTags.end() && insertionPoint->_colladaId==colladaId) {
            ThrowException(::Assets::Exceptions::FormatError(
                "Hash collision when inserting joint tag in node (%s)", name.c_str()));
        }
        Joint newJoint;
        newJoint._colladaId = colladaId;
        newJoint._name = name;
        newJoint._outputMatrixIndex = outputMatrixIndex;
        newJoint._inverseBindMatrix = inverseBindMatrix;
        _jointTags.insert(insertionPoint, newJoint);
    }

//     template <typename Iterator>
//         inline Iterator AdvanceTo16ByteAlignment(Iterator input)
//         {
//             input += ((16 - (AsPointer(input) & 0xf)) & 0xf) / sizeof(typename Iterator::value_type); // advance to 16 bit alignment
//             assert((AsPointer(input) & 0xf)==0);
//             return input;
//         }

    static void NullTransformIterator(const Float4x4& parent, const Float4x4& child, const void* userData) {}
    
    std::unique_ptr<Float4x4[]>           NascentTransformationMachine::GenerateOutputTransforms(
        const Assets::TransformationParameterSet&   parameterSet) const
    {
        std::unique_ptr<Float4x4[]> result = std::make_unique<Float4x4[]>(size_t(_outputMatrixCount));
        GenerateOutputTransformsFree(
            result.get(), _outputMatrixCount,
            &parameterSet, 
            AsPointer(_commandStream.begin()), AsPointer(_commandStream.end()),
            NullTransformIterator, nullptr);
        return result;
    }

    static unsigned int FloatBits(float input)
    {
            // (or just use a reinterpret cast)
        union Converter { float f; unsigned int i; };
        Converter c; c.f = input; 
        return c.i;
    }

    unsigned    NascentTransformationMachine::PushTransformation(const Float4x4& localToParent, const char nodeName[])
    {
        if (_pendingPops) {
            _commandStream.push_back(Assets::TransformStackCommand::PopLocalToWorld);
            _commandStream.push_back(_pendingPops);
            _pendingPops = 0;
        }

            // push a basic, unanimatable transform
            //  see also NascentTransformationMachine_Collada::PushTransformations for a complex
            //  version of this
        _commandStream.push_back(Assets::TransformStackCommand::PushLocalToWorld);
        _commandStream.push_back(Assets::TransformStackCommand::TransformFloat4x4_Static);
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

}}


