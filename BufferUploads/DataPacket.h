// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Types.h"
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

    std::shared_ptr<DataPacket> CreateBasicPacket(
        IteratorRange<const void*> data = {}, 
        TexturePitches pitches = TexturePitches());

    std::shared_ptr<DataPacket> CreateEmptyPacket(
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
            std::function<intrusive_ptr<DataPacket>(StringSection<::Assets::ResChar>, TextureLoadFlags::BitField flags)> _loader;
        };

        buffer_upload_dll_export intrusive_ptr<DataPacket> CreateStreamingTextureSource(
            IteratorRange<const TexturePlugin*> plugins,
            StringSection<::Assets::ResChar> filename, TextureLoadFlags::BitField flags = 0);

        buffer_upload_dll_export TextureDesc LoadTextureFormat(StringSection<::Assets::ResChar> filename);
    }
}
