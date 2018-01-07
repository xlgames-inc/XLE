// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/IAssetCompiler.h"
#include "../../Utility/StringUtils.h"
#include <memory>

namespace RenderCore { namespace Assets
{
    class MaterialScaffoldCompiler : public ::Assets::IAssetCompiler, public std::enable_shared_from_this<MaterialScaffoldCompiler>
    {
    public:
        std::shared_ptr<::Assets::ICompileMarker> PrepareAsset(
            uint64_t typeCode, 
            const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount,
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

