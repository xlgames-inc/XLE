//
//  Wrapper_BasicMath.mm
//  Wrapper_BasicMath
//
//  Created by David Jewsbury on 7/29/19.
//

#import <XCTest/XCTest.h>
#include "XCTestAdapter.h"
#include "MSLSpecific.cpp"

@interface UnitTests_MSLSpecific : XCTestCase

@end

@implementation UnitTests_MSLSpecific {
    std::unique_ptr<UnitTests::MSLSpecific> _underlying;
}

- (void)setUp {
    _underlying = std::make_unique<UnitTests::MSLSpecific>();
    _underlying->Init_Startup();
}

- (void)tearDown {
    _underlying->Cleanup_Shutdown();
    _underlying.reset();
}

- (void)testComplicatedBinding {
    _underlying->ComplicatedBinding(self);
}

@end
