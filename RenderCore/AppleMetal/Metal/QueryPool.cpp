// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "QueryPool.h"
#include "DeviceContext.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    auto QueryPool::SetTimeStampQuery(DeviceContext& context) -> QueryId 
    { 
        return QueryId_Invalid; 
    }

    auto QueryPool::BeginFrame(DeviceContext& context) -> FrameId
    { 
        return FrameId_Invalid;
    }

    void QueryPool::EndFrame(DeviceContext& context, FrameId frame)
    {}

    auto QueryPool::GetFrameResults(DeviceContext& context, FrameId id) -> FrameResults
    {
        return FrameResults { false, false, nullptr, nullptr, 0ull };
    }

    QueryPool::QueryPool(ObjectFactory& factory) {}
    QueryPool::~QueryPool() {}

    #if defined(GPUANNOTATIONS_ENABLE)

        void GPUAnnotation::Begin(DeviceContext& context, const char annotationName[])
        {
            context.PushDebugGroup(annotationName);
        }

        void GPUAnnotation::End(DeviceContext& context)
        {
            context.PopDebugGroup();
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
