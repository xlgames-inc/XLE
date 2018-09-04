// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace ConsoleRig
{
    class IStartupShutdownPlugin
	{
	public:
		virtual void Initialize() = 0;
		virtual void Deinitialize() = 0;

		virtual ~IStartupShutdownPlugin();
	};
}

