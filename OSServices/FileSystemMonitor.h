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
        virtual ~OnChangeCallback() = default;
    };

    class PollingThread;

    class RawFSMonitor
    {
    public:
        /// <summary>Monitor a file for changes</summary>
        /// Attaches a callback function to a file on disk. The callback will then be
        /// executed whenever the file changes.
        /// This is typically used to reload source assets after they receive 
        /// changes form an external source.
        void    Attach(StringSection<utf8> filename, std::shared_ptr<OnChangeCallback> callback);
        void    Attach(StringSection<utf16> filename, std::shared_ptr<OnChangeCallback> callback);

        /// <summary>Executed all on-change callbacks associated with file</summary>
        /// This will create a fake change event for a file, and execute any attached
        /// callbacks.
        void    FakeFileChange(StringSection<utf8> filename);
        void    FakeFileChange(StringSection<utf16> filename);

        RawFSMonitor(const std::shared_ptr<PollingThread>&);
        ~RawFSMonitor();

    private:
        class Pimpl;
        std::shared_ptr<Pimpl> _pimpl;
    };
}

