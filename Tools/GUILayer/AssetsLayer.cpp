// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsLayer.h"
#include "MarshalString.h"
#include "ExportedNativeTypes.h"
#include "../../Assets/AssetUtils.h"

namespace GUILayer
{
    System::String^ DirectorySearchRules::ResolveFile(System::String^ baseName)
	{
		char buffer[MaxPath];
		auto nativeDirName = clix::marshalString<clix::E_UTF8>(baseName);
		_searchRules->ResolveFile(buffer, nativeDirName.c_str());
		return clix::marshalString<clix::E_UTF8>(buffer);
	}

	void DirectorySearchRules::AddSearchDirectory(System::String^ dirName)
	{
		auto nativeDirName = clix::marshalString<clix::E_UTF8>(dirName);
		_searchRules->AddSearchDirectory(MakeStringSection(nativeDirName));
	}

	const ::Assets::DirectorySearchRules& DirectorySearchRules::GetNative() { return *_searchRules.get(); }

    DirectorySearchRules::DirectorySearchRules(std::shared_ptr<::Assets::DirectorySearchRules> searchRules)
	{
		_searchRules = std::move(searchRules);
	}

	DirectorySearchRules::DirectorySearchRules()
	{
		_searchRules = std::make_shared<::Assets::DirectorySearchRules>();
	}

	DirectorySearchRules::DirectorySearchRules(const ::Assets::DirectorySearchRules& searchRules)
	{
		_searchRules = std::make_shared<::Assets::DirectorySearchRules>(searchRules);
	}

	DirectorySearchRules::~DirectorySearchRules() 
	{
		_searchRules.reset();
	}
}

