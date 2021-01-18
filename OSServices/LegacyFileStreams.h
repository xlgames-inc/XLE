// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include <memory>

namespace OSServices { class BasicFile; }
namespace Utility { class OutputStream; }

namespace OSServices { namespace Legacy 
{
    std::unique_ptr<Utility::OutputStream>   OpenFileOutput(const char* path, const char* mode);
    std::unique_ptr<Utility::OutputStream>   OpenFileOutput(OSServices::BasicFile&&);
}}
