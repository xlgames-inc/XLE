// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GPUProfiler.h"
#include "DX11.h"
#include "DeviceContext.h"
#include "ObjectFactory.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Utility/Threading/Mutex.h"
#include "../../../Core/Exceptions.h"
#include "IncludeDX11.h"
#include <deque>
#include <vector>

namespace RenderCore { namespace Metal_DX11 { namespace GPUProfiler     /// Low level GPU profiler implementation (for DirectX11)
{
    bool GetDataNoFlush(DeviceContext& context, Query& query, void * destination, unsigned destinationSize)
    {
        auto hresult = context.GetUnderlying()->GetData(
            query._query.get(), destination, destinationSize, D3D11_ASYNC_GETDATA_DONOTFLUSH );
		return SUCCEEDED(hresult);
    }

    bool GetDisjointData(DeviceContext& context, Query& query, DisjointQueryData& destination)
    {
        auto hresult = GetDataNoFlush(context, query, &destination, sizeof(destination));
		return SUCCEEDED(hresult);
    }

    Query CreateQuery(bool disjoint)
    {
        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query      = disjoint?D3D11_QUERY_TIMESTAMP_DISJOINT:D3D11_QUERY_TIMESTAMP;
        queryDesc.MiscFlags  = 0;
        auto query = GetObjectFactory().CreateQuery(&queryDesc);

        Query newQuery; 
        newQuery._isAllocated = false; 
        newQuery._query = std::move(query);
        return newQuery;
    }

    void BeginQuery(DeviceContext& context, Query& query)
    {
        if (query._query)
            context.GetUnderlying()->Begin(query._query.get());
    }

    void EndQuery(DeviceContext& context, Query& query)
    {
        if (query._query)
            context.GetUnderlying()->End(query._query.get());
    }

	std::pair<uint64, uint64> CalculateSynchronisation(DeviceContext& context, Query& query, Query& disjoint)
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
				hresult = context.GetUnderlying()->GetData(
					disjoint._query.get(), &disjointData, sizeof(disjointData), 0);
			} while (hresult != S_OK);

			context.GetUnderlying()->Flush();
			QueryPerformanceCounter((LARGE_INTEGER*)&result.first);

			if (!disjointData.Disjoint) {       // (it's possible that every result is disjoint... eg, if the device is unplugged, etc). In this case, we'll just get back bad data
				do {
					hresult = context.GetUnderlying()->GetData(
						query._query.get(), &result.second, sizeof(result.second), 0);
				} while (hresult != S_OK);
			}

		}

		return result;
	}

    #if defined(GPUANNOTATIONS_ENABLE)

        DebugAnnotation::DebugAnnotation(DeviceContext& context, const wchar_t annotationName[])
        : _context(&context)
        {
            _context->GetAnnotationInterface()->BeginEvent(annotationName);
        }

        DebugAnnotation::~DebugAnnotation() 
        {
            if (_context) {
                _context->GetAnnotationInterface()->EndEvent();
            }
        }

    #endif
}}}

template Utility::intrusive_ptr<ID3D::Query>;
