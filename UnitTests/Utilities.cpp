// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Utility/ParameterBox.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    TEST_CLASS(Utilities)
    {
    public:
        TEST_METHOD(ParameterBoxTest)
        {
            ParameterBox test(
                {
                    std::make_pair("SomeParam", "1ull"),
                    std::make_pair("SomeParam1", ".4f"),
                    std::make_pair("SomeParam2", "344.f"),
                    std::make_pair("SomeParam3", "344f") 
                });

            test.SetParameter("AParam", 5);
            test.SetParameter("AParam", 5.f);
            test.SetParameter("AParam", 500.f);
        }
    };
}

