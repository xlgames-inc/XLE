// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"
#include "../RenderCore/Types.h"
#include "../RenderCore/ResourceDesc.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"                 // for StringSection
#include <functional>
#include <regex>

namespace BufferUploads
{
    //////////////////////////////////////////////
        // Legacy texture loaders //
    //////////////////////////////////////////////
    namespace TextureLoadFlags { 
        enum Enum { GenerateMipmaps = 1<<0 };
        typedef unsigned BitField;
    }

    struct TexturePlugin
    {
        std::regex _filenameMatcher;
        std::function<std::shared_ptr<IDataPacket>(StringSection<>, TextureLoadFlags::BitField)> _loader;
    };

    std::shared_ptr<IDataPacket> CreateStreamingTextureSource(
        IteratorRange<const TexturePlugin*> plugins,
        StringSection<> filename, TextureLoadFlags::BitField flags = 0);

    RenderCore::TextureDesc LoadTextureFormat(StringSection<> filename);
}
