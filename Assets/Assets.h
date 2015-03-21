// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CompileAndAsyncManager.h"
#include "../Utility/Streams/FileSystemMonitor.h"       // (for OnChangeCallback base class)
#include "../Utility/IteratorUtils.h"
#include "../Core/Types.h"
#include "../Core/Exceptions.h"
#include <vector>
#include <utility>

#if defined(_DEBUG)
    #define ASSETS_STORE_NAMES
#endif
#define ASSETS_STORE_DIVERGENT		// divergent assets are intended for tools (not in-game). But we can't selectively disable this feature

#if defined(ASSETS_STORE_DIVERGENT)
	#include "DivergentAsset.h"
#endif

namespace Assets
{
    typedef char ResChar;

        ////////////////////////////////////////////////////////////////////////

    namespace Internal
    {
		template <typename AssetType>
			class AssetTraits
		{
		public:
			using DivAsset = DivergentAsset<AssetType>;
		};

        template <typename AssetType>
            class AssetSet : public IAssetSet
        {
        public:
            ~AssetSet();
            void Clear();
            void LogReport();

            static std::vector<std::pair<uint64, std::unique_ptr<AssetType>>> _assets;
			
			#if defined(ASSETS_STORE_DIVERGENT)
				using DivAsset = typename AssetTraits<AssetType>::DivAsset;
				static std::vector<std::pair<uint64, std::unique_ptr<DivAsset>>> _divergentAssets;
			#endif

            #if defined(ASSETS_STORE_NAMES)
                static std::vector<std::pair<uint64, std::string>> _assetNames;
            #endif
        };

            // (utility functions pulled out-of-line)
        void LogHeader(unsigned count, const char typeName[]);
        void LogAssetName(unsigned index, const char name[]);
        void InsertAssetName(   
            std::vector<std::pair<uint64, std::string>>& assetNames, 
            uint64 hash,
            const char* initializers[], unsigned initializerCount);

        template<typename AssetType>
            AssetSet<AssetType>& GetAssetSet() 
        {
            static AssetSet<AssetType>* set = nullptr;
            if (!set) {
                auto s = std::make_unique<AssetSet<AssetType>>();
                set = s.get();
                auto& assetSets = CompileAndAsyncManager::GetInstance().GetAssetSets();
                assetSets.Add(std::move(s));
            }
            assert(CompileAndAsyncManager::GetInstance().GetAssetSets().IsBoundThread());  // currently not thread safe; we have to check the thread ids
            #if defined(ASSETS_STORE_NAMES)
                    // These should agree. If there's a mismatch, there may be a threading problem
                assert(set->_assets.size() == set->_assetNames.size());
            #endif
            return *set;
        }

        template<typename AssetType> struct Ptr
            { typedef std::unique_ptr<AssetType> Value; };

        template <int DoCheckDependancy> struct CheckDependancy { template<typename Resource> static bool NeedsRefresh(const Resource* resource); };
        template<> struct CheckDependancy<1>   { template <typename Resource> static bool NeedsRefresh(const Resource* resource)   { return !resource || (resource->GetDependencyValidation().GetValidationIndex()!=0); } };
        template<> struct CheckDependancy<0>   { template <typename Resource> static bool NeedsRefresh(const Resource* resource)   { return !resource; } };

        template <int BoBackgroundCompile> struct ConstructAsset {};

            //  Here, AssetInitializer is basically just a templated array.
            //  In C++11, we could use variadic template parameters, and 
            //  that would give a much better result.
            //  But at the moment, we're still keeping Visual Studio 2010 support.
        template <int InitCount> struct AssetInitializer {};
        template <> struct AssetInitializer<1> { const char *_name0; };
        template <> struct AssetInitializer<2> { const char *_name0, *_name1; };
        template <> struct AssetInitializer<3> { const char *_name0, *_name1, *_name2; };
        template <> struct AssetInitializer<4> { const char *_name0, *_name1, *_name2, *_name3; };
        template <> struct AssetInitializer<5> { const char *_name0, *_name1, *_name2, *_name3, *_name4; };
        template <> struct AssetInitializer<6> { const char *_name0, *_name1, *_name2, *_name3, *_name4, *_name5; };

        template<> struct ConstructAsset<0>
        { 
            template<typename AssetType> static typename Ptr<AssetType>::Value Create(AssetInitializer<1> init) { return std::make_unique<AssetType>(init._name0); }
            template<typename AssetType> static typename Ptr<AssetType>::Value Create(AssetInitializer<2> init) { return std::make_unique<AssetType>(init._name0, init._name1); }
            template<typename AssetType> static typename Ptr<AssetType>::Value Create(AssetInitializer<3> init) { return std::make_unique<AssetType>(init._name0, init._name1, init._name2); }
            template<typename AssetType> static typename Ptr<AssetType>::Value Create(AssetInitializer<4> init) { return std::make_unique<AssetType>(init._name0, init._name1, init._name2, init._name3); }
            template<typename AssetType> static typename Ptr<AssetType>::Value Create(AssetInitializer<5> init) { return std::make_unique<AssetType>(init._name0, init._name1, init._name2, init._name3, init._name4); }
            template<typename AssetType> static typename Ptr<AssetType>::Value Create(AssetInitializer<6> init) { return std::make_unique<AssetType>(init._name0, init._name1, init._name2, init._name3, init._name4, init._name5); }
        };

        template<> struct ConstructAsset<1>
        { 
            template<typename AssetType, int InitCount> static typename Ptr<AssetType>::Value Create(AssetInitializer<InitCount> init) 
            {
                auto& compilers = CompileAndAsyncManager::GetInstance().GetIntermediateCompilers();
                auto& store = CompileAndAsyncManager::GetInstance().GetIntermediateStore();
                auto marker = compilers.PrepareResource(GetCompileProcessType<AssetType>(), (const char**)&init, InitCount, store);
                return std::make_unique<AssetType>(std::move(marker));
            }
        };

        template <int InitCount> uint64 BuildHash(AssetInitializer<InitCount> init);

        template<bool DoCheckDependancy, bool DoBackgroundCompile, typename AssetType, int InitCount>
            AssetType& GetAsset(AssetInitializer<InitCount> init)
            {
                    //
                    //  This is the main bit of functionality in this file. Here we define
                    //  the core behaviour when querying for an asset:
                    //      * build a hash from the string inputs
                    //      * if the asset already exists:
                    //          * sometimes we check the invalidation state, and return a rebuilt asset
                    //          * otherwise return the existing asset
                    //      * otherwise we build a new asset
                    //
                auto hash = BuildHash(init);
				auto& assetSet = GetAssetSet<AssetType>();

				#if defined(ASSETS_STORE_DIVERGENT)
						// divergent assets will always shadow normal assets
						// we also don't do a dependency check for these assets
					auto di = LowerBound(assetSet._divergentAssets, hash);
					if (di != assetSet._divergentAssets.end() && di->first == hash) {
						return *di->second->GetAsset();
					}
				#endif

                auto& assets = assetSet._assets;
				auto i = LowerBound(assets, hash);
				if (i != assets.end() && i->first == hash) {
                    if (CheckDependancy<DoCheckDependancy>::NeedsRefresh(i->second.get())) {
                            // note --  old resource will stay in memory until the new one has been constructed
                            //          If we get an exception during construct, we'll be left with a null ptr
                            //          in this asset set
                        auto oldResource = std::move(i->second);
                        i->second = ConstructAsset<DoBackgroundCompile>::Create<AssetType>(init);
                    }
                    return *i->second;
                }

                auto newAsset = ConstructAsset<DoBackgroundCompile>::Create<AssetType>(init);
                #if defined(ASSETS_STORE_NAMES)
                        // This is extra functionality designed for debugging and profiling
                        // attach a name to this hash value, so we can query the contents
                        // of an asset set and get meaningful values
                        //  (only insert after we've completed creation; because creation can throw an exception)
                    InsertAssetName(GetAssetSet<AssetType>()._assetNames, hash, (const char**)&init, InitCount);
                #endif

                    // we have to search again for the insertion point
                    //  it's possible that while constructing the asset, we may have called GetAsset<>
                    //  to create another asset. This can invalidate our iterator "i". If we had some way
                    //  to test to make sure that the assetSet definitely hasn't changed, we coudl skip this.
                    //  But just doing a size check wouldn't be 100% -- because there might be an add, then a remove
                    //      (well, remove isn't possible currently. But it may happen at some point.
                    //  For the future, we should consider threading problems, also. We will probably need
                    //  a lock on the assetset -- and it may be best to release this lock while we're calling
                    //  the constructor
				i = LowerBound(assets, hash);
				return *assets.insert(i, std::make_pair(hash, std::move(newAsset)))->second;
            }

		template <typename AssetType, int InitCount>
			std::shared_ptr<typename AssetTraits<AssetType>::DivAsset>& GetDivergentAsset(AssetInitializer<InitCount> init)
			{
				#if !defined(ASSETS_STORE_DIVERGENT)
					throw ::Exceptions::BasicLabel("Could not get divergent asset, because ASSETS_STORE_DIVERGENT is not defined");
				#else

					auto hash = BuildHash(init);
					auto& assetSet = GetAssetSet<AssetType>();
					auto di = LowerBound(assetSet._divergentAssets, hash);
					if (di != assetSet._divergentAssets.end() && di->first == hash) {
						return di->second;
					}

					auto newDivAsset = std::make_shared<typename AssetTraits<AssetType>::DivAsset>(
						GetAsset<true, false, AssetType>(init));

						// Do we have to search for an insertion point here again?
						// is it possible that constructing an asset could create a new divergent
						// asset of the same type? It seems unlikely
					assert(di == LowerBound(assetSet._divergentAssets, hash));
					return assetSet._divergentAssets.insert(di, std::make_pair(hash, std::move(newDivAsset)))->second;

				#endif
			}

        template <typename AssetType>
            AssetSet<AssetType>::~AssetSet() {}

        template <typename AssetType>
            void AssetSet<AssetType>::Clear() 
            {
                _assets.clear();
				#if defined(ASSETS_STORE_DIVERGENT)
					_divergentAssets.clear();
				#endif
                #if defined(ASSETS_STORE_NAMES)
                    _assetNames.clear();
                #endif
            }

        template <typename AssetType>
            void AssetSet<AssetType>::LogReport() 
            {
                LogHeader(unsigned(_assets.size()), typeid(AssetType).name());
                #if defined(ASSETS_STORE_NAMES)
                    auto i = _assets.cbegin();
                    auto ni = _assetNames.cbegin();
                    unsigned index = 0;
                    for (;i != _assets.cend(); ++i, ++index) {
                        while (ni->first < i->first && ni != _assetNames.cend()) { ++ni; }
                        if (ni != _assetNames.cend() && ni->first == i->first) {
                            LogAssetName(index, ni->second.c_str());
                        } else {
                            char buffer[256];
                            _snprintf_s(buffer, _TRUNCATE, "Unnamed asset with hash (0x%08x%08x)", 
                                uint32(i->first>>32), uint32(i->first));
                            LogAssetName(index, buffer);
                        }
                    }
                #else
                    auto i = _assets.cbegin();
                    unsigned index = 0;
                    for (;i != _assets.cend(); ++i, ++index) {
                        char buffer[256];
                        _snprintf_s(buffer, _TRUNCATE, "Unnamed asset with hash (0x%08x%08x)", 
                            uint32(i->first>>32), uint32(i->first));
                        LogAssetName(index, buffer);
                    }
                #endif
            }

        template <typename AssetType>
            std::vector<std::pair<uint64, std::unique_ptr<AssetType>>> AssetSet<AssetType>::_assets;
        #if defined(ASSETS_STORE_NAMES)
            template <typename AssetType>
                std::vector<std::pair<uint64, std::string>> AssetSet<AssetType>::_assetNames;
        #endif
    }

    template<typename Resource> Resource& GetAsset(const char name[]) { Internal::AssetInitializer<1> init = {name}; return Internal::GetAsset<false, false, Resource>(init); }
    template<typename Resource> Resource& GetAsset(const char name0[], const char name1[]) { Internal::AssetInitializer<2> init = {name0, name1}; return Internal::GetAsset<false, false, Resource>(init); }
    template<typename Resource> Resource& GetAsset(const char name0[], const char name1[], const char name2[]) { Internal::AssetInitializer<3> init = {name0, name1, name2}; return Internal::GetAsset<false, false, Resource>(init); }
    template<typename Resource> Resource& GetAsset(const char name0[], const char name1[], const char name2[], const char name3[]) { Internal::AssetInitializer<4> init = {name0, name1, name2, name3}; return Internal::GetAsset<false, false, Resource>(init); }
    template<typename Resource> Resource& GetAsset(const char name0[], const char name1[], const char name2[], const char name3[], const char name4[], const char name5[]) { Internal::AssetInitializer<6> init = {name0, name1, name2, name3, name4, name5}; return Internal::GetAsset<false, false, Resource>(init); }

    template<typename Resource> Resource& GetAssetDep(const char name[]) { Internal::AssetInitializer<1> init = {name}; return Internal::GetAsset<true, false, Resource>(init); }
    template<typename Resource> Resource& GetAssetDep(const char name0[], const char name1[]) { Internal::AssetInitializer<2> init = {name0, name1}; return Internal::GetAsset<true, false, Resource>(init); }
    template<typename Resource> Resource& GetAssetDep(const char name0[], const char name1[], const char name2[]) { Internal::AssetInitializer<3> init = {name0, name1, name2}; return Internal::GetAsset<true, false, Resource>(init); }
    template<typename Resource> Resource& GetAssetDep(const char name0[], const char name1[], const char name2[], const char name3[]) { Internal::AssetInitializer<4> init = {name0, name1, name2, name3}; return Internal::GetAsset<true, false, Resource>(init); }
    template<typename Resource> Resource& GetAssetDep(const char name0[], const char name1[], const char name2[], const char name3[], const char name4[], const char name5[]) { Internal::AssetInitializer<6> init = {name0, name1, name2, name3, name4, name5}; return Internal::GetAsset<true, false, Resource>(init); }

    template<typename Resource> Resource& GetAssetComp(const char name[]) { Internal::AssetInitializer<1> init = {name}; return Internal::GetAsset<true, true, Resource>(init); }
    template<typename Resource> Resource& GetAssetComp(const char name0[], const char name1[]) { Internal::AssetInitializer<2> init = {name0, name1}; return Internal::GetAsset<true, true, Resource>(init); }

        ////////////////////////////////////////////////////////////////////////

    template<typename Type> uint64 GetCompileProcessType() { return Type::CompileProcessType; }

        ////////////////////////////////////////////////////////////////////////

    /// <summary>Exceptions related to rendering</summary>
    namespace Exceptions
    {
        /// <summary>A resource can't be loaded</summary>
        /// This exception means a resource failed during loading, and can
        /// never be loaded. It might mean that the resource is corrupted on
        /// disk, or maybe using an unsupported file format (or bad version).
        /// The most common cause is due to a compile error in a shader. 
        /// If we attempt to use a shader with a compile error, it will throw
        /// a InvalidResource exception.
        class InvalidResource : public ::Exceptions::BasicLabel
        {
        public: 
            InvalidResource(const char resourceId[], const char what[]);
            const char* ResourceId() const { return _resourceId; }

        private:
            char _resourceId[512];
        };

        /// <summary>Resource is still being loaded</summary>
        /// This is common exception. It occurs if we attempt to use a resource that
        /// is still being prepared. Usually this means that the resource is being
        /// loaded from disk, or compiled in a background thread.
        /// For example, shader resources can take some time to compile. If we attempt
        /// to use the shader while it's still compiling, we'll get a PendingResource
        /// exception.
        class PendingResource : public ::Exceptions::BasicLabel
        {
        public: 
            PendingResource(const char resourceId[], const char what[]);
            const char* ResourceId() const { return _resourceId; }

        private:
            char _resourceId[512];
        };

        class FormatError : public ::Exceptions::BasicLabel
        {
        public:
            FormatError(const char format[], ...) never_throws;
        };

        class UnsupportedFormat : public ::Exceptions::BasicLabel
        {
        public:
            UnsupportedFormat(const char format[], ...) never_throws;
        };
    }

        ////////////////////////////////////////////////////////////////////////

    /// <summary>Handles resource invalidation events</summary>
    /// Utility class used for detecting resource invalidation events (for example, if
    /// a shader source file changes on disk). 
    /// Resources that can receive invalidation events should use this class to declare
    /// that dependency. 
    /// <example>
    ///     For example:
    ///         <code>\code
    ///             class SomeResource
    ///             {
    ///             public:
    ///                 SomeResource(const char constructorString[]);
    ///             private:
    ///                 std::shared_ptr<DependencyValidation> _validator;
    ///             };
    /// 
    ///             SomeResource::SomeResource(const char constructorString[])
    ///             {
    ///                    // Load some data from a file named "constructorString
    ///
    ///                 auto validator = std::make_shared<DependencyValidation>();
    ///                 RegisterFileDependency(validator, constructorString);
    ///
    ///                 _validator = std::move(validator);
    ///             }
    ///         \endcode</code>
    /// </example>
    /// <seealso cref="RegisterFileDependency" />
    /// <seealso cref="RegisterResourceDependency" />
    class DependencyValidation : public Utility::OnChangeCallback
    {
    public:
        virtual void    OnChange();
        unsigned        GetValidationIndex() const        { return _validationIndex; }
        DependencyValidation() : _validationIndex(0)  {}
    private:
        unsigned _validationIndex;
    };

    /// <summary>Registers a dependency on a file on disk</summary>
    /// Registers a dependency on a file. The system will monitor that file for changes.
    /// If the file changes on disk, the system will call validationIndex->OnChange();
    /// Note the system only takes a "weak reference" to validationIndex. This means that
    /// validationIndex can be destroyed by other objects. When that happens, the system will
    /// continue to monitor the file, but OnChange() wont be called (because the object has 
    /// already been destroyed).
    /// <param name="validationIndex">Callback to receive invalidation events</param>
    /// <param name="filename">Normally formatted filename</param>
    void RegisterFileDependency(const std::shared_ptr<DependencyValidation>& validationIndex, const char filename[]);

    /// <summary>Registers a dependency on another resource</summary>
    /// Sometimes resources are dependent on other resources. This function helps registers a 
    /// dependency between resources.
    /// If <paramref name="dependency"/> ever gets a OnChange() message, then <paramref name="dependentResource"/> 
    /// will also receive the OnChange() message.
    void RegisterAssetDependency(const std::shared_ptr<DependencyValidation>& dependentResource, const DependencyValidation* dependency);

}


