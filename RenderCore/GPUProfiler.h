// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IThreadContext_Forward.h"
#include "IDevice_Forward.h"
#include "../Core/Types.h"
#include <utility>
#include <memory>
#include <functional>

#define FLEX_USE_VTABLE_Annotator   1

namespace RenderCore 
{

#define FLEX_INTERFACE Annotator
/*-----------------*/ #include "FlexBegin.h" /*-----------------*/

	class ICLASSNAME(Annotator)
	{
	public:
		IMETHOD void	Frame_Begin(IThreadContext& primaryContext, unsigned frameID) IPURE;
		IMETHOD void	Frame_End(IThreadContext& primaryContext) IPURE;

		struct EventTypes
		{
			enum Flags
			{ 
				ProfileBegin = 1<<0, ProfileEnd = 1<<1,
				MarkerBegin = 1<<2, MarkerEnd = 1<<3
			};
			using BitField = unsigned;
		};
		IMETHOD void	Event(IThreadContext& context, const char name[], EventTypes::BitField types) IPURE;

		using EventListener = std::function<void(const void* eventBufferBegin, const void* eventBufferEnd)>;
		IMETHOD unsigned	AddEventListener(const EventListener& callback) IPURE;
		IMETHOD void		RemoveEventListener(unsigned listenerId) IPURE;

		IDESTRUCTOR
	};

	#if !defined(FLEX_CONTEXT_Annotator)
		#define FLEX_CONTEXT_Annotator     FLEX_CONTEXT_INTERFACE
	#endif

	#if defined(DOXYGEN)
		typedef IAnnotator Base_Annotator;
	#endif

/*-----------------*/ #include "FlexEnd.h" /*-----------------*/

	std::unique_ptr<IAnnotator> CreateAnnotator(IDevice&);

	#if FLEX_CONTEXT_Annotator != FLEX_CONTEXT_CONCRETE
		class GPUProfilerBlock
		{
		public:
			GPUProfilerBlock(IAnnotator& annotator, IThreadContext& context, const char name[])
			: _annotator(&annotator), _name(name), _context(&context)
			{
				_annotator->Event(*_context, _name, IAnnotator::EventTypes::ProfileBegin);
			}

			~GPUProfilerBlock()
			{
				_annotator->Event(*_context, _name, IAnnotator::EventTypes::ProfileEnd);
			}

			GPUProfilerBlock(const GPUProfilerBlock&) = delete;
			GPUProfilerBlock& operator=(const GPUProfilerBlock&) = delete;
		private:
			IAnnotator*			_annotator; 
			IThreadContext*		_context;
			const char*			_name;
		};
	#endif
}

