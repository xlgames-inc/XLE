
#pragma once

#include "../Utility/StringUtils.h"
#include <string>
#include <ostream>
#include <memory>

// Legacy naming -- 
#define LogWarning Log(Warning)
#define LogAlwaysError Log(Error)

#if defined(_DEBUG)
    #define CONSOLERIG_ENABLE_LOG
#endif

namespace ConsoleRig
{
    class SourceLocation
    {
    public:
        const char*     _file = nullptr;
        unsigned        _line = ~0u;
        const char*     _function = nullptr;
    };

    class MessageTargetConfiguration
    {
    public:
        std::string _template;
        struct Sink
        {
            enum Enum { Console = 1<<0 };
            using BitField = unsigned;
        };
        Sink::BitField _enabledSinks = Sink::Console;
        Sink::BitField _disabledSinks = 0;
    };

    template<typename CharType = char, typename CharTraits = std::char_traits<CharType>>
        class MessageTarget : public std::basic_streambuf<CharType, CharTraits>
    {
    public:
        void SetNextSourceLocation(const SourceLocation& sourceLocation) { _pendingSourceLocation = sourceLocation; _sourceLocationPrimed = true; }
        void SetConfiguration(const MessageTargetConfiguration& cfg) { _cfg = cfg; }

        MessageTarget(StringSection<> id, std::basic_streambuf<CharType, CharTraits>& chain = DefaultChain());
        ~MessageTarget();

        MessageTarget(const MessageTarget&) = delete;
        MessageTarget& operator=(const MessageTarget&) = delete;

        static std::basic_streambuf<CharType, CharTraits>& DefaultChain();
    private:
        std::basic_streambuf<CharType, CharTraits>* _chain;
        SourceLocation              _pendingSourceLocation;
        bool                        _sourceLocationPrimed;
        MessageTargetConfiguration _cfg;

        using int_type = typename std::basic_streambuf<CharType>::int_type;
        virtual std::streamsize xsputn(const CharType* s, std::streamsize count) override;
        virtual int_type overflow(int_type ch) override;
        virtual int sync() override;

        std::streamsize FormatAndOutput(
            StringSection<char> msg,
            const std::string& fmtTemplate,
            const SourceLocation& sourceLocation);
    };

    class LogConfigurationSet;

    /// <summary>Manages all message targets for a module</summary>
    /// LogCentral holds a list of all active logging message targets for a given current module.
    /// This list is used when we want to apply a configuration set.
    /// We separate the management of the message targets from the management of the configuration
    /// because we want to be able to use the message targets before the configuration has been
    /// loaded (ie, during early stages of initialization).
    /// Furthermore LogCentral is bound to a single module, but the logging configuration can
    /// be shared over multiple modules.
    class LogCentral
    {
    public:
        static LogCentral& GetInstance();

        void Register(MessageTarget<>& target, StringSection<> id);
        void Deregister(MessageTarget<>& target);

        void SetConfiguration(const std::shared_ptr<LogConfigurationSet>& cfgs);

        LogCentral();
        ~LogCentral();
        LogCentral& operator=(const LogCentral&) = delete;
        LogCentral(const LogCentral&) = delete;
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    /// <summary>Manages configuration settings for logging</summary>
    /// Can be shared between multiple different modules.
    class LogCentralConfiguration
    {
    public:
        void Set(StringSection<>, MessageTargetConfiguration& cfg);
        void CheckHotReload();

        static LogCentralConfiguration& GetInstance() { assert(s_instance); return *s_instance; }
        void AttachCurrentModule();
        void DetachCurrentModule();

        LogCentralConfiguration();
        ~LogCentralConfiguration();
    private:
        std::shared_ptr<LogConfigurationSet> _cfgSet;

        static LogCentralConfiguration* s_instance;
        void Apply();
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename CharType, typename CharTraits>
        MessageTarget<CharType, CharTraits>::MessageTarget(StringSection<> id, std::basic_streambuf<CharType, CharTraits>& chain)
    : _chain(&chain)
    {
        #if defined(CONSOLERIG_ENABLE_LOG)
            LogCentral::GetInstance().Register(*this, id);
        #endif
    }

    template<typename CharType, typename CharTraits>
        MessageTarget<CharType, CharTraits>::~MessageTarget()
    {
        _chain->pubsync();
        #if defined(CONSOLERIG_ENABLE_LOG)
            LogCentral::GetInstance().Deregister(*this);
        #endif
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename CharType>
        std::basic_ostream<CharType>& operator<<(std::basic_ostream<CharType>& ostream, const SourceLocation& sourceLocation)
    {
        auto* rdbuf = ostream.rdbuf();
        ((MessageTarget<CharType>*)rdbuf)->SetNextSourceLocation(sourceLocation);
        return ostream;
    }

    namespace Internal
    {
        constexpr const char* JustFilename(const char filePath[])
        {
            const char* pastLastSlash = filePath;
            for (const auto* i=filePath; *i; ++i)
                if (*i == '\\' || *i == '/') pastLastSlash = i+1;
            return pastLastSlash;
        }
    }

#if defined(CONSOLERIG_ENABLE_LOG)
	#if defined(__PRETTY_FUNCTION__)
		#define MakeSourceLocation (::ConsoleRig::SourceLocation {::ConsoleRig::Internal::JustFilename(__FILE__), __LINE__, __PRETTY_FUNCTION__})
	#else
		#define MakeSourceLocation (::ConsoleRig::SourceLocation {::ConsoleRig::Internal::JustFilename(__FILE__), __LINE__, __FUNCTION__})
	#endif
    #define Log(X) ::std::basic_ostream<typename std::remove_reference<decltype(X)>::type::char_type, typename std::remove_reference<decltype(X)>::type::traits_type>(&X) << MakeSourceLocation
#else
    #define Log(X) if (true) {} else *((std::ostream*)nullptr)
#endif

}

extern ConsoleRig::MessageTarget<> Error;
extern ConsoleRig::MessageTarget<> Warning;
extern ConsoleRig::MessageTarget<> Debug;
extern ConsoleRig::MessageTarget<> Verbose;

