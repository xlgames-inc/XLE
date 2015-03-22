// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Prefix.h"
#include "../Core/Types.h"
#include <memory>
#include <functional>
#include <vector>
#include <assert.h>

namespace Assets
{
    class DependencyValidation; class FileAndTime; 
    namespace IntermediateResources { class CompilerSet; class Store; }
    namespace AssetState { enum Enum; }
    class ArchiveCache;

    class IPollingAsyncProcess
    {
    public:
        struct Result { enum Enum { KeepPolling, Finish }; };
        virtual Result::Enum Update() = 0;

        typedef std::function<void(
            AssetState::Enum newState,
            const std::vector<FileAndTime>&)> CallbackFn;
        IPollingAsyncProcess(CallbackFn&& fn);
        virtual ~IPollingAsyncProcess();
    protected:
        void FireTrigger(AssetState::Enum, const std::vector<FileAndTime>& = std::vector<FileAndTime>());
    private:
        CallbackFn _fn;
    };

    class IThreadPump
    {
    public:
        virtual void Update() = 0;
        virtual ~IThreadPump();
    };

    class IAssetSet
    {
    public:
        virtual void            Clear() = 0;
        virtual void            LogReport() const = 0;
        virtual uint64          GetTypeCode() const = 0;
        virtual const char*     GetTypeName() const = 0;
        virtual unsigned        GetDivergentCount() const = 0;
        virtual uint64          GetDivergentId(unsigned index) const = 0;
        virtual std::string     GetAssetName(uint64 id) const = 0;
        virtual ~IAssetSet();
    };

    class AssetSetManager
    {
    public:
        void Add(std::unique_ptr<IAssetSet>&& set);
        void Clear();
        void LogReport();
        unsigned BoundThreadId() const;
		bool IsBoundThread() const;

        unsigned GetAssetSetCount();
        const IAssetSet* GetAssetSet(unsigned index);

        AssetSetManager();
        ~AssetSetManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class CompileAndAsyncManager
    {
    public:
        void Update();

        void Add(const std::shared_ptr<IPollingAsyncProcess>& pollingProcess);
        void Add(std::unique_ptr<IThreadPump>&& threadPump);

        IntermediateResources::Store&       GetIntermediateStore();
        IntermediateResources::CompilerSet& GetIntermediateCompilers();
        AssetSetManager&                    GetAssetSets();

        CompileAndAsyncManager();
        ~CompileAndAsyncManager();

        static CompileAndAsyncManager& GetInstance() { assert(_instance); return *_instance; }
    protected:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;

        static CompileAndAsyncManager* _instance;
    };

}

