// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ParameterSignature.h"
#include "Exceptions.h"
#include "../Utility/Streams/Data.h"
#include "../Utility/StringUtils.h"

namespace ShaderSourceParser
{
    UniformBufferSignature  LoadSignature(const char sourceCode[], size_t sourceCodeLength)
    {
        Data data;
        bool loadResult = data.Load(sourceCode, (int)sourceCodeLength);
        if (!loadResult) {
            Error errors[] = { Error{0,0,0,0, "Failure while parsing object"} };
            throw Exceptions::ParsingFailure(MakeIteratorRange(errors));
        }

        UniformBufferSignature result;
        result._name                = data.StrAttribute("Name");
        result._description         = data.StrAttribute("Description");
        result._min                 = data.StrAttribute("Min");
        result._max                 = data.StrAttribute("Max");
        result._type                = data.StrAttribute("Type");
        result._typeExtra           = data.StrAttribute("TypeExtra");
        result._semantic            = data.StrAttribute("Semantic");
        result._default             = data.StrAttribute("Default");

        result._source = UniformBufferSignature::Source::Material;
        auto source = data.StrAttribute("Source");
        if (source && !XlCompareStringI(source, "System")) {
            result._source = UniformBufferSignature::Source::System;
        }
        return result;
    }

    std::string         StoreSignature(const UniformBufferSignature& signature)
    {
        Data data;
        data.SetAttribute("Name",               signature._name.c_str());
        data.SetAttribute("Description",        signature._description.c_str());
        data.SetAttribute("Min",                signature._min.c_str());
        data.SetAttribute("Max",                signature._max.c_str());
        data.SetAttribute("Type",               signature._type.c_str());
        data.SetAttribute("TypeExtra",          signature._typeExtra.c_str());
        data.SetAttribute("Semantic",           signature._semantic.c_str());
        data.SetAttribute("Default",            signature._default.c_str());

        if (signature._source == UniformBufferSignature::Source::System) {
            data.SetAttribute("Source", "System");
        } else {
            data.SetAttribute("Source", "Material");
        }
        
            //
            //      It looks like we can't tell how large the buffer needs to be in 
            //      advance. So let's just create a very big buffer and hope it fits.
            //      I can't see any way to tell if there's been an overflow during
            //      the writing process.
            //
        char buffer[4096];
        int len = dimof(buffer);
        data.SaveToBuffer(buffer, &len);
        return std::string(buffer);
    }
}

