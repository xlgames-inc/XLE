// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "QueryPool.h"
#include "DeviceContext.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    auto TimeStampQueryPool::SetTimeStampQuery(DeviceContext& context) -> QueryId 
    { 
        return QueryId_Invalid; 
    }

    auto TimeStampQueryPool::BeginFrame(DeviceContext& context) -> FrameId
    { 
        return FrameId_Invalid;
    }

    void TimeStampQueryPool::EndFrame(DeviceContext& context, FrameId frame)
    {}

    auto TimeStampQueryPool::GetFrameResults(DeviceContext& context, FrameId id) -> FrameResults
    {
        return FrameResults { false, false, nullptr, nullptr, 0ull };
    }

    TimeStampQueryPool::TimeStampQueryPool(ObjectFactory& factory) {}
    TimeStampQueryPool::~TimeStampQueryPool() {}

    #if defined(GPUANNOTATIONS_ENABLE)

        void GPUAnnotation::Begin(DeviceContext& context, const char annotationName[])
        {
            #if GL_EXT_debug_marker
                glPushGroupMarkerEXT(0, annotationName);
            #endif
        }

        void GPUAnnotation::End(DeviceContext& context)
        {
            #if GL_EXT_debug_marker
                glPopGroupMarkerEXT();
            #endif
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

    void	Annotator::Frame_Begin(IThreadContext& primaryContext, unsigned frameID) {}
    void	Annotator::Frame_End(IThreadContext& primaryContext) {}

    void	Annotator::Event(IThreadContext& context, const char name[], EventTypes::BitField types)
    {
        if (types & EventTypes::Flags::MarkerBegin) {
            GPUAnnotation::Begin(*DeviceContext::Get(context), name);
        } else if (types & EventTypes::Flags::MarkerEnd) {
            GPUAnnotation::End(*DeviceContext::Get(context));
        }
    }

    unsigned	Annotator::AddEventListener(const EventListener& callback) { return 0; }
    void		Annotator::RemoveEventListener(unsigned listenerId) {}

    Annotator::Annotator() {}
    Annotator::~Annotator() {}

}}
