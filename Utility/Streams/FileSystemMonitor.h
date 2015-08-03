// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../StringUtils.h" // for StringSection
#include <memory>

namespace Utility
{
    class OnChangeCallback
    {
    public:
        virtual void    OnChange() = 0;
    };

    void    AttachFileSystemMonitor(StringSection<char> directoryName, StringSection<char> filename, std::shared_ptr<OnChangeCallback> callback);
    void    FakeFileChange(StringSection<char> directoryName, StringSection<char> filename);
    void    TerminateFileSystemMonitoring();
}

