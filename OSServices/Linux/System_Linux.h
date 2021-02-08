// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../PollingThread.h"

namespace OSServices
{
	using IOPlatformHandle = int;
	class IConduitProducer_PlatformHandle
	{
	public:
		virtual IOPlatformHandle GetPlatformHandle() const = 0;
		virtual PollingEventType::BitField GetListenTypes() const = 0;
		virtual std::any GeneratePayload(PollingEventType::BitField triggeredEvents) = 0;
		virtual ~IConduitProducer_PlatformHandle() = default;
	};
}

