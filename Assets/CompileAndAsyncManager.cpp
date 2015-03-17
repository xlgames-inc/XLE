// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompileAndAsyncManager.h"
#include "IntermediateResources.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/PtrUtils.h"

namespace Assets
{

        ////////////////////////////////////////////////////////////

    void IPollingAsyncProcess::FireTrigger( AssetState::Enum newState,
                                            const std::vector<Assets::FileAndTime>& dependencies) 
    {
        _fn(newState, dependencies); 
    }
    IPollingAsyncProcess::IPollingAsyncProcess(CallbackFn&& fn) : _fn(std::move(fn)) {}
    IPollingAsyncProcess::~IPollingAsyncProcess() {}

    IThreadPump::~IThreadPump() {}

        ////////////////////////////////////////////////////////////

    IAssetSet::~IAssetSet() {}

    class AssetSetManager::Pimpl
    {
    public:
        std::vector<std::unique_ptr<IAssetSet>> _sets;
        unsigned _boundThreadId;
    };

    void AssetSetManager::Add(std::unique_ptr<IAssetSet>&& set)
    {
        _pimpl->_sets.push_back(std::forward<std::unique_ptr<IAssetSet>>(set));
    }

    void AssetSetManager::Clear()
    {
        for (auto i=_pimpl->_sets.begin(); i!=_pimpl->_sets.end(); ++i) {
            (*i)->Clear();
        }
    }

    void AssetSetManager::LogReport()
    {
        for (auto i=_pimpl->_sets.begin(); i!=_pimpl->_sets.end(); ++i) {
            (*i)->LogReport();
        }
    }

	bool AssetSetManager::IsBoundThread() const
	{
		return _pimpl->_boundThreadId == Threading::CurrentThreadId();
	}

    AssetSetManager::AssetSetManager()
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_boundThreadId = Threading::CurrentThreadId();
        _pimpl = std::move(pimpl);
    }

    AssetSetManager::~AssetSetManager()
    {}

        ////////////////////////////////////////////////////////////

	class CompileAndAsyncManager::Pimpl
	{
	public:
		std::unique_ptr<IntermediateResources::CompilerSet> _intMan;
		std::unique_ptr<IntermediateResources::Store> _intStore;
		std::vector<std::shared_ptr<IPollingAsyncProcess>> _pollingProcesses;
		std::unique_ptr<IThreadPump> _threadPump;
		std::unique_ptr<AssetSetManager> _assetSets;

		Utility::Threading::Mutex _pollingProcessesLock;
	};

    void CompileAndAsyncManager::Update()
    {
		if (_pimpl->_threadPump) {
			_pimpl->_threadPump->Update();
        }

		ScopedLock(_pimpl->_pollingProcessesLock);

            //  Normally the polling processes will be waiting for the thread
            //  pump to complete something. So do this after the thread pump update
		for (auto i = _pimpl->_pollingProcesses.begin(); i != _pimpl->_pollingProcesses.end();) {
            bool remove = false;
            TRY {
                auto result = (*i)->Update();
                remove = result == IPollingAsyncProcess::Result::Finish;
            } CATCH(const std::exception& e) {
                LogWarning << "Got exception during polling process update: " << e.what();
                remove = true;
            } CATCH_END

                    // remove if necessary...
			if (remove) { i = _pimpl->_pollingProcesses.erase(i); }
            else { ++i; }
        }
    }

    void CompileAndAsyncManager::Add(const std::shared_ptr<IPollingAsyncProcess>& pollingProcess)
    {
		ScopedLock(_pimpl->_pollingProcessesLock);
		_pimpl->_pollingProcesses.push_back(pollingProcess);
    }

    IntermediateResources::Store& CompileAndAsyncManager::GetIntermediateStore() 
    { 
		return *_pimpl->_intStore.get();
    }
    
    IntermediateResources::CompilerSet& CompileAndAsyncManager::GetIntermediateCompilers() 
    { 
		return *_pimpl->_intMan.get();
    }

    AssetSetManager& CompileAndAsyncManager::GetAssetSets() 
    { 
		return *_pimpl->_assetSets.get();
    }

    void CompileAndAsyncManager::Add(std::unique_ptr<IThreadPump>&& threadPump)
    {
		assert(!_pimpl->_threadPump);
		_pimpl->_threadPump = std::move(threadPump);
    }

    CompileAndAsyncManager* CompileAndAsyncManager::_instance = nullptr;

    CompileAndAsyncManager::CompileAndAsyncManager()
    {
            // todo --  this version string can be used to differentiate different
            //          versions of the compiling tools. This is important when we
            //          need to switch back and forth between different versions
            //          quickly. This can happen if we have different format 
            //          resources for debug and release. Or, while building the
            //          some new format, it's often useful to be able to compare
            //          to previous version. Also, there are times when we want to
            //          run an old version of the engine, and we don't want it to
            //          conflict with more recent work, even if there have been
            //          major changes in the meantime.
        auto intMan = std::make_unique<IntermediateResources::CompilerSet>();
        #if defined(_DEBUG)
            const char storeVersionString[] = "0.0.0d";
        #else
            const char storeVersionString[] = "0.0.0r";
        #endif
        auto intStore = std::make_unique<IntermediateResources::Store>("Int", storeVersionString);
        auto assetSets = std::make_unique<AssetSetManager>();

		_pimpl = std::make_unique<Pimpl>();

        _pimpl->_intMan = std::move(intMan);
        _pimpl->_intStore = std::move(intStore);
        _pimpl->_assetSets = std::move(assetSets);
        assert(!_instance);
        _instance = this;
    }

    CompileAndAsyncManager::~CompileAndAsyncManager()
    {
        assert(_instance == this);
        _instance = nullptr;
    }

}

