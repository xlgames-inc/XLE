
#if !defined(PASS_STANDARD_SH)
#define PASS_STANDARD_SH

bool EarlyRejectionTest_Default(VSOutput geo)
{
    return DoAlphaTest(geo, GetAlphaThreshold());
}

#endif
