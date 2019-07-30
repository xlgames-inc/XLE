//
//  Wrapper_BasicMath.mm
//  Wrapper_BasicMath
//
//  Created by David Jewsbury on 7/29/19.
//

#import <XCTest/XCTest.h>
#include "XCTestAdapter.h"
#include "BasicMaths.cpp"

@interface UnitTests_BasicMaths : XCTestCase

@end

@implementation UnitTests_BasicMaths {
    std::unique_ptr<UnitTests::BasicMaths> _underlying;
}

- (void)setUp {
    _underlying = std::make_unique<UnitTests::BasicMaths>();
}

- (void)tearDown {
    _underlying.reset();
}

- (void)testMethod1 {
    _underlying->TestMethod1(self);
}

- (void)testMatrixAccumulationAndDecomposition {
    _underlying->MatrixAccumulationAndDecomposition(self);
}

- (void)testProjectionMath {
    _underlying->ProjectionMath(self);
}

@end
