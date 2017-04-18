// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Exceptions.h"
#include <utility>      // (for std::forward)

namespace Utility
{
    #define templ template<typename T>    // (it improves readability so much just to do this)

    templ class MovePTRHelper
    {
    public:
        explicit MovePTRHelper(T* ptr) never_throws : _p(ptr) {}
        T* Adopt() never_throws { T* result = nullptr; std::swap(result, _p); return result; }
        T* _p;
    };

        /// <summary>Utility for initialising an intrusive_ptr with move semantics</summary>
        /// Sometimes we want to "move" a raw pointer into an intrusive pointer.
        /// For example, imagine we have a raw pointer to an object, and that object already
        /// has a reference count related to that raw pointer. We might want to construct
        /// an intrusive_ptr without taking a new reference. We can use moveptr to do that.
        /// <code>\code{.cpp}
        ///     T* someObject = new Object();       // constructed with ref count "1"
        ///     intrusive_ptr<T> ptr(moveptr(T));   // make intrusive_ptr without increasing ref count
        ///     // "someObject" is now nullptr
        /// \endcode</code>
        /// This happens sometimes when using 3rd party code. For example, using D3D, we
        /// often construct an object using a device method. The object is returned in raw
        /// pointer with an initial reference count already allocated. We want to "move" that
        /// raw pointer into an intrusive_ptr, without any extra AddRef/Release calls.
        ///
        /// Override intrusive_ptr_add_ref and intrusive_ptr_release to change the method
        /// of reference management for specific classes. By default, they will attempt
        /// to use the COM style method names of "AddRef" and "Release".
        ///
        /// Note -- there is no implicit conversion from instrusive_ptr<T> -> T*
        ///         this is following the pattern of std::shared_ptr, etc
        ///         use the get() method
        ///
        /// <seealso cref="intrusive_ptr_add_ref"/>
        /// <seealso cref="intrusive_ptr_release"/>
    templ MovePTRHelper<T> moveptr(T* p) { return MovePTRHelper<T>(p); }

        /// <summary>Standard intrusive pointer object</summary>
        /// Intrusive pointer implementation.
        /// based on standard designs (names kept similar to boost::intrusive_ptr intentially).
        /// Useful for interacting with 3rd party code (like D3D), or legacy code.
        /// Prefer to use std::shared_ptr<> where possible.
    templ class intrusive_ptr
    {
    public:
        typedef T element_type;

            // constructors / destructors
        intrusive_ptr() never_throws;
        intrusive_ptr(T* copyOrMoveFrom, bool takeNewReference = true);
        intrusive_ptr(MovePTRHelper<T> moveFrom) never_throws;
        intrusive_ptr(const intrusive_ptr& copyFrom);
        intrusive_ptr(intrusive_ptr&& moveFrom) never_throws;
        template<typename Y> intrusive_ptr(const intrusive_ptr<Y>& copyFrom);
        template<typename Y> intrusive_ptr(intrusive_ptr<Y>&& moveFrom) never_throws;
        ~intrusive_ptr();

            // assignment
        intrusive_ptr& operator=(const intrusive_ptr& copyFrom);
        template<typename Y> intrusive_ptr& operator=(const intrusive_ptr<Y>& copyFrom);
        intrusive_ptr& operator=(T* copyFrom);
        intrusive_ptr& operator=(MovePTRHelper<T> moveFrom) never_throws;
        intrusive_ptr& operator=(intrusive_ptr&& moveFrom) never_throws;
        template<typename Y> intrusive_ptr& operator=(intrusive_ptr<Y>&& moveFrom) never_throws;

            // reset
        void reset();
        void reset(T* copyFrom);

            // common accessors
        T& operator*() const never_throws;
        T* operator->() const never_throws;
        T* get() const never_throws;

            // cast for if(ptr) checks
        operator bool() const never_throws;

            // utilities and operators
        void swap(intrusive_ptr& b) never_throws;
        template<typename T2>
            friend T2* ReleaseOwnership(intrusive_ptr<T2>& ptr);

        template<typename T2, typename U>
            friend bool operator==(const intrusive_ptr<T2>& lhs, const intrusive_ptr<U>& rhs) never_throws;

        template<typename T2, typename U>
            friend bool operator!=(const intrusive_ptr<T2>& lhs, const intrusive_ptr<U>& rhs) never_throws;

        template<typename T2>
            friend bool operator==(const intrusive_ptr<T2>& a, T2* b) never_throws;

        template<typename T2>
            friend bool operator!=(const intrusive_ptr<T2>& a, T2* b) never_throws;

        template<typename T2>
            friend bool operator==(T2* a, const intrusive_ptr<T2>& b) never_throws;

        template<typename T2>
            friend bool operator!=(T2* a, const intrusive_ptr<T2>& b) never_throws;

        template<typename T2, typename U>
            friend bool operator<(const intrusive_ptr<T2>& a, const intrusive_ptr<U>& b) never_throws;

    private:
        T* _ptr;
    };

    templ void intrusive_ptr_add_ref(T* p) { p->AddRef(); }
    templ void intrusive_ptr_release(T* p) { p->Release(); }

    templ intrusive_ptr<T>::intrusive_ptr() never_throws : _ptr(nullptr) {}
    templ intrusive_ptr<T>::intrusive_ptr(T* copyOrMoveFrom, bool takeNewReference)
        : _ptr(copyOrMoveFrom)
    {
        if (_ptr && takeNewReference) {
            intrusive_ptr_add_ref(_ptr);
        }
    }
    templ intrusive_ptr<T>::intrusive_ptr(MovePTRHelper<T> moveFrom) never_throws : _ptr(moveFrom.Adopt()) {}
    templ intrusive_ptr<T>::intrusive_ptr(const intrusive_ptr& copyFrom)
        : _ptr(copyFrom._ptr)
    {
        if (_ptr) {
            intrusive_ptr_add_ref(_ptr);
        }
    }

    templ intrusive_ptr<T>::intrusive_ptr(intrusive_ptr&& moveFrom) never_throws
        : _ptr(nullptr)
    {
        swap(moveFrom);
    }

    templ template<typename Y> 
        intrusive_ptr<T>::intrusive_ptr(const intrusive_ptr<Y>& copyFrom)
        : _ptr(copyFrom.get())
    {
            // note -- maybe intrusive_ptr_add_ref could produce different result for T type 
            //          than Y type? We should add_ref with T type, because we'll release with
            //          that type as well.
        if (_ptr) {
            intrusive_ptr_add_ref(_ptr);
        }
    }

    templ template<typename Y> 
        intrusive_ptr<T>::intrusive_ptr(intrusive_ptr<Y>&& moveFrom) never_throws
        : _ptr(nullptr)
    {
        T* x = ReleaseOwnership(moveFrom);
        _ptr = x;
    }

    templ intrusive_ptr<T>::~intrusive_ptr()
    {
        if (_ptr) {
            intrusive_ptr_release(_ptr);
        }
    }

    templ intrusive_ptr<T>& intrusive_ptr<T>::operator=(const intrusive_ptr<T>& copyFrom)
    {
            //  should we compare "copyFrom" to "this" first? If they are equivalent,
            //  we get redundant AddRef/Release
        intrusive_ptr(copyFrom).swap(*this);
        return *this;
    }

    templ template<typename Y> intrusive_ptr<T>& intrusive_ptr<T>::operator=(const intrusive_ptr<Y>& copyFrom)
    {
        intrusive_ptr(copyFrom).swap(*this);
        return *this;
    }

    templ intrusive_ptr<T>& intrusive_ptr<T>::operator=(T* copyFrom)
    {
        intrusive_ptr(copyFrom).swap(*this);
        return *this;
    }

    templ intrusive_ptr<T>& intrusive_ptr<T>::operator=(MovePTRHelper<T> moveFrom) never_throws
    {
        intrusive_ptr(moveFrom).swap(*this);
        return *this;
    }

    templ intrusive_ptr<T>& intrusive_ptr<T>::operator=(intrusive_ptr<T>&& moveFrom) never_throws
    {
        intrusive_ptr(std::forward<intrusive_ptr<T>>(moveFrom)).swap(*this);
        return *this;
    }

    templ template<typename Y> intrusive_ptr<T>& intrusive_ptr<T>::operator=(intrusive_ptr<Y>&& moveFrom) never_throws
    {
        intrusive_ptr(std::forward<intrusive_ptr<Y>>(moveFrom)).swap(*this);
        return *this;
    }

    templ void intrusive_ptr<T>::reset()
    {
        intrusive_ptr().swap(*this);
    }

    templ void intrusive_ptr<T>::reset(T* copyFrom)
    {
        intrusive_ptr(copyFrom).swap(*this);
    }

    templ T& intrusive_ptr<T>::operator*() const never_throws           { return *_ptr; }
    templ T* intrusive_ptr<T>::operator->() const never_throws          { return _ptr; }
    templ T* intrusive_ptr<T>::get() const never_throws                 { return _ptr; }
    templ intrusive_ptr<T>::operator bool() const never_throws          { return _ptr != nullptr; }
    templ void intrusive_ptr<T>::swap(intrusive_ptr& b) never_throws    { std::swap(_ptr, b._ptr); }   // note that there's a potential for threading problems in std::swap

    templ T* ReleaseOwnership(intrusive_ptr<T>& ptr)
    {
        T* result = nullptr;
        std::swap(result, ptr._ptr);
        return result;
    }

    template<typename T, typename U>
        bool operator==(const intrusive_ptr<T>& lhs, const intrusive_ptr<U>& rhs) never_throws { return lhs._ptr == rhs._ptr; }

    template<typename T, typename U>
        bool operator!=(const intrusive_ptr<T>& lhs, const intrusive_ptr<U>& rhs) never_throws { return lhs._ptr != rhs._ptr; }

    templ bool operator==(const intrusive_ptr<T>& a, T* b) never_throws { return a._ptr == b; }
    templ bool operator!=(const intrusive_ptr<T>& a, T* b) never_throws { return a._ptr != b; }
    templ bool operator==(T* a, const intrusive_ptr<T>& b) never_throws { return a == b._ptr; }
    templ bool operator!=(T* a, const intrusive_ptr<T>& b) never_throws { return a != b._ptr; }

    template<typename T, typename U>
        bool operator<(const intrusive_ptr<T>& a, const intrusive_ptr<U>& b) never_throws { return a._ptr < b._ptr; }



        // todo -- implement variadic templates / perfect forwarding version of this
        //          (for compilers with full C++11 support)

    template <typename Result>
        intrusive_ptr<Result> make_intrusive() { return intrusive_ptr<Result>(moveptr(new Result())); }

    template <typename Result, typename X1>
        intrusive_ptr<Result> make_intrusive(X1 x1) { return intrusive_ptr<Result>(moveptr(new Result(std::forward<X1>(x1)))); }

    template <typename Result, typename X1, typename X2>
        intrusive_ptr<Result> make_intrusive(X1 x1, X2 x2) { return intrusive_ptr<Result>(moveptr(new Result(std::forward<X1>(x1), std::forward<X2>(x2)))); }

    template <typename Result, typename X1, typename X2, typename X3>
        intrusive_ptr<Result> make_intrusive(X1 x1, X2 x2, X3 x3) { return intrusive_ptr<Result>(moveptr(new Result(std::forward<X1>(x1), std::forward<X2>(x2), std::forward<X3>(x3)))); }

    template <typename Result, typename X1, typename X2, typename X3, typename X4>
        intrusive_ptr<Result> make_intrusive(X1 x1, X2 x2, X3 x3, X4 x4) { return intrusive_ptr<Result>(moveptr(new Result(std::forward<X1>(x1), std::forward<X2>(x2), std::forward<X3>(x3), std::forward<X4>(x4)))); }

    template <typename Result, typename X1, typename X2, typename X3, typename X4, typename X5>
        intrusive_ptr<Result> make_intrusive(X1 x1, X2 x2, X3 x3, X4 x4, X5 x5) { return intrusive_ptr<Result>(moveptr(new Result(std::forward<X1>(x1), std::forward<X2>(x2), std::forward<X3>(x3), std::forward<X4>(x4), std::forward<X5>(x5)))); }

    template <typename Result, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6>
        intrusive_ptr<Result> make_intrusive(X1 x1, X2 x2, X3 x3, X4 x4, X5 x5, X6 x6) { return intrusive_ptr<Result>(moveptr(new Result(std::forward<X1>(x1), std::forward<X2>(x2), std::forward<X3>(x3), std::forward<X4>(x4), std::forward<X5>(x5), std::forward<X6>(x6)))); }

    template <typename Result, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6, typename X7>
        intrusive_ptr<Result> make_intrusive(X1 x1, X2 x2, X3 x3, X4 x4, X5 x5, X6 x6, X7 x7) { return intrusive_ptr<Result>(moveptr(new Result(std::forward<X1>(x1), std::forward<X2>(x2), std::forward<X3>(x3), std::forward<X4>(x4), std::forward<X5>(x5), std::forward<X6>(x6), std::forward<X7>(x7)))); }

}

using namespace Utility;

namespace std
{
    templ void swap(Utility::intrusive_ptr<T>& a, Utility::intrusive_ptr<T>& b) never_throws { a.swap(b); }
}

#undef templ

