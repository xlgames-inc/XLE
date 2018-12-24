// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Utility/StringUtils.h"
#include <memory>

namespace RenderCore { namespace Assets
{
    class MaterialScaffoldCompiler : public ::Assets::IAssetCompiler, public std::enable_shared_from_this<MaterialScaffoldCompiler>
    {
    public:
        std::shared_ptr<::Assets::IArtifactCompileMarker> Prepare(
            uint64_t typeCode, 
            const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount);
		std::vector<uint64_t> GetTypesForAsset(const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount);
		
		void StallOnPendingOperations(bool cancelAll);

        MaterialScaffoldCompiler();
        ~MaterialScaffoldCompiler();
    };

}}

