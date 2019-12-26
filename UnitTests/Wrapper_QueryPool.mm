//
//  Wrapper_QueryPool.mm
//  XLEUnitTestsMac
//
//  Created by David Jewsbury on 7/30/19.
//

#import <XCTest/XCTest.h>
#include "XCTestAdapter.h"
#include "QueryPool.cpp"

@interface UnitTests_QueryPool : XCTestCase

@end

@implementation UnitTests_QueryPool {
    std::unique_ptr<UnitTests::QueryPool> _underlying;
}

- (void)setUp {
    _underlying = std::make_unique<UnitTests::QueryPool>();
    _underlying->Init_Startup();
}

- (void)tearDown {
    _underlying->Cleanup_Shutdown();
    _underlying.reset();
}

- (void)testQueryPool_TimeStamp {
    #if GFXAPI_TARGET != GFXAPI_APPLEMETAL && GFXAPI_TARGET != GFXAPI_OPENGLES
        _underlying->QueryPool_TimeStamp(self);
    #else
        XCTFail(@"Not supported or failing on this GFX API");
    #endif
}

- (void)testQueryPool_SyncEventSet {
    _underlying->QueryPool_SyncEventSet(self);
}

@end
