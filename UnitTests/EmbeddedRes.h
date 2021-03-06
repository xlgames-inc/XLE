// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace Assets { class IFileSystem; }

namespace UnitTests
{
    std::shared_ptr<::Assets::IFileSystem> CreateEmbeddedResFileSystem();
}
