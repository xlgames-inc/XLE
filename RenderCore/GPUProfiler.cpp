// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GPUProfiler.h"
#include "Metal/DeviceContext.h"
#include "Metal/GPUProfiler.h"
#include "../Utility/Threading/Mutex.h"
#include <vector>
#include <deque>

#if GFXAPI_TARGET == GFXAPI_DX11
	#include "DX11/Metal/IncludeDX11.h"
#endif

namespace RenderCore { namespace GPUProfiler
{
	using namespace RenderCore::Metal::GPUProfiler;

	class Profiler
	{
	public:
		void    TriggerEvent(Metal::DeviceContext& context, const char name[], EventType type);
		void    Frame_Begin(Metal::DeviceContext& context, unsigned frameId);
		void    Frame_End(Metal::DeviceContext& context);
		std::pair<uint64, uint64> CalculateSynchronisation(Metal::DeviceContext& context);

		Profiler();
		~Profiler();

	protected:
		struct EventInFlight
		{
			const char* _name;
			unsigned _queryIndex;
			EventType _type;
			unsigned _queryFrameId;
		};

		struct QueryFrame
		{
			unsigned _queryFrameId;
			unsigned _disjointQueryIndex;
			unsigned _renderFrameId;
			bool _disjointQueryHasCompleted;
			DisjointQueryData _completedQueryData;
		};

		std::deque<EventInFlight> _eventsInFlight;
		std::deque<QueryFrame> _framesInFlight;
		std::vector<Query> _queryPool;
		std::vector<Query> _disjointQueryPool;

		unsigned _currentQueryFrameId;
		unsigned _currentRenderFrameId;
		unsigned _currentDisjointQuery;
		unsigned _nextQueryFrameId;
		signed _frameRecursionDepth;

		void    FlushFinishedQueries(Metal::DeviceContext& context);

		static void Callback(const char name[], unsigned type);
		static Profiler* s_callbackProfiler;
	};

	// void Profiler::Callback(const char name[], unsigned type)
	// {
	//     assert(s_callbackProfiler);
	//     s_callbackProfiler->TriggerEvent(name, EventType(type));
	// }

	Profiler* Profiler::s_callbackProfiler = 0;
	Threading::Mutex g_globalEventListener_Mutex;
	std::vector<EventListener*> g_globalEventListener;

	//////////////////////////////////////////////////////////////////

	unsigned AllocateQuery(std::vector<Query>::iterator begin, std::vector<Query>::iterator end)
	{
		for (std::vector<Query>::iterator i = begin; i != end; ++i) {
			if (!i->_isAllocated) {
				i->_isAllocated = true;
				return unsigned(std::distance(begin, i));
			}
		}
		return ~unsigned(0);
	}

	void    Profiler::TriggerEvent(Metal::DeviceContext& context, const char name[], EventType type)
	{
		if (_currentQueryFrameId == ~unsigned(0))
			return;

		unsigned queryIndex = AllocateQuery(_queryPool.begin(), _queryPool.end());
		if (queryIndex >= _queryPool.size()) {
			// X2LogAlways("Warning -- Ran out of queries when triggering an event in GPU profiler.");
			return;
		}

		EventInFlight newEvent;
		newEvent._name = name;
		newEvent._type = type;
		newEvent._queryIndex = queryIndex;
		newEvent._queryFrameId = _currentQueryFrameId;
		_eventsInFlight.push_back(newEvent);

		EndQuery(context, _queryPool[queryIndex]);
	}

	void    Profiler::Frame_Begin(Metal::DeviceContext&context, unsigned frameId)
	{
		++_frameRecursionDepth;
		if (_currentQueryFrameId != ~unsigned(0) || _currentDisjointQuery != ~unsigned(0) || (_frameRecursionDepth>1)) {
			assert(_currentQueryFrameId != ~unsigned(0) && _currentDisjointQuery != ~unsigned(0) && (_frameRecursionDepth>1));
			return;
		}

		unsigned disjointQueryIndex = AllocateQuery(_disjointQueryPool.begin(), _disjointQueryPool.end());
		if (disjointQueryIndex >= _disjointQueryPool.size()) {
			// X2LogAlways("Warning -- Ran out of queries in begin frame in GPU profiler.");
			return;
		}

		BeginQuery(context, _disjointQueryPool[disjointQueryIndex]);

		_currentQueryFrameId = _nextQueryFrameId;
		_currentRenderFrameId = frameId;
		++_nextQueryFrameId;
		_currentDisjointQuery = disjointQueryIndex;
	}

	void    Profiler::Frame_End(Metal::DeviceContext& context)
	{
		--_frameRecursionDepth;
		if (_frameRecursionDepth == 0) {
			if (_currentQueryFrameId != ~unsigned(0) && _currentDisjointQuery != ~unsigned(0)) {
				EndQuery(context, _disjointQueryPool[_currentDisjointQuery]);

				QueryFrame frameInFlight;
				frameInFlight._queryFrameId = _currentQueryFrameId;
				frameInFlight._disjointQueryIndex = _currentDisjointQuery;
				frameInFlight._renderFrameId = _currentRenderFrameId;
				frameInFlight._disjointQueryHasCompleted = false;
				_framesInFlight.push_back(frameInFlight);

				_currentQueryFrameId = ~unsigned(0);
				_currentRenderFrameId = ~unsigned(0);
				_currentDisjointQuery = ~unsigned(0);
			}
			FlushFinishedQueries(context);
		}
	}

	void Profiler::FlushFinishedQueries(Metal::DeviceContext& context)
	{
		//
		//      Look for finished queries, and remove them from the
		//      "in-flight" list
		//

		bool continueLooping = true;
		while (continueLooping && !_framesInFlight.empty()) {
			QueryFrame& frameInFlight = *_framesInFlight.begin();
			DisjointQueryData& disjointData = frameInFlight._completedQueryData;

			if (!frameInFlight._disjointQueryHasCompleted) {
				unsigned firstDisjointQuery = _framesInFlight.begin()->_disjointQueryIndex;
				assert(firstDisjointQuery < _disjointQueryPool.size() && _disjointQueryPool[firstDisjointQuery]._isAllocated);

				auto hresult = GetDisjointData(context, _disjointQueryPool[firstDisjointQuery], disjointData);
				if (hresult) {
					frameInFlight._disjointQueryHasCompleted = true;

					//      Return the disjoint query to the pool... We don't
					//      need it anymore...
					_disjointQueryPool[firstDisjointQuery]._isAllocated = false;
					frameInFlight._disjointQueryIndex = ~unsigned(0);
				}
			}

			if (frameInFlight._disjointQueryHasCompleted) {
				uint64 evntBuffer[2048 / sizeof(uint64)];
				byte* eventBufferPtr = (byte*)evntBuffer;
				const byte* eventBufferEnd = (const byte*)&evntBuffer[dimof(evntBuffer)];

				{
					ScopedLock(g_globalEventListener_Mutex);
					//      Write an event to set the frequency. We should expect the frequency should be constant
					//      in a single play through, but it doesn't hurt to keep recording it...
					const size_t entrySize = sizeof(size_t) * 2 + sizeof(uint64);
					if (size_t(eventBufferPtr) + entrySize > size_t(eventBufferEnd)) {
						for (std::vector<EventListener*>::iterator i = g_globalEventListener.begin(); i != g_globalEventListener.end(); ++i) {
							(**i)(evntBuffer, eventBufferPtr);
						}
						eventBufferPtr = (byte*)evntBuffer;
					}

					*((size_t*)eventBufferPtr) = ~size_t(0x0);                  eventBufferPtr += sizeof(size_t);
					*((size_t*)eventBufferPtr) = frameInFlight._renderFrameId;  eventBufferPtr += sizeof(size_t);
					*((uint64*)eventBufferPtr) = disjointData.Frequency;        eventBufferPtr += sizeof(uint64);
				}

				//
				//      We've sucessfully completed this "disjoint" query.
				//      The other queries related to this frame should be finished now.
				//      Let's get their data (though, if the disjoint flag is set, we'll ignore the data)
				//
				unsigned thisFrameId = frameInFlight._queryFrameId;
				while (!_eventsInFlight.empty() && _eventsInFlight.begin()->_queryFrameId == thisFrameId) {
					EventInFlight & evnt = *_eventsInFlight.begin();
					uint64 timeResult = ~uint64(0);
					if (evnt._queryIndex < _queryPool.size()) {
						auto hresult = GetDataNoFlush(context, _queryPool[evnt._queryIndex], &timeResult, sizeof(timeResult));
						if (!hresult) {
							break;      // as soon as we get a failure, drop out... It's possible later ones have completed, but it would be strange
						}
						_queryPool[evnt._queryIndex]._isAllocated = false;
					}

					if (!disjointData.Disjoint) {
						ScopedLock(g_globalEventListener_Mutex);
						//
						//      Write an event into out buffer to represent this
						//      occurrence. If we can't fit it in; we need to flush it out 
						//      and continue on.
						//
						const size_t entrySize = sizeof(size_t) * 2 + sizeof(uint64);
						if (size_t(eventBufferPtr) + entrySize > size_t(eventBufferEnd)) {
							for (std::vector<EventListener*>::iterator i = g_globalEventListener.begin(); i != g_globalEventListener.end(); ++i) {
								(**i)(evntBuffer, eventBufferPtr);
							}
							eventBufferPtr = (byte*)evntBuffer;
						}
						*((size_t*)eventBufferPtr) = size_t(evnt._type); eventBufferPtr += sizeof(size_t);
						*((size_t*)eventBufferPtr) = size_t(evnt._name); eventBufferPtr += sizeof(size_t);
						// assert(size_t(eventBufferPtr)%sizeof(uint64)==0);
						*((uint64*)eventBufferPtr) = uint64(timeResult); eventBufferPtr += sizeof(uint64);
					}

					_eventsInFlight.pop_front();
				}

				//      if we've popped off every event related to this query frame id, it's ok
				//      to remove the frame in flight as well...
				if (_eventsInFlight.empty() || _eventsInFlight.begin()->_queryFrameId != thisFrameId) {
					_framesInFlight.pop_front();
				}
				else {
					continueLooping = false;        // we didn't fulling complete this one... we can't go further this time
				}

				//  Flush out any remaining entries in the event buffer...
				//  Note, this will insure that event if 2 frames worth of events
				//  complete in the single FlushFinishedQueries() call, we will never
				//  fill the event listener with a mixture of events from multiple frames.
				if (eventBufferPtr != (byte*)evntBuffer) {
					ScopedLock(g_globalEventListener_Mutex);
					for (std::vector<EventListener*>::iterator i = g_globalEventListener.begin(); i != g_globalEventListener.end(); ++i) {
						(**i)(evntBuffer, eventBufferPtr);
					}
				}
			}
			else {
				continueLooping = false;
			}
		}
	}

	std::pair<uint64, uint64> Profiler::CalculateSynchronisation(Metal::DeviceContext& context)
	{
		//
		//      Calculate 2 time values (one for CPU, one for GPU) that approximately
		//      match.
		//
		std::pair<uint64, uint64> result(0, 0);
		unsigned disjointQueryIndex = AllocateQuery(_disjointQueryPool.begin(), _disjointQueryPool.end());
		unsigned queryIndex = AllocateQuery(_queryPool.begin(), _queryPool.end());
		if (disjointQueryIndex > _disjointQueryPool.size() || queryIndex > _queryPool.size()) {
			// X2LogAlways("Warning -- Ran out of queries while calculating synchronisation.");
			return result;
		}

		Query& d3dDisjoint = _disjointQueryPool[disjointQueryIndex];
		Query& d3dQuery = _queryPool[queryIndex];

		result = Metal::GPUProfiler::CalculateSynchronisation(context, d3dQuery, d3dDisjoint);

		_queryPool[queryIndex]._isAllocated = false;
		_disjointQueryPool[disjointQueryIndex]._isAllocated = false;

		return result;
	}

	Profiler::Profiler()
	{
		//
		//      Allocate the queries we'll need later one
		//      Generally, we'll need more queries the faster 
		//      the frame rate, and the more events per frame.
		//
		const unsigned QueryPoolSize = 96;
		const unsigned DisjointQueryPoolSize = 4;

		std::vector<Query> queryPool; queryPool.reserve(QueryPoolSize);
		std::vector<Query> disjointQueryPool; disjointQueryPool.reserve(DisjointQueryPoolSize);

		///////////////////////////////////////~~~///////////////////////////////////////
		for (unsigned c = 0; c<DisjointQueryPoolSize; ++c) {
			disjointQueryPool.push_back(CreateQuery(true));
		}

		for (unsigned c = 0; c<QueryPoolSize; ++c) {
			queryPool.push_back(CreateQuery(false));
		}

		_currentQueryFrameId = ~unsigned(0);
		_currentRenderFrameId = ~unsigned(0);
		_currentDisjointQuery = ~unsigned(0);
		_frameRecursionDepth = 0;
		_nextQueryFrameId = 0;

		_queryPool = std::move(queryPool);
		_disjointQueryPool = std::move(disjointQueryPool);

		assert(s_callbackProfiler == NULL);
		s_callbackProfiler = this;
	}

	Profiler::~Profiler()
	{
		assert(s_callbackProfiler == this);
		s_callbackProfiler = NULL;
	}

	Ptr CreateProfiler()
	{
		TRY{
			return Ptr(new Profiler);
		} CATCH(const QueryConstructionFailure&) {
			return nullptr;
		} CATCH_END
	}

	void ProfilerDestroyer::operator()(const void* ptr)
	{
		delete (const Profiler*)ptr;
	}

	void Frame_Begin(IThreadContext& context, Profiler*profiler, unsigned frameID)
	{
		if (profiler) {
			auto metalContext = Metal::DeviceContext::Get(context);
			profiler->Frame_Begin(*metalContext, frameID);
		}
	}

	void Frame_End(IThreadContext& context, Profiler*profiler)
	{
		if (profiler) {
			auto metalContext = Metal::DeviceContext::Get(context);
			profiler->Frame_End(*metalContext);
		}
	}

	void TriggerEvent(IThreadContext& context, Profiler*profiler, const char name[], EventType type)
	{
		if (profiler) {
			auto metalContext = Metal::DeviceContext::Get(context);
			profiler->TriggerEvent(*metalContext, name, type);
		}
	}

	std::pair<uint64, uint64> CalculateSynchronisation(IThreadContext& context, Profiler*profiler)
	{
		if (profiler) {
			auto metalContext = Metal::DeviceContext::Get(context);
			return profiler->CalculateSynchronisation(*metalContext);
		} else {
			return std::pair<uint64, uint64>(0, 0);
		}
	}

	void AddEventListener(EventListener* callback)
	{
		ScopedLock(g_globalEventListener_Mutex);
		if (std::find(g_globalEventListener.begin(), g_globalEventListener.end(), callback) == g_globalEventListener.end()) {
			g_globalEventListener.push_back(callback);
		}
	}

	void RemoveEventListener(EventListener* callback)
	{
		ScopedLock(g_globalEventListener_Mutex);
		std::vector<EventListener*>::iterator i = std::find(g_globalEventListener.begin(), g_globalEventListener.end(), callback);
		if (i != g_globalEventListener.end()) {
			g_globalEventListener.erase(i);
		}
	}

}}

