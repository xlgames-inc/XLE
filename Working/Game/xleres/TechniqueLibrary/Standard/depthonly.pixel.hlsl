#include "../Framework/MainGeometry.hlsl"
#include "../../Nodes/Templates.sh"

#if (VULKAN!=1)
    [earlydepthstencil]
#endif
void frameworkEntryDepthOnly() {}

void frameworkEntryDepthOnlyWithEarlyRejection(VSOUT geo)
{
    if (EarlyRejectionTest(geo))
        discard;
}


