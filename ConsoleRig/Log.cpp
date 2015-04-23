// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Log.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Core/WinAPI/IncludeWindows.h"

_INITIALIZE_EASYLOGGINGPP

namespace ConsoleRig
{
    void Logging_Startup(const char configFile[], const char logFileName[])
    {
            // -- note --   if we're inside a DLL, we can cause the logging system
            //              to use the same logging as the main executable. (Well, at
            //              least it's supported in the newest version of easy logging,
            //              it's possible it won't work with the version we're using)
        if (!logFileName) { logFileName = "int/log.txt"; }
        easyloggingpp::Configurations c;
        c.setToDefault();
        c.setAll(easyloggingpp::ConfigurationType::Filename, logFileName);

            // if a configuration file exists, 
        if (configFile) {
            size_t configFileLength = 0;
            auto configFileData = LoadFileAsMemoryBlock(configFile, &configFileLength);
            if (configFileData && configFileLength) {
                c.parseFromText(std::string(configFileData.get(), &configFileData[configFileLength]));
            }
        }

        // easyloggingpp::Loggers::reconfigureAllLoggers(c);

        if (easyloggingpp::internal::registeredLoggers.pointer()) {
            easyloggingpp::internal::registeredLoggers->registerNew(new easyloggingpp::Logger(
                "trivial", easyloggingpp::internal::registeredLoggers->constants(), c));
        }
    }

    void Logging_Shutdown()
    {
        easyloggingpp::internal::registeredLoggers = decltype(easyloggingpp::internal::registeredLoggers)();
    }
}

namespace LogUtilMethods
{

        //  note that we can't pass a printf style string to easylogging++ easily.
        //  but we do have some utility functions (like PrintFormatV) that can
        //  handle long printf's.
    static const unsigned LogStringMaxLength = 2048;

    void LogVerboseF(unsigned level, const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        _vsnprintf_s(buffer, _TRUNCATE, format, args);
        va_end(args);

        LogVerbose(level) << buffer;
    }

    void LogInfoF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        _vsnprintf_s(buffer, _TRUNCATE, format, args);
        va_end(args);

        LogInfo << buffer;
    }

    void LogWarningF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        _vsnprintf_s(buffer, _TRUNCATE, format, args);
        va_end(args);

        LogWarning << buffer;
    }
    
    void LogAlwaysVerboseF(unsigned level, const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        _vsnprintf_s(buffer, _TRUNCATE, format, args);
        va_end(args);

        LogAlwaysVerbose(level) << buffer;
    }

    void LogAlwaysInfoF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        _vsnprintf_s(buffer, _TRUNCATE, format, args);
        va_end(args);

        LogAlwaysInfo << buffer;
    }

    void LogAlwaysWarningF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        _vsnprintf_s(buffer, _TRUNCATE, format, args);
        va_end(args);

        LogAlwaysWarning << buffer;
    }

    void LogAlwaysErrorF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        _vsnprintf_s(buffer, _TRUNCATE, format, args);
        va_end(args);

        LogAlwaysError << buffer;
    }

    void LogAlwaysFatalF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        _vsnprintf_s(buffer, _TRUNCATE, format, args);
        va_end(args);

        LogAlwaysFatal << buffer;
    }
}



namespace easyloggingpp { namespace internal { namespace utilities
{
#if _ELPP_OS_WINDOWS
    void DateUtils::gettimeofday(struct TimeType *tv) {
        if (tv != NULL) {
#   if defined(_MSC_EXTENSIONS)
            const unsigned __int64 delta_ = 11644473600000000Ui64;
#   else
            const unsigned __int64 delta_ = 11644473600000000ULL;
#   endif // defined(_MSC_EXTENSIONS)
            const double secOffSet = 0.000001;
            const unsigned long usecOffSet = 1000000;
            FILETIME fileTime_;
            GetSystemTimeAsFileTime(&fileTime_);
            unsigned __int64 present_ = 0;
            present_ |= fileTime_.dwHighDateTime;
            present_ = present_ << 32;
            present_ |= fileTime_.dwLowDateTime;
            present_ /= 10; // mic-sec
            // Subtract the difference
            present_ -= delta_;
            tv->tv_sec = static_cast<long>(present_ * secOffSet);
            tv->tv_usec = static_cast<long>(present_ % usecOffSet);
        }
    }
#endif // _ELPP_OS_WINDOWS

    // Gets current date and time with milliseconds.
    std::string DateUtils::getDateTime(const std::string& bufferFormat_, unsigned int type_, internal::Constants* constants_, std::size_t milliSecondOffset_) {
        long milliSeconds = 0;
        const int kDateBuffSize_ = 30;
        char dateBuffer_[kDateBuffSize_] = "";
        char dateBufferOut_[kDateBuffSize_] = "";
#if _ELPP_OS_UNIX
        bool hasTime_ = ((type_ & constants_->kDateTime) || (type_ & constants_->kTimeOnly));
        timeval currTime;
        gettimeofday(&currTime, NULL);
        if (hasTime_) {
            milliSeconds = currTime.tv_usec / milliSecondOffset_ ;
        }
        struct tm * timeInfo = localtime(&currTime.tv_sec);
        strftime(dateBuffer_, sizeof(dateBuffer_), bufferFormat_.c_str(), timeInfo);
        if (hasTime_) {
            SPRINTF(dateBufferOut_, "%s.%03ld", dateBuffer_, milliSeconds);
        } else {
            SPRINTF(dateBufferOut_, "%s", dateBuffer_);
        }
#elif _ELPP_OS_WINDOWS
        const char* kTimeFormatLocal_ = "HH':'mm':'ss";
        const char* kDateFormatLocal_ = "dd/MM/yyyy";
        if ((type_ & constants_->kDateTime) || (type_ & constants_->kDateOnly)) {
            if (GetDateFormatA(LOCALE_USER_DEFAULT, 0, 0, kDateFormatLocal_, dateBuffer_, kDateBuffSize_) != 0) {
                SPRINTF(dateBufferOut_, "%s", dateBuffer_);
            }
        }
        if ((type_ & constants_->kDateTime) || (type_ & constants_->kTimeOnly)) {
            if (GetTimeFormatA(LOCALE_USER_DEFAULT, 0, 0, kTimeFormatLocal_, dateBuffer_, kDateBuffSize_) != 0) {
                milliSeconds = static_cast<long>(GetTickCount()) % milliSecondOffset_;
                if (type_ & constants_->kDateTime) {
                    SPRINTF(dateBufferOut_, "%s %s.%03ld", dateBufferOut_, dateBuffer_, milliSeconds);
                } else {
                    SPRINTF(dateBufferOut_, "%s.%03ld", dateBuffer_, milliSeconds);
                }
            }
        }
#endif // _ELPP_OS_UNIX
        return std::string(dateBufferOut_);
    }

    std::string DateUtils::formatMilliSeconds(double milliSeconds_) {
        double result = milliSeconds_;
        std::string unit = "ms";
        std::stringstream stream_;
        if (result > 1000.0f) {
            result /= 1000; unit = "seconds";
            if (result > 60.0f) {
                result /= 60; unit = "minutes";
                if (result > 60.0f) {
                    result /= 60; unit = "hours";
                    if (result > 24.0f) {
                        result /= 24; unit = "days";
                    }
                }
            }
        }
        stream_ << result << " " << unit;
        return stream_.str();
    }

    double DateUtils::getTimeDifference(const TimeType& endTime_, const TimeType& startTime_) {
        return static_cast<double>((((endTime_.tv_sec - startTime_.tv_sec) * 1000000) + (endTime_.tv_usec - startTime_.tv_usec)) / 1000);
    }

}}}
