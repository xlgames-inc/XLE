//
//  Wrapper_ResourceUpdateAndReadBack.mm
//  XLEUnitTestsMac
//
//  Created by David Jewsbury on 7/30/19.
//

#define GL_SILENCE_DEPRECATION

#import <XCTest/XCTest.h>
#include "XCTestAdapter.h"
#include "ResourceUpdateAndReadBack.cpp"

@interface UnitTests_ResourceUpdateAndReadBack : XCTestCase

@end

@implementation UnitTests_ResourceUpdateAndReadBack {
    std::unique_ptr<UnitTests::ResourceUpdateAndReadBack> _underlying;
}

- (void)setUp {
    _underlying = std::make_unique<UnitTests::ResourceUpdateAndReadBack>();
    _underlying->Init_Startup();
}

- (void)tearDown {
    _underlying->Cleanup_Shutdown();
    _underlying.reset();
}

- (void)testUpdateConstantBuffer {
    #if GFXAPI_TARGET != GFXAPI_APPLEMETAL
        _underlying->UpdateConstantBuffer(self);
    #else
        XCTFail(@"Not supported or failing on this GFX API");
    #endif
}

@end
