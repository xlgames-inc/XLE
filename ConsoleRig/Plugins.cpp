// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Plugins.h"
#include "AttachablePtr.h"
#include "AttachableLibrary.h"
#include "Log.h"
#include "../OSServices/RawFS.h"
#include "../OSServices/RawFS.h"
#include <vector>

namespace ConsoleRig
{
	class PluginSet::Pimpl
	{
	public:
		std::vector<AttachableLibrary> _pluginLibraries;
		std::vector<std::shared_ptr<IStartupShutdownPlugin>> _plugins;
	};

	PluginSet::PluginSet()
	{
		_pimpl = std::make_unique<Pimpl>();

		char processPath[MaxPath], cwd[MaxPath];
		OSServices::GetProcessPath((utf8*)processPath, dimof(processPath));
    	OSServices::GetCurrentDirectory(dimof(cwd), cwd);

		auto group0 = OSServices::FindFiles(std::string(processPath) + "/*Plugin.dll");
		auto group1 = OSServices::FindFiles(std::string(cwd) + "/*Plugin.dll");

		std::vector<std::string> candidatePlugins = group0;
		candidatePlugins.insert(candidatePlugins.end(), group1.begin(), group1.end());

		for (auto& c:candidatePlugins) {
			ConsoleRig::AttachableLibrary library{c};
			std::string errorMsg;
			if (library.TryAttach(errorMsg)) {
				using PluginFn = std::shared_ptr<ConsoleRig::IStartupShutdownPlugin>();
				auto fn = library.GetFunction<PluginFn*>("GetStartupShutdownPlugin");
				if (fn) {
					auto plugin = (*fn)();
					plugin->Initialize();
					_pimpl->_plugins.emplace_back(std::move(plugin));
				}
				_pimpl->_pluginLibraries.emplace_back(std::move(library));
			} else {
				Log(Warning) << "Plugin failed to attach with error msg (" << errorMsg << ")" << std::endl;
			}
		}
	}

	PluginSet::~PluginSet()
	{
		_pimpl->_plugins.clear();
		for (auto&a:_pimpl->_pluginLibraries)
			a.Detach();
	}
}
