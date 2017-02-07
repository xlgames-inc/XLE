// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "AssetsInternal.h"
#include "../Utility/Streams/FileSystemMonitor.h"       // (for OnChangeCallback base class)

namespace Assets
{

    template<typename AssetType, typename... Params> const AssetType& GetAsset(Params... initialisers)		    { return Internal::GetAsset<false, true, AssetType>(std::forward<Params>(initialisers)...); }
    template<typename AssetType, typename... Params> const AssetType& GetAssetDep(Params... initialisers)	    { return Internal::GetAsset<true, true, AssetType>(std::forward<Params>(initialisers)...); }
    template<typename AssetType, typename... Params> const AssetType& GetAssetComp(Params... initialisers)	    { return Internal::GetAsset<true, true, AssetType>(std::forward<Params>(initialisers)...); }

    template<typename AssetType, typename... Params> 
        std::shared_ptr<typename Internal::AssetTraits<AssetType>::DivAsset>& GetDivergentAsset(Params... initialisers)	
            { return Internal::GetDivergentAsset<AssetType, false>(std::forward<Params>(initialisers)...); }

    template<typename AssetType, typename... Params> 
        std::shared_ptr<typename Internal::AssetTraits<AssetType>::DivAsset>& GetDivergentAssetComp(Params... initialisers)	
            { return Internal::GetDivergentAsset<AssetType, true>(std::forward<Params>(initialisers)...); }

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

        void    RegisterDependency(const std::shared_ptr<DependencyValidation>& dependency);

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
        StringSection<ResChar> filename);

    /// <summary>Registers a dependency on another resource</summary>
    /// Sometimes resources are dependent on other resources. This function helps registers a 
    /// dependency between resources.
    /// If <paramref name="dependency"/> ever gets a OnChange() message, then <paramref name="dependentResource"/> 
    /// will also receive the OnChange() message.
    void RegisterAssetDependency(
        const std::shared_ptr<DependencyValidation>& dependentResource, 
        const std::shared_ptr<DependencyValidation>& dependency);

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


