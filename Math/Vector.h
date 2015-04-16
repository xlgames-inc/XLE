// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Math.h"

#if MATHLIBRARY_ACTIVE == MATHLIBRARY_CML
    #pragma warning(push)
        #pragma warning(disable:4512)       // assignment operator could not be generated
        #include <cml/vector.h>
    #pragma warning(pop)
#endif

namespace XLEMath
{
    #if MATHLIBRARY_ACTIVE == MATHLIBRARY_CML

        typedef cml::vector< float, cml::fixed<2> >     Float2;
        typedef cml::vector< float, cml::fixed<3> >     Float3;
        typedef cml::vector< float, cml::fixed<4> >     Float4;

        typedef cml::vector< int, cml::fixed<2> >       Int2;
        typedef cml::vector< int, cml::fixed<3> >       Int3;
        typedef cml::vector< int, cml::fixed<4> >       Int4;

        typedef cml::vector< unsigned, cml::fixed<2> >  UInt2;
        typedef cml::vector< unsigned, cml::fixed<3> >  UInt3;
        typedef cml::vector< unsigned, cml::fixed<4> >  UInt4;

        template <typename Type, size_t N>
            cml::vector<Type, cml::fixed<N+1>>      Expand(const cml::vector<Type, cml::fixed<N>>& input, Type extra);

        template <typename Type>
            cml::vector<Type, cml::fixed<4>>        Expand(const cml::vector<Type, cml::fixed<3>>& input, Type extra)
            {
                return cml::vector<Type, cml::fixed<4>>(input[0], input[1], input[2], extra);
            }

        template <typename Type>
            cml::vector<Type, cml::fixed<3>>        Expand(const cml::vector<Type, cml::fixed<2>>& input, Type extra)
            {
                return cml::vector<Type, cml::fixed<3>>(input[0], input[1], extra);
            }

        template <typename Type, size_t N>
            cml::vector<Type, cml::fixed<N-1>>      Truncate(const cml::vector<Type, cml::fixed<N>>& input);

        template <typename Type>
            cml::vector<Type, cml::fixed<3>>        Truncate(const cml::vector<Type, cml::fixed<4>>& input)
            {
                return cml::vector<Type, cml::fixed<3>>(input[0], input[1], input[2]);
            }

        template <typename Type>
            cml::vector<Type, cml::fixed<2>>        Truncate(const cml::vector<Type, cml::fixed<3>>& input)
            {
                return cml::vector<Type, cml::fixed<2>>(input[0], input[1]);
            }

                //      cml takes many different types of objects as input to length, length_squared, etc...
                //      (it's not just vector classes, but also vector expression classes, and maybe others)
                //      So, we have to make very general widely matching declarations for these functions

        template <typename Vector>
            inline auto Magnitude(const Vector& vector) -> decltype(cml::length(vector))
                { return cml::length(vector); }

        template <typename Vector>
            inline auto MagnitudeSquared(const Vector& vector) -> decltype(cml::length_squared(vector))
                { return cml::length_squared(vector); }

        template <typename Vector>
            inline auto Normalize(const Vector& vector) -> decltype(cml::normalize(vector))
                { return cml::normalize(vector); }

        template <typename LHSType, typename RHSType>
            inline auto Cross(const LHSType& lhs, const RHSType& rhs) -> decltype(cml::cross(lhs, rhs))
                { return cml::cross(lhs, rhs); }

        template <typename LHSType, typename RHSType>
            inline auto Dot(const LHSType& lhs, const RHSType& rhs) -> decltype(cml::dot(lhs, rhs)) 
                { return cml::dot(lhs, rhs); }

        template<typename BasicType, int Count>
            inline cml::vector<BasicType, cml::fixed<Count>>
                LinearInterpolate(cml::vector<BasicType, cml::fixed<Count>> lhs, cml::vector<BasicType, cml::fixed<Count>> rhs, BasicType alpha)
        {
            return (rhs - lhs) * alpha + lhs;
        }


		template<typename BasicType, int Count>
			inline bool Equivalent(
				cml::vector<BasicType, cml::fixed<Count>> lhs, 
				cml::vector<BasicType, cml::fixed<Count>> rhs, BasicType tolerance)
			{
				for (unsigned i=0; i<Count; ++i)
					if (!Equivalent(lhs[i], rhs[i], tolerance))
						return false;
				return true;
			}

    #endif

    template<> inline const Float3& Zero<Float3>()
    {
        const Float3::value_type zero = Float3::value_type(0);
        static Float3 result(zero, zero, zero);
        return result;
    }

    template<> inline const Float4& Zero<Float4>()
    {
        const Float4::value_type zero = Float4::value_type(0);
        static Float4 result(zero, zero, zero, zero);
        return result;
    }

}

