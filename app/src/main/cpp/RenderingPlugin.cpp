#include <assert.h>
#include <math.h>
#include <vector>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "Unity/IUnityGraphics.h"
#include <jni.h>
#include <string>
#include <list>

static JavaVM* gJvm = nullptr;
static jobject gClassLoader;
static jmethodID gFindClassMethod;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

static std::list<jobject> surfaceTextures;

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

static void CreateSurfaceTexture(int videoID){
    // Create the texture
    uint textureID;
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
    jobject jniSurfaceTexture = env->NewGlobalRef(surfaceTextureObject);
    //add to list so we can iterate through and update all
    surfaceTextures.push_back(jniSurfaceTexture);

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
    jstring videoPlayerID = env->NewStringUTF(std::to_string(videoID).c_str());
    int textureNum = (int)textureID;
    jstring textureIDString = env->NewStringUTF(std::to_string(textureNum).c_str());
    jmethodID playVideoMethodID = env->GetStaticMethodID(videoClass, "CreateSurface", "(Landroid/view/Surface;Ljava/lang/String;Ljava/lang/String;)V");
    // Pass the JNI Surface object to the videoPlayer with video and texture ID
    env->CallStaticVoidMethod(videoClass, playVideoMethodID, jniSurface, videoPlayerID, textureIDString);
}

static UnityGfxRenderer s_DeviceType;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    if (eventType == kUnityGfxDeviceEventInitialize) {
        s_DeviceType = s_Graphics->GetRenderer();
    }
}

static void UpdateAndroidSurfaces(){
    //call update on android surface texture object
    auto env = getEnv();
    const jclass surfaceTextureClass = env->FindClass("android/graphics/SurfaceTexture");
    jmethodID updateTexImageMethodId = env->GetMethodID(surfaceTextureClass, "updateTexImage", "()V");

    for (jobject surfaceTex : surfaceTextures) {
        env->CallVoidMethod(surfaceTex, updateTexImageMethodId);
    }
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID) {
    if (eventID == -1){
        UpdateAndroidSurfaces();
    } else {
        CreateSurfaceTexture(eventID);
    }
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() {
    return OnRenderEvent;
}