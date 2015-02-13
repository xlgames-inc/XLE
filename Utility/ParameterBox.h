// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include <string>
#include <vector>

namespace Utility
{

        //////////////////////////////////////////////////////////////////
            //      P A R A M E T E R   B O X                       //
        //////////////////////////////////////////////////////////////////

            //      a handy abstraction to represent a number of 
            //      parameters held together. We must be able to
            //      quickly merge and filter values in this table.

    class ParameterBox
    {
    public:
        typedef uint32 ParameterNameHash;
        void        SetParameter(const std::string& name, uint32 value);
        uint32      GetParameter(const std::string& name) const;
        uint32      GetParameter(ParameterNameHash name) const;

        uint64      GetHash() const;
        uint64      GetParameterNamesHash() const;
        uint64      TranslateHash(const ParameterBox& source) const;

        void        BuildStringTable(std::vector<std::pair<std::string, std::string>>& defines) const;
        void        OverrideStringTable(std::vector<std::pair<std::string, std::string>>& defines) const;

        void        MergeIn(const ParameterBox& source);

        static ParameterNameHash    MakeParameterNameHash(const std::string& name);

        bool        ParameterNamesAreEqual(const ParameterBox& other) const;

        ParameterBox();
        ParameterBox(ParameterBox&& moveFrom);
        ParameterBox& operator=(ParameterBox&& moveFrom);
        ~ParameterBox();
    private:
        mutable uint64      _cachedHash;
        mutable uint64      _cachedParameterNameHash;
    
        std::vector<ParameterNameHash>  _parameterHashValues;
        std::vector<uint32>             _parameterOffsets;
        std::vector<std::string>        _parameterNames;
        std::vector<uint8>              _values;

        uint32      GetValue(size_t index) const;
        uint64      CalculateHash() const;
        uint64      CalculateParameterNamesHash() const;
    };

}

using namespace Utility;
