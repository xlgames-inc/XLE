// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Forward.h"
#include "DX11.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Core/Types.h"
#include <algorithm>
#include <memory>

#define GPUANNOTATIONS_ENABLE

namespace RenderCore { namespace Metal_DX11
{
    class DeviceContext;

    namespace GPUProfiler
    {
		class Query
		{
		public:
			intrusive_ptr<ID3D::Query> _query;
			bool _isAllocated;

			Query() : _isAllocated(false) {}
			Query(Query&& moveFrom) never_throws : _isAllocated(moveFrom._isAllocated), _query(std::move(moveFrom._query)) {}
			Query& operator=(Query&& moveFrom) never_throws
			{
				_isAllocated = moveFrom._isAllocated;
				_query = std::move(moveFrom._query);
				moveFrom._isAllocated = false;
				return *this;
			}
		};

		class QueryConstructionFailure : public ::Exceptions::BasicLabel
		{
		public:
			QueryConstructionFailure() : ::Exceptions::BasicLabel("Failed while constructing query. This will happen on downleveled interfaces.") {}
		};

		typedef D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointQueryData;

		bool GetDataNoFlush(DeviceContext& context, Query& query, void * destination, unsigned destinationSize);
		bool GetDisjointData(DeviceContext& context, Query& query, DisjointQueryData& destination);
		Query CreateQuery(bool disjoint);
		void BeginQuery(DeviceContext& context, Query& query);
		void EndQuery(DeviceContext& context, Query& query);
		std::pair<uint64, uint64> CalculateSynchronisation(DeviceContext& context, Query& query, Query& disjoint);

        #if defined(GPUANNOTATIONS_ENABLE)

                /// <summary>Add a debugging animation</summary>
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

#pragma warning(push)
#pragma warning(disable:4231)   // nonstandard extension used : 'extern' before template explicit instantiation
/// \cond INTERNAL
extern template Utility::intrusive_ptr<ID3D::Query>;
/// \endcond
#pragma warning(pop)

