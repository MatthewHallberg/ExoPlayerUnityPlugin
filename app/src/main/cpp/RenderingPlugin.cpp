// Example low level rendering Unity plugin
#include "RenderAPI.h"

#include <assert.h>
#include <math.h>
#include <vector>
#include <GLES2/gl2.h>

// --------------------------------------------------------------------------
// SetTextureFromUnity, an example function we export which is called by one of the scripts.

static void* g_TextureHandle = NULL;
static int   g_TextureWidth  = 0;
static int   g_TextureHeight = 0;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTextureFromUnity(void* textureHandle, int w, int h)
{
    // A script calls this at initialization time; just remember the texture pointer here.
    // Will update texture pixels each frame from the plugin rendering event (texture update
    // needs to happen on the rendering thread).
    g_TextureHandle = textureHandle;
    g_TextureWidth = w;
    g_TextureHeight = h;
}

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    s_UnityInterfaces = unityInterfaces;
    s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
    s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

// --------------------------------------------------------------------------
// GraphicsDeviceEvent


static RenderAPI* s_CurrentAPI = NULL;
static UnityGfxRenderer s_DeviceType;


static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    // Create graphics API implementation upon initialization
    if (eventType == kUnityGfxDeviceEventInitialize)
    {
        assert(s_CurrentAPI == NULL);
        s_DeviceType = s_Graphics->GetRenderer();
        s_CurrentAPI = CreateRenderAPI(s_DeviceType);
    }

    // Let the implementation process the device related events
    if (s_CurrentAPI)
    {
        s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
    }

    // Cleanup graphics API implementation upon shutdown
    if (eventType == kUnityGfxDeviceEventShutdown)
    {
        delete s_CurrentAPI;
        s_CurrentAPI = NULL;
    }
}

static void ModifyTexturePixels()
{
    void* textureHandle = g_TextureHandle;
    if (!textureHandle)
        return;


    int textureRowPitch = g_TextureWidth * 4;
    // Just allocate a system memory buffer here for simplicity
    unsigned char* textureDataPtr = new unsigned char[textureRowPitch * g_TextureHeight];

    if (!textureDataPtr)
        return;

    const float t = 0;

    unsigned char* dst = (unsigned char*)textureDataPtr;
    for (int y = 0; y < g_TextureHeight; ++y)
    {
        unsigned char* ptr = dst;
        for (int x = 0; x < g_TextureWidth; ++x)
        {
            // Simple "plasma effect": several combined sine waves
            int vv = int(
                    (127.0f + (127.0f * sinf(x / 7.0f + t))) +
                    (127.0f + (127.0f * sinf(y / 5.0f - t))) +
                    (127.0f + (127.0f * sinf((x + y) / 6.0f - t))) +
                    (127.0f + (127.0f * sinf(sqrtf(float(x*x + y*y)) / 4.0f - t)))
            ) / 4;

            // Write the texture pixel
            ptr[0] = vv;
            ptr[1] = vv;
            ptr[2] = vv;
            ptr[3] = vv;

            // To next pixel (our pixels are 4 bpp)
            ptr += 4;
        }

        // To next image row
        dst += textureRowPitch;
    }

    GLuint gltex = (GLuint)(size_t)(textureHandle);
    // Update texture data, and free the memory buffer
    glBindTexture(GL_TEXTURE_2D, gltex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_TextureWidth, g_TextureHeight, GL_RGBA, GL_UNSIGNED_BYTE, textureDataPtr);
    delete[](unsigned char*)textureDataPtr;
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
    // Unknown / unsupported graphics device type? Do nothing
    if (s_CurrentAPI == NULL)
        return;

    ModifyTexturePixels();
}


// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
    return OnRenderEvent;
}

