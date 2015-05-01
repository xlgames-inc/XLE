// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Exceptions.h"
#include <string>

namespace Assets
{
    typedef char ResChar;
	using rstring = std::basic_string<ResChar>;

    enum class AssetState { Pending, Ready, Invalid };

    /// <summary>Exceptions related to rendering</summary>
    namespace Exceptions
    {
        /// <summary>A resource can't be loaded</summary>
        /// This exception means a resource failed during loading, and can
        /// never be loaded. It might mean that the resource is corrupted on
        /// disk, or maybe using an unsupported file format (or bad version).
        /// The most common cause is due to a compile error in a shader. 
        /// If we attempt to use a shader with a compile error, it will throw
        /// a InvalidResource exception.
        class InvalidResource : public ::Exceptions::BasicLabel
        {
        public: 
            InvalidResource(const char resourceId[], const char what[]);
            const char* ResourceId() const { return _resourceId; }

        private:
            char _resourceId[512];
        };

        /// <summary>Resource is still being loaded</summary>
        /// This is common exception. It occurs if we attempt to use a resource that
        /// is still being prepared. Usually this means that the resource is being
        /// loaded from disk, or compiled in a background thread.
        /// For example, shader resources can take some time to compile. If we attempt
        /// to use the shader while it's still compiling, we'll get a PendingResource
        /// exception.
        class PendingResource : public ::Exceptions::BasicLabel
        {
        public: 
            PendingResource(const char resourceId[], const char what[]);
            const char* ResourceId() const { return _resourceId; }

        private:
            char _resourceId[512];
        };

        class FormatError : public ::Exceptions::BasicLabel
        {
        public:
            FormatError(const char format[], ...) never_throws;
        };

        class UnsupportedFormat : public ::Exceptions::BasicLabel
        {
        public:
            UnsupportedFormat(const char format[], ...) never_throws;
        };
    }
}

