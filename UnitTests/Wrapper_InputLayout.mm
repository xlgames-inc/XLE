//
//  Wrapper_InputLayout.mm
//  XLEUnitTestsMac
//
//  Created by David Jewsbury on 7/30/19.
//

#import <XCTest/XCTest.h>
#include "XCTestAdapter.h"
#include "InputLayout.cpp"

@interface UnitTests_InputLayout : XCTestCase

@end

@implementation UnitTests_InputLayout {
    std::unique_ptr<UnitTests::InputLayout> _underlying;
}

- (void)setUp {
    _underlying = std::make_unique<UnitTests::InputLayout>( );
    _underlying->Init_Startup();
}

- (void)tearDown {
    _underlying->Cleanup_Shutdown();
    _underlying.reset();
}

- (void)testBasicBinding {
    _underlying->BasicBinding(self);
}

@end
