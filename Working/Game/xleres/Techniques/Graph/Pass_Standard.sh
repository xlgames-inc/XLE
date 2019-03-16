
#if !defined(PASS_STANDARD_SH)
#define PASS_STANDARD_SH

#include "../../MainGeometry.h"
#include "../../gbuffer.h"

bool EarlyRejectionTest(VSOutput geo);
GBufferValues PerPixel(VSOutput geo);

bool EarlyRejectionTest_Default(VSOutput geo)
{
    return DoAlphaTest(geo, GetAlphaThreshold());
}

#endif
