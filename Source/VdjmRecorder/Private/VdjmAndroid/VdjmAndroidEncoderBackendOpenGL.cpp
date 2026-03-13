// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendOpenGL.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
bool FVdjmAndroidEncoderBackendOpenGL::Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow)
{
	if (config.IsValidateEncoderArguments() && ANativeWindow != nullptr)
	{
		mConfig = config;
		mInputWindow = inputWindow;
		return true;
	}
	return false;
}

bool FVdjmAndroidEncoderBackendOpenGL::Start()
{
	if (not mOwnerSession.IsValid())
		return false;
	FVdjmAndroidRecordSession* pinnedSession = mOwnerSession.Pin().Get();
	
	ANativeWindow* window = mInputWindow;
	
	if (window==nullptr)
	{
		return false;
	}

	mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (mDisplay == EGL_NO_DISPLAY)
	{
		return false;
	}

	if (!eglInitialize(mDisplay, nullptr, nullptr))
	{
		return false;
	}

	const EGLint configAttribs[] =
	{
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_NONE
	};

	EGLint numConfigs = 0;
	if (!eglChooseConfig(mDisplay, configAttribs, &mEglConfig, 1, &numConfigs) || numConfigs == 0)
	{
		return false;
	}

	const EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};

	mContext = eglCreateContext(mDisplay, mEglConfig, EGL_NO_CONTEXT, contextAttribs);
	if (mContext == EGL_NO_CONTEXT)
	{
		return false;
	}

	mSurface = eglCreateWindowSurface(mDisplay, mEglConfig, window, nullptr);
	if (mSurface == EGL_NO_SURFACE)
	{
		Terminate();
		return false;
	}

	if (!eglMakeCurrent(mDisplay, mSurface, mSurface, mContext))
	{
		Terminate();
		return false;
	}

	if (not CreateFullScreenPipeline())
	{
		Terminate();
		return false;
	}

	mPaused = false;
	mStarted = true;
	return true;
}

void FVdjmAndroidEncoderBackendOpenGL::Stop()
{
	
}

void FVdjmAndroidEncoderBackendOpenGL::Terminate()
{
}

bool FVdjmAndroidEncoderBackendOpenGL::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,
	double timeStampSec)
{
	return FVdjmAndroidEncoderBackend::Running(RHICmdList, srcTexture, timeStampSec);
}

bool FVdjmAndroidEncoderBackendOpenGL::CreateFullScreenPipeline()
{
	static const char* vsSrc = R"(
            #version 300 es
            layout(location = 0) in vec2 InPos;
            layout(location = 1) in vec2 InUV;
            out vec2 UV;
            void main()
            {
                UV = InUV;
                gl_Position = vec4(InPos, 0.0, 1.0);
            }
        )";

        static const char* fsSrc = R"(
            #version 300 es
            precision mediump float;
            in vec2 UV;
            layout(location = 0) out vec4 OutColor;
            uniform sampler2D SrcTex;
            void main()
            {
                OutColor = texture(SrcTex, UV);
            }
        )";

        const GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSrc);
        const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc);
        if (!vs || !fs)
            return false;

        mProgram = glCreateProgram();
        glAttachShader(mProgram, vs);
        glAttachShader(mProgram, fs);
        glLinkProgram(mProgram);

        GLint linked = 0;
        glGetProgramiv(mProgram, GL_LINK_STATUS, &linked);

        glDeleteShader(vs);
        glDeleteShader(fs);

        if (!linked)
            return false;

        const float quad[] =
        {
            // pos      // uv
            -1.f, -1.f, 0.f, 0.f,
             1.f, -1.f, 1.f, 0.f,
            -1.f,  1.f, 0.f, 1.f,
             1.f,  1.f, 1.f, 1.f,
        };

        glGenVertexArrays(1, &mVao);
        glGenBuffers(1, &mVbo);

        glBindVertexArray(mVao);
        glBindBuffer(GL_ARRAY_BUFFER, mVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        mTextureLoc = glGetUniformLocation(mProgram, "SrcTex");
        return true;
}

void FVdjmAndroidEncoderBackendOpenGL::DestroyFullScreenPipeline()
{
}

GLuint FVdjmAndroidEncoderBackendOpenGL::CompileShader(GLenum shaderType, const char* shaderSource)
{
}
#endif
