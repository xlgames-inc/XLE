// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "State.h"

namespace RenderCore { namespace Metal_AppleMetal
{

    SamplerState::SamplerState(
        FilterMode filter,
        AddressMode addressU, AddressMode addressV, AddressMode addressW,
        CompareOp comparison)
    {
    }

    BlendState::BlendState() {}
    void BlendState::Apply() const never_throws
    {
        assert(0);
    }

    ViewportDesc::ViewportDesc(DeviceContext& viewport)
    {
        assert(0);
    }
    
}}
