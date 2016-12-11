// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Assets.h"
#include "AssetUtils.h"
#include "../Utility/Streams/FileUtils.h"

namespace Assets 
{
    class PendingCompileMarker; 
    class DependentFileState; 
    class DependencyValidation; 
    class ArchiveCache;

    class IntermediateAssetLocator
    {
    public:
        std::shared_ptr<DependencyValidation> _dependencyValidation;

        ResChar     _sourceID0[MaxPath];
        uint64      _sourceID1;
        std::shared_ptr<ArchiveCache> _archive;

        IntermediateAssetLocator();
        ~IntermediateAssetLocator();
    };

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
        // this has become very much like a std::promise<IntermediateAssetLocator>!
        const IntermediateAssetLocator& GetLocator() const;
        IntermediateAssetLocator& GetLocator();
        void SetLocator(const IntermediateAssetLocator& locator);

        PendingCompileMarker();
        ~PendingCompileMarker();

		PendingCompileMarker(PendingCompileMarker&&) = delete;
		PendingCompileMarker& operator=(PendingCompileMarker&&) = delete;
		PendingCompileMarker(const PendingCompileMarker&) = delete;
		PendingCompileMarker& operator=(const PendingCompileMarker&) = delete;

	private:
		IntermediateAssetLocator _locator;
    };

    class ICompileMarker
    {
    public:
        virtual IntermediateAssetLocator GetExistingAsset() const = 0;
        virtual std::shared_ptr<PendingCompileMarker> InvokeCompile() const = 0;
        virtual StringSection<ResChar> Initializer() const = 0;
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

        static auto GetDependentFileState(StringSection<ResChar> filename) -> DependentFileState;
        static void ShadowFile(StringSection<ResChar> filename);

        Store(const ResChar baseDirectory[], const ResChar versionString[], const ResChar configString[], bool universal = false);
        ~Store();
        Store(const Store&) = delete;
        Store& operator=(const Store&) = delete;

    protected:
        std::string _baseDirectory;
        BasicFile _markerFile;
    };

    class IAssetCompiler
    {
    public:
        virtual std::shared_ptr<ICompileMarker> PrepareAsset(
            uint64 typeCode, const StringSection<ResChar> initializers[], unsigned initializerCount,
            const Store& destinationStore) = 0;
        virtual void StallOnPendingOperations(bool cancelAll) = 0;
        virtual ~IAssetCompiler();
    };

    class CompilerSet
    {
    public:
        void AddCompiler(uint64 typeCode, const std::shared_ptr<IAssetCompiler>& processor);
        std::shared_ptr<ICompileMarker> PrepareAsset(
            uint64 typeCode, const StringSection<ResChar> initializers[], unsigned initializerCount,
            Store& store);
        void StallOnPendingOperations(bool cancelAll);

        CompilerSet();
        ~CompilerSet();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}}



