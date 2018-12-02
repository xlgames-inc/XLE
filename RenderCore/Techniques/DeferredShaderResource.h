// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Types_Forward.h"
#include "../Metal/TextureView.h"
#include "../../Assets/AssetsCore.h"

namespace Utility { class ParameterBox; }
namespace Assets { class DirectorySearchRules; }

namespace RenderCore { namespace Techniques 
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
		const Metal::ShaderResourceView&        GetShaderResource() const			{ return _srv; }
        const ::Assets::DepValPtr&				GetDependencyValidation() const     { return _depVal; }
		StringSection<>							Initializer() const					{ return MakeStringSection(_initializer); }

        static Metal::ShaderResourceView LoadImmediately(StringSection<::Assets::ResChar> initializer);
        static Format LoadFormat(StringSection<::Assets::ResChar> initializer);
        static bool IsDXTNormalMap(StringSection<::Assets::ResChar> initializer);

        DeferredShaderResource(
			const Metal::ShaderResourceView& srv,
			const std::string& initializer,
			const ::Assets::DepValPtr& depVal);
        ~DeferredShaderResource();

		static void ConstructToFuture(
			::Assets::AssetFuture<DeferredShaderResource>&,
			StringSection<> initializer);
    private:
		Metal::ShaderResourceView _srv;
        std::string _initializer;
		::Assets::DepValPtr _depVal;
    };
    
	/// <summary>Set the RES_HAS technique materials parameter</summary>
    /// There are a few standard RES_HAS shader parameters that are defined by
    /// the resources that are bound to a technique material. This function builds
    /// a parameter box with these parameters set.
    ParameterBox TechParams_SetResHas(
        const ParameterBox& inputMatParameters, const ParameterBox& resBindings,
        const ::Assets::DirectorySearchRules& searchRules);
}}

