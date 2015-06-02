// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace ConsoleRig
{
    class IStep;
    class IProgress
    {
    public:
        virtual std::shared_ptr<IStep> BeginStep(const char name[], unsigned progressMax) = 0;
        virtual ~IProgress();
    };

    class IStep
    {
    public:
        virtual void SetProgress(unsigned progress) = 0;
        virtual void Advance() = 0;
        virtual bool IsCancelled() const = 0;
        virtual ~IStep();
    };
}

