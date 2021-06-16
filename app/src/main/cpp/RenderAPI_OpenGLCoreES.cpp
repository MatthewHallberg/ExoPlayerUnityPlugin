#include "RenderAPI.h"

#include <assert.h>
#include <GLES2/gl2.h>

class RenderAPI_OpenGLCoreES : public RenderAPI
{
public:
	RenderAPI_OpenGLCoreES(UnityGfxRenderer apiType);
	virtual ~RenderAPI_OpenGLCoreES() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

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
	m_VertexShader = CreateShader(GL_VERTEX_SHADER, kGlesVProgTextGLES3);
	m_FragmentShader = CreateShader(GL_FRAGMENT_SHADER, kGlesFShaderTextGLES3);


	// Link shaders into a program and find uniform locations
	m_Program = glCreateProgram();
	glBindAttribLocation(m_Program, kVertexInputPosition, "pos");
	glBindAttribLocation(m_Program, kVertexInputColor, "color");
	glAttachShader(m_Program, m_VertexShader);
	glAttachShader(m_Program, m_FragmentShader);

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