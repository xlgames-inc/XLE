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
    namespace IntermediateAssets { class Store; }
    class ArchiveCache;

    class IPollingAsyncProcess
    {
    public:
        struct Result { enum Enum { KeepPolling, Finish }; };
        virtual Result::Enum Update() = 0;

        IPollingAsyncProcess();
        virtual ~IPollingAsyncProcess();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

	class IArtifactPrepareMarker;

	class IAssetCompiler
	{
	public:
		virtual std::shared_ptr<IArtifactPrepareMarker> Prepare(
			uint64_t typeCode, const StringSection<ResChar> initializers[], unsigned initializerCount) = 0;
		virtual void StallOnPendingOperations(bool cancelAll) = 0;
		virtual ~IAssetCompiler();
	};

	class CompilerSet
	{
	public:
		void AddCompiler(uint64_t typeCode, const std::shared_ptr<IAssetCompiler>& processor);
		std::shared_ptr<IArtifactPrepareMarker> Prepare(
			uint64_t typeCode, const StringSection<ResChar> initializers[], unsigned initializerCount);
		void StallOnPendingOperations(bool cancelAll);

		CompilerSet();
		~CompilerSet();
	protected:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

    class CompileAndAsyncManager
    {
    public:
        void Update();

        void Add(const std::shared_ptr<IPollingAsyncProcess>& pollingProcess);

		CompilerSet&				GetIntermediateCompilers();
        IntermediateAssets::Store&	GetIntermediateStore();
		IntermediateAssets::Store&	GetShadowingStore();

        CompileAndAsyncManager();
        ~CompileAndAsyncManager();
    protected:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
    };

}

