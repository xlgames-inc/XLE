//
//  XCTestAdapter.h
//  UnitTests
//
//  Created by David Jewsbury on 7/29/19.
//

#pragma once

#define XC_TEST_ADAPTER

#define TEST_CLASS(X) class X
#define TEST_METHOD(X) void X(XCTestCase* self)
#define TEST_CLASS_INITIALIZE(X) void Init_##X()
#define TEST_CLASS_CLEANUP(X) void Cleanup_##X()

#define IsTrue(...) IsTrue_(self, __VA_ARGS__)
#define AreEqual(...) AreEqual_(self, __VA_ARGS__)
#define AreNotEqual(...) AreNotEqual_(self, __VA_ARGS__)

namespace Assert
{
    static void IsTrue_(XCTestCase* self, bool comparison)
    {
        XCTAssertTrue(comparison);
    }

    static void IsTrue_(XCTestCase* self, bool comparison, const wchar_t msg[])
    {
        XCTAssertTrue(comparison);
    }

    template<typename Type>
        static void AreEqual_(XCTestCase* self, Type lhs, Type rhs)
    {
        XCTAssertEqual(lhs, rhs);
    }

    template<typename Type>
        static void AreEqual_(XCTestCase* self, Type lhs, Type rhs, const wchar_t msg[])
    {
        XCTAssertEqual(lhs, rhs);
    }

    template<typename Type>
        static void AreEqual_(XCTestCase* self, Type lhs, Type rhs, Type tolerance)
    {
        XCTAssertEqualWithAccuracy(lhs, rhs, tolerance);
    }

    template<typename Type>
        static void AreEqual_(XCTestCase* self, Type lhs, Type rhs, Type tolerance, const wchar_t msg[])
    {
        XCTAssertEqualWithAccuracy(lhs, rhs, tolerance);
    }

    template<typename Type>
        static void AreNotEqual_(XCTestCase* self, Type lhs, Type rhs)
    {
        XCTAssertNotEqual(lhs, rhs);
    }

    template<typename Type>
        static void AreNotEqual_(XCTestCase* self, Type lhs, Type rhs, const wchar_t msg[])
    {
        XCTAssertNotEqual(lhs, rhs);
    }

    template<typename Type>
        static void AreNotEqual_(XCTestCase* self, Type lhs, Type rhs, Type tolerance)
    {
        XCTAssertNotEqualWithAccuracy(lhs, rhs, tolerance);
    }

    template<typename Type>
        static void AreNotEqual_(XCTestCase* self, Type lhs, Type rhs, Type tolerance, const wchar_t msg[])
    {
        XCTAssertNotEqualWithAccuracy(lhs, rhs, tolerance);
    }
}
