// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/Streams/StreamTypes.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/FunctionUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Math/Vector.h"
#include <CppUnitTest.h>
#include <stdexcept>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    static int foo(int x, int y, int z) { return x + y + z; }
    static int foo1(int x, int y, int z) { return x + y + z; }
    static float foo1(int x, int y, float z) { return x + y + z; }

    class ThrowOnDestructor
    {
    public:
        static bool s_expectingDestroy;
        static unsigned s_destroyCount;
        ~ThrowOnDestructor() { ++s_destroyCount; if (!s_expectingDestroy) Throw(std::runtime_error("Object was destroyed at unexpected time")); }
    };

    bool ThrowOnDestructor::s_expectingDestroy = false;
    unsigned ThrowOnDestructor::s_destroyCount = 0;

    TEST_CLASS(Utilities)
    {
    public:
        TEST_METHOD(ParameterBoxTest)
        {
            ParameterBox test(
                {
                    std::make_pair((const utf8*)"SomeParam", "1u"),
                    std::make_pair((const utf8*)"SomeParam1", ".4f"),
                    std::make_pair((const utf8*)"SomeParam2", "344.f"),
                    std::make_pair((const utf8*)"SomeParam3", ".56f"),
                    std::make_pair((const utf8*)"VectorParam", "{4.5f, 7.5f, 9.5f}v"),
                    std::make_pair((const utf8*)"ColorParam", "{.5f, .5f, .5f}c")
                });

            Assert::AreEqual(test.GetParameter<unsigned>((const utf8*)"SomeParam").second, 1u, L"String parsing and constructor");
            Assert::AreEqual(test.GetParameter<float>((const utf8*)"SomeParam1").second, .4f, L"String parsing and constructor");
            Assert::AreEqual(test.GetParameter<float>((const utf8*)"SomeParam2").second, 344.f, 0.001f, L"String parsing and constructor");
            Assert::AreEqual(test.GetParameter<float>((const utf8*)"SomeParam3").second, .56f, 0.001f, L"String parsing and constructor");

            test.SetParameter((const utf8*)"AParam", false);
            test.SetParameter((const utf8*)"AParam", 5);
            test.SetParameter((const utf8*)"AParam", 5.f);
            test.SetParameter((const utf8*)"AParam", 500.f);
            Assert::AreEqual(test.GetParameter<float>((const utf8*)"AParam").second, 500.f, 0.001f, L"Changing parameter types");

            test.SetParameter((const utf8*)"ShouldBeTrue", true);
            Assert::AreEqual(test.GetParameter<bool>((const utf8*)"ShouldBeTrue").second, true, L"Store/retrieve boolean");

            std::vector<std::pair<const utf8*, std::string>> stringTable;
            BuildStringTable(stringTable, test);

            // for (auto i=stringTable.begin(); i!=stringTable.end(); ++i) {
            //     XlOutputDebugString(
            //         StringMeld<256>() << i->first << " = " << i->second << "\n");
            // }

        }

        TEST_METHOD(ImpliedTypingTest)
        {
            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

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
                stream.Write((const utf8*)"<<StringB>>");
                stream.Write((const ucs2*)L"<<StringD>>");
            }

        TEST_METHOD(MemoryStreamTest)
        {
            auto memStreamA = std::make_unique<MemoryOutputStream<char>>();
            auto memStreamB = std::make_unique<MemoryOutputStream<utf16>>();
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
            Assert::AreEqual((const wchar_t*)stringB.c_str(), (const wchar_t*)u"BD<<StringB>><<StringD>>");
            Assert::AreEqual((const char*)stringC.c_str(), (const char*)u8"BD<<StringB>><<StringD>>");
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

            fns.Add(0, foo);
            Assert::AreEqual(fns.Call<int>(0, 10, 20, 30), 60);
            fns.Remove(0);

            fns.Add(0, [](int x, int y) { return x+y; });
            Assert::AreEqual(fns.Call<int>(0, 10, 20), 30);

            {
                    // test holding a reference along with the function ptr
                {
                    auto obj = std::make_shared<ThrowOnDestructor>();
                    fns.Add(1000, [obj]() { return obj;});
                }

                auto ptr = fns.Call<std::shared_ptr<ThrowOnDestructor>>(1000);
                ptr.reset();

                    // the actual object should only be destroyed during
                    // this "Remove" call
                ThrowOnDestructor::s_expectingDestroy = true;
                fns.Remove(1000);
                ThrowOnDestructor::s_expectingDestroy = false;
            
                Assert::AreEqual(ThrowOnDestructor::s_destroyCount, 1u);
            }

            auto bindFn = MakeFunction<int, int>(
                std::bind(
                    [](int x, int y) { return x+y; },
                    _1, 20));
            fns.Add(1, std::move(bindFn));
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
                fns.Add(100+i, [](int x, int y) { return x+y; });
        }

        TEST_METHOD(MakeRelativePathTest)
		{
			Assert::AreEqual(
				std::string("somedir/source/sourcefile.cpp"),
				MakeRelativePath(
                    SplitPath<char>("D:\\LM\\Code"), 
                    SplitPath<char>("D:\\LM\\Code\\SomeDir\\Source\\SourceFile.cpp")));

			Assert::AreEqual(
				std::string("d:/lm/.source/sourcefile.cpp"),
				SplitPath<char>("D:\\LM\\Code\\.././\\SomeDir\\..\\.Source/////\\SourceFile.cpp").Simplify().Rebuild());

			Assert::AreEqual(
				std::string("d:/lm/somedir/"),
				SplitPath<char>("D:\\LM\\Code../..\\SomeDir/").Simplify().Rebuild());

			Assert::AreEqual(
				std::string("someobject"),
				MakeRelativePath(SplitPath<char>("D:\\LM\\Code"), SplitPath<char>("D:\\LM\\Code\\SomeObject")));

			Assert::AreEqual(
				std::string("someobject/"),
				MakeRelativePath(SplitPath<char>("D:\\LM\\Code"), SplitPath<char>("D:\\LM\\Code\\SomeObject\\")));

			Assert::AreEqual(
				std::string("../../somedir/source/sourcefile.cpp"),
				MakeRelativePath(SplitPath<char>("D:\\LM\\Code\\SomeOtherDirectory\\Another\\"), SplitPath<char>("D:\\LM\\Code\\SomeDir\\Source\\SourceFile.cpp")));

			Assert::AreEqual(
				std::string("../../code/somedir/source/sourcefile.cpp"),
				MakeRelativePath(SplitPath<char>("D:\\./LM\\\\Code\\..\\SomeOtherDirectory\\/\\Another\\"), SplitPath<char>("D:\\LM\\Code\\SomeDir\\Source\\SourceFile.cpp")));

			Assert::AreEqual(
				std::string("source/sourcefile.cpp"),
				MakeRelativePath(SplitPath<char>("D:\\LM\\Code\\SomeOtherDirectory\\Another\\../.."), SplitPath<char>("D:\\LM\\Code\\SomeDir\\../.\\Source\\./SourceFile.cpp")));
		}

        TEST_METHOD(MiscHashTest)
        {
            StringSection<> s0("somestring"), s1("1234567890qwerty");
            Assert::AreEqual(
                ConstHash64<'some', 'stri', 'ng'>::Value,
                ConstHash64FromString(s0.begin(), s0.end()));
            Assert::AreEqual(
                ConstHash64<'1234', '5678', '90qw', 'erty'>::Value,
                ConstHash64FromString(s1.begin(), s1.end()));
        }
    };
}

