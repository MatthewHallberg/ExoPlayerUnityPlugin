#include "RenderAPI.h"
#include "PlatformBase.h"

// OpenGL Core profile (desktop) or OpenGL ES (mobile) implementation of RenderAPI.
// Supports several flavors: Core, ES2, ES3


#if SUPPORT_OPENGL_UNIFIED


#include <assert.h>
#include <GLES2/gl2.h>

class RenderAPI_OpenGLCoreES : public RenderAPI
{
public:
	RenderAPI_OpenGLCoreES(UnityGfxRenderer apiType);
	virtual ~RenderAPI_OpenGLCoreES() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

	virtual bool GetUsesReverseZ() { return false; }

	virtual void* BeginModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int* outRowPitch);
	virtual void EndModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr);

private:
	void CreateResources();

private:
	UnityGfxRenderer m_APIType;
	GLuint m_VertexShader;
	GLuint m_FragmentShader;
	GLuint m_Program;
	GLuint m_VertexArray;
	GLuint m_VertexBuffer;
	int m_UniformWorldMatrix;
	int m_UniformProjMatrix;
};


RenderAPI* CreateRenderAPI_OpenGLCoreES(UnityGfxRenderer apiType)
{
	return new RenderAPI_OpenGLCoreES(apiType);
}


enum VertexInputs
{
	kVertexInputPosition = 0,
	kVertexInputColor = 1
};


// Simple vertex shader source
#define VERTEX_SHADER_SRC(ver, attr, varying)						\
	ver																\
	attr " highp vec3 pos;\n"										\
	attr " lowp vec4 color;\n"										\
	"\n"															\
	varying " lowp vec4 ocolor;\n"									\
	"\n"															\
	"uniform highp mat4 worldMatrix;\n"								\
	"uniform highp mat4 projMatrix;\n"								\
	"\n"															\
	"void main()\n"													\
	"{\n"															\
	"	gl_Position = (projMatrix * worldMatrix) * vec4(pos,1);\n"	\
	"	ocolor = color;\n"											\
	"}\n"															\

static const char* kGlesVProgTextGLES2 = VERTEX_SHADER_SRC("\n", "attribute", "varying");
static const char* kGlesVProgTextGLES3 = VERTEX_SHADER_SRC("#version 300 es\n", "in", "out");
#if SUPPORT_OPENGL_CORE
static const char* kGlesVProgTextGLCore = VERTEX_SHADER_SRC("#version 150\n", "in", "out");
#endif

#undef VERTEX_SHADER_SRC


// Simple fragment shader source
#define FRAGMENT_SHADER_SRC(ver, varying, outDecl, outVar)	\
	ver												\
	outDecl											\
	varying " lowp vec4 ocolor;\n"					\
	"\n"											\
	"void main()\n"									\
	"{\n"											\
	"	" outVar " = ocolor;\n"						\
	"}\n"											\

static const char* kGlesFShaderTextGLES2 = FRAGMENT_SHADER_SRC("\n", "varying", "\n", "gl_FragColor");
static const char* kGlesFShaderTextGLES3 = FRAGMENT_SHADER_SRC("#version 300 es\n", "in", "out lowp vec4 fragColor;\n", "fragColor");
#if SUPPORT_OPENGL_CORE
static const char* kGlesFShaderTextGLCore = FRAGMENT_SHADER_SRC("#version 150\n", "in", "out lowp vec4 fragColor;\n", "fragColor");
#endif

#undef FRAGMENT_SHADER_SRC


static GLuint CreateShader(GLenum type, const char* sourceText)
{
	GLuint ret = glCreateShader(type);
	glShaderSource(ret, 1, &sourceText, NULL);
	glCompileShader(ret);
	return ret;
}


void RenderAPI_OpenGLCoreES::CreateResources()
{
	// Create shaders
	if (m_APIType == kUnityGfxRendererOpenGLES20)
	{
		m_VertexShader = CreateShader(GL_VERTEX_SHADER, kGlesVProgTextGLES2);
		m_FragmentShader = CreateShader(GL_FRAGMENT_SHADER, kGlesFShaderTextGLES2);
	}
	else if (m_APIType == kUnityGfxRendererOpenGLES30)
	{
		m_VertexShader = CreateShader(GL_VERTEX_SHADER, kGlesVProgTextGLES3);
		m_FragmentShader = CreateShader(GL_FRAGMENT_SHADER, kGlesFShaderTextGLES3);
	}
#	if SUPPORT_OPENGL_CORE
	else if (m_APIType == kUnityGfxRendererOpenGLCore)
	{
#		if UNITY_WIN
		gl3wInit();
#		endif

		m_VertexShader = CreateShader(GL_VERTEX_SHADER, kGlesVProgTextGLCore);
		m_FragmentShader = CreateShader(GL_FRAGMENT_SHADER, kGlesFShaderTextGLCore);
	}
#	endif // if SUPPORT_OPENGL_CORE


	// Link shaders into a program and find uniform locations
	m_Program = glCreateProgram();
	glBindAttribLocation(m_Program, kVertexInputPosition, "pos");
	glBindAttribLocation(m_Program, kVertexInputColor, "color");
	glAttachShader(m_Program, m_VertexShader);
	glAttachShader(m_Program, m_FragmentShader);
#	if SUPPORT_OPENGL_CORE
	if (m_APIType == kUnityGfxRendererOpenGLCore)
		glBindFragDataLocation(m_Program, 0, "fragColor");
#	endif // if SUPPORT_OPENGL_CORE
	glLinkProgram(m_Program);

	GLint status = 0;
	glGetProgramiv(m_Program, GL_LINK_STATUS, &status);
	assert(status == GL_TRUE);

	m_UniformWorldMatrix = glGetUniformLocation(m_Program, "worldMatrix");
	m_UniformProjMatrix = glGetUniformLocation(m_Program, "projMatrix");

	// Create vertex buffer
	glGenBuffers(1, &m_VertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, 1024, NULL, GL_STREAM_DRAW);

	assert(glGetError() == GL_NO_ERROR);
}


RenderAPI_OpenGLCoreES::RenderAPI_OpenGLCoreES(UnityGfxRenderer apiType)
	: m_APIType(apiType)
{
}


void RenderAPI_OpenGLCoreES::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	if (type == kUnityGfxDeviceEventInitialize)
	{
		CreateResources();
	}
	else if (type == kUnityGfxDeviceEventShutdown)
	{
		//@TODO: release resources
	}
}

void* RenderAPI_OpenGLCoreES::BeginModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int* outRowPitch)
{
	const int rowPitch = textureWidth * 4;
	// Just allocate a system memory buffer here for simplicity
	unsigned char* data = new unsigned char[rowPitch * textureHeight];
	*outRowPitch = rowPitch;
	return data;
}


void RenderAPI_OpenGLCoreES::EndModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr)
{
	GLuint gltex = (GLuint)(size_t)(textureHandle);
	// Update texture data, and free the memory buffer
	glBindTexture(GL_TEXTURE_2D, gltex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, textureHeight, GL_RGBA, GL_UNSIGNED_BYTE, dataPtr);
	delete[](unsigned char*)dataPtr;
}

#endif // #if SUPPORT_OPENGL_UNIFIED
