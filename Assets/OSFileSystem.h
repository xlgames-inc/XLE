// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/StringUtils.h"
#include "../Utility/UTFUtils.h"
#include <memory>

namespace Assets
{
	class IFileSystem;
	std::shared_ptr<IFileSystem>	CreateFileSystem_OS(StringSection<utf8> root = StringSection<utf8>(), bool ignorePaths = false);
}
