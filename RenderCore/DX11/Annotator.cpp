// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../IAnnotator.h"
#include "../../Utility/Threading/Mutex.h"
#include "../../Core/Types.h"
#include <vector>
#include <deque>
#include <assert.h>

#include "Metal/QueryPool.h"
#include "Metal/DeviceContext.h"
#include "Metal/ObjectFactory.h"

namespace RenderCore { namespace ImplDX11
{
	namespace Metal = RenderCore::Metal_DX11;

	class AnnotatorImpl : public IAnnotator
	{
	public:
		void    Event(IThreadContext& context, const char name[], EventTypes::BitField types);
		void    Frame_Begin(IThreadContext& context, unsigned frameId);
		void    Frame_End(IThreadContext& context);
		void	FlushFinishedQueries(Metal::DeviceContext& context);

		std::pair<uint64, uint64> CalculateSynchronisation(Metal::DeviceContext& context);

		unsigned	AddEventListener(const EventListener& callback);
		void		RemoveEventListener(unsigned id);

		AnnotatorImpl(Metal::ObjectFactory&);
		~AnnotatorImpl();

	protected:
		struct EventInFlight
		{
			const char* _name;
			Metal::QueryPool::QueryId _queryIndex;
			EventTypes::Flags _type;
			unsigned _queryFrameId;
		};

		struct QueryFrame
		{
			Metal::QueryPool::FrameId _queryFrameId;
			unsigned _renderFrameId;
		};

		std::deque<EventInFlight> _eventsInFlight;
		std::deque<QueryFrame> _framesInFlight;

		Metal::QueryPool _queryPool;
		Metal::QueryPool::FrameId _currentQueryFrameId;

		unsigned _currentRenderFrameId;
		signed _frameRecursionDepth;

		Threading::Mutex _listeners_Mutex;
		std::vector<std::pair<unsigned, EventListener>> _listeners;
		unsigned _nextListenerId;
	};

	//////////////////////////////////////////////////////////////////

	void    AnnotatorImpl::Event(IThreadContext& context, const char name[], EventTypes::BitField types)
	{
		if (types & EventTypes::MarkerBegin) {
			Metal::GPUAnnotation::Begin(*Metal::DeviceContext::Get(context), name);
		} else if (types & EventTypes::MarkerEnd) {
			Metal::GPUAnnotation::End(*Metal::DeviceContext::Get(context));
		}

		if (!(types & (EventTypes::ProfileBegin|EventTypes::ProfileEnd)))
			return;

        if (_currentQueryFrameId == Metal::QueryPool::FrameId_Invalid)
            return;

		auto metalContext = Metal::DeviceContext::Get(context);
		EventInFlight newEvent;
		newEvent._name = name;
		newEvent._type = (EventTypes::Flags)types;
		newEvent._queryIndex = _queryPool.SetTimeStampQuery(*metalContext);
		newEvent._queryFrameId = _currentQueryFrameId;
		_eventsInFlight.push_back(newEvent);
	}

	void    AnnotatorImpl::Frame_Begin(IThreadContext&context, unsigned frameId)
	{
		++_frameRecursionDepth;
		if (_currentQueryFrameId != Metal::QueryPool::FrameId_Invalid || (_frameRecursionDepth>1)) {
			assert(_currentQueryFrameId != Metal::QueryPool::FrameId_Invalid && (_frameRecursionDepth>1));
			return;
		}

		auto metalContext = Metal::DeviceContext::Get(context);
		_currentQueryFrameId = _queryPool.BeginFrame(*metalContext);
		_currentRenderFrameId = frameId;
	}

	void    AnnotatorImpl::Frame_End(IThreadContext& context)
	{
		auto metalContext = Metal::DeviceContext::Get(context);

		--_frameRecursionDepth;
		if (_frameRecursionDepth == 0) {
			if (_currentQueryFrameId != Metal::QueryPool::FrameId_Invalid) {
				QueryFrame frameInFlight;
				frameInFlight._queryFrameId = _currentQueryFrameId;
				frameInFlight._renderFrameId = _currentRenderFrameId;
				_framesInFlight.push_back(frameInFlight);
				_queryPool.EndFrame(*metalContext, _currentQueryFrameId);

				_currentQueryFrameId = Metal::QueryPool::FrameId_Invalid;
				_currentRenderFrameId = ~unsigned(0);
			}
		}

		FlushFinishedQueries(*metalContext);
	}

	static size_t AsListenerType(IAnnotator::EventTypes::BitField types)
	{
		if (types & IAnnotator::EventTypes::ProfileEnd) return 1;
		return 0;
	}

	void AnnotatorImpl::FlushFinishedQueries(Metal::DeviceContext& context)
	{
		//
		//      Look for finished queries, and remove them from the
		//      "in-flight" list
		//

		while (!_framesInFlight.empty()) {
			QueryFrame& frameInFlight = *_framesInFlight.begin();
			auto results = _queryPool.GetFrameResults(context, frameInFlight._queryFrameId);
			if (!results._resultsReady) return;

			uint64 evntBuffer[2048 / sizeof(uint64)];
			byte* eventBufferPtr = (byte*)evntBuffer;
			const byte* eventBufferEnd = (const byte*)&evntBuffer[dimof(evntBuffer)];

			{
				ScopedLock(_listeners_Mutex);
				//      Write an event to set the frequency. We should expect the frequency should be constant
				//      in a single play through, but it doesn't hurt to keep recording it...
				const size_t entrySize = sizeof(size_t) * 2 + sizeof(uint64);
				if (size_t(eventBufferPtr) + entrySize > size_t(eventBufferEnd)) {
					for (auto i = _listeners.begin(); i != _listeners.end(); ++i) {
						(i->second)(evntBuffer, eventBufferPtr);
					}
					eventBufferPtr = (byte*)evntBuffer;
				}

				*((size_t*)eventBufferPtr) = ~size_t(0x0);                  eventBufferPtr += sizeof(size_t);
				*((size_t*)eventBufferPtr) = frameInFlight._renderFrameId;  eventBufferPtr += sizeof(size_t);
				*((uint64*)eventBufferPtr) = results._frequency;			eventBufferPtr += sizeof(uint64);
			}

			//
			//      We've sucessfully completed this "disjoint" query.
			//      The other queries related to this frame should be finished now.
			//      Let's get their data (though, if the disjoint flag is set, we'll ignore the data)
			//
			unsigned thisFrameId = frameInFlight._queryFrameId;
			while (!_eventsInFlight.empty() && _eventsInFlight.begin()->_queryFrameId == thisFrameId) {
				const auto& evnt = *_eventsInFlight.begin();
				auto timeResult = results._resultsStart[evnt._queryIndex];
				if (!results._isDisjoint) {
					ScopedLock(_listeners_Mutex);
					//
					//      Write an event into out buffer to represent this
					//      occurrence. If we can't fit it in; we need to flush it out 
					//      and continue on.
					//
					const size_t entrySize = sizeof(size_t) * 2 + sizeof(uint64);
					if (size_t(eventBufferPtr) + entrySize > size_t(eventBufferEnd)) {
						for (auto i = _listeners.begin(); i != _listeners.end(); ++i) {
							(i->second)(evntBuffer, eventBufferPtr);
						}
						eventBufferPtr = (byte*)evntBuffer;
					}
					*((size_t*)eventBufferPtr) = AsListenerType(evnt._type); eventBufferPtr += sizeof(size_t);
					*((size_t*)eventBufferPtr) = size_t(evnt._name); eventBufferPtr += sizeof(size_t);
					// assert(size_t(eventBufferPtr)%sizeof(uint64)==0);
					*((uint64*)eventBufferPtr) = uint64(timeResult); eventBufferPtr += sizeof(uint64);
				}

				_eventsInFlight.pop_front();
			}

			_framesInFlight.pop_front();

			//  Flush out any remaining entries in the event buffer...
			//  Note, this will insure that event if 2 frames worth of events
			//  complete in the single FlushFinishedQueries() call, we will never
			//  fill the event listener with a mixture of events from multiple frames.
			if (eventBufferPtr != (byte*)evntBuffer) {
				ScopedLock(_listeners_Mutex);
				for (auto i = _listeners.begin(); i != _listeners.end(); ++i) {
					(i->second)(evntBuffer, eventBufferPtr);
				}
			}
		}
	}

	std::pair<uint64, uint64> AnnotatorImpl::CalculateSynchronisation(Metal::DeviceContext& context)
	{
		//
		//      Calculate 2 time values (one for CPU, one for GPU) that approximately
		//      match.
		//
		std::pair<uint64, uint64> result(0, 0);
		#if 0
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
		#endif
		return result;
	}

	unsigned AnnotatorImpl::AddEventListener(const EventListener& callback)
	{
		ScopedLock(_listeners_Mutex);
		auto id = _nextListenerId++;
		_listeners.push_back(std::make_pair(id, callback));
		return id;
	}

	void AnnotatorImpl::RemoveEventListener(unsigned id)
	{
		ScopedLock(_listeners_Mutex);
		auto i = std::find_if(_listeners.begin(), _listeners.end(), 
			[id](const std::pair<unsigned, EventListener>& p) { return p.first == id; });
		if (i != _listeners.end())
			_listeners.erase(i);
	}

	AnnotatorImpl::AnnotatorImpl(Metal::ObjectFactory& factory)
	: _queryPool(factory)
	{
		_currentRenderFrameId = ~unsigned(0);
		_frameRecursionDepth = 0;
		_currentQueryFrameId = Metal::QueryPool::FrameId_Invalid;
		_nextListenerId = 0;
	}

	AnnotatorImpl::~AnnotatorImpl()
	{
	}

	std::unique_ptr<IAnnotator> CreateAnnotator(IDevice& device)
	{
		return std::make_unique<AnnotatorImpl>(Metal::GetObjectFactory(device));
	}

}}

