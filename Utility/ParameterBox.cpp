// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ParameterBox.h"
#include "MemoryUtils.h"
#include "PtrUtils.h"
#include "StringUtils.h"
#include "IteratorUtils.h"
#include <algorithm>
#include <utility>

namespace Utility
{
    namespace ImpliedTyping
    {
        uint32 TypeDesc::GetSize() const
        {
            switch (_type) {
            case TypeCat::Int:
            case TypeCat::UInt:
            case TypeCat::Float: return sizeof(unsigned)*std::max(1u,unsigned(_arrayCount));
            default: return 0;
            }
        }

        void    TypeDesc::Serialize(Serialization::NascentBlockSerializer& serializer) const
        {
            Serialization::Serialize(serializer, *(uint32*)this);
        }

        bool operator==(const TypeDesc& lhs, const TypeDesc& rhs)
        {
            return lhs._type == rhs._type
                && lhs._typeHint == rhs._typeHint
                && lhs._arrayCount == rhs._arrayCount;
        }


        TypeDesc::TypeDesc()
        {
            _type = TypeCat::UInt;
            _typeHint = TypeHint::None;
            _arrayCount = 1;
        }

        template<typename Type> TypeDesc TypeOf() { return TypeDesc(); }

        TypeDesc TypeOf(const char expression[]) { return TypeDesc(); }
        template <typename Type> Type Parse(const char expression[]) { return Type(0); }

        std::string AsString(const void*, const TypeDesc&) { return std::string(); }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    ParameterBox::ParameterNameHash ParameterBox::MakeParameterNameHash(const std::string& name)
    {
        return Hash32(AsPointer(name.cbegin()), AsPointer(name.cend()));
    }

    ParameterBox::ParameterNameHash    ParameterBox::MakeParameterNameHash(const char name[])
    {
        return Hash32(name, &name[XlStringLen(name)]);
    }

    void ParameterBox::SetParameter(const char name[], const char data[])
    {
        assert(0);
    }

    template<typename Type>
        void ParameterBox::SetParameter(const char name[], Type value)
    {
        const auto insertType = ImpliedTyping::TypeOf<Type>();
        auto size = insertType.GetSize();
        assert(size == sizeof(Type));
        SetParameter(name, &value, insertType);
    }

    void ParameterBox::SetParameter(
        const char name[], const void* value, 
        const ImpliedTyping::TypeDesc& insertType)
    {
        auto hash = MakeParameterNameHash(name);
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), hash);
        if (i==_parameterHashValues.cend()) {
                // push new value onto the end (including name & type info)
            _parameterHashValues.push_back(hash);

            auto valueOffset = _values.size();
            auto nameOffset = _names.size();
            
            _values.insert(_values.end(), (const uint8*)&value, (const uint8*)PtrAdd(&value, sizeof(value)));

            auto nameLength = XlStringLen(name)+1;
            _names.insert(_names.end(), name, &name[nameLength]);

            _offsets.push_back(std::make_pair(valueOffset, nameOffset));
            _types.push_back(insertType);

            _cachedHash = 0;
            _cachedParameterNameHash = 0;
            return;
        }

        const auto valueSize = insertType.GetSize();

        size_t index = std::distance(_parameterHashValues.cbegin(), i);
        if (*i!=hash) {
                // insert new value in the middle somewhere
            _parameterHashValues.insert(i, hash);

            const auto nameLength = XlStringLen(name)+1;
            auto dstOffsets = _offsets[index];

            _offsets.insert(_offsets.begin()+index, dstOffsets);
            for (auto i2=_offsets.begin()+index+1; i2<_offsets.end(); ++i2) {
                i2->first += nameLength;
                i2->second += valueSize;
            }

            _values.insert(
                _values.cbegin()+dstOffsets.second, 
                (uint8*)&value, (uint8*)PtrAdd(&value, valueSize));
            _names.insert(
                _names.cbegin()+dstOffsets.first, 
                name, &name[nameLength]);
            _types.insert(_types.begin() + index, insertType);

            _cachedHash = 0;
            _cachedParameterNameHash = 0;
            return;
        }

            // just update the value
        assert(!XlCompareString(&_names[index], name));
        const auto offset = _offsets[index];
        const auto& existingType = _types[index];

        if (existingType.GetSize() == valueSize) {

                // same type, or type with the same size...
            XlCopyMemory(&_values[offset.first], (uint8*)&value, valueSize);
            _types[index] = insertType;

        } else {

                // if the size of the type changes, we need to adjust the values table a bit
                // hopefully this should be an uncommon case
            auto dstOffsets = _offsets[index];
            signed sizeChange = signed(existingType.GetSize()) - signed(valueSize);

            for (auto i2=_offsets.begin()+index+1; i2<_offsets.end(); ++i2) {
                i2->second += sizeChange;
            }

            _values.erase(
                _values.cbegin()+dstOffsets.second,
                _values.cbegin()+dstOffsets.second+existingType.GetSize());
            _values.insert(
                _values.cbegin()+dstOffsets.second, 
                (uint8*)&value, (uint8*)PtrAdd(&value, valueSize));
            _types[index] = insertType;

        }

        _cachedHash = 0;
    }

    template<typename Type>
        std::pair<bool, Type> ParameterBox::GetParameter(const char name[]) const
    {
        auto hash = MakeParameterNameHash(name);
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), hash);
        if (i!=_parameterHashValues.cend() && *i == hash) {
            size_t index = std::distance(_parameterHashValues.cbegin(), i);
            auto offset = _offsets[index];
            return std::make_pair(true, *(Type*)&_values[offset.second]);
        }
        return std::make_pair(false, Type(0));
    }

    template<typename Type>
        std::pair<bool, Type> ParameterBox::GetParameter(ParameterNameHash name) const
    {
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), name);
        if (i!=_parameterHashValues.cend() && *i == name) {
            size_t index = std::distance(_parameterHashValues.cbegin(), i);
            auto offset = _offsets[index];
            return std::make_pair(true, *(Type*)&_values[offset.second]);
        }
        return std::make_pair(false, Type(0));
    }

    template void ParameterBox::SetParameter(const char name[], uint32 value);
    template std::pair<bool, uint32> ParameterBox::GetParameter(const char name[]) const;
    template std::pair<bool, uint32> ParameterBox::GetParameter(ParameterNameHash name) const;

    template void ParameterBox::SetParameter(const char name[], int32 value);
    template std::pair<bool, int32> ParameterBox::GetParameter(const char name[]) const;
    template std::pair<bool, int32> ParameterBox::GetParameter(ParameterNameHash name) const;

    template void ParameterBox::SetParameter(const char name[], bool value);
    template std::pair<bool, bool> ParameterBox::GetParameter(const char name[]) const;
    template std::pair<bool, bool> ParameterBox::GetParameter(ParameterNameHash name) const;

    template void ParameterBox::SetParameter(const char name[], float value);
    template std::pair<bool, float> ParameterBox::GetParameter(const char name[]) const;
    template std::pair<bool, float> ParameterBox::GetParameter(ParameterNameHash name) const;


    uint64      ParameterBox::CalculateParameterNamesHash() const
    {
            //  Note that the parameter names are always in the same order (unless 
            //  two names resolve to the same 32 bit hash value). So, even though
            //  though the xor operation here doesn't depend on order, it should be
            //  ok -- because if the same parameter names appear in two different
            //  parameter boxes, they should have the same order.
        return Hash64(AsPointer(_names.cbegin()), AsPointer(_names.cend()));
    }

    uint64      ParameterBox::CalculateHash() const
    {
        return Hash64(AsPointer(_values.cbegin()), AsPointer(_values.cend()));
    }

    const void* ParameterBox::GetValue(size_t index) const
    {
        if (index < _offsets.size()) {
            auto offset = _offsets[index].second;
            return &_values[offset];
        }
        return 0;    
    }

    uint64      ParameterBox::GetHash() const
    {
        if (!_cachedHash) {
            _cachedHash = CalculateHash();
        }
        return _cachedHash;
    }

    uint64      ParameterBox::GetParameterNamesHash() const
    {
        if (!_cachedParameterNameHash) {
            _cachedParameterNameHash = CalculateParameterNamesHash();
        }
        return _cachedParameterNameHash;
    }

    uint64      ParameterBox::TranslateHash(const ParameterBox& source) const
    {
        if (_values.size() > 1024) {
            assert(0);
            return 0;
        }

        uint8 temporaryValues[1024];
        std::copy(_values.cbegin(), _values.cend(), temporaryValues);

        auto i  = _parameterHashValues.cbegin();
        auto i2 = source._parameterHashValues.cbegin();
        while (i < _parameterHashValues.cend() && i2 < source._parameterHashValues.cend()) {

            if (*i < *i2)       { ++i; } 
            else if (*i > *i2)  { ++i2; } 
            else if (*i == *i2) {
                auto offsetDest = _offsets[std::distance(_parameterHashValues.cbegin(), i)].second;
                auto typeDest   = _types[std::distance(_parameterHashValues.cbegin(), i)];
                auto offsetSrc  = source._offsets[std::distance(source._parameterHashValues.cbegin(), i2)].second;
                auto typeSrc    = source._types[std::distance(source._parameterHashValues.cbegin(), i2)];
                
                if (typeDest == typeSrc) {
                    XlCopyMemory(
                        PtrAdd(temporaryValues, offsetDest), 
                        PtrAdd(AsPointer(source._values.cbegin()), offsetSrc),
                        typeDest.GetSize());
                }

                ++i; ++i2;
            }

        }

        return Hash64(temporaryValues, PtrAdd(temporaryValues, _values.size()));
    }

    class StringTableComparison
    {
    public:
        bool operator()(const char* lhs, const std::pair<const char*, std::string>& rhs) const 
        {
            return XlCompareString(lhs, rhs.first) < 0;
        }

        bool operator()(const std::pair<const char*, std::string>& lhs, const std::pair<const char*, std::string>& rhs) const 
        {
            return XlCompareString(lhs.first, rhs.first) < 0;
        }

        bool operator()(const std::pair<const char*, std::string>& lhs, const char* rhs) const 
        {
            return XlCompareString(lhs.first, rhs) < 0;
        }
    };

    void ParameterBox::BuildStringTable(std::vector<std::pair<const char*, std::string>>& defines) const
    {
        for (auto i=_offsets.cbegin(); i!=_offsets.cend(); ++i) {
            const auto* name = &_names[i->first];
            const void* value = &_values[i->second];
            const auto& type = _types[std::distance(_offsets.begin(), i)];
            auto stringFormat = ImpliedTyping::AsString(value, type);

            auto insertPosition = std::lower_bound(
                defines.begin(), defines.end(), name, StringTableComparison());
            if (insertPosition!=defines.cend() && !XlCompareString(insertPosition->first, name)) {
                insertPosition->second = stringFormat;
            } else {
                defines.insert(insertPosition, std::make_pair(name, stringFormat));
            }
        }
    }

    void ParameterBox::OverrideStringTable(std::vector<std::pair<const char*, std::string>>& defines) const
    {
        for (auto i=_offsets.cbegin(); i!=_offsets.cend(); ++i) {
            const auto* name = &_names[i->first];
            const void* value = &_values[i->second];
            const auto& type = _types[std::distance(_offsets.begin(), i)];

            auto insertPosition = std::lower_bound(
                defines.begin(), defines.end(), name, StringTableComparison());

            if (insertPosition!=defines.cend() && !XlCompareString(insertPosition->first, name)) {
                insertPosition->second = ImpliedTyping::AsString(value, type);
            }
        }
    }

    bool ParameterBox::ParameterNamesAreEqual(const ParameterBox& other) const
    {
            // return true iff both boxes have exactly the same parameter names, in the same order
        if (_names.size() != other._names.size()) {
            return false;
        }
        return GetParameterNamesHash() == other.GetParameterNamesHash();
    }

    void ParameterBox::MergeIn(const ParameterBox& source)
    {
            // simple implementation... 
            //  We could build a more effective implementation taking into account
            //  the fact that both parameter boxes are sorted.
        for (auto i=_offsets.cbegin(); i!=_offsets.cend(); ++i) {
            const auto* name = &source._names[i->first];
            SetParameter(
                name, 
                &source._values[i->second],
                source._types[std::distance(_offsets.cbegin(), i)]);
        }
    }

    void ParameterBox::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _cachedHash);
        Serialization::Serialize(serializer, _cachedParameterNameHash);
        Serialization::Serialize(serializer, _parameterHashValues);
        Serialization::Serialize(serializer, _offsets);
        Serialization::Serialize(serializer, _names);
        Serialization::Serialize(serializer, _values);
        Serialization::Serialize(serializer, _types);
    }

    ParameterBox::ParameterBox()
    {
        _cachedHash = _cachedParameterNameHash = 0;
    }

    ParameterBox::ParameterBox(
        std::initializer_list<std::pair<const char*, const char*>> init)
    {
        for (auto i=init.begin(); i!=init.end(); ++i) {
            SetParameter(i->first, i->second);
        }
    }

    ParameterBox::ParameterBox(ParameterBox&& moveFrom)
    : _parameterHashValues(std::move(moveFrom._parameterHashValues))
    , _offsets(std::move(moveFrom._offsets))
    , _names(std::move(moveFrom._names))
    , _values(std::move(moveFrom._values))
    , _types(std::move(moveFrom._types))
    {
        _cachedHash = moveFrom._cachedHash;
        _cachedParameterNameHash = moveFrom._cachedParameterNameHash;
    }
        
    ParameterBox& ParameterBox::operator=(ParameterBox&& moveFrom)
    {
        _parameterHashValues = std::move(moveFrom._parameterHashValues);
        _offsets = std::move(moveFrom._offsets);
        _names = std::move(moveFrom._names);
        _values = std::move(moveFrom._values);
        _types = std::move(moveFrom._types);
        _cachedHash = moveFrom._cachedHash;
        _cachedParameterNameHash = moveFrom._cachedParameterNameHash;
        return *this;
    }

    ParameterBox::~ParameterBox()
    {
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

}



