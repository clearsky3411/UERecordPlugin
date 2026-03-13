// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendOpenGL.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
bool FVdjmAndroidEncoderBackendOpenGL::Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow)
{
	if (config.IsValidateEncoderArguments() && inputWindow != nullptr)
	{
		mConfig = config;
		mInputWindow = inputWindow;
		return true;
	}
	return false;
}

bool FVdjmAndroidEncoderBackendOpenGL::Start()
{
	ANativeWindow* window = mInputWindow;
	if (window==nullptr)
	{
		return false;
	}
	//
	// mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	// if (mDisplay == EGL_NO_DISPLAY)
	// {
	// 	return false;
	// }
	//
	// if (!eglInitialize(mDisplay, nullptr, nullptr))
	// {
	// 	return false;
	// }
	//
	// const EGLint configAttribs[] =
	// {
	// 	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
	// 	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	// 	EGL_RED_SIZE, 8,
	// 	EGL_GREEN_SIZE, 8,
	// 	EGL_BLUE_SIZE, 8,
	// 	EGL_ALPHA_SIZE, 8,
	// 	EGL_NONE
	// };
	//
	// EGLint numConfigs = 0;
	// if (!eglChooseConfig(mDisplay, configAttribs, &mEglConfig, 1, &numConfigs) || numConfigs == 0)
	// {
	// 	return false;
	// }
	//
	// const EGLint contextAttribs[] =
	// {
	// 	EGL_CONTEXT_CLIENT_VERSION, 3,
	// 	EGL_NONE
	// };
	//
	// mContext = eglCreateContext(mDisplay, mEglConfig, EGL_NO_CONTEXT, contextAttribs);
	// if (mContext == EGL_NO_CONTEXT)
	// {
	// 	return false;
	// }
	//
	// mSurface = eglCreateWindowSurface(mDisplay, mEglConfig, window, nullptr);
	// if (mSurface == EGL_NO_SURFACE)
	// {
	// 	Terminate();
	// 	return false;
	// }
	//
	// if (!eglMakeCurrent(mDisplay, mSurface, mSurface, mContext))
	// {
	// 	Terminate();
	// 	return false;
	// }
	//
	// if (not CreateFullScreenPipeline())
	// {
	// 	Terminate();
	// 	return false;
	// }

	mPaused = false;
	mStarted = true;
	return true;
}

void FVdjmAndroidEncoderBackendOpenGL::Stop()
{
	mStarted = false;
	mPaused = false;
}

void FVdjmAndroidEncoderBackendOpenGL::Terminate()
{
	DestroyFullScreenPipeline();
	if (mDisplay != EGL_NO_DISPLAY)
	{
		eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

		if (mSurface != EGL_NO_SURFACE)
		{
			eglDestroySurface(mDisplay, mSurface);
			mSurface = EGL_NO_SURFACE;
		}

		if (mContext != EGL_NO_CONTEXT)
		{
			eglDestroyContext(mDisplay, mContext);
			mContext = EGL_NO_CONTEXT;
		}

		eglTerminate(mDisplay);
		mDisplay = EGL_NO_DISPLAY;
	}

	mInputWindow = nullptr;
	mStarted = false;
	mPaused = false;
}

bool FVdjmAndroidEncoderBackendOpenGL::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,
	double timeStampSec)
{
	if (!mStarted || mPaused)
	{
		return false;
	}

	if (!srcTexture.IsValid())
	{
		return false;
	}
	
	if (!EnsureEGLContextReady())
	{
		return false;
	}

	void* nativeResource = srcTexture->GetNativeResource();
	if (nativeResource == nullptr)
	{
		return false;
	}

	const GLuint srcTextureName = static_cast<GLuint>(reinterpret_cast<UPTRINT>(nativeResource));
	if (srcTextureName == 0)
	{
		return false;
	}

	// 현재 UE context 저장
	EGLDisplay prevDisplay = eglGetCurrentDisplay();
	EGLSurface prevDraw = eglGetCurrentSurface(EGL_DRAW);
	EGLSurface prevRead = eglGetCurrentSurface(EGL_READ);
	EGLContext prevContext = eglGetCurrentContext();

	if (!eglMakeCurrent(mDisplay, mSurface, mSurface, mContext))
	{
		return false;
	}

	glViewport(0, 0, mConfig.VideoWidth, mConfig.VideoHeight);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glUseProgram(mProgram);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, srcTextureName);
	glUniform1i(mTextureLoc, 0);

	glBindVertexArray(mVao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);

	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

	glFlush();

	const bool bSwapOk = (eglSwapBuffers(mDisplay, mSurface) == EGL_TRUE);

	// UE context 복구
	if (prevDisplay != EGL_NO_DISPLAY && prevContext != EGL_NO_CONTEXT)
	{
		eglMakeCurrent(prevDisplay, prevDraw, prevRead, prevContext);
	}

	return bSwapOk;
}

bool FVdjmAndroidEncoderBackendOpenGL::EnsureEGLContextReady()
{
	if (mDisplay != EGL_NO_DISPLAY &&
		mContext != EGL_NO_CONTEXT &&
		mSurface != EGL_NO_SURFACE)
	{
		return true;
	}
	if (mInputWindow == nullptr)
	{
		return false;
	}
	
	EGLDisplay currentDisplay = eglGetCurrentDisplay();
	EGLContext currentContext = eglGetCurrentContext();

	if (currentDisplay == EGL_NO_DISPLAY || currentContext == EGL_NO_CONTEXT)
	{
		// UE GL context가 없는 스레드에서는 공유 컨텍스트 생성 불가
		return false;
	}
	mDisplay = currentDisplay;
	const EGLint configAttribs[] =
	{
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
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

	mSurface = eglCreateWindowSurface(mDisplay, mEglConfig, mInputWindow, nullptr);
	if (mSurface == EGL_NO_SURFACE)
	{
		Terminate();
		return false;
	}

	const EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};

	// 핵심: UE 현재 컨텍스트와 공유
	mContext = eglCreateContext(mDisplay, mEglConfig, currentContext, contextAttribs);
	if (mContext == EGL_NO_CONTEXT)
	{
		Terminate();
		return false;
	}

	// 파이프라인 생성용으로 잠깐 current
	EGLSurface prevDraw = eglGetCurrentSurface(EGL_DRAW);
	EGLSurface prevRead = eglGetCurrentSurface(EGL_READ);
	EGLContext prevContext = currentContext;

	if (!eglMakeCurrent(mDisplay, mSurface, mSurface, mContext))
	{
		Terminate();
		return false;
	}

	if (!CreateFullScreenPipeline())
	{
		Terminate();
		return false;
	}

	// 원래 UE context 복구
	eglMakeCurrent(mDisplay, prevDraw, prevRead, prevContext);
	return true;
}

bool FVdjmAndroidEncoderBackendOpenGL::CreateFullScreenPipeline()
{
	// 추후에 GLSL 파일로 분리할 수도 있겠지만, 일단은 간단한 풀스크린 텍스처 복사용이므로 소스 내에 직접 작성
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

		if (not linked )
		{
			GLint logLength = 0;
			glGetProgramiv(mProgram, GL_INFO_LOG_LENGTH, &logLength);
	
			if (logLength > 1)
			{
				TArray<ANSICHAR> logBuffer;
				logBuffer.SetNumZeroed(logLength);
				glGetProgramInfoLog(mProgram, logLength, nullptr, logBuffer.GetData());
				UE_LOG(LogTemp, Error, TEXT("OpenGL program link failed: %s"), UTF8_TO_TCHAR(logBuffer.GetData()));
			}
	
			glDeleteProgram(mProgram);
			mProgram = 0;
			return false;
		}

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
	if (mVbo)
	{
		glDeleteBuffers(1, &mVbo);
		mVbo = 0;
	}
    if (mVao)
    {
	    glDeleteVertexArrays(1, &mVao);
    	mVao = 0;
    }
    if (mProgram)
    {
        glDeleteProgram(mProgram);
        mProgram = 0;
    }
}

GLuint FVdjmAndroidEncoderBackendOpenGL::CompileShader(GLenum shaderType, const char* shaderSource)
{
	GLuint shader = glCreateShader(shaderType);
	if (shader == 0)
	{
		return 0;
	}

	glShaderSource(shader, 1, &shaderSource, nullptr);
	glCompileShader(shader);

	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		GLint logLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

		if (logLength > 1)
		{
			TArray<ANSICHAR> logBuffer;
			logBuffer.SetNumZeroed(logLength);
			glGetShaderInfoLog(shader, logLength, nullptr, logBuffer.GetData());
			UE_LOG(LogTemp, Error, TEXT("OpenGL shader compile failed: %s"), UTF8_TO_TCHAR(logBuffer.GetData()));
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}
#endif
