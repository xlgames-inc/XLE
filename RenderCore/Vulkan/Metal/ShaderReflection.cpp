// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderReflection.h"
#include "../../../Utility/MemoryUtils.h"

// Vulkan SDK includes -- 
#include <glslang/SPIRV/spirv.hpp>
#include <glslang/SPIRV/doc.h>

// #include <glslang/SPIRV/disassemble.h>
// #include <sstream>

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

    SPIRVReflection::SPIRVReflection(IteratorRange<const uint32*> byteCode)
    {
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
                    MemberId id(paramStart[0], paramStart[1]);
                    auto i = LowerBound(_memberNames, id);
                    if (i == _memberNames.end() || i->first != id)
                        i = _memberNames.insert(i, std::make_pair(id, Name()));
                    i->second = MakeStringSection((const char*)&paramStart[2]);
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
            }
        }

        // build the quick lookup table, which matches hash names to binding values
        for (auto& b:_bindings) {
            auto n = LowerBound(_names, b.first);
            if (n == _names.end() || n->first != b.first) continue;
            _quickLookup.push_back(std::make_pair(Hash64(n->second.begin(), n->second.end()), b.second));
        }

        std::sort(
            _quickLookup.begin(), _quickLookup.end(),
            CompareFirst<uint64, Binding>());

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
    , _bindings(cloneFrom._bindings)
    , _memberNames(cloneFrom._memberNames)
    , _memberBindings(cloneFrom._memberBindings)
    , _quickLookup(cloneFrom._quickLookup)
    {
    }

    SPIRVReflection& SPIRVReflection::operator=(const SPIRVReflection& cloneFrom)
    {
        _names = cloneFrom._names;
        _bindings = cloneFrom._bindings;
        _memberNames = cloneFrom._memberNames;
        _memberBindings = cloneFrom._memberBindings;
        _quickLookup = cloneFrom._quickLookup;
        return *this;
    }

    SPIRVReflection::SPIRVReflection(SPIRVReflection&& moveFrom)
    : _names(std::move(moveFrom._names))
    , _bindings(std::move(moveFrom._bindings))
    , _memberNames(std::move(moveFrom._memberNames))
    , _memberBindings(std::move(moveFrom._memberBindings))
    , _quickLookup(std::move(moveFrom._quickLookup))
    {
    }

    SPIRVReflection& SPIRVReflection::operator=(SPIRVReflection&& moveFrom)
    {
        _names = std::move(moveFrom._names);
        _bindings = std::move(moveFrom._bindings);
        _memberNames = std::move(moveFrom._memberNames);
        _memberBindings = std::move(moveFrom._memberBindings);
        _quickLookup = std::move(moveFrom._quickLookup);
        return *this;
    }


}}
