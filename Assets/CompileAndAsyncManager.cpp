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

    IPollingAsyncProcess::IPollingAsyncProcess() {}
    IPollingAsyncProcess::~IPollingAsyncProcess() {}

        ////////////////////////////////////////////////////////////

	class CompileAndAsyncManager::Pimpl
	{
	public:
		std::unique_ptr<IntermediateAssets::Store> _intStore;
		std::unique_ptr<IntermediateAssets::Store> _shadowingStore;
        std::unique_ptr<CompilerSet> _intMan;
		std::vector<std::shared_ptr<IPollingAsyncProcess>> _pollingProcesses;

		Utility::Threading::Mutex _pollingProcessesLock;
	};

    void CompileAndAsyncManager::Update()
    {
        if (_pimpl->_pollingProcessesLock.try_lock()) {
            TRY
            {
                    //  Normally the polling processes will be waiting for the thread
                    //  pump to complete something. So do this after the thread pump update
		        for (auto i = _pimpl->_pollingProcesses.begin(); i != _pimpl->_pollingProcesses.end();) {
                    bool remove = false;
                    TRY {
                        auto result = (*i)->Update();
                        remove = result == IPollingAsyncProcess::Result::Finish;
                    } CATCH(const std::exception& e) {
                        Log(Warning) << "Got exception during polling process update: " << e.what() << std::endl;
                        remove = true;
						(void)e;
                    } CATCH_END

                            // remove if necessary...
			        if (remove) { i = _pimpl->_pollingProcesses.erase(i); }
                    else { ++i; }
                }
            } CATCH (...) {
                _pimpl->_pollingProcessesLock.unlock();
                throw;
            } CATCH_END
            _pimpl->_pollingProcessesLock.unlock();
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
    
    CompilerSet& CompileAndAsyncManager::GetIntermediateCompilers() 
    { 
		return *_pimpl->_intMan.get();
    }

	IntermediateAssets::Store& CompileAndAsyncManager::GetShadowingStore()
	{
		return *_pimpl->_shadowingStore.get();
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
        const char storeVersionString[] = "0.0.0";
        #if defined(_DEBUG)
            #if TARGET_64BIT
                const char storeConfigString[] = "d64";
            #else
                const char storeConfigString[] = "d";
            #endif
        #else
            #if TARGET_64BIT
                const char storeConfigString[] = "r64";
            #else
                const char storeConfigString[] = "r";
            #endif
        #endif

		_pimpl = std::make_unique<Pimpl>();
		_pimpl->_intStore = std::make_unique<IntermediateAssets::Store>("int", storeVersionString, storeConfigString);
		_pimpl->_shadowingStore = std::make_unique<IntermediateAssets::Store>("int", storeVersionString, storeConfigString, true);

		_pimpl->_intMan = std::make_unique<CompilerSet>();
    }

    CompileAndAsyncManager::~CompileAndAsyncManager()
    {
            // note -- this order is important. The compiler set
            // can make use of the IntermediateAssets::Store during
            // it's destructor (eg, when flushing an archive cache to disk). 
        _pimpl->_intMan.reset();
        _pimpl->_intStore.reset();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	class CompilerSet::Pimpl
	{
	public:
		std::vector<std::pair<uint64, std::shared_ptr<IAssetCompiler>>> _compilers;
	};

	void CompilerSet::AddCompiler(uint64 typeCode, const std::shared_ptr<IAssetCompiler>& processor)
	{
		auto i = LowerBound(_pimpl->_compilers, typeCode);
		if (i != _pimpl->_compilers.cend() && i->first == typeCode) {
			i->second = processor;
		}
		else {
			_pimpl->_compilers.insert(i, std::make_pair(typeCode, processor));
		}
	}

	std::shared_ptr<IArtifactPrepareMarker> CompilerSet::Prepare(
		uint64 typeCode, const StringSection<ResChar> initializers[], unsigned initializerCount)
	{
		// look for a "processor" object with the given type code, and rebuild the file
		// write the .deps file containing dependencies information
		//  Note that there's a slight race condition type problem here. We are querying
		//  the dependency files for their state after the processing has completed. So
		//  if the dependency file changes state during processing, we might not recognize
		//  that change properly. It's probably ignorable, however.

		// note that ideally we want to be able to schedule this in the background
		auto i = LowerBound(_pimpl->_compilers, typeCode);
		if (i != _pimpl->_compilers.cend() && i->first == typeCode) {
			return i->second->Prepare(typeCode, initializers, initializerCount);
		}
		else {
			assert(0);  // couldn't find a processor for this asset type
		}

		return nullptr;
	}

	void CompilerSet::StallOnPendingOperations(bool cancelAll)
	{
		for (auto i = _pimpl->_compilers.cbegin(); i != _pimpl->_compilers.cend(); ++i)
			i->second->StallOnPendingOperations(cancelAll);
	}

	CompilerSet::CompilerSet()
	{
		auto pimpl = std::make_unique<Pimpl>();
		_pimpl = std::move(pimpl);
	}

	CompilerSet::~CompilerSet()
	{
	}

	IAssetCompiler::~IAssetCompiler() {}
}

