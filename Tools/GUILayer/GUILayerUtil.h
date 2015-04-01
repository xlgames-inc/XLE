// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once
#include "AutoToShared.h"

namespace RenderCore { namespace Techniques { class TechniqueContext; } }

namespace GUILayer
{
    public ref class TechniqueContextWrapper
    {
    public:
        AutoToShared<RenderCore::Techniques::TechniqueContext> _techniqueContext;

        TechniqueContextWrapper(std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext);
    };
}
