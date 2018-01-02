// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Techniques/RenderStateResolver.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Utility/MemoryUtils.h"
#include <memory>

namespace Assets 
{ 
    class DependencyValidation;
	class ChunkFileContainer;
	class DeferredConstruction;
}
namespace Utility { class Data; }

namespace RenderCore { namespace Techniques { class Material; } }

namespace RenderCore { namespace Assets
{
    using MaterialGuid = uint64_t;

    class MaterialImmutableData;

	static constexpr uint64 ChunkType_ResolvedMat = ConstHash64<'ResM', 'at'>::Value;
	static constexpr unsigned ResolvedMat_ExpectedVersion = 1;

    /// <summary>An asset containing compiled material settings</summary>
    /// This is the equivalent of other scaffold objects (like ModelScaffold
    /// and AnimationSetScaffold). It contains the processed and ready-to-use
    /// material information.
    class MaterialScaffold
    {
    public:
        const MaterialImmutableData&    ImmutableData() const;
        const Techniques::Material*		GetMaterial(MaterialGuid guid) const;
        StringSection<>					GetMaterialName(MaterialGuid guid) const;

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

        static const auto CompileProcessType = ConstHash64<'ResM', 'at'>::Value;

        MaterialScaffold(const ::Assets::ChunkFileContainer& chunkFile);
        MaterialScaffold(MaterialScaffold&& moveFrom) never_throws;
        MaterialScaffold& operator=(MaterialScaffold&& moveFrom) never_throws;
        ~MaterialScaffold();

    protected:
        std::unique_ptr<uint8[], PODAlignedDeletor>	_rawMemoryBlock;
		::Assets::DepValPtr _depVal;
    };

	MaterialGuid MakeMaterialGuid(StringSection<utf8> name);

}}

