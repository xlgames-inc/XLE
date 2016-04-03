// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IThreadContext_Forward.h"
#include "../Core/Types.h"
#include <utility>
#include <memory>

namespace RenderCore 
{
	namespace GPUProfiler
	{
		class Profiler;

		struct ProfilerDestroyer { void operator()(const void* ptr); };
		typedef std::unique_ptr<Profiler, ProfilerDestroyer> Ptr;
		Ptr			CreateProfiler();

		void        Frame_Begin(IThreadContext& context, Profiler*profiler, unsigned frameID);
		void        Frame_End(IThreadContext& context, Profiler*profiler);

		enum EventType { Begin, End };
		void        TriggerEvent(IThreadContext& context, Profiler*profiler, const char name[], EventType type);

		std::pair<uint64, uint64> CalculateSynchronisation(IThreadContext& context, Profiler*profiler);

		typedef void (EventListener)(const void* eventBufferBegin, const void* eventBufferEnd);
		void        AddEventListener(EventListener* callback);
		void        RemoveEventListener(EventListener* callback);
	}
}

