// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SystemUniformsDelegate.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Techniques
{
	void SystemUniformsDelegate::WriteImmediateData(ParsingContext& context, const void* objectContext, unsigned idx, IteratorRange<void*> dst)
	{
		switch (idx) {
		case 0:
			*(GlobalTransformConstants*)dst.begin() = _globalTransform;
			break;
		case 1:
			*(LocalTransformConstants*)dst.begin() = _localTransformFallback;
			break;
		}
	}

	size_t SystemUniformsDelegate::GetImmediateDataSize(ParsingContext& context, const void* objectContext, unsigned idx)
	{
		switch (idx) {
		case 0:
			return sizeof(GlobalTransformConstants);
		case 1:
			return sizeof(LocalTransformConstants);
		default:
			return 0;
		}
	}

	const UniformsStreamInterface& SystemUniformsDelegate::GetInterface() { return _interface; }

	SystemUniformsDelegate::SystemUniformsDelegate()
	{
		_interface.BindImmediateData(0, Hash64("GlobalTransform"));
		_interface.BindImmediateData(1, Hash64("LocalTransform"));

		XlZeroMemory(_globalTransform);
		XlZeroMemory(_localTransformFallback);
		_globalTransform = BuildGlobalTransformConstants(ProjectionDesc{});
		_localTransformFallback._localToWorld = Identity<Float3x4>();
		_localTransformFallback._localSpaceView = Float3(0.f, 0.f, 0.f);
	}

	SystemUniformsDelegate::~SystemUniformsDelegate()
	{
	}
}}
