/**
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// OpenGL ES 2.0 code

#include <jni.h>
#include <sstream>
#include <iomanip>

#include <android/sensor.h>

#include <GLES2/gl2.h>

#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include "NDKHelper.h"

#include <cassert>

#define  LOG_TAG    "sensorgraph"
//#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

//const int LOOPER_ID_USER = 3;
const int SENSOR_HISTORY_LENGTH = 100;
const int SENSOR_REFRESH_RATE = 100;
const float SENSOR_FILTER_ALPHA = 0.1f;

//-------------------------------------------------------------------------
//Preprocessor
//-------------------------------------------------------------------------
#define HELPER_CLASS_NAME "com/sample/helper/NDKHelper" //Class name of helper function
//-------------------------------------------------------------------------
//Shared state for our app.
//-------------------------------------------------------------------------
struct android_app;

class SensorGraph {

    ndk_helper::GLContext *gl_context_;

    std::string vertexShaderSource;
    std::string fragmentShaderSource;

    android_app *app_;

//    ALooper *looper;
    ASensorManager *sensorManager;
    const ASensor *accelerometerSensor;
    ASensorEventQueue *accelerometerSensorEventQueue;

    GLuint shaderProgram;
    GLuint vPositionHandle;
    GLuint vSensorValueHandle;
    GLuint uFragColorHandle;
    GLfloat xPos[SENSOR_HISTORY_LENGTH];

    bool initialized_resources_;
    bool has_focus_;

    ndk_helper::DoubletapDetector doubletap_detector_;
    ndk_helper::PinchDetector pinch_detector_;
    ndk_helper::DragDetector drag_detector_;
    ndk_helper::PerfMonitor monitor_;

    struct AccelerometerData {
        GLfloat x;
        GLfloat y;
        GLfloat z;
    };
    AccelerometerData sensorData[SENSOR_HISTORY_LENGTH * 2];
    AccelerometerData sensorDataFilter;
    int sensorDataIndex;

public:
    //-------------------------------------------------------------------------
    //Ctor
    //-------------------------------------------------------------------------
    SensorGraph() : initialized_resources_(false),
                    has_focus_(false),
                    app_(NULL),
                    sensorManager(NULL),
                    accelerometerSensor(NULL),
                    accelerometerSensorEventQueue(NULL),
                    sensorDataIndex(0),
                    lasttime(0) {
        gl_context_ = ndk_helper::GLContext::GetInstance();
    }

    /**
     * Load resources
     */
    void LoadResources() {
        LOGI("LoadResources");
        init();
    }

    /**
     * Unload resources
     */
    void UnloadResources() {
        LOGI("UnloadResources");
//        renderer_.Unload();
    }

    AAssetManager* getAssetManager() {
        JNIEnv *jni;
        app_->activity->vm->AttachCurrentThread(&jni, NULL);
        jclass clazz = jni->GetObjectClass(app_->activity->clazz);
        LOGI("attached Thread for %p",clazz);
        jmethodID methodIDgAM = jni->GetMethodID(clazz, "getAssetManager", "()Landroid/content/res/AssetManager;");
        jobject assetManager = jni->CallObjectMethod(app_->activity->clazz, methodIDgAM);
        AAssetManager *localAssetManager = AAssetManager_fromJava(jni, assetManager);
        app_->activity->vm->DetachCurrentThread();
        LOGI("detached Thread for AM J:%p C:%p", assetManager, localAssetManager);
        return localAssetManager;
    }

    // take the AAssetManager from Java Env via JNI call
    void init() {
        AAssetManager *assetManager = getAssetManager();
        LOGI("init(", assetManager ,")");
        AAsset *vertexShaderAsset = AAssetManager_open(assetManager, "shader.glslv",
                                                       AASSET_MODE_BUFFER);
        assert(vertexShaderAsset != NULL);
        const void *vertexShaderBuf = AAsset_getBuffer(vertexShaderAsset);
        assert(vertexShaderBuf != NULL);
        off_t vertexShaderLength = AAsset_getLength(vertexShaderAsset);
        vertexShaderSource = std::string((const char *) vertexShaderBuf,
                                         (size_t) vertexShaderLength);
        AAsset_close(vertexShaderAsset);

        AAsset *fragmentShaderAsset = AAssetManager_open(assetManager, "shader.glslf",
                                                         AASSET_MODE_BUFFER);
        assert(fragmentShaderAsset != NULL);
        const void *fragmentShaderBuf = AAsset_getBuffer(fragmentShaderAsset);
        assert(fragmentShaderBuf != NULL);
        off_t fragmentShaderLength = AAsset_getLength(fragmentShaderAsset);
        fragmentShaderSource = std::string((const char *) fragmentShaderBuf,
                                           (size_t) fragmentShaderLength);
        AAsset_close(fragmentShaderAsset);

        InitSensors();
        //-------------------------------------------------------------------------
        //Sensor handlers
        //-------------------------------------------------------------------------

        generateXPos();
    }


    void InitSensors() {
        LOGI("InitSensors");
        sensorManager = ASensorManager_getInstance();
        assert(sensorManager != NULL);
        accelerometerSensor = ASensorManager_getDefaultSensor(sensorManager,
                                                              ASENSOR_TYPE_ACCELEROMETER);
        assert(accelerometerSensor != NULL);
//        looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
//        assert(looper != NULL);
        accelerometerSensorEventQueue = ASensorManager_createEventQueue(sensorManager, app_->looper,
                                                                        LOOPER_ID_USER, NULL, NULL);
        assert(accelerometerSensorEventQueue != NULL);
        int setEventRateResult = ASensorEventQueue_setEventRate(accelerometerSensorEventQueue,
                                                                accelerometerSensor,
                                                                int32_t(1000000 /
                                                                        SENSOR_REFRESH_RATE));
        int enableSensorResult = ASensorEventQueue_enableSensor(accelerometerSensorEventQueue,
                                                                accelerometerSensor);
        assert(enableSensorResult >= 0);
   }

    void ProcessSensors(int32_t id) {
        // If a sensor has data, process it now.
        if (id == LOOPER_ID_USER) {
            int i=5;
            if (accelerometerSensor != NULL) {
                update();
//                ASensorEvent event;
//                while (ASensorEventQueue_getEvents(accelerometerSensorEventQueue, &event, 1) > 0) {
//                    LOGI("accelerometer: x=%f y=%f z=%f",
//                         event.acceleration.x, event.acceleration.y,
//                         event.acceleration.z);
//                }
            }
        }
    }

    void SetState(android_app *state) {
        app_ = state;
        doubletap_detector_.SetConfiguration(app_->config);
        drag_detector_.SetConfiguration(app_->config);
        pinch_detector_.SetConfiguration(app_->config);
    }

    void surfaceCreated() {
        LOGI("GL_VERSION: %s", glGetString(GL_VERSION));
        LOGI("GL_VENDOR: %s", glGetString(GL_VENDOR));
        LOGI("GL_RENDERER: %s", glGetString(GL_RENDERER));
        LOGI("GL_EXTENSIONS: %s", glGetString(GL_EXTENSIONS));

        shaderProgram = createProgram(vertexShaderSource, fragmentShaderSource);
        assert(shaderProgram != 0);
        GLint getPositionLocationResult = glGetAttribLocation(shaderProgram, "vPosition");
        assert(getPositionLocationResult != -1);
        vPositionHandle = (GLuint) getPositionLocationResult;
        GLint getSensorValueLocationResult = glGetAttribLocation(shaderProgram, "vSensorValue");
        assert(getSensorValueLocationResult != -1);
        vSensorValueHandle = (GLuint) getSensorValueLocationResult;
        GLint getFragColorLocationResult = glGetUniformLocation(shaderProgram, "uFragColor");
        assert(getFragColorLocationResult != -1);
        uFragColorHandle = (GLuint) getFragColorLocationResult;
    }

    void surfaceChanged(int w, int h) {
        glViewport(0, 0, w, h);
    }

    void generateXPos() {
        for (int i = 0; i < SENSOR_HISTORY_LENGTH; i++) {
            float t = static_cast<float>(i) / static_cast<float>(SENSOR_HISTORY_LENGTH - 1);
            xPos[i] = -1.f * (1.f - t) + 1.f * t;
        }
    }

    GLuint createProgram(const std::string &pVertexSource, const std::string &pFragmentSource) {
        GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
        GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
        GLuint program = glCreateProgram();
        assert(program != 0);
        glAttachShader(program, vertexShader);
        glAttachShader(program, pixelShader);
        glLinkProgram(program);
        GLint programLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &programLinked);
        assert(programLinked != 0);
        glDeleteShader(vertexShader);
        glDeleteShader(pixelShader);
        return program;
    }

    GLuint loadShader(GLenum shaderType, const std::string &pSource) {
        GLuint shader = glCreateShader(shaderType);
        assert(shader != 0);
        const char *sourceBuf = pSource.c_str();
        glShaderSource(shader, 1, &sourceBuf, NULL);
        glCompileShader(shader);
        GLint shaderCompiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &shaderCompiled);
        assert(shaderCompiled != 0);
        return shader;
    }

    void update() {
        ALooper_pollAll(0, NULL, NULL, NULL);
        ASensorEvent event;
        float a = SENSOR_FILTER_ALPHA;
        while (ASensorEventQueue_getEvents(accelerometerSensorEventQueue, &event, 1) > 0) {
            sensorDataFilter.x = a * event.acceleration.x + (1.0f - a) * sensorDataFilter.x;
            sensorDataFilter.y = a * event.acceleration.y + (1.0f - a) * sensorDataFilter.y;
            sensorDataFilter.z = a * event.acceleration.z + (1.0f - a) * sensorDataFilter.z;
        }
        sensorData[sensorDataIndex] = sensorDataFilter;
        sensorData[SENSOR_HISTORY_LENGTH + sensorDataIndex] = sensorDataFilter;
        sensorDataIndex = (sensorDataIndex + 1) % SENSOR_HISTORY_LENGTH;
    }

    void render() {
        glClearColor(0.f, 0.f, 0.f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glEnableVertexAttribArray(vPositionHandle);
        glVertexAttribPointer(vPositionHandle, 1, GL_FLOAT, GL_FALSE, 0, xPos);

        glEnableVertexAttribArray(vSensorValueHandle);
        glVertexAttribPointer(vSensorValueHandle, 1, GL_FLOAT, GL_FALSE,
                              sizeof(AccelerometerData),
                              &sensorData[sensorDataIndex].x);

        glUniform4f(uFragColorHandle, 1.0f, 1.0f, 0.0f, 1.0f);
        glDrawArrays(GL_LINE_STRIP, 0, SENSOR_HISTORY_LENGTH);

        glVertexAttribPointer(vSensorValueHandle, 1, GL_FLOAT, GL_FALSE,
                              sizeof(AccelerometerData),
                              &sensorData[sensorDataIndex].y);
        glUniform4f(uFragColorHandle, 1.0f, 0.0f, 1.0f, 1.0f);
        glDrawArrays(GL_LINE_STRIP, 0, SENSOR_HISTORY_LENGTH);

        glVertexAttribPointer(vSensorValueHandle, 1, GL_FLOAT, GL_FALSE,
                              sizeof(AccelerometerData),
                              &sensorData[sensorDataIndex].z);
        glUniform4f(uFragColorHandle, 0.0f, 1.0f, 1.0f, 1.0f);
        glDrawArrays(GL_LINE_STRIP, 0, SENSOR_HISTORY_LENGTH);
    }

    void ShowUI() {
        LOGI("ShowUI");
        JNIEnv *jni;
        app_->activity->vm->AttachCurrentThread(&jni, NULL);

        //Default class retrieval
        jclass clazz = jni->GetObjectClass(app_->activity->clazz);
        jmethodID methodID = jni->GetMethodID(clazz, "showUI", "()V");
        jni->CallVoidMethod(app_->activity->clazz, methodID);

        app_->activity->vm->DetachCurrentThread();
        return;
    }

   /**
 * Just the current frame in the display.
 */
    double lasttime;

    void DrawFrame() {
        float fFPS;
        double currentTime = monitor_.GetCurrentTime();
        bool updateViews = UpdateViews();
//        LOGI("DrawFrame % %b", currentTime, updateViews);
        if (updateViews) {
            if (monitor_.Update(fFPS)) {
                UpdateFPS(fFPS);
            }
            {
                std::ostringstream stringStream;
                stringStream.setf(std::ios_base::fixed);
                //LOGI(" R: ", renderer_.colorfade[0]);
                stringStream << "CurrentTime: " << std::setprecision(4) << currentTime <<
                std::endl;
                std::string copyOfStr = stringStream.str();
                UpdateBottomLine(copyOfStr);
            }
//            renderer_.computeMaterialRGB();
        } else {
            ReadSliderValues();
        }

        update();

        // Just fill the screen with a color.
        glClearColor(0.5f, 0.5f, 0.5f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render();

        // Swap
        if (EGL_SUCCESS != gl_context_->Swap()) {
            UnloadResources();
            LoadResources();
        }
    }


    /**
 * Process the next main command.
 */
    static void HandleCmd(struct android_app *app, int32_t cmd) {
        SensorGraph *eng = (SensorGraph *) app->userData;
        LOGI("HandleCmd %d", cmd);
        switch (cmd) {
            case APP_CMD_SAVE_STATE:
                break;
            case APP_CMD_INIT_WINDOW:
                // The window is being shown, get it ready.
                if (app->window != NULL) {
                    eng->InitDisplay();
                    eng->DrawFrame();
                }
                break;
            case APP_CMD_TERM_WINDOW:
                // The window is being hidden or closed, clean it up.
                eng->TermDisplay();
                eng->has_focus_ = false;
                break;
            case APP_CMD_STOP:
                break;
            case APP_CMD_GAINED_FOCUS:
                eng->ResumeSensors();
                //Start animation
                eng->has_focus_ = true;
                break;
            case APP_CMD_LOST_FOCUS:
                eng->SuspendSensors();
                // Also stop animating.
                eng->has_focus_ = false;
                eng->DrawFrame();
                break;
            case APP_CMD_LOW_MEMORY:
                //Free up GL resources
                eng->TrimMemory();
                break;
            default:
                LOGI("HandleCmd %d UNKNOWN", cmd);
        }
    }

    /**
 * Process the next input event.
 */
    static int32_t HandleInput(android_app *app,
                                AInputEvent *event) {
        SensorGraph *eng = (SensorGraph *) app->userData;
        if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
            ndk_helper::GESTURE_STATE doubleTapState = eng->doubletap_detector_.Detect(event);
            ndk_helper::GESTURE_STATE dragState = eng->drag_detector_.Detect(event);
            ndk_helper::GESTURE_STATE pinchState = eng->pinch_detector_.Detect(event);

            //Double tap detector has a priority over other detectors
            if (doubleTapState == ndk_helper::GESTURE_STATE_ACTION) {
                //Detect double tap
//                eng->tap_camera_.Reset(true);
            }
            else {
                //Handle drag state
                if (dragState & ndk_helper::GESTURE_STATE_START) {
                    //Otherwise, start dragging
                    ndk_helper::Vec2 v;
                    eng->drag_detector_.GetPointer(v);
//                    eng->TransformPosition(v);
//                    eng->tap_camera_.BeginDrag(v);
                }
                else if (dragState & ndk_helper::GESTURE_STATE_MOVE) {
                    ndk_helper::Vec2 v;
                    eng->drag_detector_.GetPointer(v);
//                    eng->TransformPosition(v);
//                    eng->tap_camera_.Drag(v);
                }
                else if (dragState & ndk_helper::GESTURE_STATE_END) {
//                    eng->tap_camera_.EndDrag();
                }

                //Handle pinch state
                if (pinchState & ndk_helper::GESTURE_STATE_START) {
                    //Start new pinch
                    ndk_helper::Vec2 v1;
                    ndk_helper::Vec2 v2;
                    eng->pinch_detector_.GetPointers(v1, v2);
//                    eng->TransformPosition(v1);
//                    eng->TransformPosition(v2);
//                    eng->tap_camera_.BeginPinch(v1, v2);
                }
                else if (pinchState & ndk_helper::GESTURE_STATE_MOVE) {
                    //Multi touch
                    //Start new pinch
                    ndk_helper::Vec2 v1;
                    ndk_helper::Vec2 v2;
                    eng->pinch_detector_.GetPointers(v1, v2);
//                    eng->TransformPosition(v1);
//                    eng->TransformPosition(v2);
//                    eng->tap_camera_.Pinch(v1, v2);
                }
            }
            return 1;
        }
        return 0;
    }

    /**
 * Initialize an EGL context for the current display.
 */
    int InitDisplay() {
        if (!initialized_resources_) {
            LOGI("InitDisplay NOT initialized");
            gl_context_->Init(app_->window);
            LoadResources();
            initialized_resources_ = true;
        }
        else {
            LOGI("InitDisplay initialized");
            // initialize OpenGL ES and EGL
            if (EGL_SUCCESS != gl_context_->Resume(app_->window)) {
                UnloadResources();
                LoadResources();
            }
        }

        ShowUI();

        // Initialize GL state.
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        //Note that screen size might have been changed
        glViewport(0, 0, gl_context_->GetScreenWidth(), gl_context_->GetScreenHeight());
//        renderer_.UpdateViewport();

//        tap_camera_.SetFlip(1.f, -1.f, -1.f);
//        tap_camera_.SetPinchTransformFactor(2.f, 2.f, 8.f);

        // emulate callback surface created
        surfaceCreated();

        return 0;
    }

    /**
     * Tear down the EGL context currently associated with the display.
     */
    void TermDisplay() {
        gl_context_->Suspend();

    }

    void UpdateFPS(float fFPS) {
        JNIEnv *jni;
        app_->activity->vm->AttachCurrentThread(&jni, NULL);

        //Default class retrieval
        jclass clazz = jni->GetObjectClass(app_->activity->clazz);
        jmethodID methodID = jni->GetMethodID(clazz, "updateFPS", "(F)V");
        jni->CallVoidMethod(app_->activity->clazz, methodID, fFPS);

        app_->activity->vm->DetachCurrentThread();
        return;
    }


    /**  only in panel 1 we do the updates
     *
     */
    bool UpdateViews() {
        JNIEnv *jni;
        app_->activity->vm->AttachCurrentThread(&jni, NULL);

        jclass clazz = jni->GetObjectClass(app_->activity->clazz);
        jmethodID methodID = jni->GetMethodID(clazz, "updateViews", "()Z");
        jboolean updateEnabled = jni->CallBooleanMethod(app_->activity->clazz, methodID);

        app_->activity->vm->DetachCurrentThread();
        return updateEnabled != NULL;
    }

    void UpdateBottomLine(std::string text) {
        JNIEnv *jni;
        app_->activity->vm->AttachCurrentThread(&jni, NULL);

        //Default class retrieval
        jclass clazz = jni->GetObjectClass(app_->activity->clazz);
        jstring jString = jni->NewStringUTF(text.c_str());
        jmethodID methodID = jni->GetMethodID(clazz, "updateBottomLine", "(Ljava/lang/String;)V");
        jni->CallVoidMethod(app_->activity->clazz, methodID, jString);

        app_->activity->vm->DetachCurrentThread();
        return;
    }

    // read the slider values from
    int GetProgressValue(int which) {
        JNIEnv *jni;
        app_->activity->vm->AttachCurrentThread(&jni, NULL);

        jclass clazz = jni->GetObjectClass(app_->activity->clazz);
        jmethodID methodID = jni->GetMethodID(clazz, "getProgressValue", "(I)I");
        jint progress = jni->CallIntMethod(app_->activity->clazz, methodID, which);

        app_->activity->vm->DetachCurrentThread();
        return progress;
    }


    void ReadSliderValues() {
//        LOGI("ReadSliderValues");
        float colorfade[3];
        for (int i = 0; i < 3; ++i) {
            colorfade[i] = GetProgressValue(i) / 100.f;
        }
    }
    void TrimMemory() {
        LOGI("Trimming memory");
        gl_context_->Invalidate();
    }

    void ResumeSensors() {
        // When our app gains focus, we start monitoring the accelerometer.
        if (accelerometerSensor != NULL) {
            ASensorEventQueue_enableSensor(accelerometerSensorEventQueue, accelerometerSensor);
            // We'd like to get 60 events per second (in us).
            ASensorEventQueue_setEventRate(accelerometerSensorEventQueue, accelerometerSensor,
                                           (1000L / 60) * 1000);
        }
    }

    void SuspendSensors() {
        // When our app loses focus, we stop monitoring the accelerometer.
        // This is to avoid consuming battery while not being used.
        if (accelerometerSensor != NULL) {
            ASensorEventQueue_disableSensor(accelerometerSensorEventQueue, accelerometerSensor);
        }
    }


    bool IsReady() {
//        LOGI("IsReady %s", has_focus_?"true":"false");
        if (has_focus_)
            return true;
        return false;
    }

};


//SensorGraph gSensorGraph;

//extern "C" {
//JNIEXPORT void JNICALL
//Java_com_android_sensorgraph_SensorGraphJNI_init(JNIEnv *env, jclass type,
//                                                 jobject assetManager) {
//    (void) type;
//    AAssetManager *nativeAssetManager = AAssetManager_fromJava(env, assetManager);
//    gSensorGraph.init(nativeAssetManager);
//}
//
//JNIEXPORT void JNICALL
//Java_com_android_sensorgraph_SensorGraphJNI_surfaceCreated(JNIEnv *env, jclass type) {
//    (void) env;
//    (void) type;
//    gSensorGraph.surfaceCreated();
//}
//
//JNIEXPORT void JNICALL
//Java_com_android_sensorgraph_SensorGraphJNI_surfaceChanged(JNIEnv *env, jclass type,
//                                                           jint width,
//                                                           jint height) {
//    (void) env;
//    (void) type;
//    gSensorGraph.surfaceChanged(width, height);
//}
//
//
//JNIEXPORT void JNICALL
//Java_com_android_sensorgraph_SensorGraphJNI_drawFrame(JNIEnv *env, jclass type) {
//    (void) env;
//    (void) type;
//    gSensorGraph.update();
//    gSensorGraph.render();
//}
//
//JNIEXPORT void JNICALL
//Java_com_android_sensorgraph_SensorGraphJNI_pause(JNIEnv *env, jclass type) {
//    (void) env;
//    (void) type;
//    gSensorGraph.pause();
//}
//
//JNIEXPORT void JNICALL
//Java_com_android_sensorgraph_SensorGraphJNI_resume(JNIEnv *env, jclass type) {
//    (void) env;
//    (void) type;
//    gSensorGraph.resume();
//}
//}
//

SensorGraph g_engine;

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(android_app *state) {
    app_dummy();

    g_engine.SetState(state);

    //Init helper functions
    ndk_helper::JNIHelper::Init(state->activity, HELPER_CLASS_NAME);

    LOGI(LOG_TAG, "android_main", "after JNI Init");
    state->userData = &g_engine;
    state->onAppCmd = SensorGraph::HandleCmd;
    state->onInputEvent = SensorGraph::HandleInput;

#ifdef USE_NDK_PROFILER
    monstartup("libTeapotNativeActivity.so");
#endif

    // Prepare to monitor accelerometerSensor
    g_engine.InitSensors();

    // loop waiting for stuff to do.
    while (1) {
        // Read all pending events.
        int id;
        int events;
        android_poll_source *source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((id = ALooper_pollAll(g_engine.IsReady() ? 0 : -1, NULL, &events,
                                     (void **) &source))
               >= 0) {
            // Process this event.
            if (source != NULL)
                source->process(state, source);

            g_engine.ProcessSensors(id);

            g_engine.DrawFrame();

            // Check if we are exiting.
            if (state->destroyRequested != 0) {
                g_engine.TermDisplay();
                return;
            }
        }

        if (g_engine.IsReady()) {
            // Drawing is throttled to the screen update rate, so there
            // is no need to do timing here.
            g_engine.DrawFrame();
        }
    }
}
