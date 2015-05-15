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
        Attachable<Obj>::AttachRef::AttachRef(AttachRef&& moveFrom)
    {
        _isAttached = moveFrom._isAttached;
        _attachedServices = moveFrom._attachedServices;
        moveFrom._isAttached = false;
        moveFrom._attachedServices = nullptr;
    }

    template<typename Obj>
        auto Attachable<Obj>::AttachRef::operator=(AttachRef&& moveFrom)
            -> AttachRef&
    {
        _isAttached = moveFrom._isAttached;
        _attachedServices = moveFrom._attachedServices;
        moveFrom._isAttached = false;
        moveFrom._attachedServices = nullptr;
        return *this;
    }

    template<typename Obj>
        Attachable<Obj>::AttachRef::AttachRef()
    {
        _isAttached = false;
        _attachedServices = nullptr;
    }

    template<typename Obj>
        Attachable<Obj>::AttachRef::AttachRef(Attachable<Obj>& obj)
    : _attachedServices(&obj)
    {
        ++_attachedServices->_attachReferenceCount;
        _isAttached = true;

        _attachedServices->_object->AttachCurrentModule();
    }
    
    template<typename Obj>
        void Attachable<Obj>::AttachRef::Detach()
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
        Obj& Attachable<Obj>::AttachRef::Get()
    {
        return *_attachedServices->_object;
    }

    template<typename Obj>
        Attachable<Obj>::AttachRef::~AttachRef()
    {
        Detach();
    }
    
    template<typename Obj>
        auto Attachable<Obj>::Attach() -> AttachRef 
    {
        return AttachRef(*this);
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

