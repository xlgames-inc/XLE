#include "Log.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/AssetUtils.h"
#include "../Assets/DepVal.h"
#include "../Utility/StringFormat.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Foreign/fmt/format.h"
#include <iostream>

namespace ConsoleRig
{
    template<typename CharType, typename CharTraits>
        std::streamsize MessageTarget<CharType, CharTraits>::FormatAndOutput(
            StringSection<char> msg,
            const std::string& fmtTemplate,
            const SourceLocation& sourceLocation)
    {
        if (!fmtTemplate.empty()) {
            auto fmt = fmt::format(
                fmtTemplate,
                fmt::arg("file", sourceLocation._file),
                fmt::arg("line", sourceLocation._line));
            _chain->sputn(fmt.data(), fmt.size());
        }
        return _chain->sputn(msg.begin(), msg.size());       // (note; don't include the length of the formatted section; because it will confuse the caller when it is a basic_ostream
    }

    template<typename CharType, typename CharTraits>
        std::streamsize MessageTarget<CharType, CharTraits>::xsputn(const CharType* s, std::streamsize count)
    {
        if (_cfg._enabledSinks & MessageTargetConfiguration::Sink::Console) {
            auto result = FormatAndOutput(
                MakeStringSection(s, s + count),
                _sourceLocationPrimed ? _cfg._template : std::string(),
                _pendingSourceLocation);
            _sourceLocationPrimed = false;
            return result;
        } else {
            return 0;
        }
    }

    template<typename CharType, typename CharTraits>
        auto MessageTarget<CharType, CharTraits>::overflow(int_type ch) -> int_type
    {
        if (std::basic_streambuf<CharType, CharTraits>::traits_type::not_eof(ch)) {
            if (_cfg._enabledSinks & MessageTargetConfiguration::Sink::Console) {
                _chain->sputc((CharType)ch);
                _sourceLocationPrimed |= std::basic_streambuf<CharType, CharTraits>::traits_type::eq_int_type(ch, (int_type)'\n');
                static_assert(0!=std::basic_streambuf<CharType, CharTraits>::traits_type::eof(), "Expecting char traits EOF character to be something other than 0");
                return 0;   // (anything other than traits_type::eof() signifies success)
            }
        }

        return std::basic_streambuf<CharType, CharTraits>::overflow(ch);
    }

    template<typename CharType, typename CharTraits>
        int MessageTarget<CharType, CharTraits>::sync() { return _chain->pubsync(); }

    template<>
        std::basic_streambuf<char>& MessageTarget<char>::DefaultChain()
    {
        return *std::cout.rdbuf();
    }

    template class MessageTarget<>;

////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(CONSOLERIG_ENABLE_LOG)

    class LogConfigurationSet
    {
    public:
        MessageTargetConfiguration ResolveConfig(StringSection<> name) const;
        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }
        
        void Set(StringSection<> id, MessageTargetConfiguration& cfg);

        LogConfigurationSet();
        LogConfigurationSet(
            InputStreamFormatter<char>& formatter,
            const ::Assets::DirectorySearchRules&,
			const std::shared_ptr<::Assets::DependencyValidation>& depVal);
        ~LogConfigurationSet();
    private:
        class Config
        {
        public:
            std::vector<std::string> _inherit;
            MessageTargetConfiguration _cfg;
        };

        std::vector<std::pair<std::string, Config>> _configs;
        std::shared_ptr<::Assets::DependencyValidation> _depVal;

        Config LoadConfig(InputStreamFormatter<char>& formatter);
    };

    static void MergeIn(MessageTargetConfiguration& dst, const MessageTargetConfiguration& src)
    {
        if (!src._template.empty())
            dst._template = src._template;
        dst._enabledSinks |= src._enabledSinks;
        dst._enabledSinks &= ~src._disabledSinks;
    }

    MessageTargetConfiguration LogConfigurationSet::ResolveConfig(StringSection<> name) const
    {
        MessageTargetConfiguration result;
        auto i = std::find_if(_configs.begin(), _configs.end(),
            [name](const std::pair<std::string, Config>&p) { return XlEqString(MakeStringSection(p.first), name); });
        if (i == _configs.end()) return result;

        const auto& src = i->second;
        for (const auto&inherit:src._inherit)
            MergeIn(result, ResolveConfig(inherit));
        MergeIn(result, src._cfg);
        return result;
    }

    void LogConfigurationSet::Set(StringSection<> id, MessageTargetConfiguration& cfg)
    {
        auto str = id.AsString();
        auto existing = std::find_if(
            _configs.begin(), _configs.end(),
            [str](const std::pair<std::string, Config>& p) { return p.first == str; });
        if (existing != _configs.end()) {
            existing->second = {{}, cfg};
        } else {
            _configs.emplace_back(std::make_pair(str, Config{{}, cfg}));
        }
    }

    LogConfigurationSet::LogConfigurationSet() {}
    LogConfigurationSet::LogConfigurationSet(
        InputStreamFormatter<char>& formatter,
        const ::Assets::DirectorySearchRules&,
        const std::shared_ptr<::Assets::DependencyValidation>& depVal)
    : _depVal(depVal)
    {
        for (;;) {
            using Blob = InputStreamFormatter<char>::Blob;
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
            case Blob::AttributeValue:
                break;

            case Blob::BeginElement:
                {
                    InputStreamFormatter<char>::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                    auto cfg = LoadConfig(formatter);
                    _configs.emplace_back(std::make_pair(eleName.AsString(), std::move(cfg)));

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    break;
                }

            case Blob::EndElement:
            case Blob::None:
                return;
            }
        }
    }

    auto LogConfigurationSet::LoadConfig(InputStreamFormatter<char>& formatter) -> Config
    {
        Document<InputStreamFormatter<char>> doc(formatter);
        Config cfg;
        if (doc.Attribute("OutputToConsole")) {
            bool outputToConsole = doc.Attribute("OutputToConsole", false);
            cfg._cfg._enabledSinks = (outputToConsole ? MessageTargetConfiguration::Sink::Console : 0u);
            cfg._cfg._disabledSinks = ((!outputToConsole) ? MessageTargetConfiguration::Sink::Console : 0u);
        }
        cfg._cfg._template = doc.Attribute("Template").Value().AsString();

        auto inheritList = doc.Element("Inherit").FirstAttribute();
        while (inheritList) {
            cfg._inherit.push_back(inheritList.Name().AsString());
            inheritList = inheritList.Next();
        }

        return cfg;
    }

    LogConfigurationSet::~LogConfigurationSet() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    static std::shared_ptr<LogConfigurationSet> LoadConfigSet(StringSection<> fn)
    {
        size_t fileSize = 0;
        auto file = ::Assets::TryLoadFileAsMemoryBlock(fn, &fileSize);
        if (!file.get() || !fileSize)
            return nullptr;
        
        InputStreamFormatter<char> fmtr(MemoryMappedInputStream(file.get(), PtrAdd(file.get(), fileSize)));
        auto depVal = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(depVal, fn);
        return std::make_shared<LogConfigurationSet>(fmtr, ::Assets::DirectorySearchRules(), depVal);
    }

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////

    class LogCentral::Pimpl
    {
    public:
        struct Target
        {
            std::string _id;
            MessageTarget<>* _target;
        };
        std::vector<std::pair<uint64_t, Target>> _activeTargets;

        #if defined(CONSOLERIG_ENABLE_LOG)
            std::shared_ptr<LogConfigurationSet> _activeCfgSet;
        #endif
    };

    LogCentral& LogCentral::GetInstance()
    {
        static LogCentral instance;
        return instance;
    }

    void LogCentral::Register(MessageTarget<>& target, StringSection<> id)
    {
        assert(!id.IsEmpty());      // empty id's are not supported -- without the id, there's no way to assign a configuration

        auto hash = Hash64(id);
        auto i = LowerBound(_pimpl->_activeTargets, hash);

        // If you hit the following assert, it means there are 2 message targets with the same name
        // This could happen if a target is copied from one place to another, or if it's defined in
        // a header, instead of a source file.
        assert(i == _pimpl->_activeTargets.end() || i->first != hash);

        _pimpl->_activeTargets.insert(i, std::make_pair(hash, Pimpl::Target{id.AsString(), &target}));

        #if defined(CONSOLERIG_ENABLE_LOG)
            // Set the initial configuration
            if (_pimpl->_activeCfgSet)
                target.SetConfiguration(_pimpl->_activeCfgSet->ResolveConfig(id));
        #endif
    }

    void LogCentral::Deregister(MessageTarget<>& target)
    {
        auto i = std::find_if(_pimpl->_activeTargets.begin(), _pimpl->_activeTargets.end(),
            [&target](const std::pair<uint64_t, Pimpl::Target>& p) { return p.second._target == &target; });
        assert(i!=_pimpl->_activeTargets.end());
        _pimpl->_activeTargets.erase(i);
    }

    void LogCentral::SetConfiguration(const std::shared_ptr<LogConfigurationSet>& cfgs)
    {
        #if defined(CONSOLERIG_ENABLE_LOG)
            _pimpl->_activeCfgSet = cfgs;
            if (cfgs) {
                for (const auto&t:_pimpl->_activeTargets)
                    t.second._target->SetConfiguration(cfgs->ResolveConfig(t.second._id));
            } // else reset to default?
        #endif
    }

    LogCentral::LogCentral()
    {
        _pimpl = std::make_unique<Pimpl>();
        // We can't load the config set here, because we will frequently get here during static initialization
        // before the file system has been properly initialized
    }

    LogCentral::~LogCentral() 
    {
        snprintf(buffer, dimof(buffer), format, args);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void LogCentralConfiguration::Set(StringSection<> id, MessageTargetConfiguration& cfg)
    {
        #if defined(CONSOLERIG_ENABLE_LOG)
            _cfgSet->Set(id, cfg);

            // Reapply all configurations to the LogCentral in the local module
            LogCentral::GetInstance().SetConfiguration(_cfgSet);
            /*
            auto& central = LogCentral::GetInstance();
            auto hash = Hash64(id);
            auto i = LowerBound(central._pimpl->_activeTargets, hash);
            if (i!=central._pimpl->_activeTargets.end() && i->first == hash)
                i->second._target->SetConfiguration(cfg);
            */
        #endif
    }

    void LogCentralConfiguration::CheckHotReload()
    {
        #if defined(CONSOLERIG_ENABLE_LOG)
            if (!_cfgSet || !_cfgSet->GetDependencyValidation() || _cfgSet->GetDependencyValidation()->GetValidationIndex() > 0) {
                _cfgSet = LoadConfigSet("log.dat");
                LogCentral::GetInstance().SetConfiguration(_cfgSet);
            }
        #endif
    }

    void LogCentralConfiguration::AttachCurrentModule()
    {
        assert(s_instance == nullptr);
        s_instance = this;
        LogCentral::GetInstance().SetConfiguration(_cfgSet);
    }

    void LogCentralConfiguration::DetachCurrentModule()
    {
        assert(s_instance == this);
        s_instance = nullptr;
         #if defined(CONSOLERIG_ENABLE_LOG)
            LogCentral::GetInstance().SetConfiguration(nullptr);
        #endif
    }

    LogCentralConfiguration* LogCentralConfiguration::s_instance = nullptr;

    LogCentralConfiguration::LogCentralConfiguration()
    {
        #if defined(CONSOLERIG_ENABLE_LOG)
            _cfgSet = LoadConfigSet("log.dat");
        #endif
    }

    LogCentralConfiguration::~LogCentralConfiguration() 
    {
    }


}

ConsoleRig::MessageTarget<> Error("Error");
ConsoleRig::MessageTarget<> Warning("Warning");
ConsoleRig::MessageTarget<> Debug("Debug");
ConsoleRig::MessageTarget<> Verbose("Verbose");


#elif PLATFORMOS_TARGET == PLATFORMOS_LINUX

namespace el { namespace base { namespace utils
{
    void DateTime::gettimeofday(struct timeval *tv) {
      ::gettimeofday((struct ::timeval*)tv, nullptr);
    }
}}}

#endif
