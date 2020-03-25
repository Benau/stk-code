/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2018 Dawid Gan
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
 *
 */

#include <jni.h>

#include <errno.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

#include "stk_android_native_app_glue.h"
#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "threaded_app", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "threaded_app", __VA_ARGS__))

/* For debug builds, always enable the debug traces in this library */
#ifndef NDEBUG
#  define LOGV(...)  ((void)__android_log_print(ANDROID_LOG_VERBOSE, "threaded_app", __VA_ARGS__))
#else
#  define LOGV(...)  ((void)0)
#endif

extern void start_stk(struct android_app* app);

static void free_saved_state(struct android_app* android_app) {
    pthread_mutex_lock(&android_app->mutex);
    if (android_app->savedState != NULL) {
        free(android_app->savedState);
        android_app->savedState = NULL;
        android_app->savedStateSize = 0;
    }
    pthread_mutex_unlock(&android_app->mutex);
}

int8_t android_app_read_cmd(struct android_app* android_app) {
    int8_t cmd;
    if (read(android_app->msgread, &cmd, sizeof(cmd)) == sizeof(cmd)) {
        switch (cmd) {
            case APP_CMD_SAVE_STATE:
                free_saved_state(android_app);
                break;
        }
        return cmd;
    } else {
        LOGE("No data on command pipe!");
    }
    return -1;
}

static void print_cur_config(struct android_app* android_app) {
    char lang[2], country[2];
    AConfiguration_getLanguage(android_app->config, lang);
    AConfiguration_getCountry(android_app->config, country);

    LOGV("Config: mcc=%d mnc=%d lang=%c%c cnt=%c%c orien=%d touch=%d dens=%d "
            "keys=%d nav=%d keysHid=%d navHid=%d sdk=%d size=%d long=%d "
            "modetype=%d modenight=%d",
            AConfiguration_getMcc(android_app->config),
            AConfiguration_getMnc(android_app->config),
            lang[0], lang[1], country[0], country[1],
            AConfiguration_getOrientation(android_app->config),
            AConfiguration_getTouchscreen(android_app->config),
            AConfiguration_getDensity(android_app->config),
            AConfiguration_getKeyboard(android_app->config),
            AConfiguration_getNavigation(android_app->config),
            AConfiguration_getKeysHidden(android_app->config),
            AConfiguration_getNavHidden(android_app->config),
            AConfiguration_getSdkVersion(android_app->config),
            AConfiguration_getScreenSize(android_app->config),
            AConfiguration_getScreenLong(android_app->config),
            AConfiguration_getUiModeType(android_app->config),
            AConfiguration_getUiModeNight(android_app->config));
}

void android_app_pre_exec_cmd(struct android_app* android_app, int8_t cmd) {
    switch (cmd) {
        case APP_CMD_INPUT_CHANGED:
            LOGV("APP_CMD_INPUT_CHANGED\n");
            pthread_mutex_lock(&android_app->mutex);
            if (android_app->inputQueue != NULL) {
                AInputQueue_detachLooper(android_app->inputQueue);
            }
            android_app->inputQueue = android_app->pendingInputQueue;
            if (android_app->inputQueue != NULL) {
                LOGV("Attaching input queue to looper");
                AInputQueue_attachLooper(android_app->inputQueue,
                        android_app->looper, LOOPER_ID_INPUT, NULL,
                        &android_app->inputPollSource);
            }
            pthread_cond_broadcast(&android_app->cond);
            pthread_mutex_unlock(&android_app->mutex);
            break;

        case APP_CMD_INIT_WINDOW:
            LOGV("APP_CMD_INIT_WINDOW\n");
            pthread_mutex_lock(&android_app->mutex);
            android_app->window = android_app->pendingWindow;
            pthread_cond_broadcast(&android_app->cond);
            pthread_mutex_unlock(&android_app->mutex);
            break;

        case APP_CMD_TERM_WINDOW:
            LOGV("APP_CMD_TERM_WINDOW\n");
            pthread_cond_broadcast(&android_app->cond);
            break;

        case APP_CMD_RESUME:
        case APP_CMD_START:
        case APP_CMD_PAUSE:
        case APP_CMD_STOP:
            LOGV("activityState=%d\n", cmd);
            pthread_mutex_lock(&android_app->mutex);
            android_app->activityState = cmd;
            pthread_cond_broadcast(&android_app->cond);
            pthread_mutex_unlock(&android_app->mutex);
            break;

        case APP_CMD_CONFIG_CHANGED:
            LOGV("APP_CMD_CONFIG_CHANGED\n");
            AConfiguration_fromAssetManager(android_app->config,
                    android_app->activity->assetManager);
            print_cur_config(android_app);
            break;

        case APP_CMD_DESTROY:
            LOGV("APP_CMD_DESTROY\n");
            android_app->destroyRequested = 1;
            break;
    }
}

void android_app_post_exec_cmd(struct android_app* android_app, int8_t cmd) {
    switch (cmd) {
        case APP_CMD_TERM_WINDOW:
            LOGV("APP_CMD_TERM_WINDOW\n");
            pthread_mutex_lock(&android_app->mutex);
            android_app->window = NULL;
            pthread_cond_broadcast(&android_app->cond);
            pthread_mutex_unlock(&android_app->mutex);
            break;

        case APP_CMD_SAVE_STATE:
            LOGV("APP_CMD_SAVE_STATE\n");
            pthread_mutex_lock(&android_app->mutex);
            android_app->stateSaved = 1;
            pthread_cond_broadcast(&android_app->cond);
            pthread_mutex_unlock(&android_app->mutex);
            break;

        case APP_CMD_RESUME:
            free_saved_state(android_app);
            break;
    }
}

void app_dummy() {

}

static void android_app_destroy(struct android_app* android_app) {
    LOGV("android_app_destroy!");
    free_saved_state(android_app);
    pthread_mutex_lock(&android_app->mutex);
    if (android_app->inputQueue != NULL) {
        AInputQueue_detachLooper(android_app->inputQueue);
    }
    AConfiguration_delete(android_app->config);
    android_app->destroyed = 1;
    pthread_cond_broadcast(&android_app->cond);
    pthread_mutex_unlock(&android_app->mutex);
    // Can't touch android_app object after this.
}

static void process_input(struct android_app* app, struct android_poll_source* source) {
    AInputEvent* event = NULL;
    while (AInputQueue_getEvent(app->inputQueue, &event) >= 0) {
        LOGV("New input event: type=%d\n", AInputEvent_getType(event));
        if (AInputQueue_preDispatchEvent(app->inputQueue, event)) {
            continue;
        }
        int32_t handled = 0;
        if (app->onInputEvent != NULL) handled = app->onInputEvent(app, event);
        AInputQueue_finishEvent(app->inputQueue, event, handled);
    }
}

static void process_cmd(struct android_app* app, struct android_poll_source* source) {
    int8_t cmd = android_app_read_cmd(app);
    android_app_pre_exec_cmd(app, cmd);
    if (app->onAppCmd != NULL) app->onAppCmd(app, cmd);
    android_app_post_exec_cmd(app, cmd);
}

void (*g_android_main)(struct android_app* app);
void* g_dl_handle;

static void* android_app_entry(void* param) {
    struct android_app* android_app = (struct android_app*)param;

    // This is now in stk thread, attach jvm now
    JNIEnv* env = NULL;
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_6;
    args.name = "NativeThread";
    args.group = NULL;
    JavaVM* vm = android_app->activity->vm;
    jint status = (*vm)->AttachCurrentThread(vm, &env, &args);
    if (status != JNI_OK)
    {
        LOGE("Failed to attach jni thread.");
        return NULL;
    }

    android_app->config = AConfiguration_new();
    AConfiguration_fromAssetManager(android_app->config, android_app->activity->assetManager);

    print_cur_config(android_app);

    android_app->cmdPollSource.id = LOOPER_ID_MAIN;
    android_app->cmdPollSource.app = android_app;
    android_app->cmdPollSource.process = process_cmd;
    android_app->inputPollSource.id = LOOPER_ID_INPUT;
    android_app->inputPollSource.app = android_app;
    android_app->inputPollSource.process = process_input;

    ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    ALooper_addFd(looper, android_app->msgread, LOOPER_ID_MAIN, ALOOPER_EVENT_INPUT, NULL,
            &android_app->cmdPollSource);
    android_app->looper = looper;

    pthread_mutex_lock(&android_app->mutex);
    android_app->running = 1;
    pthread_cond_broadcast(&android_app->cond);
    pthread_mutex_unlock(&android_app->mutex);

    g_android_main(android_app);

    (*vm)->DetachCurrentThread(vm);
    android_app_destroy(android_app);
    dlclose(g_dl_handle);
    g_dl_handle = NULL;
    return NULL;
}

// --------------------------------------------------------------------
// Native activity interaction (called from main thread)
// --------------------------------------------------------------------

static struct android_app* android_app_create(ANativeActivity* activity,
        void* savedState, size_t savedStateSize) {
    struct android_app* android_app = (struct android_app*)malloc(sizeof(struct android_app));
    memset(android_app, 0, sizeof(struct android_app));
    android_app->activity = activity;

    pthread_mutex_init(&android_app->mutex, NULL);
    pthread_cond_init(&android_app->cond, NULL);

    if (savedState != NULL) {
        android_app->savedState = malloc(savedStateSize);
        android_app->savedStateSize = savedStateSize;
        memcpy(android_app->savedState, savedState, savedStateSize);
    }

    int msgpipe[2];
    if (pipe(msgpipe)) {
        LOGE("could not create pipe: %s", strerror(errno));
        return NULL;
    }
    android_app->msgread = msgpipe[0];
    android_app->msgwrite = msgpipe[1];

    return android_app;
}

static void android_app_write_cmd(struct android_app* android_app, int8_t cmd) {
    if (write(android_app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd)) {
        LOGE("Failure writing android_app cmd: %s\n", strerror(errno));
    }
}

static void android_app_set_input(struct android_app* android_app, AInputQueue* inputQueue) {
    pthread_mutex_lock(&android_app->mutex);
    android_app->pendingInputQueue = inputQueue;
    android_app_write_cmd(android_app, APP_CMD_INPUT_CHANGED);
    while (android_app->inputQueue != android_app->pendingInputQueue) {
        pthread_cond_wait(&android_app->cond, &android_app->mutex);
    }
    pthread_mutex_unlock(&android_app->mutex);
}

static void android_app_set_window(struct android_app* android_app, ANativeWindow* window) {
    pthread_mutex_lock(&android_app->mutex);
    if (android_app->pendingWindow != NULL) {
        android_app_write_cmd(android_app, APP_CMD_TERM_WINDOW);
    }
    android_app->pendingWindow = window;
    if (window != NULL) {
        android_app_write_cmd(android_app, APP_CMD_INIT_WINDOW);
    }
    while (android_app->window != android_app->pendingWindow) {
        pthread_cond_wait(&android_app->cond, &android_app->mutex);
    }
    pthread_mutex_unlock(&android_app->mutex);
}

static void android_app_set_activity_state(struct android_app* android_app, int8_t cmd) {
    pthread_mutex_lock(&android_app->mutex);
    android_app_write_cmd(android_app, cmd);
    while (android_app->activityState != cmd) {
        pthread_cond_wait(&android_app->cond, &android_app->mutex);
    }
    pthread_mutex_unlock(&android_app->mutex);
}

static void android_app_free(struct android_app* android_app) {
    pthread_mutex_lock(&android_app->mutex);
    android_app_write_cmd(android_app, APP_CMD_DESTROY);
    while (!android_app->destroyed) {
        pthread_cond_wait(&android_app->cond, &android_app->mutex);
    }
    pthread_mutex_unlock(&android_app->mutex);

    close(android_app->msgread);
    close(android_app->msgwrite);
    pthread_cond_destroy(&android_app->cond);
    pthread_mutex_destroy(&android_app->mutex);
    free(android_app);
}

static void onDestroy(ANativeActivity* activity) {
    LOGV("Destroy: %p\n", activity);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_DESTROY);
    android_app_free(app);
}

static void onStart(ANativeActivity* activity) {
    LOGV("Start: %p\n", activity);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_START);
    android_app_set_activity_state(app, APP_CMD_START);
}

static void onResume(ANativeActivity* activity) {
    LOGV("Resume: %p\n", activity);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_RESUME);
    android_app_set_activity_state(app, APP_CMD_RESUME);
}

static void* onSaveInstanceState(ANativeActivity* activity, size_t* outLen) {
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_SAVE_STATE);
    void* savedState = NULL;

    LOGV("SaveInstanceState: %p\n", activity);
    pthread_mutex_lock(&app->mutex);
    app->stateSaved = 0;
    android_app_write_cmd(app, APP_CMD_SAVE_STATE);
    while (!app->stateSaved) {
        pthread_cond_wait(&app->cond, &app->mutex);
    }

    if (app->savedState != NULL) {
        savedState = app->savedState;
        *outLen = app->savedStateSize;
        app->savedState = NULL;
        app->savedStateSize = 0;
    }

    pthread_mutex_unlock(&app->mutex);

    return savedState;
}

static void onPause(ANativeActivity* activity) {
    LOGV("Pause: %p\n", activity);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_PAUSE);
    android_app_set_activity_state(app, APP_CMD_PAUSE);
}

static void onStop(ANativeActivity* activity) {
    LOGV("Stop: %p\n", activity);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_STOP);
    android_app_set_activity_state(app, APP_CMD_STOP);
}

static void onConfigurationChanged(ANativeActivity* activity) {
    LOGV("ConfigurationChanged: %p\n", activity);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_CONFIG_CHANGED);
    android_app_write_cmd(app, APP_CMD_CONFIG_CHANGED);
}

static void onLowMemory(ANativeActivity* activity) {
    LOGV("LowMemory: %p\n", activity);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_LOW_MEMORY);
    android_app_write_cmd(app, APP_CMD_LOW_MEMORY);
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused) {
    LOGV("WindowFocusChanged: %p -- %d\n", activity, focused);
    struct android_app* app = (struct android_app*)activity->instance;
    int cmd = focused ? APP_CMD_GAINED_FOCUS : APP_CMD_LOST_FOCUS;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, cmd);
    android_app_write_cmd(app, cmd);
}

static void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window) {
    LOGV("NativeWindowCreated: %p -- %p\n", activity, window);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_INIT_WINDOW);
    android_app_set_window(app, window);
}

static void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window) {
    LOGV("NativeWindowDestroyed: %p -- %p\n", activity, window);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_TERM_WINDOW);
    android_app_set_window(app, NULL);
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue) {
    LOGV("InputQueueCreated: %p -- %p\n", activity, queue);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_INPUT_CHANGED);
    android_app_set_input(app, queue);
}

static void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue) {
    LOGV("InputQueueDestroyed: %p -- %p\n", activity, queue);
    struct android_app* app = (struct android_app*)activity->instance;
    if (app->onAppCmdDirect != NULL) app->onAppCmdDirect(activity, APP_CMD_INPUT_CHANGED);
    android_app_set_input(app, NULL);
}

// For startSTK before onCreate in SuperTuxKartActivity
struct android_app* g_android_app;

void initSTK(ANativeActivity* app)
{
    JNIEnv* jenv = app->env;
    jclass contextClass = (*jenv)->GetObjectClass(jenv, app->clazz);
    jmethodID getApplicationContextMethod =
        (*jenv)->GetMethodID(jenv, contextClass, "getApplicationContext", "()Landroid/content/Context;");
    jobject contextObject =
        (*jenv)->CallObjectMethod(jenv, app->clazz, getApplicationContextMethod);
    jmethodID getApplicationInfoMethod = (*jenv)->GetMethodID(jenv,
        contextClass, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    jobject applicationInfoObject =
        (*jenv)->CallObjectMethod(jenv, contextObject, getApplicationInfoMethod);
    jfieldID nativeLibraryDirField = (*jenv)->GetFieldID(jenv,
        (*jenv)->GetObjectClass(jenv, applicationInfoObject), "nativeLibraryDir", "Ljava/lang/String;");
    jobject nativeLibraryDirObject =
        (*jenv)->GetObjectField(jenv, applicationInfoObject, nativeLibraryDirField);
    jmethodID getBytesMethod = (*jenv)->GetMethodID(jenv,
        (*jenv)->GetObjectClass(jenv, nativeLibraryDirObject), "getBytes", "(Ljava/lang/String;)[B");
    jbyteArray bytesObject = (jbyteArray)((*jenv)->CallObjectMethod(jenv,
        nativeLibraryDirObject, getBytesMethod, (*jenv)->NewStringUTF(jenv, "UTF-8")));
    size_t length = (*jenv)->GetArrayLength(jenv, bytesObject);
    jbyte* bytes = (*jenv)->GetByteArrayElements(jenv, bytesObject, NULL);
    const char* stkso = "/libstk.so";
    size_t stkso_len = strlen(stkso);
    char* path = calloc(length + stkso_len + 1, 1);
    if (!path)
        return;
    memcpy(path, bytes, length);
    memcpy(path + length, stkso, stkso_len);
    // Clear all error first
    char* err = dlerror();
    g_dl_handle = NULL;
    g_dl_handle = dlopen(path, RTLD_NOW);
    err = dlerror();
    if (!g_dl_handle)
    {
        // For android app bundle the libstk.so is compressed, we just need to
        // use relative path to get it
        g_dl_handle = dlopen("libstk.so", RTLD_NOW);
        err = dlerror();
    }
    if (!g_dl_handle)
    {
        LOGE("Failed to load libstk.so, exiting");
        exit(0);
    }
    free(path);
    g_android_main = (void(*)(struct android_app*))dlsym(g_dl_handle, "android_main");
    err = dlerror();
    if (err)
        LOGE("%s", err);
    if (!g_android_main)
    {
        LOGE("Failed to load android_main from libstk.so, exiting");
        exit(0);
    }
}

void ANativeActivity_onCreate(ANativeActivity* activity,
        void* savedState, size_t savedStateSize) {
    LOGV("Creating: %p\n", activity);
    activity->callbacks->onDestroy = onDestroy;
    activity->callbacks->onStart = onStart;
    activity->callbacks->onResume = onResume;
    activity->callbacks->onSaveInstanceState = onSaveInstanceState;
    activity->callbacks->onPause = onPause;
    activity->callbacks->onStop = onStop;
    activity->callbacks->onConfigurationChanged = onConfigurationChanged;
    activity->callbacks->onLowMemory = onLowMemory;
    activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;
    activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;

    activity->instance = android_app_create(activity, savedState, savedStateSize);
    g_android_app = activity->instance;
    initSTK(activity);
}

// From SDL
#define arraysize(array)    (sizeof(array)/sizeof(array[0]))
static void
register_methods(JNIEnv *env, const char *classname, JNINativeMethod *methods, int nb)
{
    jclass clazz = (*env)->FindClass(env, classname);
    if (clazz == NULL || (*env)->RegisterNatives(env, clazz, methods, nb) < 0)
    {
        __android_log_print(ANDROID_LOG_ERROR, "SuperTuxKart", "Failed to register methods of %s", classname);
        return;
    }
}

#if !defined(ANDROID_PACKAGE_CLASS_NAME)
    #error
#endif
#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)
// Called by loadLibrary after onCreate, get all required function pointer used in STK
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    // Called when System.loadLibrary("main") in SuperTuxKartActivity
    JNIEnv* env = NULL;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK)
    {
        __android_log_print(ANDROID_LOG_ERROR, "SuperTuxKart", "Failed to get JNI Env");
        exit(0);
    }

    JNINativeMethod* methods = malloc(sizeof(JNINativeMethod) * 2);
    if (!methods)
        exit(0);

    char activity_class[100];
    memset(activity_class, 0, 100);
    snprintf(activity_class, 100, "%s%s", STR(ANDROID_PACKAGE_CLASS_NAME), "SuperTuxKartActivity");
    memset(methods, 0, sizeof(JNINativeMethod) * 2);
    methods[0].name = "saveKeyboardHeight";
    methods[0].signature = "(I)V";
    methods[0].fnPtr = dlsym(g_dl_handle, "jni_saveKeyboardHeight");
    register_methods(env, activity_class, methods, 1);

    memset(activity_class, 0, 100);
    snprintf(activity_class, 100, "%s%s", STR(ANDROID_PACKAGE_CLASS_NAME), "STKEditText");
    memset(methods, 0, sizeof(JNINativeMethod) * 2);
    methods[0].name = "editText2STKEditbox";
    methods[0].signature = "(ILjava/lang/String;IIII)V";
    methods[0].fnPtr = dlsym(g_dl_handle, "jni_editText2STKEditbox");
    methods[1].name = "handleActionNext";
    methods[1].signature = "(I)V";
    methods[1].fnPtr = dlsym(g_dl_handle, "jni_handleActionNext");

    register_methods(env, activity_class, methods, 2);

    free(methods);
    return JNI_VERSION_1_6;
}

#define MAKE_ANDROID_START_STK_CALLBACK(x) JNIEXPORT void JNICALL Java_ ## x##_SuperTuxKartActivity_startSTK(JNIEnv* env, jobject this_obj)
#define ANDROID_START_STK_CALLBACK(PKG_NAME) MAKE_ANDROID_START_STK_CALLBACK(PKG_NAME)

ANDROID_START_STK_CALLBACK(ANDROID_PACKAGE_CALLBACK_NAME)
{
    // This will be called after ANativeActivity_onCreate
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&g_android_app->thread, &attr, android_app_entry, g_android_app);

    // Wait for thread to start.
    pthread_mutex_lock(&g_android_app->mutex);
    while (!g_android_app->running)
    {
        pthread_cond_wait(&g_android_app->cond, &g_android_app->mutex);
    }
    pthread_mutex_unlock(&g_android_app->mutex);
}
