// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../ResourceDesc.h"
#include "../../IDevice.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore
{
    class SubResourceInitData;
}

namespace RenderCore { namespace Metal_AppleMetal
{
    class ObjectFactory;

    class Resource : public IResource
    {
    public:
        using Desc = ResourceDesc;

        Desc GetDesc() const         { return _desc; }

        virtual void*       QueryInterface(size_t guid);

        Resource(
            ObjectFactory& factory, const Desc& desc,
            const SubResourceInitData& initData = SubResourceInitData{});
        Resource(
            ObjectFactory& factory, const Desc& desc,
            const IDevice::ResourceInitializer& initData);
        Resource();
        ~Resource();
    protected:
        Desc _desc;
    };
}}


