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
	class ObjectFactory;

    namespace GPUProfiler
    {
		class QueryConstructionFailure : public ::Exceptions::BasicLabel
		{
		public:
			QueryConstructionFailure() : ::Exceptions::BasicLabel("Failed while constructing query. This will happen on downleveled interfaces.") {}
		};

		typedef D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointQueryData;

		bool GetDataNoFlush(DeviceContext& context, ID3D::Query& query, void * destination, unsigned destinationSize);
		bool GetDisjointData(DeviceContext& context, ID3D::Query& query, DisjointQueryData& destination);
		intrusive_ptr<ID3D::Query> CreateQuery(ObjectFactory& factory, bool disjoint);
		void BeginQuery(DeviceContext& context, ID3D::Query& query);
		void EndQuery(DeviceContext& context, ID3D::Query& query);
		std::pair<uint64, uint64> CalculateSynchronisation(DeviceContext& context, ID3D::Query& query, ID3D::Query& disjoint);

        #if defined(GPUANNOTATIONS_ENABLE)

            /// <summary>Add a debugging annotation into the GPU command buffer</summary>
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

