// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Types.h"
#include "../RenderCore/ResourceDesc.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"                 // for StringSection
#include <functional>
#include <regex>
#include <future>

namespace BufferUploads
{
    using TexturePitches = RenderCore::TexturePitches;
    using SubResourceId = RenderCore::SubResourceId;
    using ResourceDesc = RenderCore::ResourceDesc;

    class IDataPacket
    {
    public:
        virtual void*           GetData         (SubResourceId subRes = {}) = 0;
        virtual size_t          GetDataSize     (SubResourceId subRes = {}) const = 0;
        virtual TexturePitches  GetPitches      (SubResourceId subRes = {}) const = 0;
        virtual ~IDataPacket();
    };

    class IAsyncDataSource
    {
    public:
        virtual std::future<ResourceDesc> GetDesc () const = 0;

        struct SubResource
        {
            SubResourceId _id;
            IteratorRange<void*> _destination;
            TexturePitches _pitches;
        };

        virtual std::future<void> PrepareData(IteratorRange<const SubResource*> subResources);
    };

        /////////////////////////////////////////////////

    std::shared_ptr<IDataPacket> CreateBasicPacket(
        IteratorRange<const void*> data = {}, 
        TexturePitches pitches = TexturePitches());

    std::shared_ptr<IDataPacket> CreateEmptyPacket(
        const ResourceDesc& desc);

    namespace TextureHandlers
    {
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
}
