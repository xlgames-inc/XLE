//
//  AppleMetal_HookValidation.inc
//  RenderCore_AppleMetal
//
//  Created by Andrew Barnert on 10/23/19.
//

// NOTE: This code cannot be included in builds submitted to the
// App Store. So, to make sure you don't accidentally link it into
// deployment code, you have to #include it into the middle of a
// function, and also make sure to #include <dlfcn.h> at the top
// of the file. See GraphicsStartupStep.mm in dragons3d for an example.

// Code to hook the way Metal validation reports failures. By default,
// errors print an assertion (which does not go to our syslogs, and
// cannot be found anywhere at all if you're not running in Xcode or
// in the Terminal) and then abort; warnings do nothing; and I'm not
// sure what extendedChecks do because I can't trigger one. There are
// multiple (undocumented) ways to customize this; one way is to
// install a ReportFailureBlock, which will get called in place of the
// normal handling. We want to log rather than print on errors, log
// (but not abort) rather than do nothing on warnings, and... guess at
// the right thing to do on extendedChecks.

// For further details, see
// https://github.com/abarnert-pg/abarnert-pg.github.io/blob/master/_post/MetalValidationInternals.md

#if !DEVELOPMENT_BUILD
#error "Cannot use AppleMetal_HookValidation.inc in distribution builds"
#endif

{
    typedef void (^reportFailureBlock)(int64_t a, const char *func, int32_t line, NSString *msg);
    typedef void MTLSetReportFailureBlock_t(reportFailureBlock block);
    MTLSetReportFailureBlock_t *_MTLSetReportFailureBlock = (MTLSetReportFailureBlock_t *)dlsym(RTLD_SELF, "MTLSetReportFailureBlock");
    if (!_MTLSetReportFailureBlock) {
        NSLog(@"Failed to find MTLSetReportFailureBlock; Apple Metal validation will not be hooked: %s (%d)", strerror(errno), errno);
        return;
    }
    _MTLSetReportFailureBlock(^(int64_t type, const char *func, int32_t line, NSString *msg) {
        const char *typeName;
        bool doAbort=true;
        switch (type) {
            case 1: // "error"
                typeName="assertion"; break;
            case 2: // "warning"
                typeName="warning"; doAbort=false; break;
            case 3: // "extendedCheck", probably
                typeName="extendedCheck"; break;
            default:
                NSLog(@"Apple Metal validation unknown failure type %lld", type);
                typeName="assertion"; break;
        }
        NSLog(@"%s:%d: failed %s `%@'", func, line, typeName, msg);
        if (doAbort) {
            abort();
        }
        // Note that returning from here skips the normal validation handler.
        // Which probably means you get a whole slew of further assertions,
        // and then again on the next frame, and the next.
    });
}
