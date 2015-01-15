// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Transformations.h"
#include <assert.h>

namespace Math
{

        //
        //      4x4 matrix transform (for reference)
        //
        //          result(0,0) = lhs(0,0) * rhs(0,0) + lhs(0,1) * rhs(1,0) + lhs(0,2) * rhs(2,0) + lhs(0,3) * rhs(3,0);
        //          result(0,1) = lhs(0,0) * rhs(0,1) + lhs(0,1) * rhs(1,1) + lhs(0,2) * rhs(2,1) + lhs(0,3) * rhs(3,1);
        //          result(0,2) = lhs(0,0) * rhs(0,2) + lhs(0,1) * rhs(1,2) + lhs(0,2) * rhs(2,2) + lhs(0,3) * rhs(3,2);
        //          result(0,3) = lhs(0,0) * rhs(0,3) + lhs(0,1) * rhs(1,3) + lhs(0,2) * rhs(2,3) + lhs(0,3) * rhs(3,3);
        // 
        //          result(1,0) = lhs(1,0) * rhs(0,0) + lhs(1,1) * rhs(1,0) + lhs(1,2) * rhs(2,0) + lhs(1,3) * rhs(3,0);
        //          result(1,1) = lhs(1,0) * rhs(0,1) + lhs(1,1) * rhs(1,1) + lhs(1,2) * rhs(2,1) + lhs(1,3) * rhs(3,1);
        //          result(1,2) = lhs(1,0) * rhs(0,2) + lhs(1,1) * rhs(1,2) + lhs(1,2) * rhs(2,2) + lhs(1,3) * rhs(3,2);
        //          result(1,3) = lhs(1,0) * rhs(0,3) + lhs(1,1) * rhs(1,3) + lhs(1,2) * rhs(2,3) + lhs(1,3) * rhs(3,3);
        // 
        //          result(2,0) = lhs(2,0) * rhs(0,0) + lhs(2,1) * rhs(1,0) + lhs(2,2) * rhs(2,0) + lhs(2,3) * rhs(3,0);
        //          result(2,1) = lhs(2,0) * rhs(0,1) + lhs(2,1) * rhs(1,1) + lhs(2,2) * rhs(2,1) + lhs(2,3) * rhs(3,1);
        //          result(2,2) = lhs(2,0) * rhs(0,2) + lhs(2,1) * rhs(1,2) + lhs(2,2) * rhs(2,2) + lhs(2,3) * rhs(3,2);
        //          result(2,3) = lhs(2,0) * rhs(0,3) + lhs(2,1) * rhs(1,3) + lhs(2,2) * rhs(2,3) + lhs(2,3) * rhs(3,3);
        // 
        //          result(3,0) = lhs(3,0) * rhs(0,0) + lhs(3,1) * rhs(1,0) + lhs(3,2) * rhs(2,0) + lhs(3,3) * rhs(3,0);
        //          result(3,1) = lhs(3,0) * rhs(0,1) + lhs(3,1) * rhs(1,1) + lhs(3,2) * rhs(2,1) + lhs(3,3) * rhs(3,1);
        //          result(3,2) = lhs(3,0) * rhs(0,2) + lhs(3,1) * rhs(1,2) + lhs(3,2) * rhs(2,2) + lhs(3,3) * rhs(3,2);
        //          result(3,3) = lhs(3,0) * rhs(0,3) + lhs(3,1) * rhs(1,3) + lhs(3,2) * rhs(2,3) + lhs(3,3) * rhs(3,3);
        //
        //      [4x4] . [4x1] transform (for reference)
        //
        //          result(0,0) = lhs(0,0) * rhs(0) + lhs(0,1) * rhs(1) + lhs(0,2) * rhs(2) + lhs(0,3) * rhs(3);
        //          result(1,0) = lhs(1,0) * rhs(0) + lhs(1,1) * rhs(1) + lhs(1,2) * rhs(2) + lhs(1,3) * rhs(3);
        //          result(2,0) = lhs(2,0) * rhs(0) + lhs(2,1) * rhs(1) + lhs(2,2) * rhs(2) + lhs(2,3) * rhs(3);
        //          result(3,0) = lhs(3,0) * rhs(0) + lhs(3,1) * rhs(1) + lhs(3,2) * rhs(2) + lhs(3,3) * rhs(3);
        //
        //      [1x4] . [4x4] transform (for reference)
        //
        //          result(0,0) = lhs(0) * rhs(0,0) + lhs(1) * rhs(1,0) + lhs(2) * rhs(2,0) + lhs(3) * rhs(3,0);
        //          result(0,1) = lhs(0) * rhs(0,1) + lhs(1) * rhs(1,1) + lhs(2) * rhs(2,1) + lhs(3) * rhs(3,1);
        //          result(0,2) = lhs(0) * rhs(0,2) + lhs(1) * rhs(1,2) + lhs(2) * rhs(2,2) + lhs(3) * rhs(3,2);
        //          result(0,3) = lhs(0) * rhs(0,3) + lhs(1) * rhs(1,3) + lhs(2) * rhs(2,3) + lhs(3) * rhs(3,3);
        //

    void Combine_InPlace(const Float3& translate, Float4x4& transform)
    {
        Float4x4& lhs = transform;
        // const float rhs00 = 1.f, rhs11 = 1.f, rhs22 = 1.f;
        const float rhs33 = 1.f;
        const float rhs03 = translate[0], rhs13 = translate[1], rhs23 = translate[2];

        // lhs(0,0) = lhs(0,0) * rhs00;
        // lhs(0,1) =                      lhs(0,1) * rhs11;
        // lhs(0,2) =                                          lhs(0,2) * rhs22;
        lhs(0,3) = lhs(0,0) * rhs03 +   lhs(0,1) * rhs13 +  lhs(0,2) * rhs23 +      lhs(0,3) * rhs33;

        // lhs(1,0) = lhs(1,0) * rhs00;
        // lhs(1,1) =                      lhs(1,1) * rhs11;
        // lhs(1,2) =                                          lhs(1,2) * rhs22;
        lhs(1,3) = lhs(1,0) * rhs03 +   lhs(1,1) * rhs13 +  lhs(1,2) * rhs23 +      lhs(1,3) * rhs33;

        // lhs(2,0) = lhs(2,0) * rhs00;
        // lhs(2,1) =                      lhs(2,1) * rhs11;
        // lhs(2,2) =                                          lhs(2,2) * rhs22;
        lhs(2,3) = lhs(2,0) * rhs03 +   lhs(2,1) * rhs13 +  lhs(2,2) * rhs23 +      lhs(2,3) * rhs33;

        // lhs(3,0) = lhs(3,0) * rhs00;
        // lhs(3,1) =                      lhs(3,1) * rhs11;
        // lhs(3,2) =                                          lhs(3,2) * rhs22;
        lhs(3,3) = lhs(3,0) * rhs03 +   lhs(3,1) * rhs13 +  lhs(3,2) * rhs23 +      lhs(3,3) * rhs33;
    }

    void Combine_InPlace(Float4x4& transform, const Float3& translate)
    {
        Float4x4& rhs = transform;
        // const float lhs00 = 1.f, lhs11 = 1.f, lhs22 = 1.f;
        // const float lhs33 = 1.f;
        const float lhs03 = translate[0], lhs13 = translate[1], lhs23 = translate[2];

        rhs(0,0) = rhs(0,0) + lhs03 * rhs(3,0);
        rhs(0,1) = rhs(0,1) + lhs03 * rhs(3,1);
        rhs(0,2) = rhs(0,2) + lhs03 * rhs(3,2);
        rhs(0,3) = rhs(0,3) + lhs03 * rhs(3,3);
        
        rhs(1,0) = rhs(1,0) + lhs13 * rhs(3,0);
        rhs(1,1) = rhs(1,1) + lhs13 * rhs(3,1);
        rhs(1,2) = rhs(1,2) + lhs13 * rhs(3,2);
        rhs(1,3) = rhs(1,3) + lhs13 * rhs(3,3);
        
        rhs(2,0) = rhs(2,0) + lhs23 * rhs(3,0);
        rhs(2,1) = rhs(2,1) + lhs23 * rhs(3,1);
        rhs(2,2) = rhs(2,2) + lhs23 * rhs(3,2);
        rhs(2,3) = rhs(2,3) + lhs23 * rhs(3,3);
    }

    void Combine_InPlace(const UniformScale& scale, Float4x4& transform)
    {
        transform(0,0) *= scale._scale; transform(0,1) *= scale._scale; transform(0,2) *= scale._scale;
        transform(1,0) *= scale._scale; transform(1,1) *= scale._scale; transform(1,2) *= scale._scale;
        transform(2,0) *= scale._scale; transform(2,1) *= scale._scale; transform(2,2) *= scale._scale;
    }

    void Combine_InPlace(const ArbitraryScale& scale, Float4x4& transform)
    {
        transform(0,0) *= scale._scale[0]; transform(0,1) *= scale._scale[1]; transform(0,2) *= scale._scale[2];
        transform(1,0) *= scale._scale[0]; transform(1,1) *= scale._scale[1]; transform(1,2) *= scale._scale[2];
        transform(2,0) *= scale._scale[0]; transform(2,1) *= scale._scale[1]; transform(2,2) *= scale._scale[2];
    }

    Float4x4 Combine(const RotationMatrix& rotation, const Float4x4& transform)
    {
            //      But our "transform" is a geometric transform... We just want
            //      to multiply the top-left 3x3 part of the 4x4 matrix by the
            //      input 3x3 rotation matrix.

        Float4x4 result;
        const Float4x4& lhs          = transform;
        const RotationMatrix& rhs    = rotation;
        result(0,0) = lhs(0,0) * rhs(0,0) + lhs(0,1) * rhs(1,0) + lhs(0,2) * rhs(2,0) ;
        result(0,1) = lhs(0,0) * rhs(0,1) + lhs(0,1) * rhs(1,1) + lhs(0,2) * rhs(2,1) ;
        result(0,2) = lhs(0,0) * rhs(0,2) + lhs(0,1) * rhs(1,2) + lhs(0,2) * rhs(2,2) ;
        result(0,3) =                                                                  + lhs(0,3);

        result(1,0) = lhs(1,0) * rhs(0,0) + lhs(1,1) * rhs(1,0) + lhs(1,2) * rhs(2,0) ;
        result(1,1) = lhs(1,0) * rhs(0,1) + lhs(1,1) * rhs(1,1) + lhs(1,2) * rhs(2,1) ;
        result(1,2) = lhs(1,0) * rhs(0,2) + lhs(1,1) * rhs(1,2) + lhs(1,2) * rhs(2,2) ;
        result(1,3) =                                                                  + lhs(1,3);

        result(2,0) = lhs(2,0) * rhs(0,0) + lhs(2,1) * rhs(1,0) + lhs(2,2) * rhs(2,0) ;
        result(2,1) = lhs(2,0) * rhs(0,1) + lhs(2,1) * rhs(1,1) + lhs(2,2) * rhs(2,1) ;
        result(2,2) = lhs(2,0) * rhs(0,2) + lhs(2,1) * rhs(1,2) + lhs(2,2) * rhs(2,2) ;
        result(2,3) =                                                                  + lhs(2,3);

        result(3,0) = lhs(3,0);
        result(3,1) = lhs(3,1);
        result(3,2) = lhs(3,2);
        result(3,3) = lhs(3,3);

        return result;
    }

    Float4x4 Combine(const Float4x4& transform, const RotationMatrix& rotation)
    {
        Float4x4 result;
        const Float4x4& rhs          = transform;
        const RotationMatrix& lhs    = rotation;

        result(0,0) = lhs(0,0) * rhs(0,0) + lhs(0,1) * rhs(1,0) + lhs(0,2) * rhs(2,0);
        result(0,1) = lhs(0,0) * rhs(0,1) + lhs(0,1) * rhs(1,1) + lhs(0,2) * rhs(2,1);
        result(0,2) = lhs(0,0) * rhs(0,2) + lhs(0,1) * rhs(1,2) + lhs(0,2) * rhs(2,2);
        result(0,3) = lhs(0,0) * rhs(0,3) + lhs(0,1) * rhs(1,3) + lhs(0,2) * rhs(2,3);
        
        result(1,0) = lhs(1,0) * rhs(0,0) + lhs(1,1) * rhs(1,0) + lhs(1,2) * rhs(2,0);
        result(1,1) = lhs(1,0) * rhs(0,1) + lhs(1,1) * rhs(1,1) + lhs(1,2) * rhs(2,1);
        result(1,2) = lhs(1,0) * rhs(0,2) + lhs(1,1) * rhs(1,2) + lhs(1,2) * rhs(2,2);
        result(1,3) = lhs(1,0) * rhs(0,3) + lhs(1,1) * rhs(1,3) + lhs(1,2) * rhs(2,3);
        
        result(2,0) = lhs(2,0) * rhs(0,0) + lhs(2,1) * rhs(1,0) + lhs(2,2) * rhs(2,0);
        result(2,1) = lhs(2,0) * rhs(0,1) + lhs(2,1) * rhs(1,1) + lhs(2,2) * rhs(2,1);
        result(2,2) = lhs(2,0) * rhs(0,2) + lhs(2,1) * rhs(1,2) + lhs(2,2) * rhs(2,2);
        result(2,3) = lhs(2,0) * rhs(0,3) + lhs(2,1) * rhs(1,3) + lhs(2,2) * rhs(2,3);
        
        result(3,0) = rhs(3,0);
        result(3,1) = rhs(3,1);
        result(3,2) = rhs(3,2);
        result(3,3) = rhs(3,3);

        return result;
    }

    void Combine_InPlace(RotationX rotation, Float4x4& transform)
    {
            /*
                Following the OpenGL standard for rotation around an axis
                (also used by Collada).

                From the OpenGL Red Book:

                Multiplies the current matrix by a matrix that rotates an object (or the local coordinate system) in a 
                counterclockwise direction about the ray from the origin through the point (x, y, z).

                Holds true for right hand coordinate systems
            */
        float sine, cosine;
        std::tie(sine, cosine) = XlSinCos(rotation._angle);

            //      Almost all values in rhs are zero. So we can simplify the math.
            //      (try to minimize how many elements we need to copy lhs into temporaries)
        const float rhs11 = cosine, rhs12 = -sine;
        const float rhs21 = sine,   rhs22 =  cosine;
        // const float rhs00 = 1.f,    rhs33 = 1.f;
        Float4x4& lhs          = transform;

        // lhs(0,0) = lhs(0,0);
        const float lhs01 = lhs(0,1);
        lhs(0,1) = lhs01 * rhs11 + lhs(0,2) * rhs21;
        lhs(0,2) = lhs01 * rhs12 + lhs(0,2) * rhs22;
        // lhs(0,3) = lhs(0,3);

        // lhs(1,0) = lhs(1,0);
        const float lhs11 = lhs(1,1);
        lhs(1,1) = lhs11 * rhs11 + lhs(1,2) * rhs21;
        lhs(1,2) = lhs11 * rhs12 + lhs(1,2) * rhs22;
        // lhs(1,3) = lhs(1,3);

        // lhs(2,0) = lhs(2,0);
        const float lhs21 = lhs(2,1);
        lhs(2,1) = lhs21 * rhs11 + lhs(2,2) * rhs21;
        lhs(2,2) = lhs21 * rhs12 + lhs(2,2) * rhs22;
        // lhs(2,3) = lhs(2,3);

        // lhs(3,0) = lhs(3,0) * rhs33;
        // lhs(3,1) = lhs(3,1) * rhs33;
        // lhs(3,2) = lhs(3,2) * rhs33;
        // lhs(3,3) = lhs(3,3) * rhs33;
    }

    void Combine_InPlace(RotationY rotation, Float4x4& transform)
    {
            /*
                Following the OpenGL standard for rotation around an axis
                (also used by Collada).

                From the OpenGL Red Book:

                Multiplies the current matrix by a matrix that rotates an object (or the local coordinate system) in a 
                counterclockwise direction about the ray from the origin through the point (x, y, z).

                Holds true for right hand coordinate systems
            */
        float sine, cosine;
        std::tie(sine, cosine) = XlSinCos(rotation._angle);

            //      Almost all values in rhs are zero. So we can simplify the math.
            //      (try to minimize how many elements we need to copy lhs into temporaries)
        const float rhs00 = cosine, rhs02 = sine;
        const float rhs20 = -sine,  rhs22 = cosine;
        // const float rhs11 = 1.f,    rhs33 = 1.f;
        Float4x4& lhs          = transform;

        const float lhs00 = lhs(0,0);
        lhs(0,0) = lhs00 * rhs00 + lhs(0,2) * rhs20;
        // lhs(0,1) = lhs(0,1);
        lhs(0,2) = lhs00 * rhs02 + lhs(0,2) * rhs22;
        // lhs(0,3) = lhs(0,3);

        const float lhs10 = lhs(1,0);
        lhs(1,0) = lhs10 * rhs00 + lhs(1,2) * rhs20;
        // lhs(1,1) = lhs(1,1);
        lhs(1,2) = lhs10 * rhs02 + lhs(1,2) * rhs22;
        // lhs(1,3) = lhs(1,3);

        const float lhs20 = lhs(2,0);
        lhs(2,0) = lhs20 * rhs00 + lhs(2,2) * rhs20;
        // lhs(2,1) = lhs(2,1);
        lhs(2,2) = lhs20 * rhs02 + lhs(2,2) * rhs22;
        // lhs(2,3) = lhs(2,3);

        // lhs(3,0) = lhs(3,0);
        // lhs(3,1) = lhs(3,1);
        // lhs(3,2) = lhs(3,2);
        // lhs(3,3) = lhs(3,3);
    }

    void Combine_InPlace(RotationZ rotation, Float4x4& transform)
    {
            /*
                Following the OpenGL standard for rotation around an axis
                (also used by Collada).

                From the OpenGL Red Book:

                Multiplies the current matrix by a matrix that rotates an object (or the local coordinate system) in a 
                counterclockwise direction about the ray from the origin through the point (x, y, z).

                Holds true for right hand coordinate systems
            */
        float sine, cosine;
        std::tie(sine, cosine) = XlSinCos(rotation._angle);

            //      Almost all values in rhs are zero. So we can simplify the math.
            //      (try to minimize how many elements we need to copy lhs into temporaries)
        const float rhs00 = cosine, rhs01 = -sine;
        const float rhs10 = sine,   rhs11 =  cosine;
        // const float rhs22 = 1.f,    rhs33 = 1.f;
        Float4x4& lhs          = transform;

        const float lhs00 = lhs(0,0);
        lhs(0,0) = lhs00 * rhs00 + lhs(0,1) * rhs10;
        lhs(0,1) = lhs00 * rhs01 + lhs(0,1) * rhs11;
        // lhs(0,2) = lhs(0,2) * rhs22;
        // lhs(0,3) = lhs(0,3) * rhs33;

        const float lhs10 = lhs(1,0);
        lhs(1,0) = lhs10 * rhs00 + lhs(1,1) * rhs10;
        lhs(1,1) = lhs10 * rhs01 + lhs(1,1) * rhs11;
        // lhs(1,2) = lhs(1,2) * rhs22;
        // lhs(1,3) = lhs(1,3) * rhs33;

        const float lhs20 = lhs(2,0);
        lhs(2,0) = lhs20 * rhs00 + lhs(2,1) * rhs10;
        lhs(2,1) = lhs20 * rhs01 + lhs(2,1) * rhs11;
        // lhs(2,2) = lhs(2,2) * rhs22;
        // lhs(2,3) = lhs(2,3) * rhs33;

        // lhs(3,0) = lhs(3,0) * rhs33;
        // lhs(3,1) = lhs(3,1) * rhs33;
        // lhs(3,2) = lhs(3,2) * rhs33;
        // lhs(3,3) = lhs(3,3) * rhs33;
    }

    void Combine_InPlace(Float4x4& transform, RotationX rotation)
    {
        float sine, cosine;
        std::tie(sine, cosine) = XlSinCos(rotation._angle);

        const float lhs11 = cosine, lhs12 = -sine;
        const float lhs21 = sine,   lhs22 =  cosine;
        // const float lhs00 = 1.f,    lhs33 = 1.f;
        Float4x4& rhs          = transform;

        // rhs(0,0) = rhs(0,0);
        // rhs(0,1) = rhs(0,1);
        // rhs(0,2) = rhs(0,2);
        // rhs(0,3) = rhs(0,3);
        
        const float rhs10 = rhs(1,0), rhs11 = rhs(1,1), rhs12 = rhs(1,2), rhs13 = rhs(1,3);
        rhs(1,0) = lhs11 * rhs10 + lhs12 * rhs(2,0);
        rhs(1,1) = lhs11 * rhs11 + lhs12 * rhs(2,1);
        rhs(1,2) = lhs11 * rhs12 + lhs12 * rhs(2,2);
        rhs(1,3) = lhs11 * rhs13 + lhs12 * rhs(2,3);
        
        rhs(2,0) = lhs21 * rhs10 + lhs22 * rhs(2,0);
        rhs(2,1) = lhs21 * rhs11 + lhs22 * rhs(2,1);
        rhs(2,2) = lhs21 * rhs12 + lhs22 * rhs(2,2);
        rhs(2,3) = lhs21 * rhs13 + lhs22 * rhs(2,3);
        
        // rhs(3,0) = rhs(3,0);
        // rhs(3,1) = rhs(3,1);
        // rhs(3,2) = rhs(3,2);
        // rhs(3,3) = rhs(3,3);
    }

    void Combine_InPlace(Float4x4& transform, RotationY rotation)
    {
        float sine, cosine;
        std::tie(sine, cosine) = XlSinCos(rotation._angle);

        const float lhs00 = cosine, lhs02 = sine;
        const float lhs20 = -sine,  lhs22 = cosine;
        // const float lhs11 = 1.f,    lhs33 = 1.f;
        Float4x4& rhs = transform;

        const float rhs00 = rhs(0,0), rhs01 = rhs(0,1), rhs02 = rhs(0,2), rhs03 = rhs(0,3);
        rhs(0,0) = lhs00 * rhs00 + lhs02 * rhs(2,0);
        rhs(0,1) = lhs00 * rhs01 + lhs02 * rhs(2,1);
        rhs(0,2) = lhs00 * rhs02 + lhs02 * rhs(2,2);
        rhs(0,3) = lhs00 * rhs03 + lhs02 * rhs(2,3);
        
        // rhs(1,0) = rhs(1,0);
        // rhs(1,1) = rhs(1,1);
        // rhs(1,2) = rhs(1,2);
        // rhs(1,3) = rhs(1,3);
        
        rhs(2,0) = lhs20 * rhs00 + lhs22 * rhs(2,0);
        rhs(2,1) = lhs20 * rhs01 + lhs22 * rhs(2,1);
        rhs(2,2) = lhs20 * rhs02 + lhs22 * rhs(2,2);
        rhs(2,3) = lhs20 * rhs03 + lhs22 * rhs(2,3);
        
        // rhs(3,0) = rhs(3,0);
        // rhs(3,1) = rhs(3,1);
        // rhs(3,2) = rhs(3,2);
        // rhs(3,3) = rhs(3,3);
    }

    void Combine_InPlace(Float4x4& transform, RotationZ rotation)
    {
        float sine, cosine;
        std::tie(sine, cosine) = XlSinCos(rotation._angle);

        const float lhs00 = cosine, lhs01 = -sine;
        const float lhs10 = sine,   lhs11 =  cosine;
        // const float lhs22 = 1.f,    lhs33 = 1.f;
        Float4x4& rhs          = transform;

        const float rhs00 = rhs(0,0), rhs01 = rhs(0,1), rhs02 = rhs(0,2), rhs03 = rhs(0,3);
        rhs(0,0) = lhs00 * rhs00 + lhs01 * rhs(1,0);
        rhs(0,1) = lhs00 * rhs01 + lhs01 * rhs(1,1);
        rhs(0,2) = lhs00 * rhs02 + lhs01 * rhs(1,2);
        rhs(0,3) = lhs00 * rhs03 + lhs01 * rhs(1,3);
        
        rhs(1,0) = lhs10 * rhs00 + lhs11 * rhs(1,0);
        rhs(1,1) = lhs10 * rhs01 + lhs11 * rhs(1,1);
        rhs(1,2) = lhs10 * rhs02 + lhs11 * rhs(1,2);
        rhs(1,3) = lhs10 * rhs03 + lhs11 * rhs(1,3);
        
        rhs(2,0) = rhs(2,0);
        rhs(2,1) = rhs(2,1);
        rhs(2,2) = rhs(2,2);
        rhs(2,3) = rhs(2,3);
        
        rhs(3,0) = rhs(3,0);
        rhs(3,1) = rhs(3,1);
        rhs(3,2) = rhs(3,2);
        rhs(3,3) = rhs(3,3);
    }


    Float3          TransformPoint(const Float3x4& transform, Float3 pt)
    {
        return transform * Expand(pt, 1.f);
    }

    Float3          TransformPoint(const Float4x4& transform, Float3 pt)
    {
        return Truncate(transform * Expand(pt, 1.f));
    }

    Float3          TransformDirectionVector(const Float3x3& transform, Float3 pt)
    {
        return transform * pt;
    }

    Float3          TransformDirectionVector(const Float3x4& transform, Float3 pt)
    {
        return transform * Expand(pt, 0.f);
    }

    Float3          TransformDirectionVector(const Float4x4& transform, Float3 pt)
    {
        return Truncate(transform * Expand(pt, 0.f));
    }


    bool IsOrthonormal(const Float3x3& rotationMatrix, float tolerance = 0.01f)
    {
        Float3 A(rotationMatrix(0,0), rotationMatrix(0,1), rotationMatrix(0,2));
        Float3 B(rotationMatrix(1,0), rotationMatrix(1,1), rotationMatrix(1,2));
        Float3 C(rotationMatrix(2,0), rotationMatrix(2,1), rotationMatrix(2,2));
        return  Equivalent(Dot(A,B), 0.f, tolerance)
            &&  Equivalent(Dot(A,C), 0.f, tolerance)
            &&  Equivalent(Dot(B,C), 0.f, tolerance)
            &&  Equivalent(MagnitudeSquared(A), 1.f, tolerance)
            &&  Equivalent(MagnitudeSquared(B), 1.f, tolerance)
            &&  Equivalent(MagnitudeSquared(C), 1.f, tolerance)
            ;
    }

    Float4x4 InvertOrthonormalTransform(const Float4x4& input)
    {
            //
            //      Given a standard orthonormal transformation matrix, we can calculate
            //      the inverse quickly...
            //

        float t[3];
        t[0] = input(0,0) * -input(0,3) + input(1,0) * -input(1,3) + input(2,0) * -input(2,3);
        t[1] = input(0,1) * -input(0,3) + input(1,1) * -input(1,3) + input(2,1) * -input(2,3);
        t[2] = input(0,2) * -input(0,3) + input(1,2) * -input(1,3) + input(2,2) * -input(2,3);
        Float4x4 result;
        result(0,0) = input(0,0);   result(0,1) = input(1,0);   result(0,2) = input(2,0);   result(0,3) = t[0];
        result(1,0) = input(0,1);   result(1,1) = input(1,1);   result(1,2) = input(2,1);   result(1,3) = t[1];
        result(2,0) = input(0,2);   result(2,1) = input(1,2);   result(2,2) = input(2,2);   result(2,3) = t[2];
        result(3,0) = 0.f; result(3,1) = 0.f; result(3,2) = 0.f; result(3,3) = 1.f;
        return result;
    }

    Float4x4 Expand(const Float3x3& rotationScalePart, const Float3& translationPart)
    {
        return Float4x4(
            rotationScalePart(0,0), rotationScalePart(0,1), rotationScalePart(0,2), translationPart[0],
            rotationScalePart(1,0), rotationScalePart(1,1), rotationScalePart(1,2), translationPart[1],
            rotationScalePart(2,0), rotationScalePart(2,1), rotationScalePart(2,2), translationPart[2],
            0, 0, 0, 1);
    }


        ////////////////////////////////////////////////////////////////

    RotationScaleTranslation::RotationScaleTranslation(const Float4x4& copyFrom)
    {
        Float3 scale(
            Magnitude(Float3(copyFrom(0,0), copyFrom(1,0), copyFrom(2,0))),
            Magnitude(Float3(copyFrom(0,1), copyFrom(1,1), copyFrom(2,1))),
            Magnitude(Float3(copyFrom(0,2), copyFrom(1,2), copyFrom(2,2))));
        Float3 translation(copyFrom(0,3), copyFrom(1,3), copyFrom(2,3));
        Float3x3 rotationAsMatrix(
            copyFrom(0,0)/scale[0], copyFrom(0,1)/scale[1], copyFrom(0,2)/scale[2],
            copyFrom(1,0)/scale[0], copyFrom(1,1)/scale[1], copyFrom(1,2)/scale[2],
            copyFrom(2,0)/scale[0], copyFrom(2,1)/scale[1], copyFrom(2,2)/scale[2]);
        assert(IsOrthonormal(rotationAsMatrix));

        cml::quaternion_rotation_matrix(_rotation, rotationAsMatrix);
        _scale = scale;
        _translation = translation;
    }

    RotationScaleTranslation SphericalInterpolate(
        const RotationScaleTranslation& lhs, const RotationScaleTranslation& rhs, float alpha)
    {
        return RotationScaleTranslation(
            SphericalInterpolate(lhs._rotation, rhs._rotation, alpha),
            LinearInterpolate(lhs._scale, rhs._scale, alpha),
            LinearInterpolate(lhs._translation, rhs._translation, alpha));
    }

    Float3x3   AsFloat3x3(const Quaternion& input)
    {
        Float3x3 result;
        cml::matrix_rotation_quaternion(result, input);
        return result;
    }

    Float4x4   AsFloat4x4(const RotationScaleTranslation& input)
    {
            //
            //          Convert from our separate rotation/scale/translation representation to
            //          a general 4x4 matrix
            //
        Float3x3 rotationPart = AsFloat3x3(input._rotation);
        Float3 s = input._scale;
        Float4x4 result(
            rotationPart(0,0) * s[0], rotationPart(0,1) * s[1], rotationPart(0,2) * s[2], input._translation[0],
            rotationPart(1,0) * s[0], rotationPart(1,1) * s[1], rotationPart(1,2) * s[2], input._translation[1],
            rotationPart(2,0) * s[0], rotationPart(2,1) * s[1], rotationPart(2,2) * s[2], input._translation[2],
            0.f, 0.f, 0.f, 1.f);
        return result;
    }

    Float4x4   AsFloat4x4(const UniformScale& input)
    {
        return Float4x4(
            input._scale, 0.f, 0.f, 0.f,
            0.f, input._scale, 0.f, 0.f,
            0.f, 0.f, input._scale, 0.f,
            0.f, 0.f, 0.f, 1.f);
    }

    Float4x4    AsFloat4x4(const Float3& translation)
    {
        return Float4x4(
            1.f, 0.f, 0.f, translation[0],
            0.f, 1.f, 0.f, translation[1],
            0.f, 0.f, 1.f, translation[2],
            0.f, 0.f, 0.f, 1.f);
    }

    Float3x4    MakeFloat3x4(
        float e00, float e10, float e20, float e30,
        float e01, float e11, float e21, float e31,
        float e02, float e12, float e22, float e32)
    {
        float arrayForm[3][4] = {
            { e00, e10, e20, e30 },
            { e01, e11, e21, e31 },
            { e02, e12, e22, e32 }
        };
        return Float3x4(arrayForm);
    }
    
    Float3x4    AsFloat3x4(const Float3& translation)
    {
        return MakeFloat3x4(
            1.f, 0.f, 0.f, translation[0],
            0.f, 1.f, 0.f, translation[1],
            0.f, 0.f, 1.f, translation[2]);
    }

    Float4x4    AsFloat4x4(const Float3x4& orthonormalTransform)
    {
        return Float4x4(
            orthonormalTransform(0,0), orthonormalTransform(0,1), orthonormalTransform(0,2), orthonormalTransform(0,3), 
            orthonormalTransform(1,0), orthonormalTransform(1,1), orthonormalTransform(1,2), orthonormalTransform(1,3), 
            orthonormalTransform(2,0), orthonormalTransform(2,1), orthonormalTransform(2,2), orthonormalTransform(2,3), 
            0.f, 0.f, 0.f, 1.f);
    }

    Float3x4    AsFloat3x4(const Float4x4& orthonormalTransform)
    {
            //  Make sure the last row is the same as the last row
            //  that we assume when using Float3x4 as a transformation
            //  matrix.
        assert(orthonormalTransform(3, 0) == 0.f);
        assert(orthonormalTransform(3, 1) == 0.f);
        assert(orthonormalTransform(3, 2) == 0.f);
        assert(orthonormalTransform(3, 3) == 1.f);
        return MakeFloat3x4(
            orthonormalTransform(0,0), orthonormalTransform(0,1), orthonormalTransform(0,2), orthonormalTransform(0,3), 
            orthonormalTransform(1,0), orthonormalTransform(1,1), orthonormalTransform(1,2), orthonormalTransform(1,3), 
            orthonormalTransform(2,0), orthonormalTransform(2,1), orthonormalTransform(2,2), orthonormalTransform(2,3));
    }

    Float4x4 MakeCameraToWorld(const Float3& forward, const Float3& up, const Float3& position)
    {
        Float3 right            = Cross(forward, up);
        Float3 adjustedUp       = Normalize(Cross(right, forward));
        Float3 adjustedRight    = Normalize(Cross(forward, adjustedUp));

        return Float4x4(
            adjustedRight[0], adjustedUp[0], -forward[0], position[0],
            adjustedRight[1], adjustedUp[1], -forward[1], position[1],
            adjustedRight[2], adjustedUp[2], -forward[2], position[2],
            0.f, 0.f, 0.f, 1.f);
    }

}