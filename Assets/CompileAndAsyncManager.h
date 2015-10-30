// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "../Core/Prefix.h"
#include "../Core/Types.h"
#include <memory>
#include <functional>
#include <vector>
#include <assert.h>

namespace Assets
{
    class DependencyValidation; class DependentFileState; 
    namespace IntermediateAssets { class CompilerSet; class Store; }
    class ArchiveCache;

    class IPollingAsyncProcess
    {
    public:
        struct Result { enum Enum { KeepPolling, Finish }; };
        virtual Result::Enum Update() = 0;

        IPollingAsyncProcess();
        virtual ~IPollingAsyncProcess();
    };

    class IThreadPump
    {
    public:
        virtual void Update() = 0;
        virtual ~IThreadPump();
    };

    class CompileAndAsyncManager
    {
    public:
        void Update();

        void Add(const std::shared_ptr<IPollingAsyncProcess>& pollingProcess);
        void Add(std::unique_ptr<IThreadPump>&& threadPump);

        IntermediateAssets::Store&       GetIntermediateStore();
        IntermediateAssets::CompilerSet& GetIntermediateCompilers();

        CompileAndAsyncManager();
        ~CompileAndAsyncManager();
    protected:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
    };

}

