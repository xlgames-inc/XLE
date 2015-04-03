// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace Tools { class IManipulator; }

namespace PlatformRig
{
    class VisCameraSettings;

    std::shared_ptr<Tools::IManipulator> CreateCameraManipulator(
        std::shared_ptr<VisCameraSettings> visCameraSettings);
}

