// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace PlatformRig { class InputTranslator; }

namespace GUILayer
{
    class IWindowRig;
    class EngineControlPimpl
    {
    public:
        std::unique_ptr<IWindowRig> _windowRig;
        std::unique_ptr<PlatformRig::InputTranslator> _inputTranslator;
    };
}

