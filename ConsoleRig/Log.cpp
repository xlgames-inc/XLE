// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Log.h"
#include "OutputStream.h"
#include "GlobalServices.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/FunctionUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/SystemUtils.h"
#include <assert.h>

    // We can't use the default initialisation method for easylogging++
    // because is causes a "LoaderLock" exception when used with C++/CLI dlls.
    // It also doesn't work well when sharing a single log file across dlls.
    // Anyway, the default behaviour isn't great for our needs.
    // So, we need to use "INITIALIZE_NULL" here, and manually construct
    // a "GlobalStorage" object below...
INITIALIZE_NULL_EASYLOGGINGPP

#if defined(_DEBUG)
    #define REDIRECT_COUT
#endif

//////////////////////////////////

static auto Fn_GetStorage = ConstHash64<'getl', 'ogst', 'orag', 'e'>::Value;
static auto Fn_CoutRedirectModule = ConstHash64<'cout', 'redi', 'rect'>::Value;
static auto Fn_LogMainModule = ConstHash64<'logm', 'ainm', 'odul', 'e'>::Value;

namespace ConsoleRig
{
    #if defined(REDIRECT_COUT)
        template <typename CharType>
            class StdCToXLEStreamAdapter : public std::basic_streambuf<CharType>
        {
        public:
            void Reset(std::shared_ptr<Utility::OutputStream> chain) { _chain = chain; }
            StdCToXLEStreamAdapter(std::shared_ptr<Utility::OutputStream> chain);
            ~StdCToXLEStreamAdapter();
        protected:
            std::shared_ptr<Utility::OutputStream> _chain;

            virtual std::streamsize xsputn(const CharType* s, std::streamsize count);
            virtual int sync();
        };

        template <typename CharType>
            StdCToXLEStreamAdapter<CharType>::StdCToXLEStreamAdapter(std::shared_ptr<Utility::OutputStream> chain) : _chain(chain) {}
        template <typename CharType>
            StdCToXLEStreamAdapter<CharType>::~StdCToXLEStreamAdapter() {}

        template <typename CharType>
            std::streamsize StdCToXLEStreamAdapter<CharType>::xsputn(const CharType* s, std::streamsize count)
        {
            assert(_chain);
            _chain->Write(s, int(sizeof(CharType) * count));
            return count;
        }

        template <typename CharType>
            int StdCToXLEStreamAdapter<CharType>::sync()
        {
            _chain->Flush();
            return 0;
        }

        std::shared_ptr<Utility::OutputStream>      GetSharedDebuggerWarningStream();

        static StdCToXLEStreamAdapter<char> s_coutAdapter(nullptr);
        static std::basic_streambuf<char>* s_oldCoutStreamBuf = nullptr;
    #endif

    void Logging_Startup(const char configFile[], const char logFileName[])
    {
        auto currentModule = GetCurrentModuleId();
        auto& serv = GlobalServices::GetCrossModule()._services;

            // It can be handy to redirect std::cout to the debugger output
            // window in Visual Studio (etc)
            // We can do this with an adapter to connect out DebufferWarningStream
            // object to a c++ std::stream_buf
        #if defined(REDIRECT_COUT)
            
            if (!serv.Has<ModuleId()>(Fn_CoutRedirectModule)) {
                s_coutAdapter.Reset(GetSharedDebuggerWarningStream());
                s_oldCoutStreamBuf = std::cout.rdbuf();
                std::cout.rdbuf(&s_coutAdapter);

                serv.Add(Fn_CoutRedirectModule, [=](){ return currentModule; });
            }

        #endif

        using StoragePtr = decltype(el::Helpers::storage());

            //
            //  Check to see if there is an existing logging object in the
            //  global services. If there is, it will have been created by
            //  another module.
            //  If it's there, we can just re-use it. Otherwise we need to
            //  create a new one and set it up...
            //
        if (!serv.Has<StoragePtr()>(Fn_GetStorage)) {

            el::Helpers::setStorage(
                std::make_shared<el::base::Storage>(
                    el::LogBuilderPtr(new el::base::DefaultLogBuilder())));

            if (!logFileName) { logFileName = "int/log.txt"; }
            el::Configurations c;
            c.setToDefault();
            c.setGlobally(el::ConfigurationType::Filename, logFileName);

                // if a configuration file exists, 
            if (configFile) {
                size_t configFileLength = 0;
                auto configFileData = LoadFileAsMemoryBlock(configFile, &configFileLength);
                if (configFileData && configFileLength) {
                    c.parseFromText(std::string(configFileData.get(), &configFileData[configFileLength]));
                }
            }

            el::Loggers::reconfigureAllLoggers(c);

            serv.Add(Fn_GetStorage, el::Helpers::storage);
            serv.Add(Fn_LogMainModule, [=](){ return currentModule; });

        } else {

            auto storage = serv.Call<StoragePtr>(Fn_GetStorage);
            el::Helpers::setStorage(storage);

        }
    }

    void Logging_Shutdown()
    {
        auto& serv = GlobalServices::GetCrossModule()._services;
        auto currentModule = GetCurrentModuleId();

        el::Loggers::flushAll();
        el::Helpers::setStorage(nullptr);

            // this will throw an exception if no module has successfully initialised
            // logging
        if (serv.Call<ModuleId>(Fn_LogMainModule) == currentModule) {
            serv.Remove(Fn_GetStorage);
            serv.Remove(Fn_LogMainModule);
        }

        #if defined(REDIRECT_COUT)
            ModuleId testModule = 0;
            if (serv.TryCall<ModuleId>(testModule, Fn_CoutRedirectModule) && (testModule == currentModule)) {
                if (s_oldCoutStreamBuf)
                    std::cout.rdbuf(s_oldCoutStreamBuf);
                serv.Remove(Fn_CoutRedirectModule);
            }
        #endif
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


#include "../Core/WinAPI/IncludeWindows.h"

namespace el { namespace base { namespace utils
{
#define _ELPP_OS_WINDOWS 1
#define ELPP_COMPILER_MSVC 1

#if _ELPP_OS_WINDOWS
    void DateTime::gettimeofday(struct timeval *tv) {
        if (tv != nullptr) {
#   if ELPP_COMPILER_MSVC || defined(_MSC_EXTENSIONS)
            const unsigned __int64 delta_ = 11644473600000000Ui64;
#   else
            const unsigned __int64 delta_ = 11644473600000000ULL;
#   endif  // ELPP_COMPILER_MSVC || defined(_MSC_EXTENSIONS)
            const double secOffSet = 0.000001;
            const unsigned long usecOffSet = 1000000;
            FILETIME fileTime;
            GetSystemTimeAsFileTime(&fileTime);
            unsigned __int64 present = 0;
            present |= fileTime.dwHighDateTime;
            present = present << 32;
            present |= fileTime.dwLowDateTime;
            present /= 10;  // mic-sec
           // Subtract the difference
            present -= delta_;
            tv->tv_sec = static_cast<long>(present * secOffSet);
            tv->tv_usec = static_cast<long>(present % usecOffSet);
        }
    }
#endif // _ELPP_OS_WINDOWS
}}}
