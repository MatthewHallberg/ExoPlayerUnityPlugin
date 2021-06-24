#include <assert.h>
#include <math.h>
#include <vector>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "Unity/IUnityGraphics.h"
#include <jni.h>
#include <string>

static void* g_TextureHandle = NULL;
static int   g_TextureWidth  = 0;
static int   g_TextureHeight = 0;

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

    auto textureWidth = 1080;
    auto textureHeight = 1920;

    // Create the texture
    glGenTextures(1, &textureID);
    Log("native Texture ID: " + std::to_string((int)textureID));
    // Bind the texture with the proper external texture target
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureID);
    glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );

    // Retrieve the JNI environment, using SDL, it looks like that
    auto env = getEnv();

    // Create a SurfaceTexture using JNI
    const jclass surfaceTextureClass = env->FindClass("android/graphics/SurfaceTexture");
    // Find the constructor that takes an int (texture name)
    const jmethodID surfaceTextureConstructor = env->GetMethodID(surfaceTextureClass, "<init>", "(I)V" );
    jobject surfaceTextureObject = env->NewObject(surfaceTextureClass, surfaceTextureConstructor, (int)textureID);
    jniSurfaceTexture = env->NewGlobalRef(surfaceTextureObject);

    // Don't forget to update the size of the SurfaceTexture
    jmethodID setDefaultBufferSizeMethodId = env->GetMethodID(surfaceTextureClass, "setDefaultBufferSize", "(II)V" );
    env->CallVoidMethod(jniSurfaceTexture, setDefaultBufferSizeMethodId, textureWidth, textureHeight);

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

extern "C" JNIEXPORT void JNICALL Java_com_matthew_videoplayer_NativeVideoPlayer_PassTexturePointer(JNIEnv *, jclass){
    Log("Hello from c++!!!!!!!!!");
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

static void Test(){
    Log("Hello from Java!");
}

static void MakeTextureRed() {

    void* textureHandle = g_TextureHandle;
    if (!textureHandle)
        return;

    int textureRowPitch = g_TextureWidth * 4;
    char* textureDataPtr = new char[textureRowPitch * g_TextureHeight];
    unsigned char* dst = (unsigned char*)textureDataPtr;

    for (int y = 0; y < g_TextureHeight; ++y) {
        unsigned char* ptr = dst;
        for (int x = 0; x < g_TextureWidth; ++x) {
            ptr[0] = 250;
            ptr[1] = 0;
            ptr[2] = 0;
            ptr[3] = 0;
            ptr += 4;
        }
        dst += textureRowPitch;
    }

    GLuint gltex = (GLuint)(size_t)(g_TextureHandle);
    glBindTexture(GL_TEXTURE_2D, gltex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_TextureWidth, g_TextureHeight, GL_RGBA, GL_UNSIGNED_BYTE, textureDataPtr);
    delete[](unsigned char*)textureDataPtr;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTextureFromUnity(void* textureHandle, int w, int h) {
    g_TextureHandle = textureHandle;
    g_TextureWidth = w;
    g_TextureHeight = h;
}

bool shouldPlay = true;
static void UNITY_INTERFACE_API OnRenderEvent(int eventID) {
    if (shouldPlay){
        shouldPlay = false;
        InitVideo();
    }
    //MakeTextureRed();
    UpdateAndroidSurface();
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() {
    return OnRenderEvent;
}