// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../OSServices/FileSystemMonitor.h"       // (for OnChangeCallback base class)

namespace Assets
{
    typedef char ResChar;

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
    class DependencyValidation : public OSServices::OnChangeCallback, public std::enable_shared_from_this<DependencyValidation>
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

        #if defined(_DEBUG)
            std::vector<std::string> _monitoredFiles;
        #endif
    private:
        unsigned _validationIndex;

            // store a fixed number of dependencies (with room to grow)
            // this is just to avoid extra allocation where possible
        std::shared_ptr<OSServices::OnChangeCallback> _dependencies[4];
        std::vector<std::shared_ptr<OSServices::OnChangeCallback>> _dependenciesOverflow;
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
        const std::shared_ptr<DependencyValidation>& validationIndex, 
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

