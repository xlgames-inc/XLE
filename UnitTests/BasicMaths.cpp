// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Math/Transformations.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{		
	TEST_CLASS(BasicMaths)
	{
	public:
		
		TEST_METHOD(TestMethod1)
		{
			// Test some fundamental 3d geometry maths stuff
			// Using Combine_InPlace, produce 2 complex rotation
			// matrixes. Each should be the inverse of the other.

			const float tolerance = 1.e-5f; 
			
			{
				Float4x4 rotA = Identity<Float4x4>();
				Combine_InPlace(RotationX(.85f * gPI), rotA);
				Combine_InPlace(RotationY(-.35f * gPI), rotA);
				Combine_InPlace(RotationZ(.5f  * gPI), rotA);

				Float4x4 rotB = Identity<Float4x4>();
				Combine_InPlace(RotationZ(-.5f * gPI), rotB);
				Combine_InPlace(RotationY(.35f * gPI), rotB);
				Combine_InPlace(RotationX(-.85f * gPI), rotB);

				auto shouldBeIdentity = Combine(rotA, rotB);
				Assert::IsTrue(Equivalent(Identity<Float4x4>(), shouldBeIdentity, tolerance));
				
				auto invRotA = Inverse(rotA);
				auto invRotA2 = InvertOrthonormalTransform(rotA);
				Assert::IsTrue(Equivalent(rotB, invRotA, tolerance));
				Assert::IsTrue(Equivalent(rotB, invRotA2, tolerance));
				
				Float3 starterVector(1.f, 2.f, 3.f);
				auto trans1 = TransformDirectionVector(rotA, starterVector);
				auto trans2 = TransformDirectionVector(rotB, trans1);
				Assert::IsTrue(Equivalent(trans2, starterVector, tolerance));

                auto trans1a = TransformPoint(rotA, starterVector);
				auto trans2a = TransformPointByOrthonormalInverse(rotA, trans1a);
                auto trans3a = TransformPoint(InvertOrthonormalTransform(rotA), trans1a);
				Assert::IsTrue(Equivalent(trans2a, starterVector, tolerance));
                Assert::IsTrue(Equivalent(trans3a, starterVector, tolerance));
			}

				// test different types of rotation construction
			{
				auto quat = MakeRotationQuaternion(Normalize(Float3(1.f, 2.f, 3.f)), .6f * gPI);
				auto rotMat = MakeRotationMatrix(Normalize(Float3(1.f, 2.f, 3.f)), .6f * gPI);

				Assert::IsTrue(Equivalent(AsFloat4x4(quat), AsFloat4x4(rotMat), tolerance));
			}

				// Test RotationScaleTranslation 
			{
				RotationScaleTranslation rst(
					MakeRotationQuaternion(Normalize(Float3(1.f, 2.f, 3.f)), .6f * gPI),
					Float3(4.5f, 5.f, -6.f), Float3(30.f, 5.f, -10.f));

				Float4x4 accumulativeMatrix = Identity<Float4x4>();
				Combine_InPlace(accumulativeMatrix, ArbitraryScale(Float3(4.5f, 5.f, -6.f)));
				accumulativeMatrix = Combine(accumulativeMatrix, AsFloat4x4(MakeRotationMatrix(Normalize(Float3(1.f, 2.f, 3.f)), .6f * gPI)));
				Combine_InPlace(accumulativeMatrix, Float3(30.f, 5.f, -10.f));
				
				auto rstMatrix = AsFloat4x4(rst);
				Assert::IsTrue(Equivalent(rstMatrix, accumulativeMatrix, tolerance));
			}

		}

	};
}