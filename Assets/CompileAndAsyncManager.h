// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace Assets
{
    class DependencyValidation; class DependentFileState; 
    class IntermediatesStore;
	class IIntermediateCompilers;
    class ArchiveCache;
    class IFileSystem;

    class IPollingAsyncProcess
    {
    public:
        struct Result { enum Enum { KeepPolling, Finish }; };
        virtual Result::Enum Update() = 0;

        IPollingAsyncProcess();
        virtual ~IPollingAsyncProcess();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class CompileAndAsyncManager
    {
    public:
        void Update();

        void Add(const std::shared_ptr<IPollingAsyncProcess>& pollingProcess);

		IIntermediateCompilers& GetIntermediateCompilers();

        const std::shared_ptr<IntermediatesStore>&	GetIntermediateStore();
		const std::shared_ptr<IntermediatesStore>&	GetShadowingStore();

        CompileAndAsyncManager(std::shared_ptr<IFileSystem> intermediatesFilesystem);
        ~CompileAndAsyncManager();
    protected:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
    };

}

