// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define FLEX_CONTEXT_Annotator				FLEX_CONTEXT_CONCRETE

#include "GPUProfiler.h"
#include "../Utility/Threading/Mutex.h"
#include "../Core/Types.h"
#include <vector>
#include <deque>
#include <assert.h>

// #define SELECT_VULKAN

#include "Metal/Metal.h"
#include "DX11/Metal/GPUProfiler.h"
#include "DX11/Metal/IncludeDX11.h"
#include "DX11/Metal/ObjectFactory.h"
#include "DX11/Metal/DeviceContext.h"

#include "Vulkan/Metal/IncludeVulkan.h"
#include "Vulkan/Metal/ObjectFactory.h"
#include "Vulkan/Metal/DeviceContext.h"
#include "../ConsoleRig/Log.h"

namespace RenderCore { namespace Metal_Vulkan
{
	class DeviceContext;
	class ObjectFactory;

	class QueryPool
	{
	public:
		using QueryId = unsigned;
		using FrameId = unsigned;

		static const QueryId QueryId_Invalid = ~0u;
		static const FrameId FrameId_Invalid = ~0u;

		QueryId		SetTimeStampQuery(DeviceContext& context);

		FrameId		BeginFrame(DeviceContext& context);
		void		EndFrame(DeviceContext& context, FrameId frame);

		struct FrameResults
		{
			bool		_resultsReady;
			bool		_isDisjoint;
			uint64*		_resultsStart;
			uint64*		_resultsEnd;
			uint64		_frequency;
		};
		FrameResults GetFrameResults(DeviceContext& context, FrameId id);

		QueryPool(ObjectFactory& factory);
		~QueryPool();

		QueryPool(const QueryPool&) = delete;
		QueryPool& operator=(const QueryPool&) = delete;
	private:
		static const unsigned s_bufferCount = 3u;
		VulkanUniquePtr<VkQueryPool> _timeStamps;
		unsigned _nextAllocation;
		unsigned _nextFree;
		unsigned _allocatedCount;

		class Buffer
		{
		public:
			FrameId		_frameId;
			bool		_pendingReadback;
			bool		_pendingReset;
			unsigned	_queryStart;
			unsigned	_queryEnd;
		};
		Buffer		_buffers[s_bufferCount];
		unsigned	_activeBuffer;
		FrameId		_nextFrameId;

		VkDevice	_device;
		unsigned	_queryCount;
		uint64		_frequency;
		std::unique_ptr<uint64[]> _timestampsBuffer;
	};

	auto QueryPool::SetTimeStampQuery(DeviceContext& context) -> QueryId
	{
		// Attempt to allocate a query from the bit heap
		// Note that if we run out of queries, there is a way to reuse hardware queries
		//		.. we just copy the results using vkCmdCopyQueryPoolResults and then
		//		reset the bit heap. Later on we can lock and read from the buffer to
		//		get the results.
		if (_nextAllocation == _nextFree && _allocatedCount!=0)
			return QueryId_Invalid;		// (we could also look for any buffers that are pending free)
		auto query = _nextAllocation;
		context.CmdWriteTimestamp(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, _timeStamps.get(), query);
		_nextAllocation = (_nextAllocation + 1) % _queryCount;
		_allocatedCount = std::min(_allocatedCount+1, _queryCount);
		return query;
	}

	auto QueryPool::BeginFrame(DeviceContext& context) -> FrameId
	{
		auto& b = _buffers[_activeBuffer];
		if (b._pendingReadback) {
			LogWarning << "Query pool eating it's tail. Insufficient buffers.";
			return FrameId_Invalid;
		}
		if (b._pendingReset) {
			if (b._queryEnd < b._queryStart) {
				context.CmdResetQueryPool(_timeStamps.get(), b._queryStart, _queryCount - b._queryStart);
				context.CmdResetQueryPool(_timeStamps.get(), 0, b._queryEnd);
				_allocatedCount -= _queryCount - b._queryStart;
				_allocatedCount -= b._queryEnd;
			} else {
				context.CmdResetQueryPool(_timeStamps.get(), b._queryStart, b._queryEnd - b._queryStart);
				_allocatedCount -= b._queryEnd - b._queryStart;
			}
			assert(_nextFree == b._queryStart);
			assert(_allocatedCount >= 0 && _allocatedCount <= _queryCount);
			_nextFree = b._queryEnd;
			b._frameId = FrameId_Invalid;
		}
		assert(b._frameId == FrameId_Invalid);
		b._frameId = _nextFrameId;
		b._queryStart = _nextAllocation;
		++_nextFrameId;
		return b._frameId;
	}

	void QueryPool::EndFrame(DeviceContext& context, FrameId frame)
	{
		auto& b = _buffers[_activeBuffer];
		b._pendingReadback = true;
		b._queryEnd = _nextAllocation;
		assert(b._queryEnd != b._queryStart);	// problems if we allocate all queries in a single frame currently
		// roll forward to the next buffer
		_activeBuffer = (_activeBuffer + 1) % s_bufferCount;
	}

	auto QueryPool::GetFrameResults(DeviceContext& context, FrameId id) -> FrameResults
	{
		// Attempt to read the results from the query pool for the given frame.
		// The queries are completed asynchronously, so the results may not be available yet.
		// When the results are not available, just return nullptr

		for (unsigned c = 0; c < s_bufferCount; ++c) {
			auto& b = _buffers[c];
			if (b._frameId != id || !b._pendingReadback)
				continue;

			// Requesting 64 bit timestamps on all hardware. We can also
			// check the timestamp size by calling VkGetPhysicalDeviceProperties
			// Our query buffer is circular, so we may need to wrap around to the start
			if (b._queryEnd < b._queryStart) {
				unsigned firstPartCount = _queryCount - b._queryStart;
				auto res = vkGetQueryPoolResults(
					_device, _timeStamps.get(),
					b._queryStart, firstPartCount,
					sizeof(uint64)*firstPartCount,
					&_timestampsBuffer[b._queryStart], sizeof(uint64),
					VK_QUERY_RESULT_64_BIT);
				if (res == VK_NOT_READY)
					return FrameResults{ false };
				if (res != VK_SUCCESS)
					Throw(VulkanAPIFailure(res, "Failed while retrieving query pool results"));

				res = vkGetQueryPoolResults(
					_device, _timeStamps.get(),
					0, b._queryEnd,
					sizeof(uint64)*b._queryEnd,
					_timestampsBuffer.get(), sizeof(uint64),
					VK_QUERY_RESULT_64_BIT);
				if (res == VK_NOT_READY)
					return FrameResults{ false };
				if (res != VK_SUCCESS)
					Throw(VulkanAPIFailure(res, "Failed while retrieving query pool results"));
			} else {
				auto res = vkGetQueryPoolResults(
					_device, _timeStamps.get(),
					b._queryStart, b._queryEnd - b._queryStart, 
					sizeof(uint64)*(b._queryEnd - b._queryStart),
					&_timestampsBuffer[b._queryStart], sizeof(uint64),
					VK_QUERY_RESULT_64_BIT);

				// we should frequently get "not ready" -- this means the query hasn't completed yet.
				if (res == VK_NOT_READY) 
					return FrameResults{false};
				if (res != VK_SUCCESS)
					Throw(VulkanAPIFailure(res, "Failed while retrieving query pool results"));
			}

			// Succesfully retrieved results for all queries. We can reset the pool
			b._pendingReadback = false;
			b._pendingReset = true;
			return FrameResults{true, false,
				_timestampsBuffer.get(), &_timestampsBuffer[_queryCount],
				_frequency};
		}

		// couldn't find any pending results for this frame
		return FrameResults{false};
	}

	QueryPool::QueryPool(ObjectFactory& factory) 
	{
		_queryCount = 96;
		_activeBuffer = 0;
		_nextFrameId = 0;
		_device = factory.GetDevice().get();
		_timestampsBuffer = std::make_unique<uint64[]>(_queryCount);
		_timeStamps = factory.CreateQueryPool(VK_QUERY_TYPE_TIMESTAMP, _queryCount);
		_nextAllocation = _nextFree = _allocatedCount = 0;

		for (unsigned c=0; c<s_bufferCount; ++c) {
			_buffers[c]._frameId = FrameId_Invalid;
			_buffers[c]._pendingReadback = false;
			_buffers[c]._pendingReset = false;
			_buffers[c]._queryStart = _buffers[c]._queryEnd = 0;
		}

		VkPhysicalDeviceProperties physDevProps = {};
		vkGetPhysicalDeviceProperties(factory.GetPhysicalDevice(), &physDevProps);
		auto nanosecondsPerTick = physDevProps.limits.timestampPeriod;
		// awkwardly, DX uses frequency while Vulkan uses period. We have to use a divide somewhere to convert
		_frequency = uint64(1e9f / nanosecondsPerTick);
	}

	QueryPool::~QueryPool() {}
}}

namespace RenderCore { namespace Metal_DX11
{
	class DeviceContext;
	class ObjectFactory;
	using namespace GPUProfiler;

	class QueryPool
	{
	public:
		using QueryId = unsigned;
		using FrameId = unsigned;

		static const QueryId QueryId_Invalid = ~0u;
		static const FrameId FrameId_Invalid = ~0u;

		QueryId		SetTimeStampQuery(DeviceContext& context);

		FrameId		BeginFrame(DeviceContext& context);
		void		EndFrame(DeviceContext& context, FrameId frame);

		struct FrameResults
		{
			bool		_resultsReady;
			bool		_isDisjoint;
			uint64*		_resultsStart;
			uint64*		_resultsEnd;
			uint64		_frequency;
		};
		FrameResults GetFrameResults(DeviceContext& context, FrameId id);

		QueryPool(ObjectFactory& factory);
		~QueryPool();
	private:
		std::unique_ptr<intrusive_ptr<ID3D::Query>[]> _timeStamps;
		unsigned _nextAllocation;
		unsigned _nextFree;
		unsigned _allocatedCount;
		unsigned _queryCount;

		static const unsigned s_bufferCount = 3u;
		class Buffer
		{
		public:
			FrameId		_frameId;
			bool		_pendingReadback;
			bool		_pendingReset;
			unsigned	_queryStart;
			unsigned	_queryEnd;
			intrusive_ptr<ID3D::Query> _disjointQuery;
		};
		Buffer		_buffers[s_bufferCount];
		unsigned	_activeBuffer;
		FrameId		_nextFrameId;

		std::unique_ptr<uint64[]> _timestampsBuffer;

		void    FlushFinishedQueries(DeviceContext& context);
	};

	auto QueryPool::SetTimeStampQuery(DeviceContext& context) -> QueryId
	{
		if (_nextAllocation == _nextFree && _allocatedCount != 0)
			return QueryId_Invalid;		// (we could also look for any buffers that are pending free)
		auto query = _nextAllocation;
		EndQuery(context, *_timeStamps[query]);
		_nextAllocation = (_nextAllocation + 1) % _queryCount;
		_allocatedCount = std::min(_allocatedCount + 1, _queryCount);
		return query;
	}

	auto QueryPool::BeginFrame(DeviceContext& context) -> FrameId
	{
		auto& b = _buffers[_activeBuffer];
		if (b._pendingReadback) {
			LogWarning << "Query pool eating it's tail. Insufficient buffers.";
			return FrameId_Invalid;
		}
		if (b._pendingReset) {
			if (b._queryEnd < b._queryStart) {
				_allocatedCount -= _queryCount - b._queryStart;
				_allocatedCount -= b._queryEnd;
			} else {
				_allocatedCount -= b._queryEnd - b._queryStart;
			}
			assert(_nextFree == b._queryStart);
			assert(_allocatedCount >= 0 && _allocatedCount <= _queryCount);
			_nextFree = b._queryEnd;
			b._frameId = FrameId_Invalid;
		}
		assert(b._frameId == FrameId_Invalid);
		b._frameId = _nextFrameId;
		b._queryStart = _nextAllocation;
		BeginQuery(context, *b._disjointQuery);
		++_nextFrameId;
		return b._frameId;
	}

	void QueryPool::EndFrame(DeviceContext& context, FrameId frame)
	{
		auto& b = _buffers[_activeBuffer];
		b._pendingReadback = true;
		b._queryEnd = _nextAllocation;
		assert(b._queryEnd != b._queryStart);	// problems if we allocate all queries in a single frame currently
		EndQuery(context, *b._disjointQuery);
		// roll forward to the next buffer
		_activeBuffer = (_activeBuffer + 1) % s_bufferCount;
	}

	auto QueryPool::GetFrameResults(DeviceContext& context, FrameId id) -> FrameResults
	{
		for (unsigned c = 0; c < s_bufferCount; ++c) {
			auto& b = _buffers[c];
			if (b._frameId != id || !b._pendingReadback)
				continue;

			// If we can get the disjoint data, we can assume that we can also get
			// the timestamp query data.
			DisjointQueryData disjointData;
			if (GetDisjointData(context, *b._disjointQuery, disjointData)) {
				// Let's query every timestamp query to get the associated data
				bool gotFailureReadingQuery = false;
				unsigned q = b._queryStart;
				for (; q!=b._queryEnd && q!=_queryCount; ++q) {
					auto hresult = GetDataNoFlush(context, *_timeStamps[q], &_timestampsBuffer[q], sizeof(uint64));
					if (!hresult) {
						_timestampsBuffer[q] = ~0x0ull;
						gotFailureReadingQuery = true;
					}
				}
				if (q > b._queryEnd) {
					for (q=0; q!=b._queryEnd; ++q) {
						auto hresult = GetDataNoFlush(context, *_timeStamps[q], &_timestampsBuffer[q], sizeof(uint64));
						if (!hresult) {
							_timestampsBuffer[q] = ~0x0ull;
							gotFailureReadingQuery = true;
						}
					}
				}

				// Now we can release all of the allocated queries.
				// (actually, we could release them here -- but to follow the pattern used with the Vulkan implementation,
				// we'll do it in BeginFrame)
				b._pendingReadback = false;
				b._pendingReset = true;
				return FrameResults{
					true, !!disjointData.Disjoint,
					_timestampsBuffer.get(), &_timestampsBuffer[_queryCount],
					disjointData.Frequency};
			}
		}

		// either couldn't find this frame, or results not ready yet
		return FrameResults{false};
	}

	QueryPool::QueryPool(ObjectFactory& factory)
	{
		_queryCount = 96;
		_activeBuffer = 0;
		_nextFrameId = 0;
		_timestampsBuffer = std::make_unique<uint64[]>(_queryCount);
		_timeStamps = std::make_unique<intrusive_ptr<ID3D::Query>[]>(_queryCount);
		for (unsigned c=0; c<_queryCount; ++c)
			_timeStamps[c] = CreateQuery(factory, false);
		_nextAllocation = _nextFree = _allocatedCount = 0;

		for (unsigned c = 0; c<s_bufferCount; ++c) {
			_buffers[c]._frameId = FrameId_Invalid;
			_buffers[c]._pendingReadback = false;
			_buffers[c]._pendingReset = false;
			_buffers[c]._queryStart = _buffers[c]._queryEnd = 0;
			_buffers[c]._disjointQuery = CreateQuery(factory, true);
		}
	}

	QueryPool::~QueryPool() {}
}}

namespace RenderCore
{
	class AnnotatorImpl : public Base_Annotator
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

}

