#pragma once

#include "../../Core/SelectConfiguration.h"
#include "../../Core/Exceptions.h"

// IOS and OSX don't support the "thread_local" keyword, so we have to do it manually with
// pthreads. It's a bit of extra boilerplate / busycode
#if (PLATFORMOS_TARGET == PLATFORMOS_IOS) || (PLATFORMOS_TARGET ==  PLATFORMOS_OSX)

    #define FEATURE_THREAD_LOCAL_KEYWORD 0

    #include <pthread.h>

    namespace Utility
    {
        template<typename T>
            class thread_local_ptr
        {
        public:
            T* get();
            void set(T*);

            template<typename... Params>
                void allocate(Params... moveFrom);

            thread_local_ptr();
            ~thread_local_ptr();
        private:
            pthread_key_t _key = 0;

            static void DestructionFunction(void*);
        };

        template<typename T>
            T* thread_local_ptr<T>::get()
        {
            return (T*)pthread_getspecific(_key);
        }

        template<typename T>
            void thread_local_ptr<T>::set(T* newValue)
        {
            // We don't get an implicit delete of the previous value, so we must do it manually
            DestructionFunction(get());

            // Set the new value
            int success = pthread_setspecific(_key, newValue);
            if (success != 0)
                Throw(std::runtime_error("Failed calling pthread_setspecific to set thread local key"));
        }

        template<typename T>
            template<typename... Params>
                void thread_local_ptr<T>::allocate(Params... moveFrom)
        {
            set(new T(std::forward<Params...>(moveFrom...)));
        }

        template<typename T>
            thread_local_ptr<T>::thread_local_ptr()
        {
            int success = pthread_key_create(&_key, &DestructionFunction);
            if (success != 0)
                Throw(std::runtime_error("Failed calling pthread_key_create to create thread local pointer key"));
        }

        template<typename T>
            thread_local_ptr<T>::~thread_local_ptr()
        {
            pthread_key_delete(_key);
        }

        template<typename T>
            void thread_local_ptr<T>::DestructionFunction(void* ptr)
        {
            delete (T*)ptr;
        }

    }

#else

    #define FEATURE_THREAD_LOCAL_KEYWORD 1

#endif


