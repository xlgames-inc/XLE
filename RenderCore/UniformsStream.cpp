// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UniformsStream.h"
#include "Types.h"

namespace RenderCore 
{

    void UniformsStreamInterface::BindConstantBuffer(unsigned slot, const CBBinding& binding)
    {
        SavedCBBinding savedBinding {
            binding._hashName,
            std::vector<MiniInputElementDesc>(binding._elements.begin(), binding._elements.end())
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

}

