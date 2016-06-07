// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

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

    #if defined(GPUANNOTATIONS_ENABLE)

        /// <summary>Add a debugging annotation into the GPU command buffer</summary>
        /// These annotations are used for debugging. They will create a marker in debugging
        /// tools (like nsight / RenderDoc). The "annotationName" can be any arbitrary name.
        /// Note that we're taking a wchar_t for the name. This is for DirectX, which needs
        /// a wide character string. But other tools (eg, OpenGL/Android) aren't guaranteed
        /// to work this way. We could do compile time conversion with a system of macros...
        /// Otherwise, we might need fall back to run time conversion.
        class GPUAnnotation
        {
        public:
			static void Begin(DeviceContext& context, const char annotationName[]);
			static void End(DeviceContext& context);

			GPUAnnotation(DeviceContext& context, const char annotationName[]);
            ~GPUAnnotation();
        protected:
            DeviceContext* _context;
        };

    #else

        class GPUAnnotation
        {
        public:
			static void Begin(DeviceContext& context, const char annotationName[]) {}
			static void End(DeviceContext& context) {}
			GPUAnnotation(DeviceContext&, const char[]) {}
        };

    #endif
}}

