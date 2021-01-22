// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AttachablePtr.h"
#include "../Utility/IteratorUtils.h"
#include <vector>

namespace ConsoleRig
{
	namespace Internal
	{
		class InfraModuleManager::Pimpl
		{
		public:
			struct RegisteredPtr
			{
				IRegistrablePointer* _ptr = nullptr;
				RegisteredPointerId _id = 0; 
			};
			std::vector<std::pair<uint64_t, RegisteredPtr>> _registeredPointers;

			struct RegisteredType
			{
				unsigned _localReferenceCounts = 0;
				std::shared_ptr<void> _currentValue;
				std::function<void(const std::shared_ptr<void>&)> _attachModuleFn;
				std::function<void(const std::shared_ptr<void>&)> _detachModuleFn;
			};
			std::vector<std::pair<uint64_t, RegisteredType>> _registeredTypes;

			RegisteredPointerId _nextRegisteredPointerId = 1;
			CrossModule::RegisteredInfraModuleManagerId _crossModuleRegistration = ~0u;
		};

		void InfraModuleManager::PropagateChange(uint64_t id, const std::shared_ptr<void>& obj)
		{
			auto i2 = LowerBound(_pimpl->_registeredTypes, id);
			// If there are no pointers currently referencing this type, we will just early out now
			if (i2 == _pimpl->_registeredTypes.end() || i2->first != id || i2->second._localReferenceCounts == 0)
				return;

			if (i2->second._detachModuleFn && i2->second._currentValue)
				i2->second._detachModuleFn(i2->second._currentValue);
			i2->second._currentValue = obj;

			auto range = EqualRange(_pimpl->_registeredPointers, id);
			for (auto i=range.first; i!=range.second; ++i)
				i->second._ptr->PropagateChange(obj);

			if (i2->second._currentValue && i2->second._attachModuleFn)
				i2->second._attachModuleFn(i2->second._currentValue);
		}

		auto InfraModuleManager::Register(uint64_t id, IRegistrablePointer* ptr) -> RegisteredPointerId
		{
			auto result = _pimpl->_nextRegisteredPointerId++;
			auto i = LowerBound(_pimpl->_registeredPointers, id);
			_pimpl->_registeredPointers.insert(i, {id, Pimpl::RegisteredPtr{ptr, result}});

			auto i2 = LowerBound(_pimpl->_registeredTypes, id);
			if (i2 != _pimpl->_registeredTypes.end() && i2->first == id) {
				i2->second._localReferenceCounts++;
			} else {
				_pimpl->_registeredTypes.insert(i2, {id, Pimpl::RegisteredType{1}});
			}
			return result;
		}

		void InfraModuleManager::Deregister(RegisteredPointerId id)
		{
			auto i = std::find_if(_pimpl->_registeredPointers.begin(), _pimpl->_registeredPointers.end(),
				[id](auto c) { return c.second._id == id; });
			assert(i != _pimpl->_registeredPointers.end());
			if (i != _pimpl->_registeredPointers.end()) {
				auto type = i->first;
				_pimpl->_registeredPointers.erase(i);

				auto i2 = LowerBound(_pimpl->_registeredTypes, type);
				assert(i2 != _pimpl->_registeredTypes.end() && i2->first == type);
				if (i2 != _pimpl->_registeredTypes.end() && i2->first == type) {
					assert(i2->second._localReferenceCounts > 0);
					auto newRefCount = --i2->second._localReferenceCounts;

					if (newRefCount == 0 && i2->second._detachModuleFn && i2->second._currentValue) {
						i2->second._detachModuleFn(i2->second._currentValue);
						i2->second._currentValue = nullptr;
					}
				}
			}
		}

		void InfraModuleManager::ConfigureType(
			uint64_t id,
			std::function<void(const std::shared_ptr<void>&)>&& attachModuleFn,
			std::function<void(const std::shared_ptr<void>&)>&& detachModuleFn)
		{
			auto i2 = LowerBound(_pimpl->_registeredTypes, id);
			if (i2 == _pimpl->_registeredTypes.end() || i2->first != id)
				i2 = _pimpl->_registeredTypes.insert(i2, {id, Pimpl::RegisteredType{0}});
			
			assert(!i2->second._attachModuleFn && !i2->second._detachModuleFn);
			i2->second._attachModuleFn = std::move(attachModuleFn);
			i2->second._detachModuleFn = std::move(detachModuleFn);

			// if there's already local reference counts, we should in theory call the attach function here
			if (i2->second._localReferenceCounts && i2->second._currentValue)
				i2->second._attachModuleFn(i2->second._currentValue);
		}

		auto InfraModuleManager::Get(uint64_t id) -> std::shared_ptr<void>
		{
			auto cannonicalValue = CrossModule::GetInstance().Get(id);
			auto i2 = LowerBound(_pimpl->_registeredTypes, id);
			if (i2 == _pimpl->_registeredTypes.end() || i2->first != id)
				i2 = _pimpl->_registeredTypes.insert(i2, {id, Pimpl::RegisteredType{0}});
			if (!i2->second._currentValue) {
				i2->second._currentValue = cannonicalValue;

				if (i2->second._currentValue && i2->second._attachModuleFn)
					i2->second._attachModuleFn(i2->second._currentValue);
			} else {
				assert(i2->second._currentValue == cannonicalValue);
			}
			return cannonicalValue;
		}

		void InfraModuleManager::Reset(uint64_t id, const std::shared_ptr<void>& obj)
		{
			// flow will return here via PropagateChange
			CrossModule::GetInstance().Reset(id, obj, _pimpl->_crossModuleRegistration);
		}

		void InfraModuleManager::CrossModuleShuttingDown()
		{
			for (const auto&ptr:_pimpl->_registeredPointers)
				ptr.second._ptr->ManagerShuttingDown();
			_pimpl->_registeredPointers.clear();
			for (const auto&type:_pimpl->_registeredTypes)
				if (type.second._localReferenceCounts && type.second._currentValue && type.second._detachModuleFn)
					type.second._detachModuleFn(type.second._currentValue);
			_pimpl->_registeredTypes.clear();
			_pimpl->_crossModuleRegistration = ~0u;
		}

		InfraModuleManager::InfraModuleManager()
		{
			_pimpl = std::make_unique<Pimpl>();
			_pimpl->_crossModuleRegistration = CrossModule::GetInstance().Register(this);
		}

		InfraModuleManager::~InfraModuleManager()
		{
			// We can detach all of the pointers before or after we deregister from the CrossModule manager
			for (const auto&ptr:_pimpl->_registeredPointers)
				ptr.second._ptr->ManagerShuttingDown();
			_pimpl->_registeredPointers.clear();
			for (const auto&type:_pimpl->_registeredTypes)
				if (type.second._localReferenceCounts && type.second._currentValue && type.second._detachModuleFn)
					type.second._detachModuleFn(type.second._currentValue);
			_pimpl->_registeredTypes.clear();
			if (_pimpl->_crossModuleRegistration != ~0u)
				CrossModule::GetInstance().Deregister(_pimpl->_crossModuleRegistration);
		}

		InfraModuleManager& InfraModuleManager::GetInstance()
		{
			static InfraModuleManager moduleSpecificInstance;
			return moduleSpecificInstance;
		}
	}

	class CrossModule::Pimpl
	{
	public:
		struct CannonicalPtr
		{
			std::weak_ptr<void> _ptr;
			CrossModule::RegisteredInfraModuleManagerId _owningInfraModuleManager = ~0u;
		};
		std::vector<std::pair<uint64_t, CannonicalPtr>> _cannonicalPtrs;
		std::vector<std::pair<RegisteredInfraModuleManagerId, Internal::InfraModuleManager*>> _moduleSpecificManagers;
		RegisteredInfraModuleManagerId _nextInfraModuleManagerRegistration = 1u;
	};

	auto CrossModule::Get(uint64_t id) -> std::shared_ptr<void>
	{
		auto i = LowerBound(_pimpl->_cannonicalPtrs, id);
		if (i != _pimpl->_cannonicalPtrs.end() && i->first == id)
			return i->second._ptr.lock();
		return nullptr;
	}

	void CrossModule::Reset(uint64_t id, const std::shared_ptr<void>& obj, RegisteredInfraModuleManagerId owner)
	{
		// Note that there's not any threading protection during this set
		// So if another thread is reading the value at the same time, it
		// will be very messy
		auto i = LowerBound(_pimpl->_cannonicalPtrs, id);
		if (i != _pimpl->_cannonicalPtrs.end() && i->first == id) {
			i->second._ptr = obj;
			i->second._owningInfraModuleManager = owner;
		} else
			_pimpl->_cannonicalPtrs.insert(i, {id, Pimpl::CannonicalPtr{obj, owner}});
		for (auto& moduleManager:_pimpl->_moduleSpecificManagers)
			moduleManager.second->PropagateChange(id, obj);
	}

	auto CrossModule::Register(Internal::InfraModuleManager* ptr) -> RegisteredInfraModuleManagerId
	{
		auto i = std::find_if(_pimpl->_moduleSpecificManagers.begin(), _pimpl->_moduleSpecificManagers.end(), [ptr](auto c) { return c.second == ptr; });
		assert(i == _pimpl->_moduleSpecificManagers.end()); (void)i;

		auto res = _pimpl->_nextInfraModuleManagerRegistration++;
		_pimpl->_moduleSpecificManagers.push_back({res, ptr});
		return res;
	}

	void CrossModule::Deregister(RegisteredInfraModuleManagerId id)
	{
		// reset all pointers associated with this module
		// Note that this will also result in calls to PropagateChange() for the module that's actually getting shut down
		for (const auto&ptr:_pimpl->_cannonicalPtrs)
			if (ptr.second._owningInfraModuleManager == id)
				Reset(ptr.first, nullptr, ~0u);
		auto i = std::find_if(_pimpl->_moduleSpecificManagers.begin(), _pimpl->_moduleSpecificManagers.end(), [id](auto c) { return c.first == id; });
		assert(i != _pimpl->_moduleSpecificManagers.end());
		if (i != _pimpl->_moduleSpecificManagers.end())
			_pimpl->_moduleSpecificManagers.erase(i);
	}

	CrossModule& CrossModule::GetInstance()
	{
		static CrossModule wholeProcessInstance;
		return wholeProcessInstance;
	}
	
	CrossModule::CrossModule()
	{
		_pimpl = std::make_unique<Pimpl>();
	}

	CrossModule::~CrossModule()
	{
	}
}
