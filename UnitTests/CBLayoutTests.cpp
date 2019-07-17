// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../RenderCore/Assets/PredefinedCBLayout.h"
#include "../Math/Vector.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	using NameAndType = RenderCore::Assets::PredefinedCBLayout::NameAndType;

	static const std::vector<NameAndType> s_poorlyOrdered {
		NameAndType { "f_a", ImpliedTyping::TypeOf<float>() },
		NameAndType { "f3_a", ImpliedTyping::TypeOf<Float3>() },
		NameAndType { "f3_b", ImpliedTyping::TypeOf<Float3>() },
		NameAndType { "f2_a", ImpliedTyping::TypeOf<Float2>() },
		NameAndType { "f4_a", ImpliedTyping::TypeOf<Float4>() },
		NameAndType { "f_b", ImpliedTyping::TypeOf<float>() },
		NameAndType { "f2_b", ImpliedTyping::TypeOf<Float2>() },		
		NameAndType { "f4_b", ImpliedTyping::TypeOf<Float4>() },		
		NameAndType { "f_c", ImpliedTyping::TypeOf<float>() },
		NameAndType { "f_d", ImpliedTyping::TypeOf<float>() }
	};

	static const std::vector<NameAndType> s_wellOrdered {
		NameAndType { "f4_a", ImpliedTyping::TypeOf<Float4>() },
		NameAndType { "f4_b", ImpliedTyping::TypeOf<Float4>() },

		NameAndType { "f3_a", ImpliedTyping::TypeOf<Float3>() },
		NameAndType { "f_a", ImpliedTyping::TypeOf<float>() },
		
		NameAndType { "f3_b", ImpliedTyping::TypeOf<Float3>() },
		NameAndType { "f_b", ImpliedTyping::TypeOf<float>() },
		
		NameAndType { "f2_a", ImpliedTyping::TypeOf<Float2>() },		
		NameAndType { "f2_b", ImpliedTyping::TypeOf<Float2>() },
		
		NameAndType { "f_c", ImpliedTyping::TypeOf<float>() },
		NameAndType { "f_d", ImpliedTyping::TypeOf<float>() }
	};

    TEST_CLASS(PredefinedCBLayout)
	{
	public:
		TEST_METHOD(OptimizeElementOrder)
		{
            RenderCore::Assets::PredefinedCBLayout poorlyOrdered(
				MakeIteratorRange(s_poorlyOrdered));
			RenderCore::Assets::PredefinedCBLayout wellOrdered(
				MakeIteratorRange(s_wellOrdered));

			auto reorderedPoorEle = s_poorlyOrdered;
			RenderCore::Assets::PredefinedCBLayout::OptimizeElementOrder(MakeIteratorRange(reorderedPoorEle));

			auto reorderedWellEle = s_wellOrdered;
			RenderCore::Assets::PredefinedCBLayout::OptimizeElementOrder(MakeIteratorRange(reorderedWellEle));

			RenderCore::Assets::PredefinedCBLayout reorderedPoor(
				MakeIteratorRange(reorderedPoorEle));
			RenderCore::Assets::PredefinedCBLayout reorderedWell(
				MakeIteratorRange(reorderedWellEle));

			Assert::AreEqual(wellOrdered._cbSize, reorderedWell._cbSize);
			Assert::AreEqual(wellOrdered._cbSize, reorderedPoor._cbSize);
			Assert::IsTrue(poorlyOrdered._cbSize > wellOrdered._cbSize);
			Assert::AreEqual(reorderedWell.CalculateHash(), reorderedPoor.CalculateHash());
        }
    };
}

