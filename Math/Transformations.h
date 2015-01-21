// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Matrix.h"
#include "Quaternion.h"

#if MATHLIBRARY_ACTIVE == MATHLIBRARY_CML
    #pragma warning(push)
    #pragma warning(disable:4512)       // assignment operator could not be generated
    #include <cml/mathlib/matrix_rotation.h>
    #include <cml/mathlib/quaternion_rotation.h>
    #include <cml/mathlib/interpolation.h>
    #pragma warning(pop)
#endif

namespace Math
{
        //
        //      Primitive transformation types
        //

    class UniformScale      { public: float _scale;     explicit UniformScale(float scale) : _scale(scale) {} };
    class ArbitraryScale    { public: Float3 _scale;    explicit ArbitraryScale(const Float3& scale) : _scale(scale) {} };
    class RotationX         { public: float _angle;     explicit RotationX(float angle) : _angle(angle) {} };
    class RotationY         { public: float _angle;     explicit RotationY(float angle) : _angle(angle) {} };
    class RotationZ         { public: float _angle;     explicit RotationZ(float angle) : _angle(angle) {} };

    #if MATHLIBRARY_ACTIVE == MATHLIBRARY_CML
        typedef cml::matrix< float, cml::fixed<3,3>, cml::col_basis>    RotationMatrix;
        inline RotationMatrix   MakeRotationMatrix(Float3 axis, Float3::value_type angle) 
        {
            RotationMatrix result; 
            cml::matrix_rotation_axis_angle(result, axis, angle);
            return result;
        }
    #endif

    class RotationScaleTranslation
    {
    public:
            //
            //      This is a full 3D transformation.
            //      It can represent any affline transformation,
            //      and includes a non-uniform scale parameter.
            //
            //      Note that the translation part here is assumed to be applied
            //      after rotation/scale
            //
        Quaternion      _rotation;
        Float3          _scale;
        Float3          _translation;

        explicit RotationScaleTranslation(const Float4x4& copyFrom);
        RotationScaleTranslation(const Quaternion& rotation, Float3 scale, Float3 translation);
    };
        
        //
        //      "Combine" and "Combine_InPlace" patterns.
        //
        //      For many transformation types, there are overloads for Combine & Combine_InPlace.
        //      This will combine two transforms together, in such a way that the first
        //      parameter is always the first transformation applied.
        //
        //      That is to say, the following 2 cases are defined to be the same:
        //          ptResult = Transform(Combine(A, B), ptOriginal)
        //
        //      Or
        //          ptMidway = Transform(A, ptOriginal)
        //          ptResult = Transform(B, ptMidway)
        //
        //      Combine_InPlace provides basic optimisation when the output result is mostly
        //      the same as one of the input (perhaps only a few elements have changed)
        //

    inline Float4x4     Combine(const Float4x4& firstTransform, const Float4x4& secondTransform)
    {
        return secondTransform * firstTransform;
    }
    
    void            Combine_InPlace(const Float3& translate, Float4x4& transform);
    void            Combine_InPlace(const UniformScale& scale, Float4x4& transform);
    void            Combine_InPlace(const ArbitraryScale& scale, Float4x4& transform);
    void            Combine_InPlace(RotationX rotation, Float4x4& transform);
    void            Combine_InPlace(RotationY rotation, Float4x4& transform);
    void            Combine_InPlace(RotationZ rotation, Float4x4& transform);

    void            Combine_InPlace(Float4x4& transform, const Float3& translate);
    void            Combine_InPlace(Float4x4& transform, const UniformScale& scale);
    void            Combine_InPlace(Float4x4& transform, const ArbitraryScale& scale);
    void            Combine_InPlace(Float4x4& transform, RotationX rotation);
    void            Combine_InPlace(Float4x4& transform, RotationY rotation);
    void            Combine_InPlace(Float4x4& transform, RotationZ rotation);
    

    Float4x4        Combine(const RotationMatrix& rotation, const Float4x4& transform);
    Float4x4        Combine(const Float4x4& transform, const RotationMatrix& rotation);

        //
        //      Basic transformations
        //
    Float3          TransformPoint(const Float3x4& transform, Float3 pt);
    Float3          TransformPoint(const Float4x4& transform, Float3 pt);
    Float3          TransformDirectionVector(const Float3x3& transform, Float3 pt);
    Float3          TransformDirectionVector(const Float3x4& transform, Float3 pt);
    Float3          TransformDirectionVector(const Float4x4& transform, Float3 pt);

        //
        //      Orthonormal matrices have special properties. Use the following, instead
        //      of more general operations.
        //
    Float4x4        InvertOrthonormalTransform(const Float4x4& input);
    bool            IsOrthonormal(const Float4x4& input);
    Float4x4        Expand(const Float3x3& rotationScalePart, const Float3& translationPart);

        //
        //      Extract the basis vectors from transformations.
        //      Use these, instead of accessing the rows & columns of the matrix directly.
        //      This will insulate your code from matrix configuration options.
        //
        //
        //      Normal object-to-world:
        //          Forward:    +Y
        //          Up:         +Z
        //          Right:      +X
        //
        //      Camera:
        //          Forward:    -Z
        //          Up:         +Y
        //          Right:      +X
        //
    inline Float3   ExtractTranslation(const Float4x4& matrix)      { return  Float3(matrix(0,3), matrix(1,3), matrix(2,3)); }
    inline Float3   ExtractRight(const Float4x4& matrix)            { return  Float3(matrix(0,0), matrix(1,0), matrix(2,0)); }
    inline Float3   ExtractForward(const Float4x4& matrix)          { return  Float3(matrix(0,1), matrix(1,1), matrix(2,1)); }
    inline Float3   ExtractUp(const Float4x4& matrix)               { return  Float3(matrix(0,2), matrix(1,2), matrix(2,2)); }

    inline Float3   ExtractRight_Cam(const Float4x4& matrix)        { return  Float3(matrix(0,0), matrix(1,0), matrix(2,0)); }
    inline Float3   ExtractForward_Cam(const Float4x4& matrix)      { return -Float3(matrix(0,2), matrix(1,2), matrix(2,2)); }
    inline Float3   ExtractUp_Cam(const Float4x4& matrix)           { return  Float3(matrix(0,1), matrix(1,1), matrix(2,1)); }


    inline void SetTranslation(Float4x4& matrix, const Float3& position)  { matrix(0,3) = position[0]; matrix(1,3) = position[1]; matrix(2,3) = position[2]; }


    RotationScaleTranslation    SphericalInterpolate(const RotationScaleTranslation& lhs, const RotationScaleTranslation& rhs, float alpha);

    Float3x3    AsFloat3x3(const Quaternion& input);
    Float4x4    AsFloat4x4(const RotationScaleTranslation& input);
    Float4x4    AsFloat4x4(const UniformScale& input);
    Float4x4    AsFloat4x4(const Float3& translation);
    Float3x4    AsFloat3x4(const Float3& translation);
    Float4x4    AsFloat4x4(const Float3x4& orthonormalTransform);
    Float3x4    AsFloat3x4(const Float4x4& orthonormalTransform);

    inline Float4x4     Combine(const Float3x4& firstTransform, const Float4x4& secondTransform)
    {
            // placeholder for better implementation!
        return secondTransform * AsFloat4x4(firstTransform);
    }
    
    inline Float4x4     Combine(const Float4x4& firstTransform, const Float3x4& secondTransform)
    {
            // placeholder for better implementation!
        return AsFloat4x4(secondTransform) * firstTransform;
    }

    Float4x4    MakeCameraToWorld(const Float3& forward, const Float3& up, const Float3& position);
    
    inline RotationScaleTranslation::RotationScaleTranslation(const Quaternion& rotation, Float3 scale, Float3 translation)
    : _rotation(rotation), _scale(scale), _translation(translation) {}
 
    inline void CopyTransform(Float3x4& destination, const Float4x4& localToWorld)
    {
        destination(0, 0)    = localToWorld(0, 0);
        destination(0, 1)    = localToWorld(0, 1);
        destination(0, 2)    = localToWorld(0, 2);
        destination(0, 3)    = localToWorld(0, 3);
        destination(1, 0)    = localToWorld(1, 0);
        destination(1, 1)    = localToWorld(1, 1);
        destination(1, 2)    = localToWorld(1, 2);
        destination(1, 3)    = localToWorld(1, 3);
        destination(2, 0)    = localToWorld(2, 0);
        destination(2, 1)    = localToWorld(2, 1);
        destination(2, 2)    = localToWorld(2, 2);
        destination(2, 3)    = localToWorld(2, 3);
    }
}