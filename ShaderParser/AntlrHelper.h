// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <antlr3defs.h>
#include <antlr3interfaces.h>
#include <antlr3basetree.h>
#include <memory>
#include <string>

namespace ShaderSourceParser { namespace AntlrHelper
{
    namespace Internal
    {
        #pragma push_macro("free")
        #undef free
        template <typename Type> 
            void DestroyAntlrObject(Type* object) 
            {
                if (object) object->free(object);
            }
        #pragma pop_macro("free")

        template<> void DestroyAntlrObject<>(struct ::ANTLR3_INPUT_STREAM_struct*);
    }

    template <typename Type>
        class AntlrPtr
    {
    public:
        operator Type*()                { return _antlrObj; }
        operator const Type*() const    { return _antlrObj; }
        operator bool() const           { return !!_antlrObj; }
        Type* operator->()              { return _antlrObj; }
        const Type* operator->() const  { return _antlrObj; }
        Type* get()                     { return _antlrObj; }
        const Type* get() const         { return _antlrObj; }

        AntlrPtr(Type* antlrObj) : _antlrObj(antlrObj) {}
        ~AntlrPtr()     { if(_antlrObj) Internal::DestroyAntlrObject(_antlrObj); }
        AntlrPtr()      { _antlrObj = NULL; }

        AntlrPtr(const AntlrPtr&) = delete;
        AntlrPtr& operator=(const AntlrPtr&) = delete;

        AntlrPtr(AntlrPtr&& moveFrom)
        {
            _antlrObj = moveFrom._antlrObj;
            moveFrom._antlrObj = nullptr;
        }
        AntlrPtr& operator=(AntlrPtr&& moveFrom)
        {
            _antlrObj = moveFrom._antlrObj;
            moveFrom._antlrObj = nullptr;
            return *this;
        }
    private:
        Type*        _antlrObj;
    };

    pANTLR3_BASE_TREE       GetChild(pANTLR3_BASE_TREE node, ANTLR3_UINT32 childIndex);
    pANTLR3_COMMON_TOKEN    GetToken(pANTLR3_BASE_TREE node);
    ANTLR3_UINT32           GetType(pANTLR3_COMMON_TOKEN token);

    template<typename CharType>
        std::basic_string<CharType> AsString(ANTLR3_STRING* antlrString);

    class ParserRig
    {
    public:
        ANTLR3_BASE_TREE* BuildAST();

        ParserRig(const char sourceCode[], size_t sourceCodeLength);
        ~ParserRig();

        ParserRig(const ParserRig&) = delete;
        ParserRig& operator=(const ParserRig&) = delete;
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}}
