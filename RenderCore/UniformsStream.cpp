// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UniformsStream.h"
#include "Types.h"
#include "Format.h"
#include "../Utility/MemoryUtils.h"
#include "../Core/SelectConfiguration.h"

#if COMPILER_ACTIVE == COMPILER_TYPE_MSVC
	#define __builtin_expect(X, Y) X
#endif

namespace RenderCore 
{

    void UniformsStreamInterface::BindConstantBuffer(unsigned slot, const CBBinding& binding)
    {
        if (_cbBindings.size() <= slot)
            _cbBindings.resize(slot+1);

        _cbBindings[slot] = RetainedCBBinding {
            binding._hashName,
            std::vector<ConstantBufferElementDesc>(binding._elements.begin(), binding._elements.end())
        };
        _hash = 0;
    }

    void UniformsStreamInterface::BindShaderResource(unsigned slot, uint64_t hashName)
    {
        if (_srvBindings.size() <= slot)
            _srvBindings.resize(slot+1);
        _srvBindings[slot] = hashName;
        _hash = 0;
    }

    uint64_t UniformsStreamInterface::GetHash() const
    {
        if (__builtin_expect(_hash==0, false)) {
            _hash = DefaultSeed64;
            // to prevent some oddities when the same hash value could be in either a CB or SRV
            // we need to include the count of the first array we look through in the hash
            _hash = HashCombine((uint64_t)_cbBindings.size(), _hash);
            for (const auto& c:_cbBindings)
                _hash = HashCombine(c._hashName, _hash);
            _hash = HashCombine(Hash64(AsPointer(_srvBindings.begin()), AsPointer(_srvBindings.end())), _hash);
        }

        return _hash;
    }

    UniformsStreamInterface::UniformsStreamInterface() : _hash(0) {}
    UniformsStreamInterface::~UniformsStreamInterface() {}

}

