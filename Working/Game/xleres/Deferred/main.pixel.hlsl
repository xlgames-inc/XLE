#include "../TechniqueLibrary/Framework/MainGeometry.hlsl"
#include "../Nodes/Templates.sh"

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
