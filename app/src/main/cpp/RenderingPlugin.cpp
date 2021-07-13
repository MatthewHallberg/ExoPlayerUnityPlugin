#include <assert.h>
#include <math.h>
#include <vector>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "Unity/IUnityGraphics.h"
#include <jni.h>
#include <string>
#include <list>
#include <map>

static JavaVM* gJvm = nullptr;
static jobject gClassLoader;
static jmethodID gFindClassMethod;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

static std::map<int, jobject> surfaceTextures;
static std::map<int, jobject>::iterator it;
static int unityID;
static uint externalID;
static uint fbo;

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
    // Create the texture for Android Surface
    glGenTextures(1, &externalID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, externalID);
    glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );

    // Retrieve the JNI environment, using SDL, it looks like that
    auto env = getEnv();

    // Create a SurfaceTexture using JNI
    const jclass surfaceTextureClass = env->FindClass("android/graphics/SurfaceTexture");
    // Find the constructor that takes an int (texture name)
    const jmethodID surfaceTextureConstructor = env->GetMethodID(surfaceTextureClass, "<init>", "(I)V" );
    jobject surfaceTextureObject = env->NewObject(surfaceTextureClass, surfaceTextureConstructor, (int)externalID);
    jobject jniSurfaceTexture = env->NewGlobalRef(surfaceTextureObject);
    //add to map so we can iterate through and update all
    surfaceTextures[(int)externalID] = jniSurfaceTexture;

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
    int textureNum = (int)externalID;
    jstring textureIDString = env->NewStringUTF(std::to_string(textureNum).c_str());
    jmethodID createSurfaceVideoMethodID = env->GetStaticMethodID(videoClass, "CreateSurface", "(Landroid/view/Surface;Ljava/lang/String;Ljava/lang/String;)V");
    // Pass the JNI Surface object to the videoPlayer with video and texture ID
    env->CallStaticVoidMethod(videoClass, createSurfaceVideoMethodID, jniSurface, videoPlayerID, textureIDString);
}

static UnityGfxRenderer s_DeviceType;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    if (eventType == kUnityGfxDeviceEventInitialize) {
        s_DeviceType = s_Graphics->GetRenderer();
    }
}

extern "C" void RegisterUnityTextureID(int videoID, int unityTextureID){
    Log("Unity Texture ID: " + std::to_string(unityTextureID));
    unityID = unityTextureID;
}

extern "C" void DeleteSurfaceID(int videoID){
//    Log("Deleting External ID: " + std::to_string(surfaceID));
//    auto env = getEnv();
//    jobject surface = surfaceTextures[surfaceID];
//    //remove from map so it doesnt get updated
//    surfaceTextures.erase(surfaceID);
//    //delete global ref
//    env->DeleteGlobalRef(surface);
}

static bool setup = false;
static void UpdateAndroidSurfaces(){
    //call update on android surface texture object
    auto env = getEnv();
    const jclass surfaceTextureClass = env->FindClass("android/graphics/SurfaceTexture");
    jmethodID updateTexImageMethodId = env->GetMethodID(surfaceTextureClass, "updateTexImage", "()V");

    //iterate through map to update textures
    for (it = surfaceTextures.begin(); it != surfaceTextures.end(); it++) {
        env->CallVoidMethod(it->second, updateTexImageMethodId);
    }

    if (surfaceTextures.size() > 0 && !setup) {
        Log("BINDING UNITY TEXTURE: " + std::to_string(unityID));
        setup = true;
        //bind external texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, externalID);
        //bind unity texture
        glActiveTexture( GL_TEXTURE1 );
        glBindTexture( GL_TEXTURE_2D, unityID);
        //glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
         //             1080, 1920, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1080, 1920,
                        GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

        glBindTexture( GL_TEXTURE_2D, 0 );
        glActiveTexture( GL_TEXTURE0 );

        glGenFramebuffers( 1, &fbo );
        glBindFramebuffer( GL_FRAMEBUFFER, fbo );
        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                unityID, 0 );
        glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    }

    if (surfaceTextures.size() > 0){
        Log("UPDATING UNITY TEXTURE...");
        glBindFramebuffer( GL_FRAMEBUFFER, fbo );
        glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );
        glBindFramebuffer( GL_FRAMEBUFFER, 0 );



        glBindTexture( GL_TEXTURE_2D, unityID );
        glGenerateMipmap( GL_TEXTURE_2D );
        glBindTexture( GL_TEXTURE_2D, 0 );
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