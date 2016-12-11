// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/IntermediateAssets.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Core/Types.h"
#include <memory>

namespace Assets { class DependencyValidation; class PendingCompileMarker; }

namespace RenderCore { namespace Assets
{
    static const uint64 ChunkType_ResolvedMat = ConstHash64<'ResM', 'at'>::Value;

    class MaterialScaffoldCompiler : public ::Assets::IntermediateAssets::IAssetCompiler, public std::enable_shared_from_this<MaterialScaffoldCompiler>
    {
    public:
        std::shared_ptr<::Assets::ICompileMarker> PrepareAsset(
            uint64 typeCode, 
            const ::Assets::ResChar* initializers[], unsigned initializerCount,
            const ::Assets::IntermediateAssets::Store& destinationStore);

        void StallOnPendingOperations(bool cancelAll);

        MaterialScaffoldCompiler();
        ~MaterialScaffoldCompiler();

    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class MatCompilerMarker;
    };

}}

