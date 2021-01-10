// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Math/Vector.h"
#include "../Math/MathSerialization.h"
#include "../Utility/Conversion.h"
#include "../Utility/ImpliedTyping.h"
#include "../Utility/Streams/SerializationUtils.h"
#include <string>
#include <sstream>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace Catch::literals;

namespace UnitTests
{

    TEST_CASE( "ConversionPatterns-ImpliedTyping", "[utility]" )
    {
        // Conversion from string into basic value types via the ImpliedTyping system
        REQUIRE( ImpliedTyping::ParseFullMatch<unsigned>("123u") == 123u );

        // Conversion into strings from basic value types via the ImpliedTyping system
        REQUIRE( ImpliedTyping::AsString(UInt3(1, 2, 3)) == "{1, 2, 3}v" );
    }

    struct TestClass
    {
        int _c = 1;
        UInt2 _c2 = {2, 3};
    };

    void SerializationOperator(std::ostream& str, const TestClass& cls)
    {
        str << cls._c << ", ";
        str << cls._c2;
    }

    void DeserializationOperator(std::istream& str, TestClass& cls)
    {
        str >> cls._c >> cls._c2[0] >> cls._c2[1];
    }

    TEST_CASE( "ConversionPatterns-SerializationOperator", "[utility]" )
    {
        // Above we've implemented SerializationOperator and DeserializationOperator for
        // a couple of types. Typically we don't call these implementation directly -- instead
        // we access them via some more broad pattern, such as operator<< or operator>>
        //
        // Here we'll use some string streams to execute the declared serialization/deserialization
        // operators
        std::stringstream str;
        str << TestClass{};
        REQUIRE( str.str() == "1, 2 3");

        std::istringstream istr("1 2 3");
        TestClass deserialized;
        istr >> deserialized;
        REQUIRE( deserialized._c == 1 );
        REQUIRE( deserialized._c2 == UInt2{2, 3} );
    }

    //
    // StreamOperator
    // Serialize() Deserialize
    // Conversion::Convert<>
    // ParseInteger<>
    // floating point number parser
    // .As() method in SteamDOM classes
    //
    // object -> ParmeterBox
    // ParameterBox -> object
    //
    // object -> StreamDOM
    // StreamDOM -> object
    //
    // NascentBlockSerializer
    // OutputStreamFormatter
    // "Data" class
    // 
    // input formatter for json/other formats
    // walking through the input stream formatter vs via StreamDOM
    // InputStream & OutputStream types
    //

}
