// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicDelegates.h"
#include "ShaderVariationSet.h"
#include "ParsingContext.h"
#include "DeferredShaderResource.h"
#include "../Assets/MaterialScaffold.h"
#include "../Assets/PredefinedCBLayout.h"
#include "../Metal/InputLayout.h"
#include "../BufferView.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringUtils.h"

namespace RenderCore { namespace Techniques
{

    ConstantBufferView GlobalCBDelegate::WriteBuffer(ParsingContext& context, const void* objectContext)
	{
		return context.GetGlobalCB(_cbIndex);
	}

    IteratorRange<const ConstantBufferElementDesc*> GlobalCBDelegate::GetLayout() const
	{
		return {};
	}

}}
