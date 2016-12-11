// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderReflection.h"
#include "../../../Utility/MemoryUtils.h"

// Vulkan SDK includes -- 
#pragma push_macro("new")
#undef new
#include <glslang/SPIRV/spirv.hpp>
#include <glslang/SPIRV/doc.h>

// #include <glslang/SPIRV/disassemble.h>
// #include <sstream>
#pragma pop_macro("new")

namespace RenderCore { namespace Metal_Vulkan
{

    
    template<typename Id>
        void FillInBinding(
            std::vector<std::pair<Id, SPIRVReflection::Binding>>& bindings,
            Id id,
            spv::Decoration decorationType,
            const unsigned* params, unsigned paramCount)
        {
            if (    decorationType == spv::DecorationBinding
                ||  decorationType == spv::DecorationDescriptorSet
                ||  decorationType == spv::DecorationLocation
                ||  decorationType == spv::DecorationOffset) {

                if (paramCount < 1) return;

                auto i = LowerBound(bindings, id);
                if (i == bindings.end() || i->first != id)
                    i = bindings.insert(i, std::make_pair(id, SPIRVReflection::Binding()));

                switch (decorationType) {
                case spv::DecorationBinding: i->second._bindingPoint = params[0]; break;
                case spv::DecorationDescriptorSet: i->second._descriptorSet = params[0]; break;
                case spv::DecorationLocation: i->second._location = params[0]; break;
                case spv::DecorationOffset: i->second._offset = params[0]; break;
                }
            }
        }

    static SPIRVReflection::StorageType AsStorageType(unsigned type)
    {
        switch (type)
        {
        case 0:  return SPIRVReflection::StorageType::UniformConstant;
        case 1:  return SPIRVReflection::StorageType::Input;
        case 2:  return SPIRVReflection::StorageType::Uniform;
        case 3:  return SPIRVReflection::StorageType::Output;
        case 4:  return SPIRVReflection::StorageType::Workgroup;
        case 5:  return SPIRVReflection::StorageType::CrossWorkgroup;
        case 6:  return SPIRVReflection::StorageType::Private;
        case 7:  return SPIRVReflection::StorageType::Function;
        case 8:  return SPIRVReflection::StorageType::Generic;
        case 9:  return SPIRVReflection::StorageType::PushConstant;
        case 10: return SPIRVReflection::StorageType::AtomicCounter;
        case 11: return SPIRVReflection::StorageType::Image;
        default: return SPIRVReflection::StorageType::Unknown;
        }
    }

    SPIRVReflection::SPIRVReflection(IteratorRange<const uint32*> byteCode)
    {
        _entryPoint._id = ~0x0u;

        using namespace spv;
        // spv::Parameterize();

        auto* i = byteCode.begin() + 5;
        while (i < byteCode.end()) {
            // Instruction wordCount and opcode
            unsigned int firstWord = *i;
            unsigned wordCount = firstWord >> WordCountShift;
            Op opCode = (Op)(firstWord & OpCodeMask);
            auto paramStart = i+1;
            i += wordCount;

            switch (opCode) {
            case OpMemberName:
                // InstructionDesc[OpMemberName].operands.push(OperandId, "'Type'");
                // InstructionDesc[OpMemberName].operands.push(OperandLiteralNumber, "'Member'");
                // InstructionDesc[OpMemberName].operands.push(OperandLiteralString, "'Name'");
                {
                    if (((const char*)&paramStart[2])[0]) {
                        MemberId id(paramStart[0], paramStart[1]);
                        auto i = LowerBound(_memberNames, id);
                        if (i == _memberNames.end() || i->first != id)
                            i = _memberNames.insert(i, std::make_pair(id, Name()));
                        i->second = MakeStringSection((const char*)&paramStart[2]);
                    }
                    break;
                }

            case OpName:
                // InstructionDesc[OpName].operands.push(OperandId, "'Target'");
                // InstructionDesc[OpName].operands.push(OperandLiteralString, "'Name'");
                {
                    auto type = paramStart[0];
                    auto i = LowerBound(_names, type);
                    if (i == _names.end() || i->first != type)
                        i = _names.insert(i, std::make_pair(type, Name()));
                    i->second = MakeStringSection((const char*)&paramStart[1]);
                    break;
                }

            case OpDecorate:
                // InstructionDesc[OpDecorate].operands.push(OperandId, "'Target'");
                // InstructionDesc[OpDecorate].operands.push(OperandDecoration, "");
                // InstructionDesc[OpDecorate].operands.push(OperandVariableLiterals, "See <<Decoration,'Decoration'>>.");
                {
                    auto targetId = (Id)paramStart[0];
                    auto decorationType = (spv::Decoration)paramStart[1];
                    FillInBinding(_bindings, targetId, decorationType, &paramStart[2], wordCount-3);
                    break;
                }

            case OpMemberDecorate:
                // InstructionDesc[OpMemberDecorate].operands.push(OperandId, "'Structure Type'");
                // InstructionDesc[OpMemberDecorate].operands.push(OperandLiteralNumber, "'Member'");
                // InstructionDesc[OpMemberDecorate].operands.push(OperandDecoration, "");
                // InstructionDesc[OpMemberDecorate].operands.push(OperandVariableLiterals, "See <<Decoration,'Decoration'>>.");
                {
                    MemberId id(paramStart[0], paramStart[1]);
                    auto decorationType = (spv::Decoration)paramStart[2];
                    FillInBinding(_memberBindings, id, decorationType, &paramStart[3], wordCount-4);
                    break;
                }

            case OpEntryPoint:
                // InstructionDesc[OpEntryPoint].operands.push(OperandExecutionModel, "");
                // InstructionDesc[OpEntryPoint].operands.push(OperandId, "'Entry Point'");
                // InstructionDesc[OpEntryPoint].operands.push(OperandLiteralString, "'Name'");
                // InstructionDesc[OpEntryPoint].operands.push(OperandVariableIds, "'Interface'");
                {
                    assert(_entryPoint._name.IsEmpty() && _entryPoint._interface.empty());
                    auto executionModel = paramStart[0]; (void)executionModel;
                    _entryPoint._id = paramStart[1];
                    _entryPoint._name = (const char*)&paramStart[2];

                    auto nameEnd = XlStringEnd((const char*)&paramStart[2]);
                    auto len = nameEnd+1-(const char*)&paramStart[2];
                    if (len%4 != 0) len += 4-(len%4);
                    auto* interfaceStart = PtrAdd(&paramStart[2], len);
                    auto interfaceEnd = &paramStart[wordCount-1];
                    _entryPoint._interface = std::vector<ObjectId>(interfaceStart, interfaceEnd);
                    break;
                }

            case OpTypeBool:
                _basicTypes.push_back(std::make_pair(paramStart[0], BasicType::Bool));
                break;

            case OpTypeFloat:  
                _basicTypes.push_back(std::make_pair(paramStart[0], BasicType::Float));
                break;

            case OpTypeInt:  
                _basicTypes.push_back(std::make_pair(paramStart[0], BasicType::Int));
                break;

            case OpTypeVector:  
                _vectorTypes.push_back(std::make_pair(paramStart[0], VectorType{paramStart[1], paramStart[2]}));
                break;

            case OpTypeStruct:
                _structTypes.push_back(paramStart[0]);
                break;

            case OpTypePointer:  
                _pointerTypes.push_back(std::make_pair(paramStart[0], PointerType{paramStart[2], AsStorageType(paramStart[1])}));
                break;

            case OpVariable:
                if (wordCount > 3)
                    _variables.push_back(std::make_pair(paramStart[1], Variable{paramStart[0], AsStorageType(paramStart[2])}));
                break;
            }
        }

        // Our tables should be in near-sorted order, but are not guaranteed to be sorted.
        // So we have to sort here. Since they are near-sorted, quick sort is not ideal, but
        // 
        std::sort(_basicTypes.begin(), _basicTypes.end(), CompareFirst<uint64, BasicType>());
        std::sort(_vectorTypes.begin(), _vectorTypes.end(), CompareFirst<uint64, VectorType>());
        std::sort(_pointerTypes.begin(), _pointerTypes.end(), CompareFirst<uint64, PointerType>());
        std::sort(_variables.begin(), _variables.end(), CompareFirst<uint64, Variable>());

        // build the quick lookup table, which matches hash names to binding values
        for (auto& b:_bindings) {
            const auto& binding = b.second;
            auto bindingName = b.first;
            if (binding._descriptorSet==~0x0u && binding._bindingPoint==~0x0u) continue;

            // We can bind to the name of the variable, or the name of the type. This is
            // important for our HLSL path for constant buffers.
            // In that case, we get a dummy name for the variable, and the important name
            // is actually the name of the type.
            // Constant buffers become a pointer to a struct (where the struct has the name we want),
            // and the actual variable just has an empty name.
            auto n = LowerBound(_names, bindingName);
            if (n != _names.end() && n->first == bindingName) {
                auto nameStart = n->second.begin();
                auto nameEnd = n->second.end();
                if (nameStart < nameEnd)
                    _uniformQuickLookup.push_back(std::make_pair(Hash64(nameStart, nameEnd), binding));
            }

            // now insert the type name into the quick lookup table ---
            auto v = LowerBound(_variables, bindingName);
            if (v != _variables.end() && v->first == bindingName) {
                auto type = v->second._type;
                auto ptr = LowerBound(_pointerTypes, type);
                if (ptr != _pointerTypes.end() && ptr->first == type)
                    type = ptr->second._targetType;
                
                n = LowerBound(_names, type);
                if (n != _names.end() && n->first == type) {
                    auto nameStart = n->second.begin();
                    auto nameEnd = n->second.end();
                    if (nameStart < nameEnd)
                        _uniformQuickLookup.push_back(std::make_pair(Hash64(nameStart, nameEnd), binding));
                } 
            }
        }

        std::sort(
            _uniformQuickLookup.begin(), _uniformQuickLookup.end(),
            CompareFirst<uint64, Binding>());

        // build the quick lookup table for the input interface
        for (auto i:_entryPoint._interface) {
            auto v = LowerBound(_variables, i);
            if (v == _variables.end() || v->first != i) continue;
            if (v->second._storage != StorageType::Input) continue;

            auto b = LowerBound(_bindings, v->first);
            if (b == _bindings.end() || b->first != v->first) continue;

            const char* nameStart = nullptr, *nameEnd = nullptr;

            auto n = LowerBound(_names, v->first);
            if (n != _names.end() && n->first == v->first) {
                nameStart = n->second.begin();
                nameEnd = n->second.end();
            } 
            
            if (nameStart == nameEnd) {
                // If we have no name attached, we can try to get the name from the type
                // This occurs in our HLSL path for constant buffers. Constant buffers
                // become a pointer to a struct (where the struct has the name we want),
                // and the actual variable just has an empty name.
                auto type = v->second._type;
                auto ptr = LowerBound(_pointerTypes, type);
                if (ptr != _pointerTypes.end() && ptr->first == type)
                    type = ptr->second._targetType;
                
                n = LowerBound(_names, type);
                if (n != _names.end() && n->first == type) {
                    nameStart = n->second.begin();
                    nameEnd = n->second.end();
                } 
            }

            if (nameStart == nameEnd) continue;

            // Our shader path prepends "in_" infront of the semantic name
            // when generating a variable name. Remove it before we make a hash.
            if (XlBeginsWith(n->second, MakeStringSection("in_")))
                nameStart += 3;
            while (nameEnd-1 > nameStart && isdigit(*(nameEnd-1)))
                --nameEnd;

            unsigned index = 0;
            if (nameEnd != n->second.end()) {
                char temp[8];   // have to copy to get the null terminator! Awkward, need a int parser than works on a StringSection
                XlCopyString(temp, MakeStringSection(nameEnd, n->second.end()));
                index = atoi(temp);
            }

            _inputInterfaceQuickLookup.push_back(
                std::make_pair(
                    Hash64(nameStart, nameEnd, DefaultSeed64 + index),
                    InputInterfaceElement{v->second._type, b->second._location}));
        }

        std::sort(
            _inputInterfaceQuickLookup.begin(), _inputInterfaceQuickLookup.end(),
            CompareFirst<uint64, InputInterfaceElement>());

        // std::stringstream disassem;
        // std::vector<unsigned> spirv(byteCode.begin(), byteCode.end());
        // spv::Disassemble(disassem, spirv);
        // auto d = disassem.str();
        // (void)d;
    }

    SPIRVReflection::SPIRVReflection(std::pair<const void*, size_t> byteCode)
    : SPIRVReflection(
        IteratorRange<const uint32*>(
            (const uint32*)byteCode.first, 
            (const uint32*)PtrAdd(byteCode.first, byteCode.second)))
    {}


    SPIRVReflection::SPIRVReflection() {}
    SPIRVReflection::~SPIRVReflection() {}

    SPIRVReflection::SPIRVReflection(const SPIRVReflection& cloneFrom)
    : _names(cloneFrom._names)
    , _memberNames(cloneFrom._memberNames)
    , _bindings(cloneFrom._bindings)
    , _memberBindings(cloneFrom._memberBindings)
    , _uniformQuickLookup(cloneFrom._uniformQuickLookup)
    , _basicTypes(cloneFrom._basicTypes)
    , _vectorTypes(cloneFrom._vectorTypes)
    , _pointerTypes(cloneFrom._pointerTypes)
    , _variables(cloneFrom._variables)
    , _entryPoint(cloneFrom._entryPoint)
    , _inputInterfaceQuickLookup(cloneFrom._inputInterfaceQuickLookup)
    {
    }

    SPIRVReflection& SPIRVReflection::operator=(const SPIRVReflection& cloneFrom)
    {
        _names = cloneFrom._names;
        _memberNames = cloneFrom._memberNames;
        _bindings = cloneFrom._bindings;
        _memberBindings = cloneFrom._memberBindings;
        _uniformQuickLookup = cloneFrom._uniformQuickLookup;
        _basicTypes = cloneFrom._basicTypes;
        _vectorTypes = cloneFrom._vectorTypes;
        _pointerTypes = cloneFrom._pointerTypes;
        _variables = cloneFrom._variables;
        _entryPoint = cloneFrom._entryPoint;
        _inputInterfaceQuickLookup = cloneFrom._inputInterfaceQuickLookup;
        return *this;
    }

    SPIRVReflection::SPIRVReflection(SPIRVReflection&& moveFrom)
    : _names(std::move(moveFrom._names))
    , _memberNames(std::move(moveFrom._memberNames))
    , _bindings(std::move(moveFrom._bindings))
    , _memberBindings(std::move(moveFrom._memberBindings))
    , _uniformQuickLookup(std::move(moveFrom._uniformQuickLookup))
    , _basicTypes(std::move(moveFrom._basicTypes))
    , _vectorTypes(std::move(moveFrom._vectorTypes))
    , _pointerTypes(std::move(moveFrom._pointerTypes))
    , _variables(std::move(moveFrom._variables))
    , _entryPoint(std::move(moveFrom._entryPoint))
    , _inputInterfaceQuickLookup(std::move(moveFrom._inputInterfaceQuickLookup))
    {
    }

    SPIRVReflection& SPIRVReflection::operator=(SPIRVReflection&& moveFrom)
    {
        _names = std::move(moveFrom._names);
        _memberNames = std::move(moveFrom._memberNames);
        _bindings = std::move(moveFrom._bindings);
        _memberBindings = std::move(moveFrom._memberBindings);
        _uniformQuickLookup = std::move(moveFrom._uniformQuickLookup);
        _basicTypes = std::move(moveFrom._basicTypes);
        _vectorTypes = std::move(moveFrom._vectorTypes);
        _pointerTypes = std::move(moveFrom._pointerTypes);
        _variables = std::move(moveFrom._variables);
        _entryPoint = std::move(moveFrom._entryPoint);
        _inputInterfaceQuickLookup = std::move(moveFrom._inputInterfaceQuickLookup);
        return *this;
    }


}}
