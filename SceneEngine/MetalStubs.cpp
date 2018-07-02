// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MetalStubs.h"

namespace SceneEngine { namespace MetalStubs
{
	void GeometryShader::SetDefaultStreamOutputInitializers(const StreamOutputInitializers&)
	{
	}

	const GeometryShader::StreamOutputInitializers& GeometryShader::GetDefaultStreamOutputInitializers()
	{
		return *(const GeometryShader::StreamOutputInitializers*)nullptr;
	}
}}
