// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Utility/Threading/ThreadingUtils.h"
#include <memory>
#include <cstdint>
#include <future>

namespace OSServices
{
	struct PollingEventType
	{
		enum Flags
		{
			Input = 1<<0,
			Output = 1<<1
		};
		using BitField = unsigned;
	};

	using IOPlatformHandle = uint64_t;

	class IConduit
	{
	public:
		virtual void OnEvent(PollingEventType::BitField) = 0;
		virtual void OnException(const std::exception_ptr& exception) = 0;
		virtual ~IConduit() = default;
	};

	class PollingThread
	{
	public:
		// Simple event type -- just tell me when something happens. One-off, with
		// no explicit cancel mechanism
		std::future<PollingEventType::BitField> RespondOnce(
			IOPlatformHandle, 
			PollingEventType::BitField eventTypes = PollingEventType::Flags::Input);

		std::future<void> Connect(
			const std::shared_ptr<IConduit>& conduit, 
			PollingEventType::BitField eventTypes = PollingEventType::Flags::Input);

		std::future<void> Disconnect(
			const std::shared_ptr<IConduit>& conduit);

		PollingThread();
		~PollingThread();
	private:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
	};
}

