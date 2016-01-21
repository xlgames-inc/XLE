// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "GlobalServices.h"

namespace ConsoleRig
{
    template<typename Obj>
        AttachRef<Obj>::AttachRef(AttachRef&& moveFrom)
    {
        _isAttached = moveFrom._isAttached;
        _attachedServices = moveFrom._attachedServices;
        moveFrom._isAttached = false;
        moveFrom._attachedServices = nullptr;
    }

    template<typename Obj>
        auto AttachRef<Obj>::operator=(AttachRef&& moveFrom)
            -> AttachRef&
    {
        _isAttached = moveFrom._isAttached;
        _attachedServices = moveFrom._attachedServices;
        moveFrom._isAttached = false;
        moveFrom._attachedServices = nullptr;
        return *this;
    }

    template<typename Obj>
        AttachRef<Obj>::AttachRef()
    {
        _isAttached = false;
        _attachedServices = nullptr;
    }

    namespace Internal
    {
        template<typename T> struct HasAttachCurrentModule
        {
            template<typename U, void (U::*)()> struct FunctionSignature {};
            template<typename U> static std::true_type Test1(FunctionSignature<U, &U::AttachCurrentModule>*);
            template<typename U> static std::false_type Test1(...);
            static const bool Result = decltype(Test1<T>(0))::value;
        };

        template<typename Obj, typename std::enable_if<HasAttachCurrentModule<Obj>::Result>::type* = nullptr>
            static void TryAttachCurrentModule(Obj& obj)
        {
            obj.AttachCurrentModule();
        }

        template<typename Obj, typename std::enable_if<!HasAttachCurrentModule<Obj>::Result>::type* = nullptr>
            static void TryAttachCurrentModule(Obj& obj) {}
    }
    
    template<typename Obj>
        AttachRef<Obj>::AttachRef(Attachable<Obj>& obj)
    : _attachedServices(&obj)
    {
        ++_attachedServices->_attachReferenceCount;
        _isAttached = true;

        Internal::TryAttachCurrentModule(*_attachedServices->_object);
    }
    
    template<typename Obj>
        void AttachRef<Obj>::Detach()
    {
        if (_isAttached) {
            _attachedServices->_object->DetachCurrentModule();

            assert(_attachedServices->_attachReferenceCount > 0);
            --_attachedServices->_attachReferenceCount;
            _isAttached = false;
            _attachedServices = nullptr;
        }
    }

    template<typename Obj>
        Obj& AttachRef<Obj>::Get()
    {
        return *_attachedServices->_object;
    }

    template<typename Obj>
        AttachRef<Obj>::~AttachRef()
    {
        Detach();
    }
    
    template<typename Obj>
        auto Attachable<Obj>::Attach() -> AttachRef<Obj>
    {
        return AttachRef<Obj>(*this);
    }
    
    template<typename Obj>
        Attachable<Obj>::Attachable(Obj& obj)
            : _object(&obj)
    {
        _attachReferenceCount = 0;
        _mainAttachReference = Attach();
    }

    template<typename Obj>
        Attachable<Obj>::~Attachable() 
    {
        _mainAttachReference.Detach();
        assert(_attachReferenceCount == 0);
    }
}

