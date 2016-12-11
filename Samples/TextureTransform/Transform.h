// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Format.h"
#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"
#include "../../Utility/IntrusivePtr.h"
#include "../../Utility/StringUtils.h"
#include <functional>
#include <map>

namespace Utility { class ParameterBox; }
namespace RenderCore { class TextureDesc; }
namespace BufferUploads { class DataPacket; }

namespace TextureTransform
{
    class TextureResult
    {
    public:
        using Pkt = intrusive_ptr<BufferUploads::DataPacket>;
        Pkt					_pkt;
        RenderCore::Format  _format;
        UInt2				_dimensions;
        unsigned			_mipCount;
        unsigned			_arrayCount;

        void Save(StringSection<::Assets::ResChar> destinationFile) const;

	private:
		void SaveXMLFormat(StringSection<::Assets::ResChar> destinationFile) const;
    };

    using ProcessingFn = std::function<TextureResult(const RenderCore::TextureDesc&, const ParameterBox&)>;

    TextureResult ExecuteTransform(
        RenderCore::IDevice& device,
        StringSection<char> xleDir,
        StringSection<char> shaderName,
        const ParameterBox& parameters,
        std::map<std::string, ProcessingFn> fns);

    TextureResult HosekWilkieSky(const RenderCore::TextureDesc&, const ParameterBox& parameters);
    TextureResult CompressTexture(const RenderCore::TextureDesc& desc, const ParameterBox& parameters);
}
