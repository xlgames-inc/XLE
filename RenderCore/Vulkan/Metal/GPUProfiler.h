// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Core/Types.h"
#include "../../../Core/Exceptions.h"

#define GPUANNOTATIONS_ENABLE

namespace RenderCore { namespace Metal_Vulkan
{
    class DeviceContext;

    namespace GPUProfiler
    {
		class Query
		{
		public:
			bool _isAllocated;

			Query() : _isAllocated(false) {}
			Query(Query&& moveFrom) never_throws : _isAllocated(moveFrom._isAllocated) {}
			Query& operator=(Query&& moveFrom) never_throws
			{
				_isAllocated = moveFrom._isAllocated;
				moveFrom._isAllocated = false;
				return *this;
			}
		};

		class QueryConstructionFailure : public ::Exceptions::BasicLabel
		{
		public:
			QueryConstructionFailure() : ::Exceptions::BasicLabel("Failed while constructing query. This will happen on downleveled interfaces.") {}
		};

		class DisjointQueryData 
		{
		public:
			uint64 Frequency;
			bool Disjoint;
		};

		inline bool GetDataNoFlush(DeviceContext& context, Query& query, void * destination, unsigned destinationSize) { return false; }
		inline bool GetDisjointData(DeviceContext& context, Query& query, DisjointQueryData& destination) { return false; }
		inline Query CreateQuery(bool disjoint) { return Query(); }
		inline void BeginQuery(DeviceContext& context, Query& query) {}
		inline void EndQuery(DeviceContext& context, Query& query) {}
		std::pair<uint64, uint64> CalculateSynchronisation(DeviceContext& context, Query& query, Query& disjoint) { return std::make_pair(0,0); }

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