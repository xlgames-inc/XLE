// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../PollingThread.h"

namespace OSServices
{
	namespace Internal
	{
		class KEventTriggerPayload
		{
		public:
			uint16_t        _flags = 0;
			uint32_t		_fflags = 0;
		};
		
		class KEvent : public IConduitProducer
		{
		public:
			uint64_t		_ident = 0;
			int16_t			_filter = 0;
			uint32_t		_fflags = 0;

			virtual std::any GeneratePayload(const KEventTriggerPayload&) = 0;

			KEvent() {}
			KEvent(const KEvent&) = delete;
			KEvent& operator=(const KEvent&) = delete;
		};
	}
}