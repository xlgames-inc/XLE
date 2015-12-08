// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../RenderCore/IDevice.h"
#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"
#include "../../Utility/IntrusivePtr.h"
#include "../../Utility/StringUtils.h"
#include <functional>
#include <map>

namespace BufferUploads { class DataPacket; struct TextureDesc; }
namespace Utility { class ParameterBox; }

namespace TextureTransform
{
    class TextureResult
    {
    public:
        using Pkt = intrusive_ptr<BufferUploads::DataPacket>;
        Pkt             _pkt;
        unsigned        _format;
        UInt2           _dimensions;
        unsigned        _mipCount;
        unsigned        _arrayCount;

        void Save(const ::Assets::ResChar destinationFile[]) const;
    };

    using ProcessingFn = std::function<TextureResult(const BufferUploads::TextureDesc&, const ParameterBox&)>;

    TextureResult ExecuteTransform(
        RenderCore::IDevice& device,
        StringSection<char> xleDir,
        StringSection<char> shaderName,
        const ParameterBox& parameters,
        std::map<std::string, ProcessingFn> fns);

    TextureResult HosekWilkieSky(const BufferUploads::TextureDesc&, const ParameterBox& parameters);
    TextureResult CompressTexture(const BufferUploads::TextureDesc& desc, const ParameterBox& parameters);
}
