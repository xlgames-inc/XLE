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

#define BIND_SEQ_S0 : register(s0)
#define BIND_SEQ_S1 : register(s1)
#define BIND_SEQ_S2 : register(s2)
#define BIND_SEQ_S3 : register(s3)

#define BIND_NUMERIC_S0 : register(s4)
#define BIND_NUMERIC_S1 : register(s5)
#define BIND_NUMERIC_S2 : register(s6)

#define BIND_MAT_S12 : register(s7)

///////////////////////////////////////////////////////////////////////////////////////////////////

#define BIND_SEQ_T6 : register(t16)
#define BIND_SEQ_T7 : register(t17)
#define BIND_SEQ_T8 : register(t18)
#define BIND_SEQ_T9 : register(t19)
#define BIND_SEQ_T10 : register(t20)
#define BIND_SEQ_T11 : register(t21)
#define BIND_SEQ_T12 : register(t22)

#define BIND_MAT_T3 : register(t23)
#define BIND_MAT_T4 : register(t24)
#define BIND_MAT_T5 : register(t25)
#define BIND_MAT_T6 : register(t26)
#define BIND_MAT_T7 : register(t27)
#define BIND_MAT_T8 : register(t28)
#define BIND_MAT_T9 : register(t29)
#define BIND_MAT_T10 : register(t30)

#define BIND_NUMERIC_T0 : register(t0)
#define BIND_NUMERIC_T1 : register(t1)
#define BIND_NUMERIC_T2 : register(t2)
#define BIND_NUMERIC_T3 : register(t3)
#define BIND_NUMERIC_T4 : register(t4)
#define BIND_NUMERIC_T5 : register(t5)
#define BIND_NUMERIC_T6 : register(t6)
#define BIND_NUMERIC_T7 : register(t7)
#define BIND_NUMERIC_T8 : register(t8)
#define BIND_NUMERIC_T9 : register(t9)
#define BIND_NUMERIC_T10 : register(t10)
#define BIND_NUMERIC_T11 : register(t11)
#define BIND_NUMERIC_T12 : register(t12)
#define BIND_NUMERIC_T13 : register(t13)
#define BIND_NUMERIC_T14 : register(t14)

///////////////////////////////////////////////////////////////////////////////////////////////////

#define BIND_SEQ_B0 : register(b7)
#define BIND_SEQ_B1 : register(b8)
#define BIND_SEQ_B2 : register(b9)
#define BIND_SEQ_B3 : register(b10)
#define BIND_SEQ_B4 : register(b11)
#define BIND_SEQ_B5 : register(b12)

#define BIND_MAT_B0 : register(b4)
#define BIND_MAT_B1 : register(b5)
#define BIND_MAT_B2 : register(b6)

#define BIND_NUMERIC_B0 : register(b0)
#define BIND_NUMERIC_B1 : register(b1)
#define BIND_NUMERIC_B2 : register(b2)
#define BIND_NUMERIC_B3 : register(b3)

#define BIND_DRAW_B0 : register(b13)
#define BIND_DRAW_T1 : register(t15)

///////////////////////////////////////////////////////////////////////////////////////////////////

#define BIND_NUMERIC_U0 : register(u0)
#define BIND_NUMERIC_U1 : register(u1)
#define BIND_NUMERIC_U2 : register(u2)
#define BIND_NUMERIC_U3 : register(u3)

///////////////////////////////////////////////////////////////////////////////////////////////////

#define BIND_SHADOW_B0      BIND_MAT_B0
#define BIND_SHADOW_B1      BIND_MAT_B1
#define BIND_SHADOW_B2      BIND_MAT_B2

#define BIND_SHADOW_T0      BIND_MAT_T3
#define BIND_SHADOW_T1      BIND_MAT_T4
#define BIND_SHADOW_T2      BIND_MAT_T5

#define BIND_SHARED_LIGHTING_B0 BIND_DRAW_B0
// #define BIND_SHARED_LIGHTING_T0 BIND_DRAW_T0

#define BIND_SHARED_LIGHTING_S2 : register(s8)
#define BIND_SHARED_LIGHTING_S3 : register(s9)

#endif
