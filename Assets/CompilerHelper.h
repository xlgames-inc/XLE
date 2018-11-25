// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include <memory>

namespace Assets
{
    class ArtifactFuture;
	class DependentFileState;

    /// <summary>Utilities helpful when implementing asset compilers</summary>
    class CompilerHelper
    {
    public:
        class CompileResult
        {
        public:
            std::vector<DependentFileState> _dependencies;
            rstring _baseDir;
        };
    };
}

