// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace RenderCore
{
    namespace Metal_AppleMetal { class DeviceContext; }

    ////////////////////////////////////////////////////////////////////////////////

    class IThreadContextAppleMetal
    {
    public:
        virtual const std::shared_ptr<Metal_AppleMetal::DeviceContext>&  GetDeviceContext() = 0;
        ~IThreadContextAppleMetal();
    };

}

