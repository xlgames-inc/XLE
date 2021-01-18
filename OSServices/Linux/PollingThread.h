// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Utility/Threading/ThreadingUtils.h"
#include <memory>
#include <cstdint>
#include <future>

namespace OSServices
{
    struct PollingEventType
    {
        enum Flags
        {
            Input = 1<<0,
            Output = 1<<1
        };
        using BitField = unsigned;
    };

    class PollingThread
    {
    public:
        using PlatformHandle = uint64_t;

        // Simple event type -- just tell me when something happens. One-off, with
        // no explicit cancel mechanism
        Threading::ContinuationFuture<PollingEventType::BitField> RespondOnEvent(
            PlatformHandle, 
            PollingEventType::BitField eventTypes = PollingEventType::Flags::Input);

        PollingThread();
        ~PollingThread();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

