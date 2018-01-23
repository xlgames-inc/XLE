// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Font.h"

namespace RenderOverlays
{
    std::shared_ptr<Font> GetX2Font(const char* , int , FontTexKind) { return nullptr; }
    TextStyle::TextStyle(const std::shared_ptr<Font>& , const DrawTextOptions& ) {}
    TextStyle::~TextStyle() {}
}

