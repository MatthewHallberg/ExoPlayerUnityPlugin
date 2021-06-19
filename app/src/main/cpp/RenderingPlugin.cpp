#include <assert.h>
#include <math.h>
#include <vector>
#include <GLES2/gl2.h>
#include "Unity/IUnityGraphics.h"
#include "OpenGL.h"
#include <jni.h>
#include <string>

static void* g_TextureHandle = NULL;
static int   g_TextureWidth  = 0;
static int   g_TextureHeight = 0;

static JavaVM* g_JavaVM = NULL;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTextureFromUnity(void* textureHandle, int w, int h) {
    g_TextureHandle = textureHandle;
    g_TextureWidth = w;
    g_TextureHeight = h;
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    g_JavaVM = vm;
    return JNI_VERSION_1_6;
}

static void Log(std::string message){

    JNIEnv* env;
    int envStat = g_JavaVM->GetEnv((void**)&env, JNI_VERSION_1_6);

    if (envStat == JNI_EDETACHED) {
        g_JavaVM->AttachCurrentThread(&env, NULL);
    }

    jstring logMessage = env->NewStringUTF(message.c_str());
    jclass clazz = env->FindClass("com/matthew/videoplayer/NativeVideoPlayer");
    jmethodID mid = env->GetStaticMethodID(clazz, "Log", "(Ljava/lang/String;)V");
     env->CallStaticVoidMethod(clazz, mid, logMessage);
}

static UnityGfxRenderer s_DeviceType;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    if (eventType == kUnityGfxDeviceEventInitialize) {
        s_DeviceType = s_Graphics->GetRenderer();
    }

    ProcessDeviceEvent(eventType, s_UnityInterfaces);
}

static void ModifyTexturePixels() {

    Log("hello");

    void* textureHandle = g_TextureHandle;
    if (!textureHandle)
        return;

    int textureRowPitch = g_TextureWidth * 4;
    unsigned char* textureDataPtr = new unsigned char[textureRowPitch * g_TextureHeight];

    unsigned char* dst = (unsigned char*)textureDataPtr;
    for (int y = 0; y < g_TextureHeight; ++y){
        unsigned char* ptr = dst;
        for (int x = 0; x < g_TextureWidth; ++x){
            ptr[0] = 255;
            ptr[1] = 0;
            ptr[2] = 0;
            ptr[3] = 0;
            ptr += 4;
        }
        dst += textureRowPitch;
    }

    // Update texture data, and free the memory buffer
    GLuint gltex = (GLuint)(size_t)(textureHandle);
    glBindTexture(GL_TEXTURE_2D, gltex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_TextureWidth, g_TextureHeight, GL_RGBA, GL_UNSIGNED_BYTE, textureDataPtr);

    delete[](unsigned char*)textureDataPtr;
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID) {
    ModifyTexturePixels();
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() {
    return OnRenderEvent;
}