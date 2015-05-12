// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Log.h"
#include "OutputStream.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/Stream.h"
#include <assert.h>

#include "../Utility/PtrUtils.h"
#include "../Utility/IteratorUtils.h"


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

    template<typename Type> Type DefaultValue() { return Type(0); }

    class StoredFunctionSet
    {
    public:
        template<typename Fn>
            void StoreFunction(uint64 guid, std::function<Fn>&& fn);

        template<typename Result, typename... Args>
            Result CallFunction(uint64 guid, Args... args);

        StoredFunctionSet();
        ~StoredFunctionSet();
    protected:
        class StoredFunction
        {
        public:
            size_t      _offset;
            size_t      _size;
            void (*_destructor) (void*);
        };
        std::vector<uint8> _buffer;
        std::vector<std::pair<uint64, StoredFunction>> _fns;
    };

    template<typename Type>
        static void DestroyObject(void* obj)
        {
            reinterpret_cast<Type*>(obj)->~Type();
        }

    template<typename Fn>
        void StoredFunctionSet::StoreFunction(uint64 guid, std::function<Fn>&& fn)
    {
        auto i = LowerBound(_fns, guid);
        if (i != _fns.end() && i->first == guid) { assert(0); return; } // duplicate of one already here!

        StoredFunction sfn;
        sfn._offset = _buffer.size();
        sfn._size = sizeof(std::function<Fn>);
        sfn._destructor = &DestroyObject<Fn>;
        _fns.insert(i, std::make_pair(guid, sfn));
        _buffer.insert(_buffer.end(), sfn._size, uint8(0));

        auto* dst = (Fn*)PtrAdd(AsPointer(_buffer.begin()), sfn._offset);

        (*dst) = std::move(fn);
    }

    template<typename Result, typename... Args>
        Result StoredFunctionSet::CallFunction(uint64 guid, Args... args)
    {
        auto i = LowerBound(_fns, guid);
        if (i == _fns.end() || i->first != guid)
            return DefaultValue<Result>();
        
        auto* obj = (void*)PtrAdd(AsPointer(_buffer.begin()), i->second._offset);
        auto* fn = reinterpret_cast<std::function<Result(Args...)>*>(obj);

        return (*fn)(args...);
    }

    StoredFunctionSet::StoredFunctionSet() {}
    StoredFunctionSet::~StoredFunctionSet()
    {
        // Ok, here's the crazy part.. we want to call the destructors
        // of all of the types we've stored in here... But we don't know their
        // types! But we have a pointer to a function that will call their
        // destructor. So we just need to call that.
        for (auto i=_fns.begin(); i!=_fns.end(); ++i) {
            auto* obj = (void*)PtrAdd(AsPointer(_buffer.begin()), i->second._offset);
            (*i->second._destructor)(obj);
        }
    }

    void Logging_Startup(const char configFile[], const char logFileName[])
    {
            // It can be handy to redirect std::cout to the debugger output
            // window in Visual Studio (etc)
            // We can do this with an adapter to connect out DebufferWarningStream
            // object to a c++ std::stream_buf
        #if defined(REDIRECT_COUT)
            {
                s_coutAdapter.Reset(GetSharedDebuggerWarningStream());
                s_oldCoutStreamBuf = std::cout.rdbuf();
                std::cout.rdbuf(&s_coutAdapter);
            }
        #endif

        // StoredFunctionSet storedFns;
        // storedFns.StoreFunction(0, [](int lhs, int rhs) -> int { return lhs + rhs; });
        // storedFns.StoreFunction(1, [](int lhs, int rhs) -> int { return lhs * rhs; });
        // 
        // auto res = storedFns.CallFunction<int>(0, 10, 20);
        // auto res2 = storedFns.CallFunction<int>(1, 6, 6);
        // (void)res; (void)res2;

        el::Helpers::setStorage(
            std::make_shared<el::base::Storage>(
                el::LogBuilderPtr(new el::base::DefaultLogBuilder())));

            // -- note --   if we're inside a DLL, we can cause the logging system
            //              to use the same logging as the main executable. (Well, at
            //              least it's supported in the newest version of easy logging,
            //              it's possible it won't work with the version we're using)
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
    }

    void Logging_Shutdown()
    {
        el::Loggers::flushAll();
        el::Helpers::setStorage(nullptr);

        #if defined(REDIRECT_COUT)
            if (s_oldCoutStreamBuf)
                std::cout.rdbuf(s_oldCoutStreamBuf);
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
