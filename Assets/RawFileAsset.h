// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "DepVal.h"
#include "../Utility/IteratorUtils.h"
#include <string>

namespace Assets
{
	class RawFileAsset
	{
	public:
		IteratorRange<const void*> GetData() const { return MakeIteratorRange(_data.get(), PtrAdd(_data.get(), _dataSize)); }
		const std::string& GetFileName() const { return _fname; }
		const DepValPtr& GetDependencyValidation() const { return _depVal; }

		RawFileAsset(StringSection<> fname);
		~RawFileAsset();
	private:
		::Assets::DepValPtr _depVal;
		std::unique_ptr<uint8_t[]> _data;
		size_t _dataSize;
		std::string _fname;
	};
}

