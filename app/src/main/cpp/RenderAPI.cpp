#include "RenderAPI.h"
#include "Unity/IUnityGraphics.h"


RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType)
{
	if (apiType == kUnityGfxRendererOpenGLCore || apiType == kUnityGfxRendererOpenGLES20 || apiType == kUnityGfxRendererOpenGLES30)
	{
		extern RenderAPI* CreateRenderAPI_OpenGLCoreES(UnityGfxRenderer apiType);
		return CreateRenderAPI_OpenGLCoreES(apiType);
	}
	// Unknown or unsupported graphics API
	return NULL;
}
