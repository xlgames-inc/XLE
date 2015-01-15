HALF-PRECISION FLOATING POINT LIBRARY (Version 1.9.2)
-----------------------------------------------------

This is a C++ header-only library to provide an IEEE 754 conformant 16-bit 
half-precision floating point type along with corresponding arithmetic 
operators, type conversions and common mathematical functions. It aims for both 
efficiency and ease of use, trying to accurately mimic the behaviour of the 
builtin floating point types at the best performance possible.


INSTALLATION AND REQUIREMENTS
-----------------------------

Comfortably enough, the library consists of just a single header file 
containing all the functionality, which can be directly included by your 
projects, without the neccessity to build anything or link to anything.

The library needs an IEEE-754-conformant single-precision 'float' type, but 
this should be the case on most modern platforms. Whereas the library is fully 
C++98-compatible, it can profit from certain C++11 features. Support for those 
features is checked automatically at compile (or rather preprocessing) time, 
but can be explicitly enabled or disabled by defining the corresponding 
preprocessor symbols to either 1 or 0 yourself. This is useful when the 
automatic detection fails (for more exotic implementations) or when a feature 
should be explicitly disabled:

  - 'long long' integer type for mathematical functions returning 'long long' 
    results (enabled for VC++ 2003 and newer, gcc and clang, overridable with 
    'HALF_ENABLE_CPP11_LONG_LONG').

  - Static assertions for extended compile-time checks (enabled for VC++ 2010, 
    gcc 4.3, clang 2.9 and newer, overridable with 'HALF_ENABLE_CPP11_STATIC_ASSERT').

  - Generalized constant expressions (enabled for gcc 4.6, clang 3.1 and newer, 
    overridable with 'HALF_ENABLE_CPP11_CONSTEXPR').

  - noexcept exception specifications (enabled for gcc 4.6, clang 3.0 and newer, 
    overridable with 'HALF_ENABLE_CPP11_NOEXCEPT').

  - User-defined literals for half-precision literals to work (enabled for 
    gcc 4.7, clang 3.1 and newer, overridable with 'HALF_ENABLE_CPP11_USER_LITERALS').

  - Special integer types from <cstdint> (enabled for VC++ 2010, libstdc++ 4.3, 
    libc++ and newer, overridable with 'HALF_ENABLE_CPP11_CSTDINT').

  - Certain C++11 single-precision mathematical functions from <cmath> for 
    their half-precision counterparts to work (enabled for VC++ 2013, 
    libstdc++ 4.3, libc++ and newer, overridable with 'HALF_ENABLE_CPP11_CMATH').

  - Hash functor 'std::hash' from <functional> (enabled for VC++ 2010, 
    libstdc++ 4.3, libc++ and newer, overridable with 'HALF_ENABLE_CPP11_HASH').

The library has been tested successfully with Visual C++ 2005-2013, gcc 4.4-4.8 
and clang 3.1. Please contact me if you have any problems, suggestions or even 
just success testing it on other platforms.


DOCUMENTATION
-------------

Here follow some general words about the usage of the library and its 
implementation. For a complete documentation of its iterface look at the 
corresponding website http://half.sourceforge.net. You may also generate the 
complete developer documentation from the library's only include file's doxygen 
comments, but this is more relevant to developers rather than mere users (for 
reasons described below).

BASIC USAGE

To make use of the library just include its only header file half.hpp, which 
defines all half-precision functionality inside the 'half_float' namespace. The 
actual 16-bit half-precision data type is represented by the 'half' type. This 
type behaves like the builtin floating point types as much as possible, 
supporting the usual arithmetic, comparison and streaming operators, which 
makes its use pretty straight-forward:

    using half_float::half;
    half a(3.4), b(5);
    half c = a * b;
    c += 3;
    if(c > a)
	    std::cout << c << std::endl;

Additionally the 'half_float' namespace also defines half-precision versions 
for all mathematical functions of the C++ standard library, which can be used 
directly through ADL:

    half a(-3.14159);
    half s = sin(abs(a));
    long l = lround(s);

Furthermore the library provides proper specializations for 
'std::numeric_limits', defining various implementation properties, and 
'std::hash' for hashing half-precision numbers (assuming support for C++11 
'std::hash'). Similar to the corresponding preprocessor symbols from <cmath> 
the library also defines the 'HUGE_VALH' constant and maybe the 'FP_FAST_FMAH' 
symbol.

CONVERSIONS

The half is explicitly constructible/convertible from a single-precision float 
argument. Thus it is also explicitly constructible/convertible from any type 
implicitly convertible to float, but constructing it from types like double or 
int will involve the usual warnings arising when implicitly converting those to 
float because of the lost precision. On the one hand those warnings are 
intentional, because converting those types to half neccessarily also reduces 
precision. But on the other hand they are raised for explicit conversions from 
those types, when the user knows what he is doing. So if those warnings keep 
bugging you, then you won't get around first explicitly converting to float 
before converting to half, or use the 'half_cast' described below. In addition 
you can also directly assign float values to halfs.

In contrast to the float-to-half conversion, which reduces precision, the 
conversion from half to float (and thus to any other type implicitly 
convertible to float) is implicit, because all values represetable with 
half-precision are also representable with single-precision. This way the 
half-to-float conversion behaves similar to the builtin float-to-double 
conversion and all arithmetic expressions involving both half-precision and 
single-precision arguments will be of single-precision type. This way you can 
also directly use the mathematical functions of the C++ standard library, 
though in this case you will invoke the single-precision versions which will 
also return single-precision values, which is (even if maybe performing the 
exact same computation, see below) not as conceptually clean when working in a 
half-precision environment.

For performance reasons the conversion from float to half uses truncation 
(round toward zero, but mapping overflows to infinity) for rounding values not 
representable exactly in half-precision. If you are in need for other rounding 
behaviour (though this should rarely be the case), you can use the 'half_cast'. 
In addition to performning an explicit cast between half and any other type 
convertible to/from float via an explicit cast to/from float (and thus without 
any warnings due to possible precision-loss), it let's you explicitly specify 
the rounding mode to use for the float-to-half conversion. You can even 
synchronize it with the bultin single-precision implementation's rounding mode:

	half a = half_float::half_cast<half,std::numeric_limits<float>::round_style>(4.2);

You may also specify explicit half-precision literals, since the library 
provides a user-defined literal inside the 'half_float::literal' namespace, 
which you just need to import (assuming support for C++11 user-defined literals):

    using namespace half_float::literal;
    half x = 1.0_h;

IMPLEMENTATION

For performance reasons (and ease of implementation) many of the mathematical 
functions provided by the library as well as all arithmetic operations are 
actually carried out in single-precision under the hood, calling to the C++ 
standard library implementations of those functions whenever appropriate, 
meaning the arguments are converted to floats and the result back to half. But 
to reduce the conversion overhead as much as possible any temporary values 
inside of lengthy expressions are kept in single-precision as long as possible, 
while still maintaining a strong half-precision type to the outside world. Only 
when finally assigning the value to a half or calling a function that works 
directly on halfs is the actual conversion done (or never, when further 
converting the result to float.

This approach has two implications. First of all you have to treat the 
library's documentation at http://half.sourceforge.net as a simplified version, 
describing the behaviour of the library as if implemented this way. The actual 
argument and return types of functions and operators may involve other internal 
types (feel free to generate the exact developer documentation from the Doxygen 
comments in the library's header file if you really need to). But nevertheless 
the behaviour is exactly like specified in the documentation. The other 
implication is, that in the presence of rounding errors or over-/underflows 
arithmetic expressions may produce different results when compared to 
converting to half-precision after each individual operation:

    half a = (std::numeric_limits<half>::max() * 2.0_h) / 2.0_h; // a = MAX
    half b = std::numeric_limits<half>::max() * 2.0_h;           // b = INF
    b /= 2.0_h;                                                  // b stays INF

But this should only be a problem in very few cases. One last word has to be 
said when talking about performance. Even with its efforts in reducing 
conversion overhead as much as possible, the software half-precision 
implementation can most probably not beat the direct use of single-precision 
computations. Usually using actual float values for all computations and 
temproraries and using halfs only for storage is the recommended way. On the 
one hand this somehow makes the provided mathematical functions obsolete 
(especially in light of the implicit conversion from half to float), but 
nevertheless the goal of this library was to provide a complete and 
conceptually clean half-precision implementation, to which the standard 
mathematical functions belong, even if usually not needed.

IEEE CONFORMANCE

The half type uses the standard IEEE representation with 1 sign bit, 5 exponent 
bits and 10 mantissa bits (11 when counting the hidden bit). It supports all 
types of special values, like subnormal values, infinity and NaNs. But there 
are some limitations to the complete conformance to the IEEE 754 standard:

  - The implementation does not differentiate between signalling and quiet 
    NaNs, this means operations on halfs are not specified to trap on 
    signalling NaNs (though they may, see last point).

  - Though arithmetic operations are internally rounded to single-precision 
    using the underlying single-precision implementation's current rounding 
    mode, those values are then converted to half-precision using truncation 
    (round toward zero, but with overflows mapped to infinity). This is also 
    the reason why 'std::numeric_limits<half_float::half>::round_style' 
    actually returns 'std::round_indeterminate'.

  - Because of this truncation it may also be that certain single-precision 
    NaNs will be wrongly converted to half-precision infinity, though this is 
    very unlikely to happen, since most single-precision implementations don't 
    tend to only set the lowest bits of a NaN mantissa.

  - The implementation does not provide any floating point exceptions, thus 
    arithmetic operations or mathematical functions are not specified to invoke 
    proper floating point exceptions. But due to many functions implemented in 
    single-precision, those may still invoke floating point exceptions of the 
    underlying single-precision implementation.

Some of those points could have been circumvented by controlling the floating 
point environment using <cfenv> or implementing a similar exception mechanism. 
But this would have required excessive runtime checks giving two high an impact 
on performance for something that is rarely ever needed. If you really need to 
rely on proper floating point exceptions, it is recommended to explicitly 
perform computations using the builtin floating point types to be on the safe 
side. In the same way, if you really need to rely on a particular rounding 
behaviour, it is recommended to use single-precision computations and 
explicitly convert the result to half-precision using 'half_cast' and 
specifying the desired rounding mode. But those are really considered 
expert-scenarios rarely encountered in practice, since actually working with 
half-precision usually comes with a certain tolerance/ignorance of exactness 
considerations.


CREDITS AND CONTACT
-------------------

This library is developed by CHRISTIAN RAU and released under the MIT License 
(see LICENSE.txt). If you have any questions or problems with it, feel free to 
contact me at rauy@users.sourceforge.net.

Additional credit goes to JEROEN VAN DER ZIJP for his paper on "Fast Half Float 
Conversions", whose algorithms have been used in the library for converting 
between half-precision and single-precision values.
