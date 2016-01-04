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
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/StringFormat.h"
#include <memory>
#include <vector>

namespace Assets
{
    class PendingCompileMarker;

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

        ConfigFileContainer(const ResChar initializer[]);
        ~ConfigFileContainer();

        const std::shared_ptr<DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    protected:
        std::shared_ptr<DependencyValidation>  _validationCallback;
    };

    template<typename Type, typename Formatter>
        ConfigFileContainer<Type, Formatter>::ConfigFileContainer(const ResChar initializer[])
    {
        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(initializer, &fileSize);
        
        TRY
        {
            auto searchRules = DefaultDirectorySearchRules(initializer);

            Formatter formatter(
                MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), fileSize)));

            _asset = Type(formatter, searchRules);

            if (Services::GetInvalidAssetMan())
                Services::GetInvalidAssetMan()->MarkValid(initializer);
        } CATCH (const std::exception& e) {
            if (Services::GetInvalidAssetMan())
                Services::GetInvalidAssetMan()->MarkInvalid(initializer, e.what());
            throw;
        } CATCH(...) {
            if (Services::GetInvalidAssetMan())
                Services::GetInvalidAssetMan()->MarkInvalid(initializer, "Unknown error");
            throw;
        } CATCH_END

        _validationCallback = std::make_shared<DependencyValidation>();
        RegisterFileDependency(_validationCallback, initializer);
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

        ConfigFileListContainer(const ResChar initializer[]);
        ConfigFileListContainer(std::shared_ptr<PendingCompileMarker>&& marker);
        ConfigFileListContainer();
        ~ConfigFileListContainer();

        static std::unique_ptr<ConfigFileListContainer> CreateNew(const ResChar initialiser[]);

        const std::shared_ptr<DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    protected:
        std::shared_ptr<DependencyValidation>  _validationCallback;

        void Construct(const ResChar initializer[]);
    };

    template<typename Type, typename Formatter>
        void ConfigFileListContainer<Type, Formatter>::Construct(const ResChar initializer[])
    {
        ResChar filename[MaxPath];
        FileNameSplitter<ResChar> splitName(initializer);
        XlCopyString(filename, splitName.AllExceptParameters());
        StringSection<ResChar> configName;
        if (!splitName.ParametersWithDivider().Empty()) configName = splitName.Parameters();
        else configName = "default";

        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &fileSize);

        TRY
        {
            _searchRules = DefaultDirectorySearchRules(initializer);

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

                        if (XlEqStringI(StringSection<ResChar>((const ResChar*)eleName.begin(), (const ResChar*)eleName.end()), configName)) {
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

            if (Services::GetInvalidAssetMan())
                Services::GetInvalidAssetMan()->MarkValid(initializer);
        } CATCH (const std::exception& e) {
            if (Services::GetInvalidAssetMan())
                Services::GetInvalidAssetMan()->MarkInvalid(initializer, e.what());
            throw;
        } CATCH(...) {
            if (Services::GetInvalidAssetMan())
                Services::GetInvalidAssetMan()->MarkInvalid(initializer, "Unknown error");
            throw;
        } CATCH_END

        _validationCallback = std::make_shared<DependencyValidation>();
        RegisterFileDependency(_validationCallback, filename);
    }

    template<typename Type, typename Formatter>
        ConfigFileListContainer<Type, Formatter>::ConfigFileListContainer(const ResChar initializer[])
    {
        Construct(initializer);
    }

    template<typename Type, typename Formatter>
        ConfigFileListContainer<Type, Formatter>::ConfigFileListContainer(
            std::shared_ptr<PendingCompileMarker>&& marker)
    {
        auto state = marker->GetState();
        if (state == AssetState::Ready) {
            Construct(marker->_sourceID0);
        } else if (state == AssetState::Pending) {
            Throw(Exceptions::PendingAsset(marker->Initializer(), "Asset pending when loading through ConfigFileListContainer"));
        } else {
            Throw(Exceptions::PendingAsset(marker->Initializer(), "Asset invlaid when loading through ConfigFileListContainer"));
        }
    }

    template<typename Type, typename Formatter>
        ConfigFileListContainer<Type, Formatter>::ConfigFileListContainer() {}

    template<typename Type, typename Formatter>
        ConfigFileListContainer<Type, Formatter>::~ConfigFileListContainer() {}

    template<typename Type, typename Formatter>
        auto ConfigFileListContainer<Type, Formatter>::CreateNew(const ResChar initialiser[]) -> std::unique_ptr < ConfigFileListContainer >
        {
            return std::make_unique<ConfigFileListContainer>();
        }

    template<typename CharType>
        class TextChunk
    {
    public:
        StringSection<CharType> _type, _name, _content;

        TextChunk(StringSection<CharType> type, StringSection<CharType> name, StringSection<CharType> content)
            : _type(type), _name(name), _content(content) {}
    };

    template<typename CharType>
        std::vector<TextChunk<CharType>> ReadCompoundTextDocument(StringSection<CharType> doc);
}

