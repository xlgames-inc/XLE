//
//  Wrapper_CoordinateSpaces.mm
//  XLEUnitTestsMac
//
//  Created by David Jewsbury on 7/30/19.
//

#import <XCTest/XCTest.h>
#include "XCTestAdapter.h"
#include "CoordinateSpaces.cpp"

@interface UnitTests_CoordinateSpaces : XCTestCase

@end

@implementation UnitTests_CoordinateSpaces {
    std::unique_ptr<UnitTests::CoordinateSpaces> _underlying;
}

- (void)setUp {
    _underlying = std::make_unique<UnitTests::CoordinateSpaces>( );
    _underlying->Init_Startup();
}

- (void)tearDown {
    _underlying->Cleanup_Shutdown();
    _underlying.reset();
}

- (void)testWindowCoordSpaceOrientation {
    _underlying->WindowCoordSpaceOrientation(self);
}

- (void)testScissorRect {
    _underlying->ScissorRect(self);
}

- (void)testWindowCoordSpaceWindingOrder {
    _underlying->WindowCoordSpaceWindingOrder(self);
}

- (void)testRenderCopyThenReadback {
    _underlying->RenderCopyThenReadback(self);
}

- (void)testRenderBltAndThenReadback {
    _underlying->RenderBltAndThenReadback(self);
}

@end

