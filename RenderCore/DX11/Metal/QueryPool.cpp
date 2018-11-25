// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "QueryPool.h"
#include "DX11.h"
#include "DeviceContext.h"
#include "ObjectFactory.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Utility/Threading/Mutex.h"
#include "../../../Core/Exceptions.h"
#include "IncludeDX11.h"
#include <deque>
#include <vector>

namespace RenderCore { namespace Metal_DX11
{
    bool GetDataNoFlush(DeviceContext& context, ID3D::Query& query, void * destination, unsigned destinationSize)
    {
        auto hresult = context.GetUnderlying()->GetData(
            &query, destination, destinationSize, D3D11_ASYNC_GETDATA_DONOTFLUSH );
		return hresult == S_OK;
    }

    bool GetDisjointData(DeviceContext& context, ID3D::Query& query, D3D11_QUERY_DATA_TIMESTAMP_DISJOINT& destination)
    {
        return GetDataNoFlush(context, query, &destination, sizeof(destination));
    }

    intrusive_ptr<ID3D::Query> CreateQuery(ObjectFactory& factory, bool disjoint)
    {
        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query      = disjoint?D3D11_QUERY_TIMESTAMP_DISJOINT:D3D11_QUERY_TIMESTAMP;
        queryDesc.MiscFlags  = 0;
        return factory.CreateQuery(&queryDesc);
    }

    void BeginQuery(DeviceContext& context, ID3D::Query& query)
    {
        context.GetUnderlying()->Begin(&query);
    }

    void EndQuery(DeviceContext& context, ID3D::Query& query)
    {
        context.GetUnderlying()->End(&query);
    }

	std::pair<uint64, uint64> CalculateSynchronisation(DeviceContext& context, ID3D::Query& query, ID3D::Query& disjoint)
	{
		// 
		//      Do the query 3 times... We should be fully synchronised by that
		//      point.
		//
		std::pair<uint64, uint64> result(0, 0);
		for (unsigned c = 0; c<3; ++c) {

			BeginQuery(context, disjoint);
			EndQuery(context, query);
			EndQuery(context, disjoint);

			//      Note, we can't use the normal GetDataNoFlush interfaces here -- because we actually do a need a flush!
			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
			HRESULT hresult = 0;
			do {
				hresult = context.GetUnderlying()->GetData(&disjoint, &disjointData, sizeof(disjointData), 0);
			} while (hresult != S_OK);

			context.GetUnderlying()->Flush();
			QueryPerformanceCounter((LARGE_INTEGER*)&result.first);

			if (!disjointData.Disjoint) {       // (it's possible that every result is disjoint... eg, if the device is unplugged, etc). In this case, we'll just get back bad data
				do {
					hresult = context.GetUnderlying()->GetData(&query, &result.second, sizeof(result.second), 0);
				} while (hresult != S_OK);
			}
		}

		return result;
	}

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
			Log(Warning) << "Query pool eating it's tail. Insufficient buffers." << std::endl;
			return FrameId_Invalid;
		}
		if (b._pendingReset) {
			assert(b._queryEnd != ~0u);
			if (b._queryEnd < b._queryStart) {
				_allocatedCount -= _queryCount - b._queryStart;
				_allocatedCount -= b._queryEnd;
			}
			else {
				_allocatedCount -= b._queryEnd - b._queryStart;
			}
			assert(_nextFree == b._queryStart);
			assert(_allocatedCount >= 0 && _allocatedCount <= _queryCount);
			_nextFree = b._queryEnd;
			b._frameId = FrameId_Invalid;
		}
		assert(!b._pendingReadback);
		assert(b._frameId == FrameId_Invalid);
		b._frameId = _nextFrameId;
		b._queryStart = _nextAllocation;
		b._queryEnd = ~0u;
		BeginQuery(context, *b._disjointQuery);
		++_nextFrameId;
		return b._frameId;
	}

	void QueryPool::EndFrame(DeviceContext& context, FrameId frame)
	{
		auto& b = _buffers[_activeBuffer];
		b._pendingReadback = true;
		b._queryEnd = _nextAllocation;
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

			assert(b._queryEnd != ~0u);

			// If we can get the disjoint data, we can assume that we can also get
			// the timestamp query data.
			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
			if (GetDisjointData(context, *b._disjointQuery, disjointData)) {
				// Let's query every timestamp query to get the associated data
				bool gotFailureReadingQuery = false;
				unsigned q = b._queryStart;
				for (; q != b._queryEnd && q != _queryCount; ++q) {
					auto success = GetDataNoFlush(context, *_timeStamps[q], &_timestampsBuffer[q], sizeof(uint64));
					if (!success) {
						_timestampsBuffer[q] = ~0x0ull;
						gotFailureReadingQuery = true;
					}
				}
				if (q > b._queryEnd) {
					for (q = 0; q != b._queryEnd; ++q) {
						auto success = GetDataNoFlush(context, *_timeStamps[q], &_timestampsBuffer[q], sizeof(uint64));
						if (!success) {
							_timestampsBuffer[q] = ~0x0ull;
							gotFailureReadingQuery = true;
						}
					}
				}

				if (!gotFailureReadingQuery) {
					// Now we can release all of the allocated queries.
					// (actually, we could release them here -- but to follow the pattern used with the Vulkan implementation,
					// we'll do it in BeginFrame)
					b._pendingReadback = false;
					b._pendingReset = true;
					return FrameResults{
						true, !!disjointData.Disjoint,
						_timestampsBuffer.get(), &_timestampsBuffer[_queryCount],
						disjointData.Frequency };
				}
			}
		}

		// either couldn't find this frame, or results not ready yet
		return FrameResults{ false };
	}

	QueryPool::QueryPool(ObjectFactory& factory)
	{
		_queryCount = 96;
		_activeBuffer = 0;
		_nextFrameId = 0;
		_timestampsBuffer = std::make_unique<uint64[]>(_queryCount);
		_timeStamps = std::make_unique<intrusive_ptr<ID3D::Query>[]>(_queryCount);
		for (unsigned c = 0; c<_queryCount; ++c)
			_timeStamps[c] = CreateQuery(factory, false);
		_nextAllocation = _nextFree = _allocatedCount = 0;

		for (unsigned c = 0; c<s_bufferCount; ++c) {
			_buffers[c]._frameId = FrameId_Invalid;
			_buffers[c]._pendingReadback = false;
			_buffers[c]._pendingReset = false;
			_buffers[c]._queryStart = _buffers[c]._queryEnd = ~0u;
			_buffers[c]._disjointQuery = CreateQuery(factory, true);
		}
	}

	QueryPool::~QueryPool() {}

    #if defined(GPUANNOTATIONS_ENABLE)

		void GPUAnnotation::Begin(DeviceContext& context, const char annotationName[])
		{
			wchar_t wideChar[256];
			utf8_2_ucs2((utf8*)annotationName, XlStringSize(annotationName), (ucs2*)wideChar, dimof(wideChar));
			context.GetAnnotationInterface()->BeginEvent(wideChar);
		}

		void GPUAnnotation::End(DeviceContext& context)
		{
			context.GetAnnotationInterface()->EndEvent();
		}

        GPUAnnotation::GPUAnnotation(DeviceContext& context, const char annotationName[])
        : _context(&context)
        {
            Begin(*_context, annotationName);
        }

		GPUAnnotation::~GPUAnnotation()
        {
            if (_context)
				End(*_context);
        }

    #endif
}}

