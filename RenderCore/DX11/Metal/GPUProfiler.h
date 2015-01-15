// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Core/Types.h"
#include <algorithm>

#define GPUANNOTATIONS_ENABLE

namespace RenderCore { namespace Metal_DX11
{
    class DeviceContext;

    namespace GPUProfiler
    {
        class Profiler;

        struct ProfilerDestroyer { void operator()(const void* ptr); };
        typedef std::unique_ptr<Profiler, ProfilerDestroyer> Ptr;
        Ptr   CreateProfiler();

        void        Frame_Begin(DeviceContext& context, Profiler*profiler, unsigned frameID);
        void        Frame_End(DeviceContext& context, Profiler*profiler);

        enum EventType { Begin, End };
        void        TriggerEvent(DeviceContext& context, Profiler*profiler, const char name[], EventType type);

        std::pair<uint64,uint64> CalculateSynchronisation(DeviceContext& context, Profiler*profiler);
        
        typedef void (EventListener)(const void* eventBufferBegin, const void* eventBufferEnd);
        void        AddEventListener(EventListener* callback);
        void        RemoveEventListener(EventListener* callback);

        #if defined(GPUANNOTATIONS_ENABLE)

                /// <summary>Add a debugging animation<summary>
                /// These annotations are used for debugging. They will create a marker in debugging
                /// tools (like nsight / RenderDoc). The "annotationName" can be any arbitrary name.
                /// Note that we're taking a wchar_t for the name. This is for DirectX, which needs
                /// a wide character string. But other tools (eg, OpenGL/Android) aren't guaranteed
                /// to work this way. We could do compile time conversion with a system of macros...
                /// Otherwise, we might need fall back to run time conversion.
            class DebugAnnotation
            {
            public:
                DebugAnnotation(DeviceContext& context, const wchar_t annotationName[]);
                ~DebugAnnotation();
            protected:
                DeviceContext* _context;
            };

        #else

            class DebugAnnotation
            {
            public:
                DebugAnnotation(DeviceContext&, const char[]) {}
            };

        #endif
    }

}}

