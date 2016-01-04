// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Assets.h"
#include "AssetUtils.h"

namespace Assets 
{
    class PendingCompileMarker; 
    class DependentFileState; 
    class DependencyValidation; 
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
    class PendingCompileMarker : public PendingOperationMarker
    {
    public:
        std::shared_ptr<DependencyValidation> _dependencyValidation;

        char    _sourceID0[MaxPath];
        uint64  _sourceID1;
        std::shared_ptr<ArchiveCache> _archive;

        PendingCompileMarker();
        PendingCompileMarker(AssetState state, const char sourceID0[], uint64 sourceID1, std::shared_ptr<DependencyValidation> depVal);
        ~PendingCompileMarker();
    };
}

namespace Assets { namespace IntermediateAssets
{
    class IAssetCompiler;

    class Store
    {
    public:
        using DepVal = std::shared_ptr<DependencyValidation>;

        DepVal MakeDependencyValidation(
            const ResChar intermediateFileName[]) const;

        DepVal WriteDependencies(
            const ResChar intermediateFileName[], 
            StringSection<ResChar> baseDir, 
            IteratorRange<const DependentFileState*> deps,
            bool makeDepValidation = true) const;

        void    MakeIntermediateName(
            ResChar buffer[], unsigned bufferMaxCount, 
            StringSection<ResChar> firstInitializer) const;

        template<int Count>
            void    MakeIntermediateName(ResChar (&buffer)[Count], StringSection<ResChar> firstInitializer) const
            {
                MakeIntermediateName(buffer, Count, firstInitializer);
            }

        static auto GetDependentFileState(const StringSection<ResChar> filename) -> const DependentFileState&;
        static void ShadowFile(const ResChar filename[]);

        Store(const ResChar baseDirectory[], const ResChar versionString[]);
        ~Store();
        Store(const Store&) = delete;
        Store& operator=(const Store&) = delete;

    protected:
        std::string _baseDirectory;
    };

    class IAssetCompiler
    {
    public:
        virtual std::shared_ptr<PendingCompileMarker> PrepareAsset(
            uint64 typeCode, const ResChar* initializers[], unsigned initializerCount,
            const Store& destinationStore) = 0;
        virtual void StallOnPendingOperations(bool cancelAll) = 0;
        virtual ~IAssetCompiler();
    };

    class CompilerSet
    {
    public:
        void AddCompiler(uint64 typeCode, const std::shared_ptr<IAssetCompiler>& processor);
        std::shared_ptr<PendingCompileMarker> PrepareAsset(
            uint64 typeCode, const ResChar* initializers[], unsigned initializerCount,
            Store& store);
        void StallOnPendingOperations(bool cancelAll);

        CompilerSet();
        ~CompilerSet();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}}



