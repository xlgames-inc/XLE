// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Utility/ParameterBox.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/Streams/StreamTypes.h"
#include "../Utility/FunctionUtils.h"
#include "../Math/Vector.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    static int foo(int x, int y, int z) { return x + y + z; }
    static int foo1(int x, int y, int z) { return x + y + z; }
    static float foo1(int x, int y, float z) { return x + y + z; }

    TEST_CLASS(Utilities)
    {
    public:
        TEST_METHOD(ParameterBoxTest)
        {
            ParameterBox test(
                {
                    std::make_pair("SomeParam", "1ul"),
                    std::make_pair("SomeParam1", ".4f"),
                    std::make_pair("SomeParam2", "344.f"),
                    std::make_pair("SomeParam3", ".56f"),
                    std::make_pair("VectorParam", "{4.5f, 7.5f, 9.5f}v"),
                    std::make_pair("ColorParam", "{.5f, .5f, .5f}c")
                });

            Assert::AreEqual(test.GetParameter<unsigned>("SomeParam").second, 1u, L"String parsing and constructor");
            Assert::AreEqual(test.GetParameter<float>("SomeParam1").second, .4f, L"String parsing and constructor");
            Assert::AreEqual(test.GetParameter<float>("SomeParam2").second, 344.f, 0.001f, L"String parsing and constructor");
            Assert::AreEqual(test.GetParameter<float>("SomeParam3").second, .56f, 0.001f, L"String parsing and constructor");

            test.SetParameter("AParam", false);
            test.SetParameter("AParam", 5);
            test.SetParameter("AParam", 5.f);
            test.SetParameter("AParam", 500.f);
            Assert::AreEqual(test.GetParameter<float>("AParam").second, 500.f, 0.001f, L"Changing parameter types");

            test.SetParameter("ShouldBeTrue", true);
            Assert::AreEqual(test.GetParameter<bool>("ShouldBeTrue").second, true, L"Store/retrieve boolean");

            std::vector<std::pair<const char*, std::string>> stringTable;
            test.BuildStringTable(stringTable);

            for (auto i=stringTable.begin(); i!=stringTable.end(); ++i) {
                XlOutputDebugString(
                    StringMeld<256>() << i->first << " = " << i->second << "\n");
            }

        }

        TEST_METHOD(ImpliedTypingTest)
        {
            auto t0 = ImpliedTyping::Parse<Float4>("{.5f, 10, true}");
            Assert::IsTrue(
                t0.first && Equivalent(t0.second, Float4(.5f, 10.f, 1.f, 1.f), 0.001f),
                L"Parse with array element cast");
            
            auto t1 = ImpliedTyping::Parse<Float4>("23");
            Assert::IsTrue(
                t1.first && Equivalent(t1.second, Float4(23.f, 0.f, 0.f, 1.f), 0.001f),
                L"Scalar to array cast");

            Assert::AreEqual(ImpliedTyping::Parse<signed>("true").second, 1, L"bool to signed cast");
            Assert::AreEqual(ImpliedTyping::Parse<signed>("{true, 60, 1.f}").second, 1, L"bool array to signed cast");
        }

        template<typename CharType>
            static void FillStream(StreamBuf<CharType>& stream)
            {
                stream.WriteChar((utf8)'B');
                stream.WriteChar((ucs2)L'D');
                stream.WriteString((const utf8*)"<<StringB>>");
                stream.WriteString((const ucs2*)L"<<StringD>>");
            }

        TEST_METHOD(MemoryStreamTest)
        {
            auto memStreamA = std::make_unique<MemoryOutputStream<char>>();
            auto memStreamB = std::make_unique<MemoryOutputStream<wchar_t>>();
            auto memStreamC = std::make_unique<MemoryOutputStream<utf8>>();
            auto memStreamD = std::make_unique<MemoryOutputStream<ucs2>>();
            auto memStreamE = std::make_unique<MemoryOutputStream<ucs4>>();
            FillStream(*memStreamA);
            FillStream(*memStreamB);
            FillStream(*memStreamC);
            FillStream(*memStreamD);
            FillStream(*memStreamE);

            auto stringA = memStreamA->AsString();
            auto stringB = memStreamB->AsString();
            auto stringC = memStreamC->AsString();
            auto stringD = memStreamD->AsString();
            auto stringE = memStreamE->AsString();

            Assert::AreEqual(stringA.c_str(), "BD<<StringB>><<StringD>>");
            Assert::AreEqual((wchar_t*)stringB.c_str(), L"BD<<StringB>><<StringD>>");
            Assert::AreEqual((char*)stringC.c_str(), "BD<<StringB>><<StringD>>");
            Assert::AreEqual((wchar_t*)stringD.c_str(), L"BD<<StringB>><<StringD>>");
        }
            
        TEST_METHOD(MakeFunctionTest)
        {
            using namespace std::placeholders;
             
                // unambuiguous
            auto f0 = MakeFunction(foo);
            auto f1 = MakeFunction([](int x, int y, int z) { return x + y + z;});
            Assert::AreEqual(MakeFunction([](int x, int y, int z) { return x + y + z;})(1,2,3), 6);
                
            int first = 4;
            auto lambda_state = [=](int y, int z) { return first + y + z;}; //lambda with states
            Assert::AreEqual(MakeFunction(lambda_state)(1,2), 7);
                
                // ambuiguous cases
            auto f2 = MakeFunction<int,int,int,int>(std::bind(foo,_1,_2,_3)); //bind results has multiple operator() overloads
            Assert::AreEqual(f2(1,2,3), 6);
            auto f3 = MakeFunction<int,int,int,int>(foo1);     //overload1
            auto f4 = MakeFunction<float,int,int,float>(foo1); //overload2

            Assert::AreEqual(f3(1,2,3), 6);
            Assert::AreEqual(f4(1,2,3.5f), 6.5f, 0.001f);
        }

        TEST_METHOD(VariationFunctionsTest)
        {
            using namespace std::placeholders;

            VariantFunctions fns;
            fns.Store(0, [](int x, int y) { return x+y; });
            Assert::AreEqual(fns.Call<int>(0, 10, 20), 30);

            auto bindFn = MakeFunction<int, int>(
                std::bind(
                    [](int x, int y) { return x+y; },
                    _1, 20));
            fns.Store(1, std::move(bindFn));
            Assert::AreEqual(fns.Call<int>(1, 10), 30);
            Assert::AreEqual(fns.Get<int(int)>(1)(10), 30);

                // attempting to call functions that don't exist
            Assert::AreEqual(fns.CallDefault<int>(3, 10), 10);
            int res = 0;
            Assert::IsFalse(fns.TryCall<int>(res, 3));

            Assert::IsTrue(fns.Has<int(int)>(1));
            Assert::IsFalse(fns.Has<int(int)>(2));

            bool hitException = false;
            TRY { fns.Has<int(int, int)>(1); }
            CATCH(const VariantFunctions::SignatureMismatch&) { hitException = true; }
            CATCH_END

            Assert::IsTrue(hitException);

                // heavy load test (will crash if there are any failures)
            for (auto i=0u; i<100; ++i)
                fns.Store(100+i, [](int x, int y) { return x+y; });
        }
    };
}

