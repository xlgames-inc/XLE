// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Exceptions.h"
#include <string>
#include <memory>

namespace Assets
{
    typedef char ResChar;
	using rstring = std::basic_string<ResChar>;

    enum class AssetState { Pending, Ready, Invalid };

    class DependencyValidation;
    using DepValPtr = std::shared_ptr<DependencyValidation>;

    /// <summary>Exceptions related to rendering</summary>
    namespace Exceptions
    {
        class AssetException : public ::Exceptions::BasicLabel
        {
        public:
            const ResChar* Initializer() const { return _initializer; }
            virtual AssetState State() const = 0;

            AssetException(const ResChar initializer[], const char what[]);
        private:
            ResChar _initializer[512];
        };

        /// <summaryAn asset can't be loaded</summary>
        /// This exception means a asset failed during loading, and can
        /// never be loaded. It might mean that the resource is corrupted on
        /// disk, or maybe using an unsupported file format (or bad version).
        /// The most common cause is due to a compile error in a shader. 
        /// If we attempt to use a shader with a compile error, it will throw
        /// a InvalidAsset exception.
        class InvalidAsset : public AssetException
        {
        public: 
            virtual bool CustomReport() const;
            virtual AssetState State() const;

            InvalidAsset(const ResChar initializer[], const char what[]);
        };

        /// <summary>An asset is still being loaded</summary>
        /// This is common exception. It occurs if we attempt to use an asset that
        /// is still being prepared. Usually this means that the resource is being
        /// loaded from disk, or compiled in a background thread.
        /// For example, shader resources can take some time to compile. If we attempt
        /// to use the shader while it's still compiling, we'll get a PendingAsset
        /// exception.
        class PendingAsset : public AssetException
        {
        public: 
            virtual bool CustomReport() const;
            virtual AssetState State() const;

            PendingAsset(const ResChar initializer[], const char what[]);
        };

        class FormatError : public ::Exceptions::BasicLabel
        {
        public:
            enum class Reason
            {
                Success,
                UnsupportedVersion,
                FormatNotUnderstood
            };

            Reason GetReason() const { return _reason; }

            FormatError(const char format[], ...) never_throws;
            FormatError(Reason reason, const char format[], ...) never_throws;

        private:
            Reason _reason;
        };
    }
}

