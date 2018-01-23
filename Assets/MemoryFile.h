// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IFileSystem.h"

namespace Assets
{
	using Blob = std::shared_ptr<std::vector<uint8_t>>;
	std::shared_ptr<IFileInterface> CreateMemoryFile(const Blob&);
}

