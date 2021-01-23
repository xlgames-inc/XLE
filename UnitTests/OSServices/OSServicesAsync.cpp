// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../OSServices/Linux/PollingThread.h"
#include "../../OSServices/FileSystemMonitor.h"
#include "../../OSServices/RawFS.h"
#include "../../Utility/Threading/ThreadingUtils.h"
#include "../../Utility/Threading/LockFree.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "thousandeyes/futures/then.h"
#include "thousandeyes/futures/DefaultExecutor.h"
#include <stdexcept>
#include <iostream>
#include <random>
#include <filesystem>

// linux specific...
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace OSServices
{
	class IConduit_Linux
	{
	public:
		virtual IOPlatformHandle GetPlatformHandle() const = 0;
	};
}

using namespace Catch::literals;
namespace UnitTests
{
    TEST_CASE( "PollingThread-UnderlyingInterface", "[osservices]" )
    {
        OSServices::PollingThread pollingThread;

        auto executor = std::make_shared<thousandeyes::futures::DefaultExecutor>(std::chrono::milliseconds(2));
        thousandeyes::futures::Default<thousandeyes::futures::Executor>::Setter execSetter(executor);

        SECTION("RespondOnce with stall")
        {
            auto testEvent = eventfd(0, EFD_NONBLOCK);
            
            // Here, the event trigger is going to happen before we call RespondOnce
            uint64_t t = 1;
            write(testEvent, &t, sizeof(t));
            
            auto resString = thousandeyes::futures::then(
                pollingThread.RespondOnce(testEvent),
                [](auto) {
                    return "String returned from future";
                }).get();

            REQUIRE(resString == "String returned from future");
            close(testEvent);
        }
        
        SECTION("RespondOnce with continuation")
        {
            auto testEvent = eventfd(0, EFD_NONBLOCK);
            volatile bool trigger = false;
            auto future = thousandeyes::futures::then(
                pollingThread.RespondOnce(testEvent),
                [&trigger](auto) {
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

        SECTION("Thrash RespondOnce")
        {
            // This is a horrible nightmare of beginning and ending RespondOnce. It should really
            // give the PollingThread implementation a good test

            const unsigned iterations = 1000;

            std::atomic<signed> eventsInFlight(0);
            std::vector<int> events;
            LockFreeFixedSizeQueue<int, 500> eventPool;
            std::vector<std::future<unsigned>> futures;
            events.reserve(500);
            for (unsigned c=0; c<500; ++c) {
                auto event = eventfd(0, EFD_NONBLOCK);
                ++eventsInFlight;
                auto future = thousandeyes::futures::then(
                    pollingThread.RespondOnce(event),
                    [&, event](auto) { 
                        eventPool.push_overflow(event); --eventsInFlight; return 0u; 
                    });
                futures.push_back(std::move(future));
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
                    auto future = thousandeyes::futures::then(
                        pollingThread.RespondOnce(reusableEvent),
                        [&, reusableEvent](auto) { eventPool.push_overflow(reusableEvent); --eventsInFlight; return 0u; });
                    futures.push_back(std::move(future));
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

        SECTION("Conduit for eventfd")
        {
            class EventFDConduit : public OSServices::IConduit, public OSServices::IConduit_Linux
            {
            public:
                int _platformHandle = 0;
                int _eventCount = 0;
                int _exceptionCount = 0;

                void OnEvent(OSServices::PollingEventType::BitField)
                {
                    uint64_t eventFdCounter=0;
                    auto ret = read(_platformHandle, &eventFdCounter, sizeof(eventFdCounter));
                    assert(ret > 0);
                    _eventCount += eventFdCounter;
                }

		        void OnException(const std::exception_ptr& exception)
                {
                    ++_exceptionCount;
                }

                OSServices::IOPlatformHandle GetPlatformHandle() const { return _platformHandle; }

                EventFDConduit()
                {
                    _platformHandle = eventfd(0, EFD_NONBLOCK);
                }

                ~EventFDConduit()
                {
                    close(_platformHandle);
                }
            };

            auto conduit = std::make_shared<EventFDConduit>();
            auto connectionFuture = pollingThread.Connect(conduit);
            connectionFuture.get();            

            const unsigned writeCount = 15;
            for (unsigned c=0; c<writeCount; ++c) {
                uint64_t t = 1;
                write(conduit->_platformHandle, &t, sizeof(t));
            }

            auto disconnectionFuture = pollingThread.Disconnect(conduit);
            disconnectionFuture.get();

            REQUIRE(conduit->_exceptionCount == 0);
            REQUIRE(conduit->_eventCount == writeCount);
        }
    }

    TEST_CASE( "PollingThread-FileChangeNotification", "[osservices]" )
    {
        auto executor = std::make_shared<thousandeyes::futures::DefaultExecutor>(std::chrono::milliseconds(2));
        thousandeyes::futures::Default<thousandeyes::futures::Executor>::Setter execSetter(executor);

        auto pollingThread = std::make_shared<OSServices::PollingThread>();

        auto tempDirPath = std::filesystem::temp_directory_path() / "xle-unit-tests";
        std::filesystem::create_directories(tempDirPath);

        class CountChanges : public OSServices::OnChangeCallback
        {
        public:
            signed _changes = 0;
            void OnChange() override { ++_changes; }
        };

        OSServices::RawFSMonitor monitor(pollingThread);
        auto changesToOne = std::make_shared<CountChanges>();
        monitor.Attach(MakeStringSection(tempDirPath.string() + "/one.txt"), changesToOne);

        auto changesToTwo = std::make_shared<CountChanges>();
        monitor.Attach(MakeStringSection(tempDirPath.string() + "/two.txt"), changesToTwo);

        auto changesToThree = std::make_shared<CountChanges>();
        monitor.Attach(MakeStringSection(tempDirPath.string() + "/three.txt"), changesToThree);

        SECTION("Detect file writes")
        {
            char strToWrite[] = "This is a string written by XLE unit tests";
            OSServices::BasicFile{(tempDirPath.string() + "/one.txt").c_str(), "wb", 0}.Write(strToWrite, 1, sizeof(strToWrite));
            OSServices::BasicFile{(tempDirPath.string() + "/three.txt").c_str(), "wb", 0}.Write(strToWrite, 1, sizeof(strToWrite));

            // give a little bit of time incase the background thread needs to catchup to all of the writes
            Threading::Sleep(1000);
            REQUIRE(changesToOne->_changes > 0);
            REQUIRE(changesToTwo->_changes == 0);
            REQUIRE(changesToThree->_changes > 0);
            auto midwayChangesToThree = changesToThree->_changes;

            OSServices::BasicFile{(tempDirPath.string() + "/two.txt").c_str(), "wb", 0}.Write(strToWrite, 1, sizeof(strToWrite));
            OSServices::BasicFile{(tempDirPath.string() + "/three.txt").c_str(), "wb", 0}.Write(strToWrite, 1, sizeof(strToWrite));
            Threading::Sleep(1000);
            REQUIRE(changesToTwo->_changes > 0);
            REQUIRE(changesToThree->_changes > midwayChangesToThree);
        }

        std::filesystem::remove_all(tempDirPath);
    }
}
