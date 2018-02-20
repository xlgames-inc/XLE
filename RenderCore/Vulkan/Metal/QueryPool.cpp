// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "QueryPool.h"
#include "IncludeVulkan.h"
#include "ObjectFactory.h"
#include "DeviceContext.h"
#include "../../../ConsoleRig/Log.h"

namespace RenderCore { namespace Metal_Vulkan
{
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
		context.GetActiveCommandList().WriteTimestamp(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, _timeStamps.get(), query);
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
			auto& cmdList = context.GetActiveCommandList();
			if (b._queryEnd < b._queryStart) {
				cmdList.ResetQueryPool(_timeStamps.get(), b._queryStart, _queryCount - b._queryStart);
				cmdList.ResetQueryPool(_timeStamps.get(), 0, b._queryEnd);
				_allocatedCount -= _queryCount - b._queryStart;
				_allocatedCount -= b._queryEnd;
			} else {
				cmdList.ResetQueryPool(_timeStamps.get(), b._queryStart, b._queryEnd - b._queryStart);
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
		assert(b._queryEnd != b._queryStart || _allocatedCount == 0);	// problems if we allocate all queries in a single frame currently
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
			} else if (b._queryEnd != b._queryStart) {
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
