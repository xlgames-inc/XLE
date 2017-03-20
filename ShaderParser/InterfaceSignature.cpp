// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InterfaceSignature.h"
#include "AntlrHelper.h"
// #include "../ConsoleRig/Log.h"
#include "Grammar/ShaderLexer.h"
#include "Grammar/ShaderParser.h"
#include "../Utility/StringUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Streams/Stream.h"
#include <assert.h>

namespace ShaderSourceParser
{
    static std::basic_string<StringType::value_type> AsTypeNameString(ANTLR3_BASE_TREE* typeNameNode)
    {
        // We're expecting the input to be a TYPE_NAME node.
        // The format should be an initial name, followed by a static expression list
        using CharType = StringType::value_type;
        auto type = typeNameNode->getType(typeNameNode);
        auto childCount = typeNameNode->getChildCount(typeNameNode);
        if (type != TYPE_NAME || childCount == 0)
            return "<<tree parsing error>>";
        
        auto baseChild = pANTLR3_BASE_TREE(typeNameNode->getChild(typeNameNode, 0));
        auto result = AntlrHelper::AsString<CharType>(baseChild->toString(baseChild));
        if (childCount > 1) {
            result += "<";
            for (unsigned c=1; c<childCount; ++c) {
                if (c!=1) result += ", ";
                auto child = pANTLR3_BASE_TREE(typeNameNode->getChild(typeNameNode, c));
                result += AntlrHelper::AsString<CharType>(baseChild->toString(child));
            }
            result += ">";
        }
        return result;
    }

    ShaderFragmentSignature     BuildShaderFragmentSignature(StringSection<char> sourceCode)
    {
        AntlrHelper::ParserRig psr(sourceCode);

        auto treeRoot = psr.BuildAST();

        ShaderFragmentSignature result;
        using CharType = StringType::value_type;

        auto childCount = treeRoot->getChildCount(treeRoot);
        for (auto i=0u; i<childCount; ++i) {
            auto node        = pANTLR3_BASE_TREE(treeRoot->getChild(treeRoot, i));
            auto token       = node->getToken(node);
            auto tokenType   = token->getType(token);

            if (tokenType == FUNCTION) {
                    
                    // Function objects should be
                    //      (return type) (function name)
                    //      (FORMAL_ARG)*
                    //      (SEMANTIC)?
                    //      BLOCK

                auto functionChildCount = node->getChildCount(node);
                if (functionChildCount >= 2) {
                    FunctionSignature functionResult;

                    auto returnTypeNode      = pANTLR3_BASE_TREE(node->getChild(node, 0));
                    auto functionNameNode    = pANTLR3_BASE_TREE(node->getChild(node, 1));

                    functionResult._returnType  = AntlrHelper::AsString<CharType>(returnTypeNode->toString(returnTypeNode));
                    functionResult._name        = AntlrHelper::AsString<CharType>(functionNameNode->toString(functionNameNode));
                        
                    for (unsigned q=2; q<functionChildCount; ++q) {
                        auto functionNode    = pANTLR3_BASE_TREE(node->getChild(node, q));
                        auto token           = functionNode->getToken(functionNode);
                        auto tokenType       = token->getType(token);
                        if (tokenType == FORMAL_ARG) {
                            auto childCount          = functionNode->getChildCount(functionNode);
                            if (childCount >= 2) {
                                auto typeNode        = pANTLR3_BASE_TREE(functionNode->getChild(functionNode, 0));
                                auto nameNode        = pANTLR3_BASE_TREE(functionNode->getChild(functionNode, 1));

                                FunctionSignature::Parameter parameter;
                                parameter._name     = AntlrHelper::AsString<CharType>(nameNode->toString(nameNode));
                                parameter._type     = AsTypeNameString(typeNode);
                                parameter._direction = FunctionSignature::Parameter::In;

                                    // look for a DIRECTION_OUT or DIRECTION_IN_OUT child
                                    // this will exist after the type and name (if it exists at all)
                                for (unsigned c=2; c<childCount; ++c) {
                                    using namespace AntlrHelper;
                                    auto child = GetChild(functionNode, c);
                                    auto tokenType = GetType(GetToken(child));
                                    if (tokenType == DIRECTION_OUT) {
                                        parameter._direction = FunctionSignature::Parameter::Out;
                                    } else if (tokenType == DIRECTION_IN_OUT) {
                                        parameter._direction = FunctionSignature::Parameter::In | FunctionSignature::Parameter::Out;
                                    } else if (tokenType == SEMANTIC) {
                                        if (child->getChildCount(child) >= 1) {
                                            auto semanticNode = pANTLR3_BASE_TREE(child->getChild(child, 0));
                                            parameter._semantic = AntlrHelper::AsString<CharType>(semanticNode->toString(semanticNode));
                                        }
                                    }
                                }
                                
                                functionResult._parameters.push_back(std::move(parameter));
                            }
                        } else if (tokenType == SEMANTIC) {
                            auto childCount = functionNode->getChildCount(functionNode);
                            if (childCount >= 1) {
                                auto semanticNode = pANTLR3_BASE_TREE(functionNode->getChild(functionNode, 0));
                                functionResult._returnSemantic = AntlrHelper::AsString<CharType>(semanticNode->toString(semanticNode));
                            }
                        }
                    }

                    result._functions.push_back(std::move(functionResult));
                }
            } else if (tokenType == STRUCT || tokenType == CBUFFER) {

                //
                //  Structs are simple...
                //      (name)
                //      (VARIABLE)*
                //      where VARIABLE is:
                //          (VARIABLE type (VARIABLE_NAME name (SUBSCRIPT ...)? (SEMANTIC ...)? (e)?)*)
                //
                //      Note that we're not distinguishing between "struct" and "cbuffer"... they both
                //      come out the same. But the shader syntax will be slightly different in each
                //      case.
                //

                auto childCount = node->getChildCount(node);
                if (childCount>= 1) {
                    ParameterStructSignature str;

                    auto nameNode = pANTLR3_BASE_TREE(node->getChild(node, 0));
                    str._name = AntlrHelper::AsString<CharType>(nameNode->toString(nameNode));

                    for (unsigned q=1; q<childCount; ++q) {
                        auto uniformNode     = pANTLR3_BASE_TREE(node->getChild(node, q));
                        auto token           = uniformNode->getToken(uniformNode);
                        auto tokenType       = token->getType(token);
                        if (tokenType == VARIABLE) {

                                //
                                //      This is a "VARIABLE" node. There is a type name,
                                //      and then a list of variables of this type. Note
                                //      that HLSL doesn't have "*" and "&", etc -- so it's
                                //      not too complex (not to mention function pointers...)
                                //

                            auto uniformChildCount = uniformNode->getChildCount(uniformNode);
                            if (uniformChildCount >= 2) {

                                auto typeNameNode   = pANTLR3_BASE_TREE(uniformNode->getChild(uniformNode, 0));
                                auto uniformType    = AsTypeNameString(typeNameNode);

                                for (unsigned n=1; n<uniformChildCount; ++n) {
                                    auto nameNode   = pANTLR3_BASE_TREE(uniformNode->getChild(uniformNode, n));
                                    auto token      = nameNode->getToken(nameNode);
                                    auto tokenType  = token->getType(token);
                                    if (tokenType == VARIABLE_NAME) {

                                        auto nameNodeChildCount = nameNode->getChildCount(nameNode);
                                        if (nameNodeChildCount >= 1) {

                                            auto variableNamePart = pANTLR3_BASE_TREE(nameNode->getChild(nameNode, 0));
                                            auto variableName = AntlrHelper::AsString<CharType>(variableNamePart->toString(variableNamePart));
                                            StringType semantic;

                                                //  look for a "SEMANTIC" node attached
                                                //      if there are multiple, it's an error.. but just use the first valid
                                            for (unsigned w=1; w<nameNodeChildCount; ++w) {
                                                auto partNode   = pANTLR3_BASE_TREE(nameNode->getChild(nameNode, n));
                                                auto token      = partNode->getToken(partNode);
                                                auto tokenType  = token->getType(token);
                                                if (tokenType == SEMANTIC) {
                                                    if (partNode->getChildCount(partNode) >= 1) {
                                                        auto semanticNamePart = pANTLR3_BASE_TREE(partNode->getChild(partNode, 0));
                                                        semantic = AntlrHelper::AsString<CharType>(semanticNamePart->toString(semanticNamePart));
                                                        break;
                                                    }
                                                }
                                            }

                                                //  We know everything about the parameter now
                                                //      ... so just add it to the list
                                            ParameterStructSignature::Parameter p;
                                            p._name = variableName;
                                            p._type = uniformType;
                                            p._semantic = semantic;
                                            str._parameters.push_back(p);

                                        }

                                    }
                                }
                            }
                        }
                    }

                    if (str._parameters.size()) {
                        result._parameterStructs.push_back(std::move(str));
                    }

                }
            }
        }

        return result;
    }

        ////////////////////////////////////////////////////////////

    FunctionSignature::FunctionSignature() {}
    FunctionSignature::~FunctionSignature() {}
    FunctionSignature::FunctionSignature(FunctionSignature&& moveFrom)
    :   _returnType(std::move(moveFrom._returnType))
    ,   _returnSemantic(std::move(moveFrom._returnSemantic))
    ,   _name(std::move(moveFrom._name))
    ,   _parameters(std::move(moveFrom._parameters))
    {}

    FunctionSignature& FunctionSignature::operator=(FunctionSignature&& moveFrom) never_throws
    {
        _returnType = std::move(moveFrom._returnType);
        _returnSemantic = std::move(moveFrom._returnSemantic);
        _name = std::move(moveFrom._name);
        _parameters = std::move(moveFrom._parameters);
        return *this;
    }

        ////////////////////////////////////////////////////////////

    ParameterStructSignature::ParameterStructSignature() {}
    ParameterStructSignature::~ParameterStructSignature() {}
    ParameterStructSignature::ParameterStructSignature(ParameterStructSignature&& moveFrom)
    :   _name(std::move(moveFrom._name))
    ,   _parameters(std::move(moveFrom._parameters))
    {

    }

    ParameterStructSignature& ParameterStructSignature::operator=(ParameterStructSignature&& moveFrom) never_throws
    {
        _name = std::move(moveFrom._name);
        _parameters = std::move(moveFrom._parameters);
        return *this;
    }

        ////////////////////////////////////////////////////////////

    ShaderFragmentSignature::ShaderFragmentSignature() {}
    ShaderFragmentSignature::~ShaderFragmentSignature() {}
    ShaderFragmentSignature::ShaderFragmentSignature(ShaderFragmentSignature&& moveFrom)
    : _functions(std::move(moveFrom._functions))
    , _parameterStructs(std::move(moveFrom._parameterStructs))
    {}
    
    ShaderFragmentSignature& ShaderFragmentSignature::operator=(ShaderFragmentSignature&& moveFrom) never_throws
    {
        _functions = std::move(moveFrom._functions);
        _parameterStructs = std::move(moveFrom._parameterStructs);
        return *this;
    }

}
