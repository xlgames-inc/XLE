// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Prefix.h"
#include "../Core/Exceptions.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/StringUtils.h"
#include <vector>

namespace ShaderSourceParser
{
    using StringType = std::string;

    class FunctionSignature
    {
    public:
        StringType  _name;
		StringType  _returnType;
        StringType  _returnSemantic;

        class Parameter
        {
        public:
            enum Direction { In = 1<<0, Out = 1<<1 };
            StringType _name;
			StringType _type;
            StringType _semantic;
            unsigned _direction;
        };

        std::vector<Parameter> _parameters;
    };

    class ParameterStructSignature
    {
    public:
        class Parameter
        {
        public:
            StringType _name;
			StringType _type;
            StringType _semantic;
        };

        StringType _name;
        std::vector<Parameter> _parameters;
    };

    class ShaderFragmentSignature
    {
    public:
        std::vector<FunctionSignature>          _functions;
        std::vector<ParameterStructSignature>   _parameterStructs;
    };


    ShaderFragmentSignature     BuildShaderFragmentSignature(StringSection<char> sourceCode);
}

