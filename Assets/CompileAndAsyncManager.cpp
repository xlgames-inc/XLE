// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompileAndAsyncManager.h"
#include "IntermediateAssets.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Core/SelectConfiguration.h"

namespace Assets
{

        ////////////////////////////////////////////////////////////

    void IPollingAsyncProcess::FireTrigger( AssetState newState,
                                            const std::vector<Assets::DependentFileState>& dependencies) 
    {
        _fn(newState, dependencies); 
    }
    IPollingAsyncProcess::IPollingAsyncProcess(CallbackFn&& fn) : _fn(std::move(fn)) {}
    IPollingAsyncProcess::~IPollingAsyncProcess() {}

    IThreadPump::~IThreadPump() {}

        ////////////////////////////////////////////////////////////


        ////////////////////////////////////////////////////////////

	class CompileAndAsyncManager::Pimpl
	{
	public:
		std::unique_ptr<IntermediateAssets::Store> _intStore;
        std::unique_ptr<IntermediateAssets::CompilerSet> _intMan;
		std::vector<std::shared_ptr<IPollingAsyncProcess>> _pollingProcesses;
		std::unique_ptr<IThreadPump> _threadPump;

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

    IntermediateAssets::Store& CompileAndAsyncManager::GetIntermediateStore() 
    { 
		return *_pimpl->_intStore.get();
    }
    
    IntermediateAssets::CompilerSet& CompileAndAsyncManager::GetIntermediateCompilers() 
    { 
		return *_pimpl->_intMan.get();
    }

    void CompileAndAsyncManager::Add(std::unique_ptr<IThreadPump>&& threadPump)
    {
		assert(!_pimpl->_threadPump);
		_pimpl->_threadPump = std::move(threadPump);
    }

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
        #if defined(_DEBUG)
            #if TARGET_64BIT
                const char storeVersionString[] = "0.0.0d64";
            #else
                const char storeVersionString[] = "0.0.0d";
            #endif
        #else
            #if TARGET_64BIT
                const char storeVersionString[] = "0.0.0r64";
            #else
                const char storeVersionString[] = "0.0.0r";
            #endif
        #endif
        auto intStore = std::make_unique<IntermediateAssets::Store>("Int", storeVersionString);

        auto intMan = std::make_unique<IntermediateAssets::CompilerSet>();
		_pimpl = std::make_unique<Pimpl>();

        _pimpl->_intMan = std::move(intMan);
        _pimpl->_intStore = std::move(intStore);
    }

    CompileAndAsyncManager::~CompileAndAsyncManager()
    {
            // note -- this order is important. The compiler set
            // can make use of the IntermediateAssets::Store during
            // it's destructor (eg, when flushing an archive cache to disk). 
        _pimpl->_intMan.reset();
        _pimpl->_intStore.reset();
    }

}

