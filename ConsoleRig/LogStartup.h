// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include <string>

namespace ConsoleRig
{
    /// <summary>Initialise the logging system</summary>
    /// Provide a filename for an optional configuration file.
    /// The configuration file should be defined in the format defined by the 
    /// "easylogging++" library.
    /// Logging_Shutdown() is optional, but helps shutdown cleanly
    void Logging_Startup(const char configFile[] = nullptr, const char logFileName[] = nullptr);
    void Logging_Shutdown();

    enum class LogLevel
    {
        Fatal,
        Error,
        Warning,
        Info,
        Verbose
    };

    /// <summary>Base class from which custom log handles can be derived</summary>
    /// The callback is initialised in a disabled state. Use Enable() to install
    /// the callback and start receiving events.
    class LogCallback : std::enable_shared_from_this<LogCallback>
    {
    public:
        virtual void OnDispatch(LogLevel, const std::string&) = 0;

        void Enable();
        void Disable();
        LogCallback();
        virtual ~LogCallback();

    private:
        uint64 _guid;
    };
}
