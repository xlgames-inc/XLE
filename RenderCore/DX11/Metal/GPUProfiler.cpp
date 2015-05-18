// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GPUProfiler.h"
#include "DX11.h"
#include "DeviceContext.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Utility/Threading/Mutex.h"
#include "../../../Core/Exceptions.h"
#include "IncludeDX11.h"
#include <deque>
#include <vector>

namespace RenderCore { namespace Metal_DX11 { namespace GPUProfiler     /// Low level GPU profiler implementation (for DirectX11)
{

            //////////////////////////////////////////////////////////////
                //      D I R E C T X  1 1  S P E C I F I C         //
            //////////////////////////////////////////////////////////////

    class Query
    {
    public:
        intrusive_ptr<ID3D::Query> _query;
        bool _isAllocated;
    };

    class QueryConstructionFailure : public ::Exceptions::BasicLabel
    {
    public:
        QueryConstructionFailure() : ::Exceptions::BasicLabel("Failed while constructing query. This will happen on downleveled interfaces.") {}
    };

    typedef D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointQueryData;
    static HRESULT GetDataNoFlush(ID3D::DeviceContext* context, Query& query, void * destination, UINT destinationSize)
    {
        return context->GetData(
            query._query.get(), destination, destinationSize, D3D11_ASYNC_GETDATA_DONOTFLUSH );
    }

    static HRESULT GetDisjointData(ID3D::DeviceContext* context, Query& query, DisjointQueryData& destination)
    {
        return GetDataNoFlush(context, query, &destination, sizeof(destination));
    }

    static Query CreateQuery(bool disjoint)
    {
        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query      = disjoint?D3D11_QUERY_TIMESTAMP_DISJOINT:D3D11_QUERY_TIMESTAMP;
        queryDesc.MiscFlags  = 0;
        auto query = ObjectFactory().CreateQuery(&queryDesc);

        Query newQuery; 
        newQuery._isAllocated = false; 
        newQuery._query = std::move(query);
        return newQuery;
    }

    static void BeginQuery(ID3D::DeviceContext* context, Query& query)
    {
        if (query._query) {
            context->Begin(query._query.get());
        }
    }

    static void EndQuery(ID3D::DeviceContext* context, Query& query)
    {
        if (query._query) {
            context->End(query._query.get());
        }
    }


            //////////////////////////////////////////////////////////////
                //      P L A T F O R M   G E N E R I C             //
            //////////////////////////////////////////////////////////////


    class Profiler
    {
    public:
        void    TriggerEvent(DeviceContext& context, const char name[], EventType type);
        void    Frame_Begin(DeviceContext& context, unsigned frameId);
        void    Frame_End(DeviceContext& context);
        std::pair<uint64,uint64> CalculateSynchronisation(DeviceContext& context);

        Profiler();
        ~Profiler();

    protected:
        struct EventInFlight
        {
            const char* _name;
            unsigned _queryIndex;
            EventType _type;
            unsigned _queryFrameId;
        };
        struct QueryFrame
        {
            unsigned _queryFrameId;
            unsigned _disjointQueryIndex;
            unsigned _renderFrameId;
            bool _disjointQueryHasCompleted;
            DisjointQueryData _completedQueryData;
        };
        std::deque<EventInFlight> _eventsInFlight;
        std::deque<QueryFrame> _framesInFlight;
        std::vector<Query> _queryPool;
        std::vector<Query> _disjointQueryPool;

        unsigned _currentQueryFrameId;
        unsigned _currentRenderFrameId;
        unsigned _currentDisjointQuery;
        unsigned _nextQueryFrameId;
        signed _frameRecursionDepth;
        
        void    FlushFinishedQueries(DeviceContext& context);

        static void Callback(const char name[], unsigned type);
        static Profiler* s_callbackProfiler;
    };

    // void Profiler::Callback(const char name[], unsigned type)
    // {
    //     assert(s_callbackProfiler);
    //     s_callbackProfiler->TriggerEvent(name, EventType(type));
    // }

    Profiler* Profiler::s_callbackProfiler = 0;
    Threading::Mutex g_globalEventListener_Mutex;
    std::vector<EventListener*> g_globalEventListener;

    //////////////////////////////////////////////////////////////////

    unsigned AllocateQuery(std::vector<Query>::iterator begin, std::vector<Query>::iterator end)
    {
        for (std::vector<Query>::iterator i=begin; i!=end; ++i) {
            if (!i->_isAllocated) {
                i->_isAllocated = true;
                return unsigned(std::distance(begin,i));
            }
        }
        return ~unsigned(0);
    }

    void    Profiler::TriggerEvent(DeviceContext& context, const char name[], EventType type)
    {
        if (_currentQueryFrameId==~unsigned(0))
            return;

        unsigned queryIndex = AllocateQuery(_queryPool.begin(), _queryPool.end());
        if (queryIndex >= _queryPool.size()) {
            // X2LogAlways("Warning -- Ran out of queries when triggering an event in GPU profiler.");
            return;
        }

        EventInFlight newEvent;
        newEvent._name = name;
        newEvent._type = type;
        newEvent._queryIndex = queryIndex;
        newEvent._queryFrameId = _currentQueryFrameId;
        _eventsInFlight.push_back(newEvent);

        EndQuery(context.GetUnderlying(), _queryPool[queryIndex]);
    }

    void    Profiler::Frame_Begin(DeviceContext&context, unsigned frameId)
    {
        ++_frameRecursionDepth;
        if (_currentQueryFrameId != ~unsigned(0) || _currentDisjointQuery != ~unsigned(0) || (_frameRecursionDepth>1)) {
            assert(_currentQueryFrameId != ~unsigned(0) && _currentDisjointQuery != ~unsigned(0) && (_frameRecursionDepth>1));
            return;
        }

        unsigned disjointQueryIndex = AllocateQuery(_disjointQueryPool.begin(), _disjointQueryPool.end());
        if (disjointQueryIndex >= _disjointQueryPool.size()) {
            // X2LogAlways("Warning -- Ran out of queries in begin frame in GPU profiler.");
            return;
        }

        BeginQuery(context.GetUnderlying(), _disjointQueryPool[disjointQueryIndex]);

        _currentQueryFrameId = _nextQueryFrameId;
        _currentRenderFrameId = frameId;
        ++_nextQueryFrameId;
        _currentDisjointQuery = disjointQueryIndex;
    }

    void    Profiler::Frame_End(DeviceContext& context)
    {
        --_frameRecursionDepth;
        if (_frameRecursionDepth==0) {
            if (_currentQueryFrameId != ~unsigned(0) && _currentDisjointQuery != ~unsigned(0)) {
                EndQuery(context.GetUnderlying(), _disjointQueryPool[_currentDisjointQuery]);

                QueryFrame frameInFlight;
                frameInFlight._queryFrameId = _currentQueryFrameId;
                frameInFlight._disjointQueryIndex = _currentDisjointQuery;
                frameInFlight._renderFrameId = _currentRenderFrameId;
                frameInFlight._disjointQueryHasCompleted = false;
                _framesInFlight.push_back(frameInFlight);

                _currentQueryFrameId = ~unsigned(0);
                _currentRenderFrameId = ~unsigned(0);
                _currentDisjointQuery = ~unsigned(0);
            }
            FlushFinishedQueries(context);
        }
    }

    void Profiler::FlushFinishedQueries(DeviceContext& context)
    {
            //
            //      Look for finished queries, and remove them from the
            //      "in-flight" list
            //

        bool continueLooping = true;
        while (continueLooping && !_framesInFlight.empty()) {
            QueryFrame& frameInFlight = *_framesInFlight.begin();
            DisjointQueryData& disjointData = frameInFlight._completedQueryData;

            if (!frameInFlight._disjointQueryHasCompleted) {
                unsigned firstDisjointQuery = _framesInFlight.begin()->_disjointQueryIndex;
                assert(firstDisjointQuery < _disjointQueryPool.size() && _disjointQueryPool[firstDisjointQuery]._isAllocated);

                HRESULT hresult = GetDisjointData(context.GetUnderlying(), _disjointQueryPool[firstDisjointQuery], disjointData);
                if (hresult == S_OK) {
                    frameInFlight._disjointQueryHasCompleted = true;

                        //      Return the disjoint query to the pool... We don't
                        //      need it anymore...
                    _disjointQueryPool[firstDisjointQuery]._isAllocated = false;
                    frameInFlight._disjointQueryIndex = ~unsigned(0);
                }
            }

            if (frameInFlight._disjointQueryHasCompleted) {
                uint64 evntBuffer[2048/sizeof(uint64)];
                byte* eventBufferPtr = (byte*)evntBuffer;
                const byte* eventBufferEnd = (const byte*)&evntBuffer[dimof(evntBuffer)];

                {
                    ScopedLock(g_globalEventListener_Mutex);
                        //      Write an event to set the frequency. We should expect the frequency should be constant
                        //      in a single play through, but it doesn't hurt to keep recording it...
                    const size_t entrySize = sizeof(size_t)*2+sizeof(uint64);
                    if (size_t(eventBufferPtr)+entrySize > size_t(eventBufferEnd)) {
                        for (std::vector<EventListener*>::iterator i=g_globalEventListener.begin(); i!=g_globalEventListener.end(); ++i) {
                            (**i)(evntBuffer, eventBufferPtr);
                        }
                        eventBufferPtr = (byte*)evntBuffer;
                    }

                    *((size_t*)eventBufferPtr) = ~size_t(0x0);                  eventBufferPtr+=sizeof(size_t);
                    *((size_t*)eventBufferPtr) = frameInFlight._renderFrameId;  eventBufferPtr+=sizeof(size_t);
                    *((uint64*)eventBufferPtr) = disjointData.Frequency;        eventBufferPtr+=sizeof(uint64);
                }

                    //
                    //      We've sucessfully completed this "disjoint" query.
                    //      The other queries related to this frame should be finished now.
                    //      Let's get their data (though, if the disjoint flag is set, we'll ignore the data)
                    //
                unsigned thisFrameId = frameInFlight._queryFrameId;
                while (!_eventsInFlight.empty() && _eventsInFlight.begin()->_queryFrameId == thisFrameId) {
                    EventInFlight & evnt = *_eventsInFlight.begin();
                    UINT64 timeResult = ~UINT64(0);
                    if (evnt._queryIndex < _queryPool.size()) {
                        HRESULT hresult = GetDataNoFlush(context.GetUnderlying(), _queryPool[evnt._queryIndex], &timeResult, sizeof(timeResult));
                        if (hresult != S_OK) {
                            break;      // as soon as we get a failure, drop out... It's possible later ones have completed, but it would be strange
                        }
                        _queryPool[evnt._queryIndex]._isAllocated = false;
                    }

                    if (!disjointData.Disjoint) {
                        ScopedLock(g_globalEventListener_Mutex);
                            //
                            //      Write an event into out buffer to represent this
                            //      occurrence. If we can't fit it in; we need to flush it out 
                            //      and continue on.
                            //
                        const size_t entrySize = sizeof(size_t)*2+sizeof(uint64);
                        if (size_t(eventBufferPtr)+entrySize > size_t(eventBufferEnd)) {
                            for (std::vector<EventListener*>::iterator i=g_globalEventListener.begin(); i!=g_globalEventListener.end(); ++i) {
                                (**i)(evntBuffer, eventBufferPtr);
                            }
                            eventBufferPtr = (byte*)evntBuffer;
                        }
                        *((size_t*)eventBufferPtr) = size_t(evnt._type); eventBufferPtr+=sizeof(size_t);
                        *((size_t*)eventBufferPtr) = size_t(evnt._name); eventBufferPtr+=sizeof(size_t);
                        // assert(size_t(eventBufferPtr)%sizeof(uint64)==0);
                        *((uint64*)eventBufferPtr) = uint64(timeResult); eventBufferPtr+=sizeof(uint64);
                    }
                            
                    _eventsInFlight.pop_front();
                }

                    //      if we've popped off every event related to this query frame id, it's ok
                    //      to remove the frame in flight as well...
                if (_eventsInFlight.empty() || _eventsInFlight.begin()->_queryFrameId != thisFrameId) {
                    _framesInFlight.pop_front();
                } else {
                    continueLooping = false;        // we didn't fulling complete this one... we can't go further this time
                }

                    //  Flush out any remaining entries in the event buffer...
                    //  Note, this will insure that event if 2 frames worth of events
                    //  complete in the single FlushFinishedQueries() call, we will never
                    //  fill the event listener with a mixture of events from multiple frames.
                if (eventBufferPtr!=(byte*)evntBuffer) {
                    ScopedLock(g_globalEventListener_Mutex);
                    for (std::vector<EventListener*>::iterator i=g_globalEventListener.begin(); i!=g_globalEventListener.end(); ++i) {
                        (**i)(evntBuffer, eventBufferPtr);
                    }
                }
            } else {
                continueLooping = false;
            }
        }
    }

    std::pair<uint64,uint64> Profiler::CalculateSynchronisation(DeviceContext& context)
    {
            //
            //      Calculate 2 time values (one for CPU, one for GPU) that approximately
            //      match.
            //
        std::pair<uint64,uint64> result(0,0);
        unsigned disjointQueryIndex = AllocateQuery(_disjointQueryPool.begin(), _disjointQueryPool.end());
        unsigned queryIndex = AllocateQuery(_queryPool.begin(), _queryPool.end());
        if (disjointQueryIndex > _disjointQueryPool.size() || queryIndex > _queryPool.size()) {
            // X2LogAlways("Warning -- Ran out of queries while calculating synchronisation.");
            return result;
        }

        Query& d3dDisjoint = _disjointQueryPool[disjointQueryIndex];
        Query& d3dQuery = _queryPool[queryIndex];

            // 
            //      Do the query 3 times... We should be fully synchronised by that
            //      point.
            //
        for (unsigned c=0; c<3; ++c) {
                
            BeginQuery( context.GetUnderlying(), d3dDisjoint );
            EndQuery( context.GetUnderlying(), d3dQuery );
            EndQuery( context.GetUnderlying(), d3dDisjoint );

                //      Note, we can't use the normal GetDataNoFlush interfaces here -- because we actually do a need a flush!
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
            HRESULT hresult = 0;
            do {
                hresult = context.GetUnderlying()->GetData(
                    d3dDisjoint._query.get(), &disjointData, sizeof(disjointData), 0 );
            } while (hresult!=S_OK);

            context.GetUnderlying()->Flush();
            QueryPerformanceCounter((LARGE_INTEGER*)&result.first);

            if (!disjointData.Disjoint) {       // (it's possible that every result is disjoint... eg, if the device is unplugged, etc). In this case, we'll just get back bad data
                do {
                    hresult = context.GetUnderlying()->GetData( 
                        d3dQuery._query.get(), &result.second, sizeof(result.second), 0);
                } while (hresult!=S_OK);
            }

        }

        _queryPool[queryIndex]._isAllocated = false;
        _disjointQueryPool[disjointQueryIndex]._isAllocated = false;

        return result;
    }

    Profiler::Profiler()
    {
            //
            //      Allocate the queries we'll need later one
            //      Generally, we'll need more queries the faster 
            //      the frame rate, and the more events per frame.
            //
        const unsigned QueryPoolSize = 96;
        const unsigned DisjointQueryPoolSize = 4;
        
        std::vector<Query> queryPool(QueryPoolSize);
        std::vector<Query> disjointQueryPool(DisjointQueryPoolSize);

            ///////////////////////////////////////~~~///////////////////////////////////////
        for (unsigned c=0; c<DisjointQueryPoolSize; ++c) {
            disjointQueryPool.push_back(CreateQuery(true));
        }

        for (unsigned c=0; c<QueryPoolSize; ++c) {
            queryPool.push_back(CreateQuery(false));
        }

        _currentQueryFrameId     = ~unsigned(0);
        _currentRenderFrameId    = ~unsigned(0);
        _currentDisjointQuery    = ~unsigned(0);
        _frameRecursionDepth     = 0;
        _nextQueryFrameId        = 0;

        _queryPool = std::move(queryPool);
        _disjointQueryPool = std::move(disjointQueryPool);

        assert(s_callbackProfiler==NULL);
        s_callbackProfiler = this;
    }

    Profiler::~Profiler()
    {
        assert(s_callbackProfiler==this);
        s_callbackProfiler = NULL;
    }

    Ptr CreateProfiler()
    {
        TRY {
            return Ptr(new Profiler);
        } CATCH(const QueryConstructionFailure& ) {
            return nullptr;
        } CATCH_END
    }

    void ProfilerDestroyer::operator()(const void* ptr)
    {
        delete (const Profiler*)ptr;
    }

    void Frame_Begin(DeviceContext& context, Profiler*profiler, unsigned frameID)
    {
        if (profiler) {
            profiler->Frame_Begin(context, frameID);
        }
    }

    void Frame_End(DeviceContext& context, Profiler*profiler)
    {
        if (profiler) {
            profiler->Frame_End(context);
        }
    }

    void TriggerEvent(DeviceContext& context, Profiler*profiler, const char name[], EventType type)
    {
        if (profiler) {
            profiler->TriggerEvent(context, name, type);
        }
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

    std::pair<uint64,uint64> CalculateSynchronisation(DeviceContext& context, Profiler*profiler) 
    { 
        if (profiler) {
            return profiler->CalculateSynchronisation(context);
        } else {
            return std::pair<uint64,uint64>(0,0);
        }
    }

    void AddEventListener(EventListener* callback)
    {
        ScopedLock(g_globalEventListener_Mutex);
        if (std::find(g_globalEventListener.begin(), g_globalEventListener.end(), callback) == g_globalEventListener.end()) {
            g_globalEventListener.push_back(callback);
        }
    }

    void RemoveEventListener(EventListener* callback)
    {
        ScopedLock(g_globalEventListener_Mutex);
        std::vector<EventListener*>::iterator i=std::find(g_globalEventListener.begin(), g_globalEventListener.end(), callback);
        if (i!=g_globalEventListener.end()) {
            g_globalEventListener.erase(i);
        }
    }
}}}

