//
//  Wrapper_InputLayout.mm
//  XLEUnitTestsMac
//
//  Created by David Jewsbury on 7/30/19.
//

#define GL_SILENCE_DEPRECATION

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

- (void)testBasicBinding_LongForm {
    _underlying->BasicBinding_LongForm(self);
}

- (void)testBasicBinding_ShortForm {
    _underlying->BasicBinding_ShortForm(self);
}

- (void)testBasicBinding_2VBs {
    _underlying->BasicBinding_2VBs(self);
}

- (void)testBasicBinding_DataRate {
    _underlying->BasicBinding_DataRate(self);
}

- (void)testBasicBinding_Uniforms {
    #if GFXAPI_TARGET != GFXAPI_OPENGLES
        _underlying->BasicBinding_Uniforms(self);
    #endif
}

- (void)testBasicBinding_TextureBinding {
    _underlying->BasicBinding_TextureBinding(self);
}

- (void)testBasicBinding_TextureSampling {
    _underlying->BasicBinding_TextureSampling(self);
}

//
// DavidJ -- note -- Apple Metal fails these test cases currently
//
- (void)testBindAttributeToGeneratorShader {
    #if GFXAPI_TARGET != GFXAPI_APPLEMETAL && GFXAPI_TARGET != GFXAPI_OPENGLES
        _underlying->BasicBinding_BindAttributeToGeneratorShader(self);
    #else
        XCTFail(@"Not supported or failing on this GFX API");
    #endif
}

- (void)testBasicBinding_BindMissingAttribute {
    #if GFXAPI_TARGET != GFXAPI_APPLEMETAL
        _underlying->BasicBinding_BindMissingAttribute(self);
    #else
        XCTFail(@"Not supported or failing on this GFX API");
    #endif
}

- (void)testBasicBinding_IncorrectUSI {
    _underlying->BasicBinding_IncorrectUSI(self);
}

- (void)testBasicBinding_IncorrectUniformsStreamShader {
    _underlying->BasicBinding_IncorrectUniformsStreamShader(self);
}

- (void)testBasicBinding_IncorrectUniformsStreamPipeline {
    _underlying->BasicBinding_IncorrectUniformsStreamPipeline(self);
}

@end

#include "../RenderCore/OpenGLES/Metal/GLWrappers.h"

glWrappers* GetGLWrappers()
{
    static glWrappers sharedWrappers { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    if (!sharedWrappers.TexImage2D) {
        sharedWrappers.TexImage2D = &glTexImage2D;
        sharedWrappers.TexStorage2D = &glTexStorage2D;
        sharedWrappers.CompressedTexImage2D = &glCompressedTexImage2D;
        sharedWrappers.RenderbufferStorage = &glRenderbufferStorage;
        sharedWrappers.BufferData = &glBufferData;
        sharedWrappers.DeleteBuffers = &glDeleteBuffers;
        sharedWrappers.DeleteRenderbuffers = &glDeleteRenderbuffers;
        sharedWrappers.DeleteTextures = &glDeleteTextures;
    }
    return &sharedWrappers;
}

