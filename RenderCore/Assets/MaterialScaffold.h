// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/IntermediateResources.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Core/Types.h"

namespace Assets { class DependencyValidation; class PendingCompileMarker; }

namespace RenderCore { namespace Assets
{

    class MaterialImmutableData;
    class ResolvedMaterial;
    typedef uint64 MaterialGuid;

    class MaterialScaffold
    {
    public:
        const MaterialImmutableData&    ImmutableData() const       { return *_data; };
        const ResolvedMaterial*         GetMaterial(MaterialGuid guid) const;
        const char*                     GetMaterialName(MaterialGuid guid) const;

        const ::Assets::DependencyValidation& GetDependencyValidation() const { return *_validationCallback; }

        static const auto CompileProcessType = ConstHash64<'ResM', 'at'>::Value;

        MaterialScaffold(std::shared_ptr<::Assets::PendingCompileMarker>&& marker);
        ~MaterialScaffold();
    protected:
        std::unique_ptr<uint8[]> _rawMemoryBlock;
        const MaterialImmutableData* _data;
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

    static const uint64 ChunkType_ResolvedMat = ConstHash64<'ResM', 'at'>::Value;

    class MaterialScaffoldCompiler : public ::Assets::IntermediateResources::IResourceCompiler
    {
    public:
        std::shared_ptr<::Assets::PendingCompileMarker> PrepareResource(
            uint64 typeCode, 
            const ::Assets::ResChar* initializers[], unsigned initializerCount,
            const ::Assets::IntermediateResources::Store& destinationStore);

        MaterialScaffoldCompiler();
        ~MaterialScaffoldCompiler();
    };

}}




