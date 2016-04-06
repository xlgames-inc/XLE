// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <vector>
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
    class SPIRVReflection
    {
    public:
        using ObjectId = unsigned;
        using MemberId = std::pair<ObjectId, unsigned>;
        using Name = StringSection<>;
        
        //
        //      Names
        //
        std::vector<std::pair<ObjectId, Name>> _names;
        std::vector<std::pair<MemberId, Name>> _memberNames;

        //
        //      Bindings for uniforms and interface elements
        //
        class Binding
        {
        public:
            unsigned _location;
            unsigned _bindingPoint;
            unsigned _descriptorSet;
            unsigned _offset;

            Binding(unsigned location = ~0x0u, unsigned bindingPoint= ~0x0u, unsigned descriptorSet= ~0x0u, unsigned offset = ~0x0)
            : _location(location), _bindingPoint(bindingPoint), _descriptorSet(descriptorSet), _offset(offset) {}
        };
        std::vector<std::pair<ObjectId, Binding>> _bindings;
        std::vector<std::pair<MemberId, Binding>> _memberBindings;
        std::vector<std::pair<uint64, Binding>> _uniformQuickLookup;

        //
        //      Types
        //
        enum class BasicType { Int, Float, Bool };
        enum class StorageType { UniformConstant, Input, Uniform, Output, Workgroup, CrossWorkgroup, Private, Function, Generic, PushConstant, AtomicCounter, Image, Unknown };
        struct VectorType { ObjectId _componentType; unsigned _componentCount; };
        struct PointerType { ObjectId _targetType; StorageType _storage; };

        std::vector<std::pair<ObjectId, BasicType>>     _basicTypes;
        std::vector<std::pair<ObjectId, VectorType>>    _vectorTypes;
        std::vector<std::pair<ObjectId, PointerType>>   _pointerTypes;

        struct Variable { ObjectId _type; StorageType _storage; };
        std::vector<std::pair<ObjectId, Variable>>      _variables;

        //
        //      Interface (eg, vertex input)
        //
        class EntryPoint
        {
        public:
            ObjectId _id;
            StringSection<> _name;
            std::vector<ObjectId> _interface;
        };
        EntryPoint _entryPoint;

        class InputInterfaceElement
        {
        public:
            ObjectId _type;
            unsigned _location;
        };
        std::vector<std::pair<uint64, InputInterfaceElement>> _inputInterfaceQuickLookup;

        SPIRVReflection(IteratorRange<const uint32*> byteCode);
        SPIRVReflection(std::pair<const void*, size_t> byteCode);
        SPIRVReflection();
        ~SPIRVReflection();

        SPIRVReflection(const SPIRVReflection& cloneFrom);
        SPIRVReflection& operator=(const SPIRVReflection& cloneFrom);
        SPIRVReflection(SPIRVReflection&& moveFrom) never_throws;
        SPIRVReflection& operator=(SPIRVReflection&& moveFrom) never_throws;
    };
}}
