// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace Utility
{
    class OnChangeCallback
    {
    public:
        virtual void    OnChange() = 0;
    };

    void    AttachFileSystemMonitor(const char directoryName[], const char filename[], const std::shared_ptr<OnChangeCallback>& callback);
    void    FakeFileChange(const char directoryName[], const char filename[]);
    void    TerminateFileSystemMonitoring();
}

