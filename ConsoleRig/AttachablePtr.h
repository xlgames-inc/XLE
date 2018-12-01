// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/FunctionUtils.h"
#include <memory>

namespace ConsoleRig
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Obj>
        class AttachablePtr : std::shared_ptr<Obj>
    {
    public:
        Obj* get() { return std::shared_ptr<Obj>::get(); }
		Obj* operator->() { return std::shared_ptr<Obj>::operator->(); }
        operator bool() { return std::shared_ptr<Obj>::operator bool(); }
		Obj& operator*() { assert(get()); return *get(); }
		void reset();

        AttachablePtr();
        AttachablePtr(AttachablePtr&& moveFrom);
        AttachablePtr& operator=(AttachablePtr&& moveFrom);
		AttachablePtr(std::shared_ptr<Obj>&& moveFrom);
        AttachablePtr& operator=(std::shared_ptr<Obj>&& moveFrom);
        ~AttachablePtr();

        AttachablePtr(const AttachablePtr&) = delete;
        AttachablePtr& operator=(const AttachablePtr&) = delete;
    };

    template<typename Obj, typename... Args>
        AttachablePtr<Obj> MakeAttachablePtr(Args... a);

    template<typename Obj>
        AttachablePtr<Obj> GetAttachablePtr();

    class CrossModule
    {
    public:
        VariantFunctions _services;

        static CrossModule& GetInstance();
		static void SetInstance(CrossModule& crossModule);
		static void ReleaseInstance();
    private:
        static CrossModule* s_instance;

		template<typename Obj, typename... Args>
			friend AttachablePtr<Obj> MakeAttachablePtr(Args... a);

		template<typename Obj>
			friend AttachablePtr<Obj> GetAttachablePtr();

		template<typename Object> auto Get() -> std::shared_ptr<Object>;
        template<typename Object> void Publish(const std::shared_ptr<Object>& obj);
        template<typename Object> void Withhold(Object& obj);
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////
    
	template<typename Obj>
        AttachablePtr<Obj>::AttachablePtr(AttachablePtr&& moveFrom)
	: std::shared_ptr<Obj>(std::move(moveFrom))
    {
    }

    template<typename Obj>
        auto AttachablePtr<Obj>::operator=(AttachablePtr&& moveFrom)
            -> AttachablePtr&
    {
		if (moveFrom.get() != get()) {
			reset();
			std::shared_ptr<Obj>::operator=(std::move(moveFrom));
		}
        return *this;
    }

	template<typename Obj>
		AttachablePtr<Obj>::AttachablePtr(std::shared_ptr<Obj>&& moveFrom)
	: std::shared_ptr<Obj>(std::move(moveFrom))
	{
		if (get())
			Internal::TryAttachCurrentModule(*get());
	}

    template<typename Obj>
		AttachablePtr<Obj>& AttachablePtr<Obj>::operator=(std::shared_ptr<Obj>&& moveFrom)
	{
		// Note that if Internal::TryAttachCurrentModule throws an exception, this
		// pointer remains attached to whatever it was previously attached to
        operator=(AttachablePtr<Obj>(std::move(moveFrom)));
        return *this;
	}

    template<typename Obj>
        AttachablePtr<Obj>::AttachablePtr()
    {
	}

	template<typename Obj>
		void AttachablePtr<Obj>::reset()
	{
		if (get())
			Internal::TryDetachCurrentModule(*get());
		std::shared_ptr<Obj>::reset();
	}
    
    template<typename Obj>
        AttachablePtr<Obj>::~AttachablePtr()
    {
		reset();
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Object>
        auto CrossModule::Get() -> std::shared_ptr<Object>
    {
		std::weak_ptr<Object> res;
        if (_services.TryCall<std::weak_ptr<Object>>(res, typeid(Object).hash_code()))
			return res.lock();
		return nullptr;
    }

    template<typename Object>
        void CrossModule::Publish(const std::shared_ptr<Object>& obj)
    {
		std::weak_ptr<Object> weakPtr = obj;
        _services.Add(
            typeid(Object).hash_code(),
            [weakPtr]() { return weakPtr; });
    }

    template<typename Object>
        void CrossModule::Withhold(Object& obj)
    {
        _services.Remove(typeid(Object).hash_code());
    }

    template<typename Object, typename... Args>
        AttachablePtr<Object> MakeAttachablePtr(Args... a)
    {
        auto asSharedPtr = std::shared_ptr<Object>(
            new Object(std::forward<Args>(a)...),
            [](Object* obj) {
                // Withhold from the cross module list before we destroy
                CrossModule::GetInstance().Withhold(*obj);
                // Now just invoke the default deletor
                std::default_delete<Object>()(obj);
            });

		AttachablePtr<Object> result = std::shared_ptr<Object>(asSharedPtr);
        CrossModule::GetInstance().Publish(asSharedPtr);
        return std::move(result);
    }

    template<typename Obj>
        AttachablePtr<Obj> GetAttachablePtr()
    {
        return CrossModule::GetInstance().Get<Obj>();
    }

}

