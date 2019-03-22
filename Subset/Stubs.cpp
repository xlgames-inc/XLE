// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../RenderOverlays/Font.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include <memory>

namespace RenderOverlays
{
    std::shared_ptr<Font> GetX2Font(StringSection<>, int) { return nullptr; }
    std::shared_ptr<Font> GetDefaultFont(unsigned points) { return nullptr; }
}

namespace RenderCore { namespace Techniques
{
    std::shared_ptr<IRenderStateDelegate> CreateRenderStateDelegate_Default()
    {
        return nullptr;
    }
}}
