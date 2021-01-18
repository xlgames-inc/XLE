// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PollingThread.h"
#include "../../OSServices/Log.h"
#include "../../Utility/Threading/Mutex.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Core/Exceptions.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace OSServices
{
    static struct epoll_event EPollEvent(PollingEventType::BitField types, bool oneShot = true)
    {
        struct epoll_event result{};
        result.events = EPOLLRDHUP | EPOLLHUP | EPOLLERR;

        const bool edgeTriggered = false;
        if (edgeTriggered)
            result.events |= EPOLLET;
        if (oneShot)
            result.events |= EPOLLONESHOT;

        if (types & PollingEventType::Input)
            result.events |= EPOLLIN;
        if (types & PollingEventType::Output)
            result.events |= EPOLLOUT;
        return result;
    }

    static PollingEventType::BitField AsPollingEventType(uint32_t osEventFlags)
    {
        PollingEventType::BitField result = 0;
        if (osEventFlags & EPOLLIN)
            result |= PollingEventType::Input;
        if (osEventFlags & EPOLLOUT)
            result |= PollingEventType::Output;
        return result;
    }

    class PollingThread::Pimpl
    {
    public:
        int _interruptPollEvent = -1;
        std::atomic<bool> _pendingShutdown;
        std::thread _backgroundThread;

        std::mutex _interfaceLock;

        struct PendingInitiates
        {
            PollingEventType::BitField _eventTypes;
            PlatformHandle _platformHandle;
            Threading::ContinuationPromise<PollingEventType::BitField> _promise;
        };
        std::vector<PendingInitiates> _pendingInitiates;

        struct ActiveEvent
        {
            PlatformHandle _platformHandle;
            Threading::ContinuationPromise<PollingEventType::BitField> _promise;
        };
        std::vector<ActiveEvent> _activeEvents;

        Pimpl() : _pendingShutdown(false)
        {
            _interruptPollEvent = eventfd(0, EFD_NONBLOCK);
            _backgroundThread = std::thread(
                [this]() {
                    TRY {
                        this->ThreadFunction(); 
                    } CATCH(const std::exception& e) {
                        Log(Error) << "Encountered exception in background epoll thread. Terminating any asynchronous operations" << std::endl;
                        Log(Error) << "Exception as follows: " << e.what() << std::endl;
                    } CATCH(...) {
                        Log(Error) << "Encountered exception in background epoll thread. Terminating any asynchronous operations" << std::endl;
                        Log(Error) << "Unknown exception type" << std::endl;
                    } CATCH_END
                });
        }

        ~Pimpl()
        {
            _pendingShutdown.store(true);
            InterruptBackgroundThread();
            _backgroundThread.join();
        }

        void ThreadFunction()
        {
            int epollContext = epoll_create1(0);
            if (epollContext < 0)
                Throw(std::runtime_error("Failure in epoll_create1"));

            {
                auto readEvent = EPollEvent(PollingEventType::Input, false);
                readEvent.data.fd = _interruptPollEvent;
                auto ret = epoll_ctl(epollContext, EPOLL_CTL_ADD, _interruptPollEvent, &readEvent);
                if (ret < 0)
                    Throw(std::runtime_error("Failure when adding interrupt event to epoll queue"));
            }

            struct epoll_event events[32];
            while (!_pendingShutdown.load()) {
                // "add" all events that are pending
                {
                    ScopedLock(_interfaceLock);
                    for (auto& event:_pendingInitiates) {
                        auto existing = std::find_if(
                            _activeEvents.begin(), _activeEvents.end(),
                            [&event](const ActiveEvent& ae) { return ae._platformHandle == event._platformHandle; });
                        if (existing != _activeEvents.end()) {
                            // We can't queue multiple poll operations on the same platform handle, because we will be using
                            // the platform handle to lookup events in _activeEvents (this would otherwise make it ambigious)
                            event._promise.set_exception(std::make_exception_ptr(std::runtime_error("Multiple asynchronous events queued for the same platform handle")));
                            continue;
                        }

                        auto evt = EPollEvent(event._eventTypes);
                        evt.data.fd = event._platformHandle;
                        auto ret = epoll_ctl(epollContext, EPOLL_CTL_ADD, event._platformHandle, &evt);
                        if (ret < 0) {
                            event._promise.set_exception(std::make_exception_ptr(std::runtime_error("Failed to add asyncronous event to epoll queue")));
                        } else {
                            ActiveEvent activeEvent;
                            activeEvent._platformHandle = event._platformHandle;
                            activeEvent._promise = std::move(event._promise);
                            _activeEvents.push_back(std::move(activeEvent));
                        }
                    }
                    _pendingInitiates.clear();
                }
                
                const int timeoutInMilliseconds = -1;
                int eventCount = epoll_wait(epollContext, events, dimof(events), timeoutInMilliseconds);
                if (eventCount <= 0 || eventCount > dimof(events)) {
                    // We will actually get here during normal shutdown. When the main
                    // thread calls _backgroundThread.join(), it seems to trigger an interrupt event on the epoll system
                    // automatically. In that case, errno will be EINTR. Since this happens during normal usage,
                    // we can't treat this as an error.
                    if (errno != EINTR)
                        Throw(std::runtime_error("Failure in epoll_wait"));
                    break;
                }

                for (const auto& triggeredEvent:MakeIteratorRange(events, &events[eventCount])) {

                    // _interruptPollEvent exists only to break us out of "epoll_wait", so don't need to do much in this case
                    // We should just drain _interruptPollEvent of all data we can read. Since we're not using EFD_SEMAPHORE
                    // for this event, we should just have to read one
                    if (triggeredEvent.data.fd == _interruptPollEvent) {
                        uint64_t eventFdCounter=0;
                        auto ret = read(_interruptPollEvent, &eventFdCounter, sizeof(eventFdCounter));
                        assert(ret > 0);
                        continue;
                    }

                    auto i = std::find_if(
                        _activeEvents.begin(), _activeEvents.end(),
                        [&triggeredEvent](const ActiveEvent& ae) { return ae._platformHandle == triggeredEvent.data.fd; });
                    if (i == _activeEvents.end()) {
                        Log(Error) << "Got an event for a platform handle that isn't in our activeEvents list" << std::endl;
                        continue;
                    }

                    if (triggeredEvent.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                        // this is a disconnection or error event. We should return an exception and also remove
                        // the event from both the queue and our list of active events
                        auto ret = epoll_ctl(epollContext, EPOLL_CTL_DEL, triggeredEvent.data.fd, nullptr);
                        assert(ret == 0);
                        auto promise = std::move(i->_promise);
                        _activeEvents.erase(i);
                        
                        promise.set_exception(std::make_exception_ptr(std::runtime_error("")));
                    } else if (triggeredEvent.events & (EPOLLIN | EPOLLOUT)) {
                        // This means data is available to read, or the fd is ready for writing to
                        // It's effectively a success. Still, for one-shot events, we will remove the event
                        // It seems that fd will still be registered in the epollContext, even for an event
                        // marked as a one-shot (it just gets set to a disabled state)
                        auto ret = epoll_ctl(epollContext, EPOLL_CTL_DEL, triggeredEvent.data.fd, nullptr);
                        assert(ret == 0);
                        auto promise = std::move(i->_promise);
                        _activeEvents.erase(i);

                        promise.set_value(AsPollingEventType(triggeredEvent.events));
                    } else {
                        Throw(std::runtime_error("Unexpected event trigger value"));
                    }

                }
            }

            close(epollContext);
        }

        void InterruptBackgroundThread()
        {
            ssize_t ret = 0;
            do {
                uint64_t counterIncrement = 1;
                ret = write(_interruptPollEvent, &counterIncrement, sizeof(counterIncrement));
            } while (ret < 0 && errno == EAGAIN);
        }
    };

    auto PollingThread::RespondOnEvent(PlatformHandle platformHandle, PollingEventType::BitField typesToWaitFor) -> Threading::ContinuationFuture<PollingEventType::BitField>
    {
        Threading::ContinuationFuture<PollingEventType::BitField> result;
        {
            ScopedLock(_pimpl->_interfaceLock);
            Pimpl::PendingInitiates pendingInit;
            pendingInit._eventTypes = typesToWaitFor;
            pendingInit._platformHandle = platformHandle;
            result = pendingInit._promise.get_future();
            _pimpl->_pendingInitiates.push_back(std::move(pendingInit));
        }
        _pimpl->InterruptBackgroundThread();
        return result;
    }

    PollingThread::PollingThread()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    PollingThread::~PollingThread()
    {

    }

}


