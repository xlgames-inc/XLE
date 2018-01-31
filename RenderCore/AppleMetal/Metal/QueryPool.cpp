// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "QueryPool.h"
#include <assert.h>

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

        /*
        #if GL_EXT_debug_marker
            GLvoid glInsertEventMarkerEXT(GLsizei length, const GLchar *marker);
            GLvoid glPushGroupMarkerEXT(GLsizei length, const GLchar *marker);
            GLvoid glPopGroupMarkerEXT(void);
        #endif
        */

        void GPUAnnotation::Begin(DeviceContext& context, const char annotationName[])
        {
            assert(0);
        }

        void GPUAnnotation::End(DeviceContext& context)
        {
            assert(0);
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
