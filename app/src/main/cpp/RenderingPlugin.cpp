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

static JavaVM* gJvm = nullptr;
static jobject gClassLoader;
static jmethodID gFindClassMethod;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

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

static void PlayVideo(){
    auto env = getEnv();
    jclass videoClass = findClass("com/matthew/videoplayer/NativeVideoPlayer");
    jint texId = static_cast<int>(reinterpret_cast<std::uintptr_t>(g_TextureHandle));
    jmethodID mid = env->GetStaticMethodID(videoClass, "playVideo", "(I)V");
    env->CallStaticVoidMethod(videoClass, mid, texId);
}

static void UpdateAndroidSurface(){
    auto env = getEnv();
    jclass videoClass = findClass("com/matthew/videoplayer/NativeVideoPlayer");
    jmethodID mid = env->GetStaticMethodID(videoClass, "UpdateSurface", "()V");
    env->CallStaticVoidMethod(videoClass, mid);
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

static void Test(){
    Log("Hello from Java!");
}

static void BindTexture(){
    void* textureHandle = g_TextureHandle;
    if (!textureHandle)
        return;

    GLuint gltex = (GLuint)(size_t)(g_TextureHandle);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, gltex);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    //glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D, gltex);
    //glUniform1i(m_UniformFrame, 0);
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
        //BindTexture();
        //PlayVideo();
    }
    MakeTextureRed();
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() {
    return OnRenderEvent;
}