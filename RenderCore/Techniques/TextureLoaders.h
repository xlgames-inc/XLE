// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../BufferUploads/IBufferUploads.h"
#include "../../Utility/StringUtils.h"
#include <regex>
#include <functional>

namespace RenderCore { namespace Techniques
{
    namespace TextureLoaderFlags
    {
        enum Enum { GenerateMipmaps = 1<<0 };
        using BitField = unsigned;
    }
    
    using TextureLoaderSignature = std::shared_ptr<BufferUploads::IAsyncDataSource>(StringSection<>, TextureLoaderFlags::BitField);
    
    void RegisterTextureLoader(
        std::regex _filenameMatcher,
        std::function<TextureLoaderSignature>&& loader);

    std::function<TextureLoaderSignature> GetDDSTextureLoader();
    std::function<TextureLoaderSignature> GetWICTextureLoader();
}}

