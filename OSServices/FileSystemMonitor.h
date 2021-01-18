// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/StringUtils.h" // for StringSection
#include <memory>

namespace OSServices
{
    class OnChangeCallback
    {
    public:
        virtual void    OnChange() = 0;
        virtual ~OnChangeCallback();
    };

    /// <summary>Monitor a file for changes</summary>
    /// Attaches a callback function to a file on disk. The callback will then be
    /// executed whenever the file changes.
    /// This is typically used to reload source assets after they receive 
    /// changes form an external source.
    void    AttachFileSystemMonitor(StringSection<utf8> directoryName, StringSection<utf8> filename, std::shared_ptr<OnChangeCallback> callback);
	void    AttachFileSystemMonitor(StringSection<utf16> directoryName, StringSection<utf16> filename, std::shared_ptr<OnChangeCallback> callback);

    /// <summary>Executed all on-change callbacks associated with file</summary>
    /// This will create a fake change event for a file, and execute any attached
    /// callbacks.
    void    FakeFileChange(StringSection<utf8> directoryName, StringSection<utf8> filename);
	void    FakeFileChange(StringSection<utf16> directoryName, StringSection<utf16> filename);

    /// <summary>Shut down all file system monitoring</summary>
    /// Intended to be called on application shutdown, this frees all resources
    /// used by file system monitoring.
    void    TerminateFileSystemMonitoring();
}

