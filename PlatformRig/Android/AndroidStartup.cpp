// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


//
//      This is originally based on the "native-activity sample from NDK8e.
//      Just the basic fundamental structure for starting and managing the app on an Android device
//

#include "../../Core/Types.h"
#include <jni.h>
#include <errno.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>
#include <android/log.h>
#include "android_native_app_glue.h"

#define Information(...)    ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define Warning(...)        ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))

/**
 * Our saved state data.
 */
class SavedState 
{
public:
    float angle;
    int32 x;
    int32 y;
};

/**
 * Shared state for our app.
 */
class Engine 
{
public:
    Engine() {}
    ~Engine() {}

    int     InitDisplay();
    void    DrawFrame();
    void    TermDisplay();

    int32   HandleInput(AInputEvent* event);
    void    HandleCmd(int32_t cmd);

    struct android_app*         _app;
    ASensorManager*             _sensorManager;
    const ASensor*              _accelerometerSensor;
    ASensorEventQueue*          _sensorEventQueue;

    int             _animating;
    EGLDisplay      _display;
    EGLSurface      _surface;
    EGLContext      _context;
    int32_t         _width;
    int32_t         _height;
    SavedState      _state;
};

/**
 * Initialize an EGL context for the current display.
 */
int Engine::InitDisplay() 
{
    // initialize OpenGL ES and EGL

        /*
         * Here specify the attributes of the desired configuration.
         * Below, we select an EGLConfig with at least 8 bits per color
         * component compatible with on-screen windows
         */
    const EGLint attribs[] = 
    {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_NONE
    };
    EGLint w, h, dummy, format;
    EGLint numConfigs;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display, 0, 0);

    /* Here, the application chooses the configuration it desires. In this
     * sample, we have a very simplified selection process, where we pick
     * the first EGLConfig that matches our criteria */
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
     * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
     * As soon as we picked a EGLConfig, we can safely reconfigure the
     * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(_app->window, 0, 0, format);

    surface = eglCreateWindowSurface(display, config, _app->window, NULL);
    context = eglCreateContext(display, config, NULL, NULL);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        Warning("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    _display = display;
    _context = context;
    _surface = surface;
    _width = w;
    _height = h;
    _state.angle = 0;

    // Initialize GL state.
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_DEPTH_TEST);

    return 0;
}

/**
 * Just the current frame in the display.
 */
void Engine::DrawFrame() 
{
    if (_display == NULL) {
        // No display.
        return;
    }

    // Just fill the screen with a color.
    glClearColor(   ((float)_state.x)/_width, _state.angle,
                    ((float)_state.y)/_height, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    eglSwapBuffers(_display, _surface);
}

/**
 * Tear down the EGL context currently associated with the display.
 */
void Engine::TermDisplay() 
{
    if (_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (_context != EGL_NO_CONTEXT) {
            eglDestroyContext(_display, _context);
        }
        if (_surface != EGL_NO_SURFACE) {
            eglDestroySurface(_display, _surface);
        }
        eglTerminate(_display);
    }
    _animating = 0;
    _display = EGL_NO_DISPLAY;
    _context = EGL_NO_CONTEXT;
    _surface = EGL_NO_SURFACE;
}

/**
 * Process the next input event.
 */
int32 Engine::HandleInput(AInputEvent* event) 
{
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        _animating = 1;
        _state.x = AMotionEvent_getX(event, 0);
        _state.y = AMotionEvent_getY(event, 0);
        return 1;
    }
    return 0;
}

/**
 * Process the next main command.
 */
void Engine::HandleCmd(int32_t cmd) 
{
    switch (cmd) 
    {
    case APP_CMD_SAVE_STATE:
        // The system has asked us to save our current state.  Do so.
        _app->savedState = malloc(sizeof(struct SavedState));
        *((struct SavedState*)_app->savedState) = _state;
        _app->savedStateSize = sizeof(struct SavedState);
        break;

    case APP_CMD_INIT_WINDOW:
        // The window is being shown, get it ready.
        if (_app->window != NULL) {
            InitDisplay();
            DrawFrame();
        }
        break;

    case APP_CMD_TERM_WINDOW:
        // The window is being hidden or closed, clean it up.
        TermDisplay();
        break;

    case APP_CMD_GAINED_FOCUS:
        // When our app gains focus, we start monitoring the accelerometer.
        if (_accelerometerSensor != NULL) {
            ASensorEventQueue_enableSensor(
                _sensorEventQueue,
                _accelerometerSensor);
            // We'd like to get 60 events per second (in us).
            ASensorEventQueue_setEventRate(
                _sensorEventQueue,
                _accelerometerSensor, (1000L/60)*1000);
        }
        break;

    case APP_CMD_LOST_FOCUS:
        // When our app loses focus, we stop monitoring the accelerometer.
        // This is to avoid consuming battery while not being used.
        if (_accelerometerSensor != NULL) {
            ASensorEventQueue_disableSensor(
                _sensorEventQueue,
                _accelerometerSensor);
        }
        // Also stop animating.
        _animating = 0;
        DrawFrame();
        break;
    }
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) 
{
    Engine* engine = (Engine*)app->userData;
    engine->HandleCmd(cmd);
}

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event)
{
    Engine* engine = (Engine*)app->userData;
    return engine->HandleInput(event);
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* state) 
{
    Engine engine;

    // Make sure glue isn't stripped.
    // app_dummy();

    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    state->onAppCmd = &engine_handle_cmd;
    state->onInputEvent = &engine_handle_input;
    engine._app = state;

    // Prepare to monitor accelerometer
    engine._sensorManager = ASensorManager_getInstance();
    engine._accelerometerSensor = ASensorManager_getDefaultSensor(engine._sensorManager,
            ASENSOR_TYPE_ACCELEROMETER);
    engine._sensorEventQueue = ASensorManager_createEventQueue(engine._sensorManager,
            state->looper, LOOPER_ID_USER, NULL, NULL);

    if (state->savedState != NULL) {
        // We are starting with a previous saved state; restore from it.
        engine._state = *(struct SavedState*)state->savedState;
    }

    // loop waiting for stuff to do.

    while (1) {
        // Read all pending events.
        int ident;
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((ident=ALooper_pollAll(engine._animating ? 0 : -1, NULL, &events,
                (void**)&source)) >= 0) {

            // Process this event.
            if (source != NULL) {
                source->process(state, source);
            }

            // If a sensor has data, process it now.
            if (ident == LOOPER_ID_USER) {
                if (engine._accelerometerSensor != NULL) {
                    ASensorEvent event;
                    while (ASensorEventQueue_getEvents(engine._sensorEventQueue,
                            &event, 1) > 0) {
                        Information("accelerometer: x=%f y=%f z=%f",
                                event.acceleration.x, event.acceleration.y,
                                event.acceleration.z);
                    }
                }
            }

            // Check if we are exiting.
            if (state->destroyRequested != 0) {
                engine.TermDisplay();
                return;
            }
        }

        if (engine._animating) {
            // Done with events; draw next animation frame.
            engine._state.angle += .01f;
            if (engine._state.angle > 1) {
                engine._state.angle = 0;
            }

            // Drawing is throttled to the screen update rate, so there
            // is no need to do timing here.
            engine.DrawFrame();
        }
    }
}
//END_INCLUDE(all)

