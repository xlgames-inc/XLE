// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../OSServices/Linux/PollingThread.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Utility/Threading/LockFree.h"
#include <stdexcept>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <iostream>
#include <random>

// linux specific...
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace Catch::literals;
namespace UnitTests
{
    TEST_CASE( "PollingThread-UnderlyingInterface", "[utility]" )
    {
        OSServices::PollingThread pollingThread;

        SECTION("Basic queuing")
        {
            auto testEvent = eventfd(0, EFD_NONBLOCK);
            volatile bool trigger = false;
            auto future = pollingThread.RespondOnEvent(testEvent).then(
                [&trigger](OSServices::PollingEventType::BitField) {
                    std::cout << "Got the event" << std::endl;
                    trigger = true;
                    return "String returned from future";
                });

            Threading::Sleep(1000);
            uint64_t t = 1;
            write(testEvent, &t, sizeof(t));

            auto resString = future.get();
            REQUIRE(resString == "String returned from future");
            REQUIRE(trigger == true);
            close(testEvent);
        }

        SECTION("Thrash polling queue")
        {
            // This is a horrible nightmare of beginning and ending events. It should really
            // give the PollingThread implementation a good test

            const unsigned iterations = 1000;

            std::atomic<signed> eventsInFlight(0);
            std::vector<int> events;
            LockFree::FixedSizeQueue<int, 500> eventPool;
            std::vector<Threading::ContinuationFuture<unsigned>> futures;
            events.reserve(500);
            for (unsigned c=0; c<500; ++c) {
                auto event = eventfd(0, EFD_NONBLOCK);
                ++eventsInFlight;
                futures.push_back(pollingThread.RespondOnEvent(event).then( [&, event](auto) { 
                    eventPool.push_overflow(event); --eventsInFlight; return 0u; 
                    }));
                events.push_back(event);
            }

            std::mt19937 rng;
            for (unsigned c=0; c<iterations; ++c) {
                auto eventsToEnd = std::uniform_int_distribution<unsigned>(0, 5)(rng);
                auto eventsToBegin = std::uniform_int_distribution<unsigned>(0, 5)(rng);

                for (auto e=0; e<eventsToEnd && !events.empty(); ++e) {
                    auto i = events.begin() + std::uniform_int_distribution<unsigned>(0, events.size()-1)(rng);
                    uint64_t t = 1;
                    write(*i, &t, sizeof(t));
                    events.erase(i);
                }

                for (auto e=0; e<eventsToBegin; ++e) {
                    int* front = nullptr;
                    if (!eventPool.try_front(front))
                        continue;

                    auto reusableEvent = *front;
                    ++eventsInFlight;
                    futures.push_back(pollingThread.RespondOnEvent(reusableEvent).then( [&, reusableEvent](auto) { eventPool.push_overflow(reusableEvent); --eventsInFlight; return 0u; }));
                    events.push_back(reusableEvent);
                    eventPool.pop();
                }
            }

            // finish remaining events
            for (auto e:events) {
                uint64_t t = 1;
                write(e, &t, sizeof(t));
            }
            events.clear();

            for (auto&f:futures)
                f.get();

            REQUIRE(eventsInFlight.load() == 0);

            for (;;) {
                int* front = nullptr;
                if (!eventPool.try_front(front))
                    break;
                auto ret = close(*front);
                assert(ret == 0);
                eventPool.pop();
            }
        }
    
    }
}
