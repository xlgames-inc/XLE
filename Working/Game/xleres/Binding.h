// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BINDING_H)
#define BINDING_H

// There's no easy way to construct a macro that will handle this case better
// In SM5, register(t[index+10]) no longer works
// In SM5.1, we could use register(t10, space1)... But it means dropping some compatibility

#define SAMPLER_GLOBAL_0 register(s0)
#define SAMPLER_GLOBAL_1 register(s1)
#define SAMPLER_GLOBAL_2 register(s2)
#define SAMPLER_GLOBAL_3 register(s3)
#define SAMPLER_GLOBAL_4 register(s4)
#define SAMPLER_GLOBAL_5 register(s5)
#define SAMPLER_GLOBAL_6 register(s6)

///////////////////////////////////////////////////////////////////////////////////////////////////

#define TEXTURE_BOUND0_0 register(t0)
#define TEXTURE_BOUND0_1 register(t1)
#define TEXTURE_BOUND0_2 register(t2)
#define TEXTURE_BOUND0_3 register(t3)
#define TEXTURE_BOUND0_4 register(t4)
#define TEXTURE_BOUND0_5 register(t5)
#define TEXTURE_BOUND0_6 register(t6)
#define TEXTURE_BOUND0_7 register(t7)

#define TEXTURE_BOUND1_0 register(t0)
#define TEXTURE_BOUND1_1 register(t1)
#define TEXTURE_BOUND1_2 register(t2)
#define TEXTURE_BOUND1_3 register(t3)
#define TEXTURE_BOUND1_4 register(t4)
#define TEXTURE_BOUND1_5 register(t5)
#define TEXTURE_BOUND1_6 register(t6)
#define TEXTURE_BOUND1_7 register(t7)

#define TEXTURE_DYNAMIC_0 register(t8)
#define TEXTURE_DYNAMIC_1 register(t9)
#define TEXTURE_DYNAMIC_2 register(t10)
#define TEXTURE_DYNAMIC_3 register(t11)

#define TEXTURE_GLOBAL_0 register(t12)
#define TEXTURE_GLOBAL_1 register(t13)
#define TEXTURE_GLOBAL_2 register(t14)
#define TEXTURE_GLOBAL_3 register(t15)
#define TEXTURE_GLOBAL_4 register(t16)
#define TEXTURE_GLOBAL_5 register(t17)
#define TEXTURE_GLOBAL_6 register(t18)

///////////////////////////////////////////////////////////////////////////////////////////////////

#define CB_BOUND0_0 register(b0)
#define CB_BOUND0_1 register(b1)
#define CB_BOUND0_2 register(b2)
#define CB_BOUND0_3 register(b3)
#define CB_BOUND0_4 register(b4)
#define CB_BOUND0_5 register(b5)
#define CB_BOUND0_6 register(b6)

#define CB_BOUND1_0 register(b0)
#define CB_BOUND1_1 register(b1)
#define CB_BOUND1_2 register(b2)
#define CB_BOUND1_3 register(b3)
#define CB_BOUND1_4 register(b4)
#define CB_BOUND1_5 register(b5)
#define CB_BOUND1_6 register(b6)

#define CB_DYNAMIC_0 register(b7)
#define CB_DYNAMIC_1 register(b8)

#define CB_GLOBAL_0 register(b9)
#define CB_GLOBAL_1 register(b10)
#define CB_GLOBAL_2 register(b11)

#endif
