// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RunLoop_WinAPI.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Utility/TimeUtils.h"

namespace PlatformRig
{
	auto OSRunLoop_BasicTimer::ScheduleTimeoutEvent(unsigned timePoint, TimeoutCallback&& callback) -> EventId
	{
		auto i = LowerBound(_timeoutEvents, timePoint);
		bool isFirst = i == _timeoutEvents.begin();
		auto eventId = _nextEventId++;
		_timeoutEvents.emplace(i, std::make_pair(timePoint, TimeoutEvent{eventId, std::move(callback)}));
		if (isFirst)
			ResetUnderlyingTimer(Millisecond_Now());
		return eventId;
	}

	void OSRunLoop_BasicTimer::RemoveEvent(EventId evnt)
	{
		auto i = std::find_if(
			_timeoutEvents.begin(), _timeoutEvents.end(),
			[evnt](const std::pair<unsigned, TimeoutEvent>& p) { return p.second._eventId == evnt; });
		if (i != _timeoutEvents.end()) {
			bool isFirst = i == _timeoutEvents.begin();
			_timeoutEvents.erase(i);
			if (isFirst)
				ResetUnderlyingTimer(Millisecond_Now());
		}
	}

	bool OSRunLoop_BasicTimer::OnOSTrigger(UINT_PTR osTimer)
	{
		if (osTimer != _osTimer) return false;

		auto currentTimepoint = Millisecond_Now();
		while (!_timeoutEvents.empty() && _timeoutEvents.begin()->first <= currentTimepoint) {
			_timeoutEvents.begin()->second._callback();
			_timeoutEvents.erase(_timeoutEvents.begin());
		}

		ResetUnderlyingTimer(currentTimepoint);
		return true;
	}

	void OSRunLoop_BasicTimer::ResetUnderlyingTimer(unsigned currentTimepoint)
	{
		if (!_timeoutEvents.empty()) {
			auto timeToNext = _timeoutEvents.begin()->first - currentTimepoint;
			if (_attachedWindow != 0) {
				_osTimer = 1453;	// must be some application defined unique id (other than 0)
			} else {
				_osTimer = 0;
			}
			_osTimer = SetTimer(_attachedWindow, _osTimer, timeToNext, nullptr);
		} else if (_osTimer != 0) {
			KillTimer(_attachedWindow, _osTimer);
			_osTimer = 0;
		}
	}

	OSRunLoop_BasicTimer::OSRunLoop_BasicTimer(HWND attachedWindow)
	: _attachedWindow(attachedWindow)
	, _osTimer(0)
	, _nextEventId(1)
	{}

	OSRunLoop_BasicTimer::~OSRunLoop_BasicTimer()
	{
		if (_osTimer != 0)
			KillTimer(_attachedWindow, _osTimer);
	}
}
