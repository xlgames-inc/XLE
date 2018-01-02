// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UniformsStream.h"
#include "Types.h"
#include "Format.h"

namespace RenderCore 
{

    void UniformsStreamInterface::BindConstantBuffer(unsigned slot, const CBBinding& binding)
    {
        SavedCBBinding savedBinding {
            binding._hashName,
            std::vector<ConstantBufferElement>(binding._elements.begin(), binding._elements.end())
        };

        auto i = LowerBound(_cbBindings, slot);
        if (i == _cbBindings.begin() || i->first != slot) {
            _cbBindings.emplace(i, std::make_pair(slot, std::move(savedBinding)));
        } else {
            i->second = std::move(savedBinding);
        }
    }

    void UniformsStreamInterface::BindShaderResource(unsigned slot, uint64_t hashName)
    {
        auto i = LowerBound(_srvBindings, slot);
        if (i == _srvBindings.begin() || i->first != slot) {
            _srvBindings.insert(i, {slot, hashName});
        } else {
            i->second = hashName;
        }
    }

    UniformsStreamInterface::UniformsStreamInterface() {}
    UniformsStreamInterface::~UniformsStreamInterface() {}

    unsigned CalculateStride(IteratorRange<const ConstantBufferElement*> elements)
    {
        // note -- following alignment rules suggested by Apple in OpenGL ES guide
        //          each element should be aligned to a multiple of 4 bytes (or a multiple of
        //          it's component size, whichever is larger).
        //          Note that this must affect the entire vertex stride, because we want the
        //
        if (elements.empty()) return 0;
        unsigned result = 0;
        for (auto i=elements.begin(); i<elements.end(); ++i) {
            assert(i->_arrayElementCount != 0);
            auto size = BitsPerPixel(i->_nativeFormat);
            result += size * i->_arrayElementCount;
        }
        return result / 8;
    }

}

