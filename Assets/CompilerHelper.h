// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "IntermediateAssets.h"
#include <memory>

namespace Assets
{
    class PendingCompileMarker;

    /// <summary>Utilities helpful when implementing asset compilers</summary>
    class CompilerHelper
    {
    public:
        class CompileResult
        {
        public:
            std::vector<::Assets::DependentFileState> _dependencies;
            ::Assets::rstring _baseDir;
        };
    };
}

