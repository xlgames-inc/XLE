//
//  Wrapper_CBLayoutTests.mm
//  Wrapper_CBLayoutTests
//
//  Created by David Jewsbury on 7/29/19.
//

#import <XCTest/XCTest.h>
#include "XCTestAdapter.h"
#include "CBLayoutTests.cpp"

@interface UnitTests_CBLayoutTests : XCTestCase

@end

@implementation UnitTests_CBLayoutTests {
    std::unique_ptr<UnitTests::PredefinedCBLayout> _underlying;
}

- (void)setUp {
    _underlying = std::make_unique<UnitTests::PredefinedCBLayout>();
}

- (void)tearDown {
    _underlying.reset();
}

- (void)testOptimizeElementOrder {
    _underlying->OptimizeElementOrder(self);
}

@end
