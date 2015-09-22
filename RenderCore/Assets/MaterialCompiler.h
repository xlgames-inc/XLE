// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/IntermediateAssets.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Core/Types.h"

namespace Assets { class DependencyValidation; class PendingCompileMarker; }

namespace RenderCore { namespace Assets
{
    static const uint64 ChunkType_ResolvedMat = ConstHash64<'ResM', 'at'>::Value;

    class MaterialScaffoldCompiler : public ::Assets::IntermediateAssets::IAssetCompiler
    {
    public:
        std::shared_ptr<::Assets::PendingCompileMarker> PrepareAsset(
            uint64 typeCode, 
            const ::Assets::ResChar* initializers[], unsigned initializerCount,
            const ::Assets::IntermediateAssets::Store& destinationStore);

        void StallOnPendingOperations(bool cancelAll);

        MaterialScaffoldCompiler();
        ~MaterialScaffoldCompiler();
    };

}}




