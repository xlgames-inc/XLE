#include "../Framework/VSOUT.hlsl"
#include "../Framework/gbuffer.hlsl"
#include "../../Nodes/Templates.pixel.sh"

#if (VULKAN!=1)
    [earlydepthstencil]
#endif
GBufferEncoded frameworkEntry(VSOUT geo)
{
	GBufferValues result = PerPixel(geo);
	return Encode(result);
}

GBufferEncoded frameworkEntryWithEarlyRejection(VSOUT geo)
{
    if (EarlyRejectionTest(geo))
        discard;

	GBufferValues result = PerPixel(geo);
	return Encode(result);
}
