// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"
#include <memory>

namespace Assets { class ICompileOperation; }

namespace ColladaConversion
{
    std::shared_ptr<::Assets::ICompileOperation> CreateCompileOperation(StringSection<> identifier);

	static const uint64 Type_Model = ConstHash64<'Mode', 'l'>::Value;
	static const uint64 Type_AnimationSet = ConstHash64<'Anim', 'Set'>::Value;
	static const uint64 Type_Skeleton = ConstHash64<'Skel', 'eton'>::Value;
	static const uint64 Type_RawMat = ConstHash64<'RawM', 'at'>::Value;
}
