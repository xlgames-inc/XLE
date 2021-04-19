// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DrawableDelegates.h"
#include "TechniqueUtils.h"
#include "../UniformsStream.h"

namespace RenderCore { namespace Techniques
{
	class SystemUniformsDelegate : public IShaderResourceDelegate
	{
	public:
		void SetGlobalTransform(const GlobalTransformConstants& input) { _globalTransform = input; }
		void SetLocalTransformFallback(const LocalTransformConstants& input) { _localTransformFallback = input; }

		const UniformsStreamInterface& GetInterface() override;
		void WriteImmediateData(ParsingContext& context, const void* objectContext, unsigned idx, IteratorRange<void*> dst) override;
		size_t GetImmediateDataSize(ParsingContext& context, const void* objectContext, unsigned idx) override;
		SystemUniformsDelegate();
		~SystemUniformsDelegate();
	private:
		UniformsStreamInterface _interface;
		GlobalTransformConstants _globalTransform;
		LocalTransformConstants _localTransformFallback;
	};
}}
