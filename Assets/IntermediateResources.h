// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Assets.h"

namespace Assets 
{
    class PendingCompileMarker; 
    class DependentFileState; 
    class DependencyValidation; 
    namespace AssetState { enum Enum; }
    class ArchiveCache;


    /// <summary>Records the state of a resource being compiled</summary>
    /// When a resource compile operation begins, we need some generic way
    /// to test it's state. We also need some breadcrumbs to find the final 
    /// result when the compile is finished.
    ///
    /// This class acts as a bridge between the compile operation and
    /// the final resource class. Therefore, we can interchangeable mix
    /// and match different resource implementations and different processing
    /// solutions.
    ///
    /// Sometimes just a filename to the processed resource will be enough.
    /// Other times, objects are stored in a "ArchiveCache" object. For example,
    /// shader compiles are typically combined together into archives of a few
    /// different configurations. So a pointer to an optional ArchiveCache is provided.
    class PendingCompileMarker
    {
    public:
        char    _sourceID0[MaxPath];
        uint64  _sourceID1;
        std::shared_ptr<ArchiveCache> _archive;
        std::shared_ptr<DependencyValidation> _dependencyValidation;

        AssetState::Enum    GetState() const { return _state; }
        void                SetState(AssetState::Enum newState);
        const char*         Initializer() const;
        void                SetInitializer(const char initializer[]);

        PendingCompileMarker();
        PendingCompileMarker(AssetState::Enum state, const char sourceID0[], uint64 sourceID1, std::shared_ptr<DependencyValidation>&& depVal);
        ~PendingCompileMarker();

    protected:
        AssetState::Enum _state;
        DEBUG_ONLY(char _initializer[MaxPath];)
    };
}

namespace Assets { namespace IntermediateResources
{
    class IResourceCompiler;

    class Store
    {
    public:
        std::shared_ptr<DependencyValidation>    MakeDependencyValidation(const ResChar intermediateFileName[]) const;
        std::shared_ptr<DependencyValidation>    WriteDependencies(
            const ResChar intermediateFileName[], const ResChar baseDir[], 
            const std::vector<DependentFileState>& dependencies) const;

        void    MakeIntermediateName(ResChar buffer[], unsigned bufferMaxCount, const ResChar firstInitializer[]) const;

        const DependentFileState& GetDependentFileState(const ResChar filename[]) const;
        void    ShadowFile(const ResChar filename[]);

        Store(const ResChar baseDirectory[], const ResChar versionString[]);
        ~Store();
    protected:
        std::string _baseDirectory;
    };

    class IResourceCompiler
    {
    public:
        virtual std::shared_ptr<PendingCompileMarker> PrepareResource(
            uint64 typeCode, const ResChar* initializers[], unsigned initializerCount,
            const Store& destinationStore) = 0;
        virtual ~IResourceCompiler();
    };

    class CompilerSet
    {
    public:
        void AddCompiler(uint64 typeCode, const std::shared_ptr<IResourceCompiler>& processor);
        std::shared_ptr<PendingCompileMarker> PrepareResource(
            uint64 typeCode, const ResChar* initializers[], unsigned initializerCount,
            Store& store);

        CompilerSet();
        ~CompilerSet();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}}



