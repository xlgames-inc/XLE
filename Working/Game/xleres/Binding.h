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

///////////////////////////////////////////////////
// 4 uniform streams
//      * SEQ "sequencer" -- this is global settings, usually set once per many draw calls
//      * MAT "material" -- once per material change
//      * OBJ "object" -- typically contains coordinate space information, such as local to world an related constants
//      * DRW "draw" -- catches anything that doesn't fit above, often used for special case rendering features
//                  and typically contains the bindings that change most frequently
//
// Also, there's a "numeric interface" bindings, which are bound by number, rather than by name, from the CPU side
// code. 
//
// Bindings that don't use specific macros implicitly fall into the "draw" stream

#define BIND_SEQ_S0 : register(s0)
#define BIND_SEQ_S1 : register(s1)
#define BIND_SEQ_S2 : register(s2)
#define BIND_SEQ_S3 : register(s3)
#define BIND_SEQ_S4 : register(s4)
#define BIND_SEQ_S5 : register(s5)
#define BIND_SEQ_S6 : register(s6)

///////////////////////////////////////////////////////////////////////////////////////////////////

#define BIND_MAT_T0 : register(t23)
#define BIND_MAT_T1 : register(t24)
#define BIND_MAT_T2 : register(t25)
#define BIND_MAT_T3 : register(t26)
#define BIND_MAT_T4 : register(t27)
#define BIND_MAT_T5 : register(t28)
#define BIND_MAT_T6 : register(t29)
#define BIND_MAT_T7 : register(t30)

#if defined(VULKAN)
    #define BIND_NUMERIC_T0 : register(t33)
    #define BIND_NUMERIC_T1 : register(t34)
    #define BIND_NUMERIC_T2 : register(t35)
    #define BIND_NUMERIC_T3 : register(t36)
    #define BIND_NUMERIC_T4 : register(t37)
    #define BIND_NUMERIC_T5 : register(t38)
#else
    #define BIND_NUMERIC_T0 : register(t0)
    #define BIND_NUMERIC_T1 : register(t1)
    #define BIND_NUMERIC_T2 : register(t2)
    #define BIND_NUMERIC_T3 : register(t3)
    #define BIND_NUMERIC_T4 : register(t4)
    #define BIND_NUMERIC_T5 : register(t5)
#endif

#define BIND_SEQ_T0 : register(t10)
#define BIND_SEQ_T1 : register(t11)
#define BIND_SEQ_T2 : register(t12)
#define BIND_SEQ_T3 : register(t13)
#define BIND_SEQ_T4 : register(t14)
#define BIND_SEQ_T5 : register(t15)
#define BIND_SEQ_T6 : register(t16)
#define BIND_SEQ_T7 : register(t17)
#define BIND_SEQ_T8 : register(t18)
#define BIND_SEQ_T9 : register(t19)
#define BIND_SEQ_T10 : register(t20)
#define BIND_SEQ_T11 : register(t21)
#define BIND_SEQ_T12 : register(t22)

///////////////////////////////////////////////////////////////////////////////////////////////////

#define BIND_SEQ_B0 : register(b6)
#define BIND_SEQ_B1 : register(b7)
#define BIND_SEQ_B2 : register(b8)
#define BIND_SEQ_B3 : register(b9)
#define BIND_SEQ_B4 : register(b10)
#define BIND_SEQ_B5 : register(b11)

#define BIND_MAT_B0 : register(b3)
#define BIND_MAT_B1 : register(b4)
#define BIND_MAT_B2 : register(b5)

#if defined(VULKAN)
    #define BIND_NUMERIC_B0 : register(b33)
    #define BIND_NUMERIC_B1 : register(b34)
    #define BIND_NUMERIC_B2 : register(b35)
    #define BIND_NUMERIC_B3 : register(b36)
    #define BIND_NUMERIC_B4 : register(b37)
    #define BIND_NUMERIC_B5 : register(b38)
#else
    #define BIND_NUMERIC_B0 : register(b0)
    #define BIND_NUMERIC_B1 : register(b1)
    #define BIND_NUMERIC_B2 : register(b2)
    #define BIND_NUMERIC_B3 : register(b3)
    #define BIND_NUMERIC_B4 : register(b4)
    #define BIND_NUMERIC_B5 : register(b5)
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

#define UAV_DYNAMIC_0 : register(u0)
#define UAV_DYNAMIC_1 : register(u1)
#define UAV_DYNAMIC_2 : register(u2)
#define UAV_DYNAMIC_3 : register(u3)
#define UAV_DYNAMIC_4 : register(u4)
#define UAV_DYNAMIC_5 : register(u5)
#define UAV_DYNAMIC_6 : register(u6)
#define UAV_DYNAMIC_7 : register(u7)

#endif
