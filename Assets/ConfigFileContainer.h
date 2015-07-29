// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Assets.h"
#include "InvalidAssetManager.h"
#include "../Utility/Streams/StreamFormatter.h"
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
}

