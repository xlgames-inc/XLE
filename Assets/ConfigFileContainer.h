// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Assets.h"
#include "AssetUtils.h" // for DirectorySearchRules
#include "InvalidAssetManager.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/StringFormat.h"
#include <memory>

namespace Assets
{
    /// <summary>Container file with with one child that is initialized via InputStreamFormatter</summary>
    ///
    /// Represents a file that contains a single serialized item. That item must be a type that
    /// can be deserialized with InputStreamFormatter.
    ///
    /// <example>
    ///     Consider a configuration object like:
    ///     <code>\code
    ///         class Config
    ///         {
    ///         public:
    ///             Config( InputStreamFormatter<utf8>& formatter,
    ///                     const ::Assets::DirectorySearchRules&);
    ///             ~Config();
    ///         };
    ///     \endcode</code>
    ///
    ///     This might contain some configuration options, maybe some simple members or maybe
    ///     even some complex members.
    ///
    ///     Sometimes we might want to store a configuration settings like this in it's own
    ///     individual file. Other times, we might want to store it within a larger file, just
    ///     as part of heirarchy of serialized objects.
    ///
    ///     Because the object is deserialized directly from the formatter, we have the flexibility
    ///     to do that.
    ///
    ///     When we want that object to exist on it's own, in an individual file, we can use
    ///     ConfigFileContainer<Config>. With a ConfigFileContainer, it can be considered a
    ///     fully functional asset, with a dependency validation, relative path rules and
    ///     reporting correctly to the InvalidAssetManager.
    /// </example>
    template<typename Type, typename Formatter = InputStreamFormatter<utf8>>
        class ConfigFileContainer
    {
    public:
        Type _asset;

        ConfigFileContainer(const ::Assets::ResChar initializer[]);
        ~ConfigFileContainer();

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    protected:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };

    template<typename Type, typename Formatter>
        ConfigFileContainer<Type, Formatter>::ConfigFileContainer(const ::Assets::ResChar initializer[])
    {
        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(initializer, &fileSize);
        
        TRY
        {
            auto searchRules = ::Assets::DefaultDirectorySearchRules(initializer);

            Formatter formatter(
                MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), fileSize)));

            _asset = Type(formatter, searchRules);

            ::Assets::Services::GetInvalidAssetMan().MarkValid(initializer);
        } CATCH (const std::exception& e) {
            ::Assets::Services::GetInvalidAssetMan().MarkInvalid(initializer, e.what());
            throw;
        } CATCH(...) {
            ::Assets::Services::GetInvalidAssetMan().MarkInvalid(initializer, "Unknown error");
            throw;
        } CATCH_END

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_validationCallback, initializer);
    }

    template<typename Type, typename Formatter> 
        ConfigFileContainer<Type, Formatter>::~ConfigFileContainer() {}


    /// <summary>Loads a single configuration setting from a file with multiple settings</summary>
    /// Like ConfigFileContainer, but this is for cases where we want to load a single setting
    /// from a file that contains a list of settings.
    /// Client should specify a configuration name after ':' in the initializer passed to the
    /// constructor. This is compared against the root element names in the file.
    template<typename Type, typename Formatter = InputStreamFormatter<utf8>>
        class ConfigFileListContainer
    {
    public:
        Type _asset;
        DirectorySearchRules _searchRules;

        ConfigFileListContainer(const ::Assets::ResChar initializer[]);
        ConfigFileListContainer();
        ~ConfigFileListContainer();

        static std::unique_ptr<ConfigFileListContainer> CreateNew(const ::Assets::ResChar initialiser[]);

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    protected:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };

    template<typename Type, typename Formatter>
        ConfigFileListContainer<Type, Formatter>::ConfigFileListContainer(const ::Assets::ResChar initializer[])
    {
        ::Assets::ResChar filename[MaxPath];
        const auto* divider = XlFindChar(initializer, ':');
        const ::Assets::ResChar* configName = "default";
        if (divider) {
            XlCopyNString(filename, dimof(filename), initializer, divider - initializer);
            configName = divider+1;
        } else {
            XlCopyString(filename, initializer);
        }

        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &fileSize);

        TRY
        {
            _searchRules = ::Assets::DefaultDirectorySearchRules(initializer);

            Formatter formatter(
                MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), fileSize)));

            bool gotConfig = false;

            // search through for the specific element we need (ignoring other elements)
            while (!gotConfig) {
                using Blob = Formatter::Blob;
                switch (formatter.PeekNext()) {
                case Blob::BeginElement:
                    {
                        Formatter::InteriorSection eleName;
                        if (!formatter.TryBeginElement(eleName))
                            Throw(Utility::FormatException("Poorly formed begin element in config file", formatter.GetLocation()));

                        if (    size_t(eleName._end - eleName._start) == XlStringLen(configName)
                            &&  !XlComparePrefixI((const ::Assets::ResChar*)eleName._start, configName, size_t(eleName._end - eleName._start))) {

                            _asset = Type(formatter, _searchRules);
                            gotConfig = true;
                        } else {
                            formatter.SkipElement();    // skip the whole element; it's not required
                        }

                        if (!formatter.TryEndElement())
                            Throw(Utility::FormatException("Expecting end element in config file", formatter.GetLocation()));

                        continue;
                    }

                case Blob::AttributeName:
                    {
                        Formatter::InteriorSection name, value;
                        formatter.TryAttribute(name, value);
                        continue;
                    }

                default:
                    break;
                }
                break;
            }
            
            //      Missing entry isn't an exception... just return the defaults
            // if (!gotConfig)
            //     Throw(::Exceptions::BasicLabel(StringMeld<256>() << "Configuration setting (" << initializer << ") is missing"));

            ::Assets::Services::GetInvalidAssetMan().MarkValid(initializer);
        } CATCH (const std::exception& e) {
            ::Assets::Services::GetInvalidAssetMan().MarkInvalid(initializer, e.what());
            throw;
        } CATCH(...) {
            ::Assets::Services::GetInvalidAssetMan().MarkInvalid(initializer, "Unknown error");
            throw;
        } CATCH_END

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_validationCallback, filename);
    }

    template<typename Type, typename Formatter>
        ConfigFileListContainer<Type, Formatter>::ConfigFileListContainer() {}

    template<typename Type, typename Formatter>
        ConfigFileListContainer<Type, Formatter>::~ConfigFileListContainer() {}

    template<typename Type, typename Formatter>
        auto ConfigFileListContainer<Type, Formatter>::CreateNew(const ::Assets::ResChar initialiser[]) -> std::unique_ptr < ConfigFileListContainer >
        {
            return std::make_unique<ConfigFileListContainer>();
        }
}

