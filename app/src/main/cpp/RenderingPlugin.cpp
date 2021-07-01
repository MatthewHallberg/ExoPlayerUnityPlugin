#include <assert.h>
#include <math.h>
#include <vector>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "Unity/IUnityGraphics.h"
#include <jni.h>
#include <string>

static uint textureID;

static JavaVM* gJvm = nullptr;
static jobject gClassLoader;
static jmethodID gFindClassMethod;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

static jobject jniSurfaceTexture;

JNIEnv* getEnv() {
    JNIEnv *env;
    int status = gJvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if(status < 0) {
        status = gJvm->AttachCurrentThread(&env, NULL);
        if(status < 0) {
            return nullptr;
        }
    }
    return env;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *pjvm, void *reserved) {
    gJvm = pjvm;  // cache the JavaVM pointer
    auto env = getEnv();
    jclass videoClass = env->FindClass("com/matthew/videoplayer/NativeVideoPlayer");
    jclass videoObject = env->GetObjectClass(videoClass);
    jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
    jmethodID getClassLoaderMethod = env->GetMethodID(videoObject, "getClassLoader",
                                                 "()Ljava/lang/ClassLoader;");
    gClassLoader = env->NewGlobalRef(env->CallObjectMethod(videoClass, getClassLoaderMethod));
    gFindClassMethod = env->GetMethodID(classLoaderClass, "findClass",
                                        "(Ljava/lang/String;)Ljava/lang/Class;");
    return JNI_VERSION_1_6;
}

static jclass findClass(const char* name) {
    return static_cast<jclass>(getEnv()->CallObjectMethod(gClassLoader, gFindClassMethod, getEnv()->NewStringUTF(name)));
}

static void Log(std::string message){
    auto env = getEnv();
    jclass videoClass = findClass("com/matthew/videoplayer/NativeVideoPlayer");
    jstring logMessage = env->NewStringUTF(message.c_str());
    jmethodID mid = env->GetStaticMethodID(videoClass, "Log", "(Ljava/lang/String;)V");
    env->CallStaticVoidMethod(videoClass, mid, logMessage);
}

static void InitVideo(){
    // Create the texture
    glGenTextures(1, &textureID);
    Log("native Texture ID: " + std::to_string((int)textureID));
    // Bind the texture with the proper external texture target
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureID);

    // Retrieve the JNI environment, using SDL, it looks like that
    auto env = getEnv();

    // Create a SurfaceTexture using JNI
    const jclass surfaceTextureClass = env->FindClass("android/graphics/SurfaceTexture");
    // Find the constructor that takes an int (texture name)
    const jmethodID surfaceTextureConstructor = env->GetMethodID(surfaceTextureClass, "<init>", "(I)V" );
    jobject surfaceTextureObject = env->NewObject(surfaceTextureClass, surfaceTextureConstructor, (int)textureID);
    jniSurfaceTexture = env->NewGlobalRef(surfaceTextureObject);

    //Create a Surface from the SurfaceTexture using JNI
    const jclass surfaceClass = env->FindClass("android/view/Surface");
    const jmethodID surfaceConstructor = env->GetMethodID(surfaceClass, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
    jobject surfaceObject = env->NewObject(surfaceClass, surfaceConstructor, jniSurfaceTexture);
    jobject jniSurface = env->NewGlobalRef(surfaceObject);

    // Now that we have a globalRef, we can free the localRef
    env->DeleteLocalRef(surfaceTextureObject);
    env->DeleteLocalRef(surfaceTextureClass);
    env->DeleteLocalRef(surfaceObject);
    env->DeleteLocalRef(surfaceClass);

    // Get the method to pass the Surface object to the videoPlayer
    jclass videoClass = findClass("com/matthew/videoplayer/NativeVideoPlayer");
    jmethodID playVideoMethodID = env->GetStaticMethodID(videoClass, "playVideo", "(Landroid/view/Surface;)V");
    // Pass the JNI Surface object to the videoPlayer
    env->CallStaticVoidMethod(videoClass, playVideoMethodID, jniSurface);
}

static UnityGfxRenderer s_DeviceType;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    if (eventType == kUnityGfxDeviceEventInitialize) {
        s_DeviceType = s_Graphics->GetRenderer();
    }
}

extern "C" int GetTextureID(){
    return textureID;
}

static void UpdateAndroidSurface(){
    //call update on android surface texture object
    auto env = getEnv();
    const jclass surfaceTextureClass = env->FindClass("android/graphics/SurfaceTexture");
    jmethodID updateTexImageMethodId = env->GetMethodID(surfaceTextureClass, "updateTexImage", "()V");
    env->CallVoidMethod(jniSurfaceTexture, updateTexImageMethodId);
}

bool shouldPlay = true;
static void UNITY_INTERFACE_API OnRenderEvent(int eventID) {
    if (shouldPlay){
        shouldPlay = false;
        InitVideo();
    }
    UpdateAndroidSurface();
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() {
    return OnRenderEvent;
}