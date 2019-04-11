// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RawFileAsset.h"
#include "IFileSystem.h"

namespace Assets
{
	RawFileAsset::RawFileAsset(StringSection<> fname)
	: _fname(fname.AsString())
	{
		_data = TryLoadFileAsMemoryBlock(fname, &_dataSize);
		_depVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_depVal, fname);
	}

	RawFileAsset::~RawFileAsset()
	{}
}

