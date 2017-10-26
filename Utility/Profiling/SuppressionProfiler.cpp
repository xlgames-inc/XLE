#include "SuppressionProfiler.h"
#include "../MemoryUtils.h"
#include "../PtrUtils.h"
#include <algorithm>
#include <queue>
#include <stack>

namespace Utility
{

    void        SuppressionProfiler::SuppressEvent(EventLiteral eventLiteral)
    {
        // note that the same event can be pushed in more that once. It will stay suppresssed until
        // a matching number of DesuppressEvent calls
        auto i = std::lower_bound(_suppressedEvents.begin(), _suppressedEvents.end(), eventLiteral);
        _suppressedEvents.insert(i, eventLiteral);
    }

    void        SuppressionProfiler::DesuppressEvent(EventLiteral eventLiteral)
    {
        auto i = std::lower_bound(_suppressedEvents.begin(), _suppressedEvents.end(), eventLiteral);
        if (i != _suppressedEvents.end())
            _suppressedEvents.erase(i);
    }

    SuppressionProfiler::SuppressionProfiler()
    {
        _isRecordingKnownEvents = 0;
        _workingId = 0;

        #if !defined(NDEBUG)
            _threadId = XlGetCurrentThreadId();
            _aeStackI = 0;
        #endif
    }

    SuppressionProfiler::~SuppressionProfiler()
    {}

}

