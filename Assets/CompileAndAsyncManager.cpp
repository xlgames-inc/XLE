// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompileAndAsyncManager.h"
#include "IntermediateResources.h"
#include "../ConsoleRig/Log.h"
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

    unsigned AssetSetManager::BoundThreadId() { return _pimpl->_boundThreadId; }

    AssetSetManager::AssetSetManager()
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_boundThreadId = Threading::CurrentThreadId();
        _pimpl = std::move(pimpl);
    }

    AssetSetManager::~AssetSetManager()
    {}

        ////////////////////////////////////////////////////////////

    void CompileAndAsyncManager::Update()
    {
        if (_threadPump) {
            _threadPump->Update();
        }

            //  Normally the polling processes will be waiting for the thread
            //  pump to complete something. So do this after the thread pump update
        for (auto i=_pollingProcesses.begin(); i!=_pollingProcesses.end();) {
            bool remove = false;
            TRY {
                auto result = (*i)->Update();
                remove = result == IPollingAsyncProcess::Result::Finish;
            } CATCH(const std::exception& e) {
                LogWarning << "Got exception during polling process update: " << e.what();
                remove = true;
            } CATCH_END

                    // remove if necessary...
            if (remove) { i = _pollingProcesses.erase(i); } 
            else { ++i; }
        }
    }

    void CompileAndAsyncManager::Add(const std::shared_ptr<IPollingAsyncProcess>& pollingProcess)
    {
        _pollingProcesses.push_back(pollingProcess);
    }

    IntermediateResources::Store& CompileAndAsyncManager::GetIntermediateStore() 
    { 
        return *_intStore.get(); 
    }
    
    IntermediateResources::CompilerSet& CompileAndAsyncManager::GetIntermediateCompilers() 
    { 
        return *_intMan.get(); 
    }

    AssetSetManager& CompileAndAsyncManager::GetAssetSets() 
    { 
        return *_assetSets.get(); 
    }

    void CompileAndAsyncManager::Add(std::unique_ptr<IThreadPump>&& threadPump)
    {
        assert(!_threadPump);
        _threadPump = std::move(threadPump);
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

        _intMan = std::move(intMan);
        _intStore = std::move(intStore);
        _assetSets = std::move(assetSets);
        assert(!_instance);
        _instance = this;
    }

    CompileAndAsyncManager::~CompileAndAsyncManager()
    {
        assert(_instance == this);
        _instance = nullptr;
    }

}

