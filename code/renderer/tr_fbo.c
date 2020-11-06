// tr_fbo -- frame buffer object handling
#ifdef USE_RENDERER_GLSLFBO
#include "tr_local.h"
#include "tr_fbo.h"

void InitFrameBufferAndRenderBuffer( void ) {
	GLenum target;
	GLenum err;
	GLenum status;
	int width;
	int height;
	GLint maxSamples;
	int samples = 0;

	tr.usingFrameBufferObject = qfalse;
	tr.usingMultiSample = qfalse;
	tr.usingFboStencil = qfalse;

	if ( !r_enableFbo->integer || !fbo ) {
		return;
	}

	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

	GL_SelectTexture(0);

	target = GL_TEXTURE_RECTANGLE_ARB;

	qglDisable(GL_TEXTURE_2D);
	qglEnable(target);

	// create framebuffer object
	qglGenFramebuffersEXT(1, &tr.frameBuffer);
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tr.frameBuffer);

	// create scene texture
	qglGenTextures(1, &tr.sceneTexture);
	qglBindTexture(target, tr.sceneTexture);
	qglTexImage2D(target, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

	err = qglGetError();
	if (err != GL_NO_ERROR) {
		Com_Printf( "^1OpenGL error creating offscreen render texture: 0x%x\n", err );
	}
	qglTexParameteri(target, GL_TEXTURE_WRAP_S, r_glClampToEdge->integer ? GL_CLAMP_TO_EDGE : GL_CLAMP);
	qglTexParameteri(target, GL_TEXTURE_WRAP_T, r_glClampToEdge->integer ? GL_CLAMP_TO_EDGE : GL_CLAMP);
	qglTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, target, tr.sceneTexture, 0);

	// create depth texture
	if (fboStencil) {
		tr.usingFboStencil = qtrue;
		qglGenTextures(1, &tr.depthTexture);
		qglBindTexture(target, tr.depthTexture);
		qglTexImage2D(target, 0, GL_DEPTH24_STENCIL8_EXT, width, height, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
		err = qglGetError();
		if (err != GL_NO_ERROR) {
			Com_Printf( "^1OpenGL error creating offscreen depth/stencil texture: 0x%x\n", err );
		}
		qglTexParameteri(target, GL_TEXTURE_WRAP_S, r_glClampToEdge->integer ? GL_CLAMP_TO_EDGE : GL_CLAMP);
		qglTexParameteri(target, GL_TEXTURE_WRAP_T, r_glClampToEdge->integer ? GL_CLAMP_TO_EDGE : GL_CLAMP);
		qglTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		qglTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qglTexParameteri(target, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);

		qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, target, tr.depthTexture, 0);
    qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, target, tr.depthTexture, 0);
	} else {
		qglGenRenderbuffersEXT(1, &tr.depthBuffer);
		qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, tr.depthBuffer);
		qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, width, height);
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, tr.depthBuffer);
	}

	status = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if ( status == GL_FRAMEBUFFER_COMPLETE_EXT ) {
		tr.usingFrameBufferObject = qtrue;
	} else {
		Com_Printf( "^1%s framebuffer error: 0x%x\n", __FUNCTION__, status );
		switch(status) {
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			Com_Printf( "^1attachment\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			Com_Printf( "^1missing attachment\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			Com_Printf( "^1dimensions\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
			Com_Printf( "^1formats\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			Com_Printf( "^1draw buffer\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			Com_Printf( "^1read buffer\n" );
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			Com_Printf( "^1unsupported\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT:
			Com_Printf("^1incomplete multisample buffer\n");
			break;
		default:
			Com_Printf( "^1unknown error\n" );
		}

		tr.usingFrameBufferObject = qfalse;
	}


	err = qglGetError();
	if ( err != GL_NO_ERROR ) {
		Com_Printf( "^1OpenGL error end of framebuffer init: 0x%x\n", err );
	}

	if ( !fboMultiSample || !tr.usingFrameBufferObject ) {
		qglDisable(target);
		qglEnable(GL_TEXTURE_2D);
		return;
	}

	glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamples);
	samples = r_fboAntiAlias->integer;
	if (samples > maxSamples) {
		samples = maxSamples;
	}
	if (samples < 0) {
		samples = 0;
	}

	if (samples <= 0) {
		qglDisable(target);
		qglEnable(GL_TEXTURE_2D);
		return;
	}

	////////////////////////////////////////////

	qglGenFramebuffersEXT(1, &tr.frameBufferMultiSample);
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tr.frameBufferMultiSample);

	qglGenRenderbuffersEXT(1, &tr.renderBufferMultiSample);
	qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, tr.renderBufferMultiSample);

	Com_Printf( "anti-alias samples: %d\n", samples );
	qglRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, samples, GL_RGB8, width, height);
	qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, tr.renderBufferMultiSample);

	err = qglGetError();
	if (err != GL_NO_ERROR) {
		Com_Printf( "^1OpenGL error creating framebuffer: 0x%x\n", err );
	}

	qglGenRenderbuffersEXT(1, &tr.depthBufferMultiSample);
	qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, tr.depthBufferMultiSample);
	if (fboStencil) {
		qglRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, samples, GL_DEPTH24_STENCIL8_EXT, width, height);
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, tr.depthBufferMultiSample);
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, tr.depthBufferMultiSample);
	} else {
		qglRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, samples, GL_DEPTH_COMPONENT24, width, height);
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, tr.depthBufferMultiSample);
	}

	err = qglGetError();
	if (err) {
		switch(err) {
		case GL_INVALID_VALUE:
			Com_Printf( "^1OpenGL error creating multisample depth render buffer: (invalid values for width and/or height)\n" );
			break;
		case GL_OUT_OF_MEMORY:
			Com_Printf( "^1OpenGL error creating multisample depth render buffer: (gl out of memory)\n" );
			break;
		default:
			Com_Printf( "^1OpenGL error creating multisample depth render buffer: 0x%x\n", err );
		}
	}

	status = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if (status == GL_FRAMEBUFFER_COMPLETE_EXT) {
		tr.usingFrameBufferObject = qtrue;
		tr.usingMultiSample = qtrue;
	} else {
		Com_Printf( "^1%s multisample framebuffer error: 0x%x\n", __FUNCTION__, status );
		switch(status) {
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			Com_Printf( "^1attachment\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			Com_Printf( "^1missing attachment\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			Com_Printf( "^1dimensions\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
			Com_Printf( "^1formats\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			Com_Printf( "^1draw buffer\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			Com_Printf( "^1read buffer\n" );
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			Com_Printf( "^1unsupported\n" );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT:
			Com_Printf( "^1incomplete multisample buffer\n" );
			break;
		default:
			Com_Printf( "^1unknown error\n" );
		}

		tr.usingFrameBufferObject = qfalse;
		tr.usingMultiSample = qfalse;
	}

	qglDisable(target);
	qglEnable(GL_TEXTURE_2D);

	err = qglGetError();
	if (err != GL_NO_ERROR) {
		Com_Printf( "^1OpenGL error end of multisample framebuffer init: 0x%x\n", err );
	}
}


void R_DeleteFramebufferObject( void ) {
	if ( !tr.usingFrameBufferObject ) {
		return;
	}

	if ( !r_enableFbo->integer || !fbo ) {
		return;
	}

	if (tr.depthTexture) {
		qglDeleteTextures(1, &tr.depthTexture);
		tr.depthTexture = 0;
	}
	if (tr.depthBuffer) {
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
		qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
		qglDeleteRenderbuffersEXT(1, &tr.depthBuffer);
		tr.depthBuffer = 0;
	}
	if (tr.depthBufferMultiSample) {
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
		qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
		qglDeleteRenderbuffersEXT(1, &tr.depthBufferMultiSample);
		tr.depthBufferMultiSample = 0;
	}
	if (tr.renderBufferMultiSample) {
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, 0);
		qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
		qglDeleteRenderbuffersEXT(1, &tr.renderBufferMultiSample);
		tr.renderBufferMultiSample = 0;
	}

	if (tr.frameBuffer) {
		// use default framebuffer
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		qglDeleteFramebuffersEXT(1, &tr.frameBuffer);
		tr.frameBuffer = 0;
	}
	if (tr.frameBufferMultiSample) {
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		qglDeleteFramebuffersEXT(1, &tr.frameBufferMultiSample);
		tr.frameBufferMultiSample = 0;
	}

	if (tr.sceneTexture) {
		qglDeleteTextures(1, &tr.sceneTexture);
		tr.sceneTexture = 0;
	}
}
#endif
