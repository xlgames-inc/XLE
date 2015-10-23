// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace Operators
{
    /// <summary>Create a redirecting operators that don't polute the global scope</summary>
    /// Create a redirecting operator<< if a function called StreamOperator() exists.
    /// This uses SFINAE to reject cases where StreamOperator() doesn't exist for the
    /// given type.
    ///
    /// This is useful in cases where we don't want to declare a operator<< in a namespace.
    /// Declaring this operator can hide other global implementations of the same operator,
    /// (such as the defaults in std)
    ///
    /// But this redirecting operator allows us to put the operator declaration in a special
    /// "Operators" namespace, while the implementation (in StreamOperator) stays in the 
    /// normal namespace. When we "using namespace Operators", our custom operator<< will be
    /// mixed in with the global operators without hiding them.
    template<typename Object, typename Stream>
        decltype(StreamOperator(std::declval<Stream>(), std::declval<Object>()))
            operator<<(Stream& stream, const Object& obj)
                { return StreamOperator(stream, obj); }
}
