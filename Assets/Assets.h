// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "CompileAndAsyncManager.h"
#include "AssetServices.h"
#include "AssetSetManager.h"
#include "../Utility/Streams/FileSystemMonitor.h"       // (for OnChangeCallback base class)
#include "../Utility/IteratorUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Core/Types.h"
#include <vector>
#include <utility>
#include <string>
#include <sstream>

#if defined(_DEBUG)
    #define ASSETS_STORE_NAMES
#endif
#define ASSETS_STORE_DIVERGENT		// divergent assets are intended for tools (not in-game). But we can't selectively disable this feature

#if defined(ASSETS_STORE_DIVERGENT)
	#include "DivergentAsset.h"
#endif

namespace Assets
{
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
            void LogReport() const;
            uint64          GetTypeCode() const;
            const char*     GetTypeName() const;
            unsigned        GetDivergentCount() const;
            uint64          GetDivergentId(unsigned index) const;
            bool            DivergentHasChanges(unsigned index) const;
            std::string     GetAssetName(uint64 id) const;

            static std::vector<std::pair<uint64, std::unique_ptr<AssetType>>> _assets;
			
			#if defined(ASSETS_STORE_DIVERGENT)
				using DivAsset = typename AssetTraits<AssetType>::DivAsset;
				static std::vector<std::pair<uint64, std::shared_ptr<DivAsset>>> _divergentAssets;
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
            uint64 hash, const std::string& name);
        void InsertAssetNameNoCollision(   
            std::vector<std::pair<uint64, std::string>>& assetNames, 
            uint64 hash, const std::string& name);

        template<typename AssetType>
            AssetSet<AssetType>& GetAssetSet() 
        {
            static AssetSet<AssetType>* set = nullptr;
            if (!set)
                set = Services::GetAssetSets().GetSetForType<AssetType>();
            
            assert(Services::GetAssetSets().IsBoundThread());  // currently not thread safe; we have to check the thread ids
            #if defined(ASSETS_STORE_NAMES)
                    // These should agree. If there's a mismatch, there may be a threading problem
                assert(set->_assets.size() == set->_assetNames.size());
            #endif
            return *set;
        }

        template<typename AssetType> using Ptr = std::unique_ptr<AssetType>;

        template <int DoCheckDependancy> struct CheckDependancy { template<typename Resource> static bool NeedsRefresh(const Resource* resource); };
        template<> struct CheckDependancy<1>   { template <typename Resource> static bool NeedsRefresh(const Resource* resource)   { return !resource || (resource->GetDependencyValidation()->GetValidationIndex()!=0); } };
        template<> struct CheckDependancy<0>   { template <typename Resource> static bool NeedsRefresh(const Resource* resource)   { return !resource; } };

        template <int BoBackgroundCompile> struct ConstructAsset {};

        template<> struct ConstructAsset<0>
        { 
            template<typename AssetType, typename... Params> 
				static typename Ptr<AssetType> Create(Params... initialisers)
			{
				return std::make_unique<AssetType>(std::forward<Params>(initialisers)...);
			}
        };

        template<> struct ConstructAsset<1>
        { 
            template<typename AssetType, typename... Params> 
				static typename Ptr<AssetType> Create(Params... initialisers)
            {
                auto& compilers = Services::GetAsyncMan().GetIntermediateCompilers();
                auto& store = Services::GetAsyncMan().GetIntermediateStore();
				const char* inits[] = { ((const char*)initialisers)... };
				auto marker = compilers.PrepareResource(GetCompileProcessType<AssetType>(), inits, dimof(inits), store);
                return std::make_unique<AssetType>(std::move(marker));
            }
        };

		template <typename... Params> uint64 BuildHash(Params... initialisers);
		template <typename... Params> std::basic_string<ResChar> AsString(Params... initialisers);
        std::basic_string<ResChar> AsString();

		template<bool DoCheckDependancy, bool DoBackgroundCompile, typename AssetType, typename... Params>
			const AssetType& GetAsset(Params... initialisers)
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
				auto hash = BuildHash(initialisers...);
				auto& assetSet = GetAssetSet<AssetType>();
				(void)assetSet;	// (is this a compiler problem? It thinks this is unreferenced?)

				#if defined(ASSETS_STORE_DIVERGENT)
						// divergent assets will always shadow normal assets
						// we also don't do a dependency check for these assets
					auto di = LowerBound(assetSet._divergentAssets, hash);
					if (di != assetSet._divergentAssets.end() && di->first == hash) {
						return di->second->GetAsset();
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
                        i->second = ConstructAsset<DoBackgroundCompile>::Create<AssetType>(std::forward<Params>(initialisers)...);
                    }
                    return *i->second;
                }

                #if defined(ASSETS_STORE_NAMES)
                    auto name = AsString(initialisers...);  // (have to do this before constructor (incase constructor does std::move operations)
                #endif

                auto newAsset = ConstructAsset<DoBackgroundCompile>::Create<AssetType>(std::forward<Params>(initialisers)...);
                #if defined(ASSETS_STORE_NAMES)
                        // This is extra functionality designed for debugging and profiling
                        // attach a name to this hash value, so we can query the contents
                        // of an asset set and get meaningful values
                        //  (only insert after we've completed creation; because creation can throw an exception)
					InsertAssetName(assetSet._assetNames, hash, name);
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

		template <typename AssetType, typename... Params>
			std::shared_ptr<typename AssetTraits<AssetType>::DivAsset>& GetDivergentAsset(Params... initialisers)
			{
				#if !defined(ASSETS_STORE_DIVERGENT)
					throw ::Exceptions::BasicLabel("Could not get divergent asset, because ASSETS_STORE_DIVERGENT is not defined");
				#else

					auto hash = BuildHash(initialisers...);
					auto& assetSet = GetAssetSet<AssetType>();
                    (void)assetSet;
					auto di = LowerBound(assetSet._divergentAssets, hash);
					if (di != assetSet._divergentAssets.end() && di->first == hash) {
						return di->second;
					}

                    typename AssetTraits<AssetType>::DivAsset::AssetIdentifier identifier;
                    identifier._descriptiveName = BuildDescriptiveName<AssetType>(initialisers...);
                    identifier._targetFilename = BuildTargetFilename<AssetType>(initialisers...);

                    std::weak_ptr<UndoQueue> undoQueue;
                    std::shared_ptr<typename AssetTraits<AssetType>::DivAsset> newDivAsset;

                    bool constructNewAsset = false;
                    TRY {
                        newDivAsset = std::make_shared<typename AssetTraits<AssetType>::DivAsset>(
                            GetAsset<true, false, AssetType>(std::forward<Params>(initialisers)...), 
                            hash, assetSet.GetTypeCode(), 
                            identifier, undoQueue);
                    } CATCH (const Assets::Exceptions::InvalidAsset&) {
                        constructNewAsset = true;
                    } CATCH_END

                    if (constructNewAsset) {

                        auto hash = BuildHash(initialisers...);
                        #if defined(ASSETS_STORE_NAMES)
                            auto name = AsString(initialisers...);
                        #endif

                            //  If we get an invalid asset, we have to create a new one
                            //  and assign it in place.
                        auto newBlankAsset = AssetType::CreateNew(initialisers...);

                        auto& assetSet = GetAssetSet<AssetType>();
                        auto& assets = assetSet._assets;

                        #if defined(ASSETS_STORE_NAMES)
					        InsertAssetNameNoCollision(assetSet._assetNames, hash, name);
                        #endif

                        auto i = LowerBound(assets, hash);
                        newDivAsset = std::make_shared<typename AssetTraits<AssetType>::DivAsset>(
                            *assets.insert(i, std::make_pair(hash, std::move(newBlankAsset)))->second, 
                            hash, assetSet.GetTypeCode(), 
                            identifier, undoQueue);
                    }

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
            void AssetSet<AssetType>::LogReport() const 
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
            uint64          AssetSet<AssetType>::GetTypeCode() const
            {
                return Hash64(typeid(AssetType).name());
            }

        template <typename AssetType>
            const char*     AssetSet<AssetType>::GetTypeName() const
            {
                return typeid(AssetType).name();
            }

        template <typename AssetType>
            unsigned        AssetSet<AssetType>::GetDivergentCount() const
            {
                return unsigned(_divergentAssets.size());
            }

        template <typename AssetType>
            uint64          AssetSet<AssetType>::GetDivergentId(unsigned index) const
            {
                if (index < _divergentAssets.size()) return _divergentAssets[index].first;
                return ~0x0ull;
            }

        template <typename AssetType>
            bool            AssetSet<AssetType>::DivergentHasChanges(unsigned index) const
            {
                if (index < _divergentAssets.size()) return _divergentAssets[index].second->HasChanges();
                return false;
            }

        template <typename AssetType>
            std::string     AssetSet<AssetType>::GetAssetName(uint64 id) const
            {
                #if defined(ASSETS_STORE_NAMES)
                    auto i = LowerBound(_assetNames, id);
                    if (i != _assetNames.end() && i->first == id)
                        return i->second;
                    return std::string();
                #else
                    return std::string();
                #endif
            }

        template <typename AssetType>
            std::vector<std::pair<uint64, std::unique_ptr<AssetType>>> AssetSet<AssetType>::_assets;
        #if defined(ASSETS_STORE_NAMES)
            template <typename AssetType>
                std::vector<std::pair<uint64, std::string>> AssetSet<AssetType>::_assetNames;
        #endif

        #if defined(ASSETS_STORE_DIVERGENT)
            template <typename AssetType>
                std::vector<std::pair<uint64, std::shared_ptr<typename AssetSet<AssetType>::DivAsset>>> AssetSet<AssetType>::_divergentAssets;
        #endif
    }

    template<typename AssetType, typename... Params> const AssetType& GetAsset(Params... initialisers)		    { return Internal::GetAsset<false, false, AssetType>(std::forward<Params>(initialisers)...); }
    template<typename AssetType, typename... Params> const AssetType& GetAssetDep(Params... initialisers)	    { return Internal::GetAsset<true, false, AssetType>(std::forward<Params>(initialisers)...); }
    template<typename AssetType, typename... Params> const AssetType& GetAssetComp(Params... initialisers)	    { return Internal::GetAsset<true, true, AssetType>(std::forward<Params>(initialisers)...); }

    template<typename AssetType, typename... Params> 
        std::shared_ptr<typename Internal::AssetTraits<AssetType>::DivAsset>& GetDivergentAsset(Params... initialisers)	
            { return Internal::GetDivergentAsset<AssetType>(std::forward<Params>(initialisers)...); }

        ////////////////////////////////////////////////////////////////////////

    template<typename Type> uint64 GetCompileProcessType() { return Type::CompileProcessType; }

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
    class DependencyValidation : public Utility::OnChangeCallback, public std::enable_shared_from_this<DependencyValidation>
    {
    public:
        virtual void    OnChange();
        unsigned        GetValidationIndex() const        { return _validationIndex; }

        void    RegisterDependency(const std::shared_ptr<Utility::OnChangeCallback>& dependency);

        DependencyValidation() : _validationIndex(0)  {}
        DependencyValidation(DependencyValidation&&) never_throws;
        DependencyValidation& operator=(DependencyValidation&&) never_throws;
        ~DependencyValidation();

        DependencyValidation(const DependencyValidation&) = delete;
        DependencyValidation& operator=(const DependencyValidation&) = delete;
    private:
        unsigned _validationIndex;

            // store a fixed number of dependencies (with room to grow)
            // this is just to avoid extra allocation where possible
        std::shared_ptr<Utility::OnChangeCallback> _dependencies[4];
        std::vector<std::shared_ptr<Utility::OnChangeCallback>> _dependenciesOverflow;
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
    void RegisterFileDependency(
        const std::shared_ptr<Utility::OnChangeCallback>& validationIndex, 
        const char filename[]);

    /// <summary>Registers a dependency on another resource</summary>
    /// Sometimes resources are dependent on other resources. This function helps registers a 
    /// dependency between resources.
    /// If <paramref name="dependency"/> ever gets a OnChange() message, then <paramref name="dependentResource"/> 
    /// will also receive the OnChange() message.
    void RegisterAssetDependency(
        const std::shared_ptr<DependencyValidation>& dependentResource, 
        const std::shared_ptr<Utility::OnChangeCallback>& dependency);

}

namespace Assets 
{
    namespace Internal
    {
        template <typename Object>
			inline void StreamCommaSeparated(std::basic_stringstream<ResChar>& result, const Object& obj)
		{
			result << ", " << obj;
		}

		template <typename P0, typename... Params>
			std::basic_string<ResChar> AsString(P0 p0, Params... initialisers)
		{
			std::basic_stringstream<ResChar> result;
            result << p0;
			int dummy[] = { 0, (StreamCommaSeparated(result, initialisers), 0)... };
			(void)dummy;
			return result.str();
		}

		template <typename... Params>
			uint64 BuildHash(Params... initialisers)
        { 
                //  Note Hash64 is a relatively expensive hash function
                //      ... we might get away with using a simpler/quicker hash function
                //  Note that if we move over to variadic template initialisers, it
                //  might not be as easy to build the hash value (because they would
                //  allow some initialisers to be different types -- not just strings).
                //  If we want to support any type as initialisers, we need to either
                //  define some rules for hashing arbitrary objects, or think of a better way
                //  to build the hash.
            uint64 result = DefaultSeed64;
			int dummy[] = { 0, (result = Hash64(initialisers, result), 0)... };
			(void)dummy;
            return result;
        }
    }
}


