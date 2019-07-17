// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"

namespace Assets { class DirectorySearchRules; }

namespace GUILayer
{
    public ref class DirectorySearchRules
    {
    public:
        clix::shared_ptr<::Assets::DirectorySearchRules> _searchRules;

		System::String^ ResolveFile(System::String^ baseName);
		void AddSearchDirectory(System::String^ dirName);

		const ::Assets::DirectorySearchRules& GetNative();

        DirectorySearchRules(std::shared_ptr<::Assets::DirectorySearchRules> searchRules);
		DirectorySearchRules(const ::Assets::DirectorySearchRules& searchRules);
		DirectorySearchRules();
        ~DirectorySearchRules();
    };
}
