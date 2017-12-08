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
        _attachedService = moveFrom._attachedService;
        moveFrom._attachedService = nullptr;
    }

    template<typename Obj>
        auto AttachRef<Obj>::operator=(AttachRef&& moveFrom)
            -> AttachRef&
    {
        _attachedService = moveFrom._attachedService;
        moveFrom._attachedService = nullptr;
        return *this;
    }

    template<typename Obj>
        AttachRef<Obj>::AttachRef()
    {
        _attachedService = nullptr;
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

        template<typename Obj, typename std::enable_if<HasAttachCurrentModule<Obj>::Result>::type* = nullptr>
            static void TryDetachCurrentModule(Obj& obj)
        {
            obj.DetachCurrentModule();
        }

        template<typename Obj, typename std::enable_if<!HasAttachCurrentModule<Obj>::Result>::type* = nullptr>
            static void TryAttachCurrentModule(Obj& obj) {}

        template<typename Obj, typename std::enable_if<!HasAttachCurrentModule<Obj>::Result>::type* = nullptr>
            static void TryDetachCurrentModule(Obj& obj) {}
    }
    
    template<typename Obj>
        AttachRef<Obj>::AttachRef(Obj& obj)
    : _attachedService(&obj)
    {
        Internal::TryAttachCurrentModule(*_attachedService);
    }
    
    template<typename Obj>
        void AttachRef<Obj>::Detach()
    {
        if (_attachedService) {
            Internal::TryDetachCurrentModule(*_attachedService);
            _attachedService = nullptr;
        }
    }

    template<typename Obj>
        Obj& AttachRef<Obj>::Get()
    {
        if (!_attachedService)
            Throw(::Exceptions::BasicLabel("Attempting to get unattached object (of type %s) from AttachRef", typeid(Obj).name()));
        return *_attachedService->_object;
    }

    template<typename Obj>
        AttachRef<Obj>::~AttachRef()
    {
        Detach();
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Object>
        auto CrossModule::Attach() -> AttachRef<Object>
    {
        return AttachRef<Object>(*_services.Call<Object*>(typeid(Object).hash_code()));
    }

    template<typename Object>
        void CrossModule::Publish(Object& obj)
    {
        auto* attachable = &obj;
        _services.Add(
            typeid(Object).hash_code(),
            [attachable]() -> Object* { return attachable; });
    }

    template<typename Object>
        void CrossModule::Withhold(Object& obj)
    {
        Internal::TryDetachCurrentModule(obj);
        _services.Remove(typeid(Object).hash_code());
    }

    template<typename Object, typename... Args>
        std::shared_ptr<Object> CrossModule::CreateAndPublish(Args... a)
    {
        std::weak_ptr<CrossModule> weakPtrToThis = shared_from_this();
        auto result = std::shared_ptr<Object>(
            new Object(a...),
            [weakPtrToThis](Object* obj) {
                // Withhold from the cross module list before we destroy
                auto that = weakPtrToThis.lock();
                if (that) {
                    that->Withhold(*obj);
                }
                // Now just invoke the default deletor
                std::default_delete<Object>()(obj);
            });
        Publish(*result);
        Internal::TryAttachCurrentModule(*result);
        return result;
    }

}

