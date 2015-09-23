// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Forward.h"
#include "../../Assets/Assets.h"

namespace RenderCore { namespace Assets 
{
    /// <summary>Prepares a shader resource from a file source</summary>
    /// This is used to load a file from disk, as use as a shader resource (eg, a texture).
    /// Disk access and GPU upload are performed in background threads. While
    /// the resource is being loaded and upload, GetShaderResource() will throw
    /// PendingAsset().
    ///
    /// The filename can have flags appended after a colon. For example:
    ///   texture.dds:l1
    /// each character after the colon represents one flag or setting.
    /// Valid flags are:
    ///   'L' -- the data in this texture is in linear color space
    ///   'S' -- the data in this texture is in SRGB color space
    ///   'T' -- do not generate mipmaps (however, if mipmaps are already present in the file, they will be used)
    ///
    /// Because all disk access is performed in background threads, the constructor will
    /// not know immediately if the requested file is missing (or invalid). So, if you
    /// attempt to load a texture, but the texture file is missing, the construction will
    /// appear to complete successfully. However, eventually GetShaderResource() will
    /// throw an InvalidAsset() exception.
    ///
    /// The system can load a variety of texture formats, and can perform data massaging as
    /// necessary (for example, building mip-map). This work will happen within the buffer
    /// uploads system, and in background threads.
    ///
    /// However, note that some texture file formats (like tga, tif, etc) cannot contain
    /// precomputed mipmaps. This means the mip-map generation must occur while the texture
    /// is loaded. Building mipmaps for a lot of textures can end up being a large amount of
    /// work -- so it is recommended to use .dds files with precompiled mip maps (.dds files
    /// also allow compressed texture formats).
    class DeferredShaderResource
    {
    public:
        const Metal::ShaderResourceView&        GetShaderResource() const;
        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     { return _validationCallback; }
        const ::Assets::ResChar*                Initializer() const;

        static Metal::ShaderResourceView LoadImmediately(const char initializer[]);
        static Metal::NativeFormat::Enum LoadFormat(const char initializer[]);

        ::Assets::AssetState GetState() const;
        ::Assets::AssetState TryResolve() const;

        explicit DeferredShaderResource(const ::Assets::ResChar resourceName[]);
        ~DeferredShaderResource();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
        
        DEBUG_ONLY(::Assets::ResChar _initializer[MaxPath];)
    };
    

}}

