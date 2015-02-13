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

namespace Utility
{
    ParameterBox::ParameterNameHash ParameterBox::MakeParameterNameHash(const std::string& name)
    {
        return Hash32(AsPointer(name.cbegin()), AsPointer(name.cend()));
    }

    void        ParameterBox::SetParameter(const std::string& name, uint32 value)
    {
        auto hash = MakeParameterNameHash(name);
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), hash);
        if (i==_parameterHashValues.cend()) {
            _parameterHashValues.push_back(hash);
            auto offset = _values.size();
            _parameterOffsets.push_back((ParameterNameHash)offset);
            _parameterNames.push_back(name);
            _values.resize(offset+sizeof(uint32), 0);
            *(uint32*)&_values[offset] = value;
            _cachedHash = 0;
            _cachedParameterNameHash = 0;
            return;
        }

        size_t index = std::distance(_parameterHashValues.cbegin(), i);
        if (*i!=hash) {
            _parameterHashValues.insert(i, hash);
            _parameterNames.insert(_parameterNames.begin()+index, name);
            size_t offset = _parameterOffsets[index];
            _parameterOffsets.insert(_parameterOffsets.begin()+index, uint32(offset));
            for (auto i2=_parameterOffsets.begin()+index+1; i2<_parameterOffsets.end(); ++i2) {
                (*i2) += sizeof(uint32);
            }
            _values.insert(_values.cbegin()+offset, (byte*)&value, (byte*)PtrAdd(&value, sizeof(uint32)));
            *(uint32*)&_values[offset] = value;
            _cachedHash = 0;
            _cachedParameterNameHash = 0;
            return;
        }

        assert(_parameterNames[index] == name);
        auto offset = _parameterOffsets[index];
        *(uint32*)&_values[offset] = value;
        _cachedHash = 0;
    }

    uint32      ParameterBox::GetParameter(const std::string& name) const
    {
        auto hash = MakeParameterNameHash(name);
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), hash);
        if (i!=_parameterHashValues.cend() && *i == hash) {
            size_t index = std::distance(_parameterHashValues.cbegin(), i);
            auto offset = _parameterOffsets[index];
            return *(uint32*)&_values[offset];
        }
        return 0;
    }

    uint32      ParameterBox::GetParameter(ParameterNameHash name) const
    {
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), name);
        if (i!=_parameterHashValues.cend() && *i == name) {
            size_t index = std::distance(_parameterHashValues.cbegin(), i);
            auto offset = _parameterOffsets[index];
            return *(uint32*)&_values[offset];
        }
        return 0;
    }

    uint64      ParameterBox::CalculateParameterNamesHash() const
    {
            //  Note that the parameter names are always in the same order (unless 
            //  two names resolve to the same 32 bit hash value). So, even though
            //  though the xor operation here doesn't depend on order, it should be
            //  ok -- because if the same parameter names appear in two different
            //  parameter boxes, they should have the same order.
        uint64 result = 0x7EF5E3B02A75ED13ui64;
        for (auto i=_parameterNames.cbegin(); i!=_parameterNames.cend(); ++i) {
            result ^= Hash64(AsPointer(i->cbegin()), AsPointer(i->cend()));
        }
        return result;
    }

    uint32      ParameterBox::GetValue(size_t index) const
    {
        if (index < _parameterOffsets.size()) {
            auto offset = _parameterOffsets[index];
            return *(uint32*)&_values[offset];
        }
        return 0;    
    }

    uint64      ParameterBox::CalculateHash() const
    {
        return Hash64(AsPointer(_values.cbegin()), AsPointer(_values.cend()));
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
            if (*i < *i2) {
                ++i;
            } else if (*i > *i2) {
                ++i2;
            } else if (*i == *i2) {
                size_t offsetDest   = _parameterOffsets[std::distance(_parameterHashValues.cbegin(), i)];
                size_t offsetSrc    = source._parameterOffsets[std::distance(source._parameterHashValues.cbegin(), i2)];
                *(uint32*)PtrAdd(temporaryValues, offsetDest) = *(uint32*)PtrAdd(AsPointer(source._values.cbegin()), offsetSrc);
                ++i;
                ++i2;
            }
        }

        return Hash64(temporaryValues, PtrAdd(temporaryValues, _values.size()));
    }

    static std::string AsString(uint32 value)
    {
        char buffer[32];
        Utility::XlI32toA_s(value, buffer, dimof(buffer), 10);
        return buffer;
    }

    void        ParameterBox::BuildStringTable(std::vector<std::pair<std::string, std::string>>& defines) const
    {
        for (auto i=_parameterNames.cbegin(); i!=_parameterNames.cend(); ++i) {
            auto insertPosition     = std::lower_bound(defines.begin(), defines.end(), *i, CompareFirst<std::string, std::string>());
            auto offset             = _parameterOffsets[std::distance(_parameterNames.cbegin(), i)];
            auto value              = *(uint32*)&_values[offset];
            if (insertPosition!=defines.cend() && insertPosition->first == *i) {
                insertPosition->second = AsString(value);
            } else {
                defines.insert(insertPosition, std::make_pair(*i, AsString(value)));
            }
        }
    }

    void        ParameterBox::OverrideStringTable(std::vector<std::pair<std::string, std::string>>& defines) const
    {
        for (auto i=_parameterNames.cbegin(); i!=_parameterNames.cend(); ++i) {
            auto insertPosition     = std::lower_bound(defines.begin(), defines.end(), *i, CompareFirst<std::string, std::string>());
            auto offset             = _parameterOffsets[std::distance(_parameterNames.cbegin(), i)];
            auto value              = *(uint32*)&_values[offset];
            if (insertPosition!=defines.cend() && insertPosition->first == *i) {
                insertPosition->second = AsString(value);
            }
        }
    }

    bool        ParameterBox::ParameterNamesAreEqual(const ParameterBox& other) const
    {
            // return true iff both boxes have exactly the same parameter names, in the same order
        if (_parameterNames.size() != other._parameterNames.size()) {
            return false;
        }
        for (unsigned c=0; c<_parameterNames.size(); ++c) {
            if (_parameterNames[c] != other._parameterNames[c]) {
                return false;
            }
        }
        return true;
    }

    void ParameterBox::MergeIn(const ParameterBox& source)
    {
            // simple implementation... 
            //  We could build a more effective implementation taking into account
            //  the fact that both parameter boxes are sorted.
        for (size_t i=size_t(0); i<source._parameterNames.size(); ++i) {
            SetParameter(source._parameterNames[i], source.GetValue(i));
        }
    }

    ParameterBox::ParameterBox()
    {
        _cachedHash = _cachedParameterNameHash = 0;
    }

    ParameterBox::ParameterBox(ParameterBox&& moveFrom)
    : _parameterHashValues(std::move(moveFrom._parameterHashValues))
    , _parameterOffsets(std::move(moveFrom._parameterOffsets))
    , _parameterNames(std::move(moveFrom._parameterNames))
    , _values(std::move(moveFrom._values))
    {
        _cachedHash = moveFrom._cachedHash;
        _cachedParameterNameHash = moveFrom._cachedParameterNameHash;
    }
        
    ParameterBox& ParameterBox::operator=(ParameterBox&& moveFrom)
    {
        _parameterHashValues = std::move(moveFrom._parameterHashValues);
        _parameterOffsets = std::move(moveFrom._parameterOffsets);
        _parameterNames = std::move(moveFrom._parameterNames);
        _values = std::move(moveFrom._values);
        _cachedHash = moveFrom._cachedHash;
        _cachedParameterNameHash = moveFrom._cachedParameterNameHash;
        return *this;
    }

    ParameterBox::~ParameterBox()
    {
    }
}



