// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "QueryPool.h"
#include "ObjectFactory.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
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

////////////////////////////////////////////////////////////////////////////////////////////////////

    auto SyncEventSet::SetEvent() -> SyncEvent
    {
        auto glName = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        auto result = _nextEvent++;
        _pendingSyncs.push_back({glName, result});
        return result;
    }

    auto SyncEventSet::LastCompletedEvent() -> SyncEvent
    {
        while (!_pendingSyncs.empty()) {
            auto nextToTest = _pendingSyncs.front();
            GLint status = GL_UNSIGNALED;
            GLsizei queriedValues = 0;
            glGetSynciv(nextToTest.first, GL_SYNC_STATUS, sizeof(status), &queriedValues, &status);
            if (status == GL_UNSIGNALED) break;
            _lastCompletedEvent = std::max(_lastCompletedEvent, nextToTest.second);
            glDeleteSync(nextToTest.first);
            _pendingSyncs.pop_front();
        }
        return _lastCompletedEvent;
    }

    void SyncEventSet::Stall()
    {
        while (!_pendingSyncs.empty()) {
            auto nextToTest = _pendingSyncs.front();
            auto result = glClientWaitSync(nextToTest.first, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull);
            if (result != GL_CONDITION_SATISFIED && result != GL_ALREADY_SIGNALED) {
                assert(false);
            }
            _lastCompletedEvent = std::max(_lastCompletedEvent, nextToTest.second);
            glDeleteSync(nextToTest.first);
            _pendingSyncs.pop_front();
        }
    }

    bool SyncEventSet::IsSupported()
    {
        return GetObjectFactory().GetFeatureSet() & FeatureSet::GLES300;
    }

    SyncEventSet::SyncEventSet()
    {
        _nextEvent = 1;
        _lastCompletedEvent = 0;
    }

    SyncEventSet::~SyncEventSet()
    {
        for (auto&s:_pendingSyncs) {
            glDeleteSync(s.first);
        }
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

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

}}
