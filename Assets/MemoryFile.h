// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IFileSystem.h"

namespace Assets
{
	using Blob = std::shared_ptr<std::vector<uint8_t>>;
	std::unique_ptr<IFileInterface> CreateMemoryFile(const Blob&);

	std::unique_ptr<IFileInterface> CreateSubFile(
		const std::shared_ptr<MemoryMappedFile>& archiveFile,
		const IteratorRange<const void*> memoryRange);

	std::unique_ptr<IFileInterface> CreateDecompressOnReadFile(
		const std::shared_ptr<MemoryMappedFile>& archiveFile,
		const IteratorRange<const void*> memoryRange,
		size_t decompressedSize);
}

