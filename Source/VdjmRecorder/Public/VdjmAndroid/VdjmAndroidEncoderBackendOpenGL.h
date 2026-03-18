// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VdjmRecoderAndroidEncoder.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x00000040
#endif

class FVdjmAndroidEncoderBackendOpenGL : public FVdjmAndroidEncoderBackend
{
public:
	virtual bool Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow) override;
	virtual bool Start() override;
	virtual void Stop() override;
	virtual void Terminate() override;
	virtual bool Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec) override;
private:
	bool EnsureEGLContextReady();
	bool CreateFullScreenPipeline();
	void DestroyFullScreenPipeline();
	static GLuint CompileShader(GLenum shaderType, const char* shaderSource);
	
	FVdjmAndroidEncoderConfigure mConfig;
	
	EGLDisplay mDisplay = EGL_NO_DISPLAY;
	EGLContext mContext = EGL_NO_CONTEXT;
	EGLSurface mSurface = EGL_NO_SURFACE;
	EGLConfig  mEglConfig = nullptr;
	
	GLuint mProgram = 0;
	GLuint mVao = 0;
	GLuint mVbo = 0;
	GLint  mTextureLoc = -1;
	
	bool mStarted = false;
	bool mPaused = false;
	
	static constexpr int32 VertexSize = 4; // x, y, u, v
	static constexpr int32 VertexCount = 4; // 4 vertices for a quad
	
	static constexpr float QuadVertices[VertexSize*VertexCount] = {
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f, 1.0f,
		 1.0f,  1.0f, 1.0f, 1.0f,
	};
};
#endif