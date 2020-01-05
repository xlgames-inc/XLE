// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DrawableDelegates.h"

namespace RenderCore { namespace Assets { class PredefinedCBLayout; }}

namespace RenderCore { namespace Techniques
{
	class GlobalCBDelegate : public IUniformBufferDelegate
    {
    public:
        ConstantBufferView WriteBuffer(ParsingContext& context, const void* objectContext);
        IteratorRange<const ConstantBufferElementDesc*> GetLayout() const;
		GlobalCBDelegate(unsigned cbIndex = 0) : _cbIndex(cbIndex) {}
	private:
		unsigned _cbIndex = 0;
    };
}}
