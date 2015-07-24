// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IteratorUtils.h"
#include "PtrUtils.h"
#include "../Core/Exceptions.h"
#include <functional>
#include <utility>
#include <vector>
#include <stdint.h>

namespace Utility
{
    template<typename Type> Type DefaultValue() { return Type(0); }

        //
        //  MakeFunction is based on a stack overflow answer:
        //      http://stackoverflow.com/questions/21738775/c11-how-to-write-a-wrapper-function-to-make-stdfunction-objects?lq=1
        //  
        //  It allows us to convert various function pointers to std::function<> form.
        //  Note that this is not something we want to do frequently! Often it's better to
        //  keep things in earlier form (eg, the result of a std::bind, or a pointer to a 
        //  lambda function). But we want to use std::function<> form for the VariantFunctions
        //  object below.
        //  

    // For generic types that are functors, delegate to its 'operator()'
    template <typename T>
    struct FunctionTraits
      : public FunctionTraits<decltype(&T::operator())>
    {};
 
    // for pointers to member function
    template <typename ClassType, typename ReturnType, typename... Args>
    struct FunctionTraits<ReturnType(ClassType::*)(Args...) const> {
      enum { arity = sizeof...(Args) };
      typedef std::function<ReturnType (Args...)> f_type;
    };
 
    // for pointers to member function
    template <typename ClassType, typename ReturnType, typename... Args>
    struct FunctionTraits<ReturnType(ClassType::*)(Args...) > {
      enum { arity = sizeof...(Args) };
      typedef std::function<ReturnType (Args...)> f_type;
    };
 
    // for function pointers
    template <typename ReturnType, typename... Args>
    struct FunctionTraits<ReturnType (*)(Args...)>  {
      enum { arity = sizeof...(Args) };
      typedef std::function<ReturnType (Args...)> f_type;
    };
 
    template <typename L> 
    static typename FunctionTraits<L>::f_type MakeFunction(L l){
      return (typename FunctionTraits<L>::f_type)(l);
    }
 
    //handles bind & multiple function call operator()'s
    template<typename ReturnType, typename... Args, class T>
    auto MakeFunction(T&& t) 
      -> std::function<decltype(ReturnType(t(std::declval<Args>()...)))(Args...)> 
    {return {std::forward<T>(t)};}
 
    //handles explicit overloads
    template<typename ReturnType, typename... Args>
    auto MakeFunction(ReturnType(*p)(Args...))
        -> std::function<ReturnType(Args...)> {
      return {p};
    }
 
    //handles explicit overloads
    template<typename ReturnType, typename... Args, typename ClassType>
    auto MakeFunction(ReturnType(ClassType::*p)(Args...)) 
        -> std::function<ReturnType(Args...)> { 
      return {p};
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    //      V A R I A N T   F U N C T I O N S
///////////////////////////////////////////////////////////////////////////////////////////////////

    class VariantFunctions
    {
    public:
        typedef uint64_t Id;

        template<typename Result, typename... Args>
            Result Call(Id id, Args... args) const;

        template<typename Result, typename... Args>
            Result CallDefault(Id id, const Result& defaultResult, Args... args) const;

        template<typename Result, typename... Args>
            bool TryCall(Result& res, Id id, Args... args) const;

        template<typename FnSig>
            std::function<FnSig>& Get(Id id) const;

        template<typename FnSig>
            bool Has(Id id) const;

        template<typename Fn>
            void Add(Id guid, std::function<Fn>&& fn);

        template<typename ReturnType, typename... Args>
            void Add(Id id, ReturnType (*p) (Args...));

        template <typename L> 
            void Add(Id id, L&& l);

        template<typename ReturnType, typename... Args, typename ClassType>
            void Add(Id id, ReturnType(ClassType::*p)(Args...));

        bool Remove(Id id);

        class DuplicateFunction;
        class NoFunction;
        class SignatureMismatch;

        VariantFunctions();
        ~VariantFunctions();

        VariantFunctions(const VariantFunctions&) = delete;
        VariantFunctions& operator=(const VariantFunctions&) = delete;
    protected:
        class StoredFunction
        {
        public:
            size_t  _offset;
            size_t  _size;
            size_t  _typeHashCode;
            void (*_destructor)(void*);
            void (*_moveConstructor)(void*, void*);
        };
        std::vector<uint8_t> _buffer;
        std::vector<std::pair<Id, StoredFunction>> _fns;

        void ExpandBuffer(size_t newCapacity);
    };

    namespace Internal
    {
        template<typename Type>
            static void Destructor(void* obj)
            {
                reinterpret_cast<Type*>(obj)->~Type();
            }

        template<typename Type>
            static void MoveConstructor(void* dst, void* src)
            {
                auto* d = reinterpret_cast<Type*>(dst);
                auto* s = reinterpret_cast<Type*>(src);
                #pragma push_macro("new")
                #undef new
                    new (d) Type(std::move(*s));
                #pragma pop_macro("new")
            }
    }

    class VariantFunctions::DuplicateFunction : public std::runtime_error
    {
    public:
        DuplicateFunction() : std::runtime_error("Attempting to push multiple functions with the same id into a variant functions set") {}
    };

    class VariantFunctions::NoFunction : public std::runtime_error
    {
    public:
        NoFunction() : std::runtime_error("No function found matching the requested id") {}
    };

    class VariantFunctions::SignatureMismatch : public std::runtime_error
    {
    public:
        SignatureMismatch() : std::runtime_error("Function signature does not match expected signature") {}
    };

    template<typename Fn>
        void VariantFunctions::Add(Id id, std::function<Fn>&& fn)
    {
        auto i = LowerBound(_fns, id);
        if (i != _fns.end() && i->first == id) { Throw(DuplicateFunction()); } // duplicate of one already here!

        StoredFunction sfn;
        sfn._offset = _buffer.size();
        sfn._size = sizeof(std::function<Fn>);
        sfn._destructor = &Internal::Destructor<std::function<Fn>>;
        sfn._moveConstructor = &Internal::MoveConstructor<std::function<Fn>>;
        sfn._typeHashCode = typeid(std::function<Fn>).hash_code();
        
        if ((_buffer.size() + sfn._size) > _buffer.capacity())
            ExpandBuffer((_buffer.size() + sfn._size) * 2);
        assert((_buffer.size() + sfn._size) <= _buffer.capacity());
        _buffer.insert(_buffer.end(), sfn._size, uint8(0xcd));

        _fns.insert(i, std::make_pair(id, sfn));

        auto* dst = reinterpret_cast<std::function<Fn>*>(PtrAdd(AsPointer(_buffer.begin()), sfn._offset));
        #pragma push_macro("new")
        #undef new
            new(dst) std::function<Fn>(std::move(fn));
        #pragma pop_macro("new")
    }

    template<typename Result, typename... Args>
        Result VariantFunctions::Call(Id id, Args... args) const
    {
        auto i = LowerBound(_fns, id);
        if (i == _fns.end() || i->first != id)
            Throw(NoFunction());
        
        using FnType = std::function<Result (Args...)>;
        auto expectedSize = sizeof(FnType);
        if (i->second._size != expectedSize || typeid(FnType).hash_code() != i->second._typeHashCode)
            Throw(SignatureMismatch());

        auto* obj = (void*)PtrAdd(AsPointer(_buffer.begin()), i->second._offset);
        auto* fn = reinterpret_cast<std::function<Result (Args...)>*>(obj);

        return (*fn)(args...);
    }

    template<typename Result, typename... Args>
        bool VariantFunctions::TryCall(Result& result, Id id, Args... args) const
    {
        auto i = LowerBound(_fns, id);
        if (i == _fns.end() || i->first != id)
            return false;
        
        using FnType = std::function<Result (Args...)>;
        auto expectedSize = sizeof(FnType);
        if (i->second._size != expectedSize || typeid(FnType).hash_code() != i->second._typeHashCode)
            return false;

        auto* obj = (void*)PtrAdd(AsPointer(_buffer.begin()), i->second._offset);
        auto* fn = reinterpret_cast<FnType*>(obj);

        result = (*fn)(args...);
        return true;
    }

    template<typename Result, typename... Args>
        Result VariantFunctions::CallDefault(Id id, const Result& defaultResult, Args... args) const
    {
        auto i = LowerBound(_fns, id);
        if (i == _fns.end() || i->first != id)
            return defaultResult;
        
        using FnType = std::function<Result (Args...)>;
        auto expectedSize = sizeof(FnType);
        if (i->second._size != expectedSize || typeid(FnType).hash_code() != i->second._typeHashCode)
            Throw(SignatureMismatch());

        auto* obj = (void*)PtrAdd(AsPointer(_buffer.begin()), i->second._offset);
        auto* fn = reinterpret_cast<std::function<Result (Args...)>*>(obj);

        return (*fn)(args...);
    }

    template<typename FnSig>
        std::function<FnSig>& VariantFunctions::Get(Id id) const
    {
        auto i = LowerBound(_fns, id);
        if (i == _fns.end() || i->first != id)
            Throw(NoFunction());
        
        using FnType = std::function<FnSig>;
        auto expectedSize = sizeof(FnType);
        if (i->second._size != expectedSize || typeid(FnType).hash_code() != i->second._typeHashCode)
            Throw(SignatureMismatch());

        auto* obj = (void*)PtrAdd(AsPointer(_buffer.begin()), i->second._offset);
        auto* fn = reinterpret_cast<std::function<FnSig>*>(obj);

        return (*fn);
    }

    template<typename FnSig>
        bool VariantFunctions::Has(Id id) const
    {
        auto i = LowerBound(_fns, id);
        if (i == _fns.end() || i->first != id)
            return false;
        
            // Verify the type matches. If we get a mis-match, we will throw
            // an exception. (this is the only use for the FnSig argument
        using FnType = std::function<FnSig>;
        auto expectedSize = sizeof(FnType);
        if (i->second._size != expectedSize || typeid(FnType).hash_code() != i->second._typeHashCode)
            Throw(SignatureMismatch());

        return true;
    }

    template<typename ReturnType, typename... Args>
        void VariantFunctions::Add(Id id, ReturnType (*p) (Args...))
    {
        Add(id, std::function<ReturnType(Args...)>(p));
    }

    template<typename ReturnType, typename... Args, typename ClassType>
        void VariantFunctions::Add(Id id, ReturnType(ClassType::*p)(Args...))
    {
        Add(id, std::function<ReturnType(Args...)>(p));
    }

    template <typename L> 
        void VariantFunctions::Add(Id id, L&& l)
    {
        Add(id, (typename FunctionTraits<L>::f_type)(l));
    }

}

using namespace Utility;

