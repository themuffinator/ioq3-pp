// tr_glsl -- gl fragment/vertex shader handling and postprocessing
#ifdef USE_RENDERER_GLSLFBO
#include "tr_local.h"
#include "tr_glsl.h"

//============================================================================
// GLSL INITIALISATION
//============================================================================

extern const char *fallbackShader_blurhoriz;
extern const char *fallbackShader_blurvertical;
extern const char *fallbackShader_brightpass;
extern const char *fallbackShader_colorcorrect;
extern const char *fallbackShader_combine;
extern const char *fallbackShader_downsample1;
extern const char *fallbackShader_posteffect;

static void printGlslLog( GLhandleARB obj ) {
	int infoLogLength = 0;
	char infoLog[1024];
	int len;

	qglGetObjectParameterivARB(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &infoLogLength);
	if (infoLogLength > 0) {
		qglGetInfoLogARB(obj, 1024, &len, infoLog);
		Com_VPrintf( "%s\n", infoLog );
	}
}

//static char ShaderExtensions[] = "#version 140\n#extension GL_ARB_texture_rectangle : enable";
static char ShaderExtensions[] = "#extension GL_ARB_texture_rectangle : enable";

static void R_InitFragmentShader( const char *filename, GLhandleARB *fragmentShader, GLhandleARB *program, GLhandleARB vertexShader, const char *fallbackShader ) {
	int 		len;
	int 		slen;
	void 		*shaderSource;
	char 		*text;
	qboolean	fallback = qfalse;

	if ( !glsl ) {
		return;
	}

	Com_VPrintf( "^5%s ->\n", filename );
	*fragmentShader = qglCreateShaderObjectARB( GL_FRAGMENT_SHADER_ARB );
	len = ri.FS_ReadFile( filename, &shaderSource );

	if ( len <= 0 ) {
		len = strlen(fallbackShader);
		if ( len ) {
			Com_VPrintf( "^1using fallback shader\n" );
			//ri.FS_FreeFile(shaderSource);
			//shaderSource = (void *)fallbackShader;
			fallback = qtrue;
		} else {
			Com_VPrintf( "^1couldn't find file\n" );
			R_DeleteGlslShadersAndPrograms();
			glsl = qfalse;
			return;
		}
	}

	slen = strlen(ShaderExtensions);
	text = (char *)malloc(len + slen + 3);
	if (!text) {
		Com_VPrintf( "R_InitFragmentShader() couldn't allocate memory for GLSL shader file\n" );
		if ( !fallbackShader ) ri.FS_FreeFile(shaderSource);
		qglDeleteObjectARB(*fragmentShader);
		return;
	}
	Com_sprintf( text, len + slen + 3, "%s\n%s\n", ShaderExtensions, fallback ? fallbackShader : (char *)shaderSource );
	qglShaderSourceARB(*fragmentShader, 1, (const char **)&text, NULL);
	qglCompileShaderARB(*fragmentShader);
	printGlslLog(*fragmentShader);
	if ( !fallbackShader ) ri.FS_FreeFile(shaderSource);
	free(text);

	*program = qglCreateProgramObjectARB();
	qglAttachObjectARB(*program, vertexShader);
	qglAttachObjectARB(*program, *fragmentShader);
	qglLinkProgramARB(*program);
	printGlslLog(*program);
	//Com_VPrintf("\n");
}

void InitGlslShadersAndPrograms( void ) {
	void *shaderSource;
	GLenum target;
	float bloomTextureScale;
	int ret;

	if ( !r_enablePostProcess->integer || !glsl ) {
		return;
	}

	GL_SelectTexture(0);
	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_TEXTURE_RECTANGLE_ARB );

	bloomTextureScale = r_BloomTextureScale->value;
	if ( bloomTextureScale < 0.01 ) {
		bloomTextureScale = 0.01;
	} else if ( bloomTextureScale > 1 ) {
		bloomTextureScale = 1;
	}
	target = GL_TEXTURE_RECTANGLE_ARB;
	tr.bloomWidth = glConfig.vidWidth * bloomTextureScale;
	tr.bloomHeight = glConfig.vidHeight * bloomTextureScale;
	qglGenTextures(1, &tr.bloomTexture);
	qglBindTexture(target, tr.bloomTexture);
	qglTexImage2D(target, 0, GL_RGBA8, tr.bloomWidth, tr.bloomHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	qglTexParameteri(target, GL_TEXTURE_WRAP_S, r_glClampToEdge->integer ? GL_CLAMP_TO_EDGE : GL_CLAMP);
	qglTexParameteri(target, GL_TEXTURE_WRAP_T, r_glClampToEdge->integer ? GL_CLAMP_TO_EDGE : GL_CLAMP);
	qglTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	target = GL_TEXTURE_RECTANGLE_ARB;
	qglGenTextures(1, &tr.backBufferTexture);
	qglBindTexture(target, tr.backBufferTexture);

	qglTexImage2D(target, 0, GL_RGB8, glConfig.vidWidth, glConfig.vidHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	qglTexParameteri(target, GL_TEXTURE_WRAP_S, r_glClampToEdge->integer ? GL_CLAMP_TO_EDGE : GL_CLAMP);
	qglTexParameteri(target, GL_TEXTURE_WRAP_T, r_glClampToEdge->integer ? GL_CLAMP_TO_EDGE : GL_CLAMP);
	qglTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);

	GL_SelectTexture(0);

	Com_VPrintf("^5scripts/posteffect.vs ->\n");
	ret = ri.FS_ReadFile("scripts/posteffect.vs", &shaderSource);

	if (ret > 0) {
		tr.mainVs = qglCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
		qglShaderSourceARB(tr.mainVs, 1, (const char **)&shaderSource, NULL);
		qglCompileShaderARB(tr.mainVs);
		printGlslLog(tr.mainVs);
		ri.FS_FreeFile(shaderSource);
	} else if ( strlen(fallbackShader_posteffect) ) {
		Com_VPrintf("^1file not found, using fallback shader\n");
		//ri.FS_FreeFile(shaderSource);
		tr.mainVs = qglCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
		qglShaderSourceARB(tr.mainVs, 1, &fallbackShader_posteffect, NULL);
		qglCompileShaderARB(tr.mainVs);
		printGlslLog(tr.mainVs);
	} else {
		Com_VPrintf("^1file not found\n");
		glsl = qfalse;
		R_DeleteGlslShadersAndPrograms();
	}

	R_InitFragmentShader( "scripts/colorcorrect.fs", &tr.colorCorrectFs, &tr.colorCorrectSp, tr.mainVs, fallbackShader_colorcorrect );
	R_InitFragmentShader( "scripts/blurhoriz.fs", &tr.blurHorizFs, &tr.blurHorizSp, tr.mainVs, fallbackShader_blurhoriz );
	R_InitFragmentShader( "scripts/blurvertical.fs", &tr.blurVerticalFs, &tr.blurVerticalSp, tr.mainVs, fallbackShader_blurvertical );
	R_InitFragmentShader( "scripts/brightpass.fs", &tr.brightPassFs, &tr.brightPassSp, tr.mainVs, fallbackShader_brightpass );
	R_InitFragmentShader( "scripts/combine.fs", &tr.combineFs, &tr.combineSp, tr.mainVs, fallbackShader_combine );
	R_InitFragmentShader( "scripts/downsample1.fs", &tr.downSample1Fs, &tr.downSample1Sp, tr.mainVs, fallbackShader_downsample1 );
}

void R_DeleteGlslShadersAndPrograms( void ) {
	if ( !r_enablePostProcess->integer || !glsl ) {
		return;
	}

	// detach

	qglDetachObjectARB(tr.blurHorizSp, tr.blurHorizFs);
	qglDetachObjectARB(tr.blurHorizSp, tr.mainVs);

	qglDetachObjectARB(tr.blurVerticalSp, tr.blurVerticalFs);
	qglDetachObjectARB(tr.blurVerticalSp, tr.mainVs);

	qglDetachObjectARB(tr.brightPassSp, tr.brightPassFs);
	qglDetachObjectARB(tr.brightPassSp, tr.mainVs);

	qglDetachObjectARB(tr.downSample1Sp, tr.downSample1Fs);
	qglDetachObjectARB(tr.downSample1Sp, tr.mainVs);

	qglDetachObjectARB(tr.combineSp, tr.combineFs);
	qglDetachObjectARB(tr.combineSp, tr.mainVs);

	qglDetachObjectARB(tr.colorCorrectSp, tr.colorCorrectFs);
	qglDetachObjectARB(tr.colorCorrectSp, tr.mainVs);

	// delete

	qglDeleteObjectARB(tr.blurHorizSp);
	qglDeleteObjectARB(tr.blurHorizFs);

	qglDeleteObjectARB(tr.blurVerticalSp);
	qglDeleteObjectARB(tr.blurVerticalFs);

	qglDeleteObjectARB(tr.brightPassSp);
	qglDeleteObjectARB(tr.brightPassFs);

	qglDeleteObjectARB(tr.downSample1Sp);
	qglDeleteObjectARB(tr.downSample1Fs);

	qglDeleteObjectARB(tr.combineSp);
	qglDeleteObjectARB(tr.combineFs);

	qglDeleteObjectARB(tr.colorCorrectSp);
	qglDeleteObjectARB(tr.colorCorrectFs);

	qglDeleteObjectARB(tr.mainVs);

	// reset

	tr.blurHorizSp = 0;
	tr.blurHorizFs = 0;
	tr.blurVerticalSp = 0;
	tr.blurVerticalFs = 0;
	tr.brightPassSp = 0;
	tr.brightPassFs = 0;
	tr.downSample1Sp = 0;
	tr.downSample1Fs = 0;
	tr.combineSp = 0;
	tr.combineFs = 0;
	tr.colorCorrectSp = 0;
	tr.colorCorrectFs = 0;
	tr.mainVs = 0;
}

//============================================================================
// POSTPROCESSING BACKEND - BLOOM, COLOR CORRECTION
//============================================================================

static void RB_BloomDownSample( void ) {
	GLenum target;
	int width, height;
	GLint loc;

	GL_SelectTexture(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	target = GL_TEXTURE_RECTANGLE_ARB;

	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

	qglBindTexture(target, tr.backBufferTexture);

	qglCopyTexSubImage2D(target, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight);

	GL_SelectTexture(1);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	qglBindTexture(target, tr.bloomTexture);

	qglUseProgramObjectARB(tr.downSample1Sp);
	loc = qglGetUniformLocationARB(tr.downSample1Sp, "backBufferTex");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get backBufferTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 0);

	//qglDisable(GL_BLEND);

    qglBegin(GL_QUADS);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, 0, 0);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, 0, 0);
	qglVertex2i(0, tr.bloomHeight);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, width, 0);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, width, 0);
	qglVertex2i(tr.bloomWidth, tr.bloomHeight);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, width, height);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, width, height);
	qglVertex2i(tr.bloomWidth, 0);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, 0, height);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, 0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(0);
	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

static void RB_BloomBrightness( void ) {
	GLenum target;
	int width, height;
	GLint loc;

	GL_SelectTexture(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	target = GL_TEXTURE_RECTANGLE_ARB;

	width = tr.bloomWidth;
	height = tr.bloomHeight;

	qglBindTexture(target, tr.bloomTexture);
	qglCopyTexSubImage2D(target, 0, 0, 0, 0, glConfig.vidHeight - height, width, height);

	qglUseProgramObjectARB(tr.brightPassSp);
	loc = qglGetUniformLocationARB(tr.brightPassSp, "backBufferTex");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get backBufferTex", __FUNCTION__);
	}

	qglUniform1iARB(loc, 0);
	loc = qglGetUniformLocationARB(tr.brightPassSp, "p_brightthreshold");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get p_brightthreshold", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_BloomBrightThreshold->value);

	qglBegin(GL_QUADS);

	qglTexCoord2i(0, 0);
	qglVertex2i(0, height);

	qglTexCoord2i(width, 0);
	qglVertex2i(width, height);

	qglTexCoord2i(width, height);
	qglVertex2i(width, 0);

	qglTexCoord2i(0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

static void RB_BloomBlurHorizontal( void ) {
	GLenum target;
	int width, height;
	GLint loc;

	GL_SelectTexture(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	target = GL_TEXTURE_RECTANGLE_ARB;

	width = tr.bloomWidth;
	height = tr.bloomHeight;

	qglBindTexture(target, tr.bloomTexture);
	qglCopyTexSubImage2D(target, 0, 0, 0, 0, glConfig.vidHeight - height, width, height);

	qglUseProgramObjectARB(tr.blurHorizSp);
	loc = qglGetUniformLocationARB(tr.blurHorizSp, "backBufferTex");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get backBufferTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 0);

	qglBegin(GL_QUADS);

	qglTexCoord2i(0, 0);
	qglVertex2i(0, height);

	qglTexCoord2i(width, 0);
	qglVertex2i(width, height);

	qglTexCoord2i(width, height);
	qglVertex2i(width, 0);

	qglTexCoord2i(0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

static void RB_BloomBlurVertical( void ) {
	GLenum target;
	int width, height;
	GLint loc;

	GL_SelectTexture(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	target = GL_TEXTURE_RECTANGLE_ARB;

	width = tr.bloomWidth;
	height = tr.bloomHeight;

	qglBindTexture(target, tr.bloomTexture);
	qglCopyTexSubImage2D(target, 0, 0, 0, 0, glConfig.vidHeight - height, width, height);

	qglUseProgramObjectARB(tr.blurVerticalSp);
	loc = qglGetUniformLocationARB(tr.blurVerticalSp, "backBufferTex");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get backBufferTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 0);

	qglBegin(GL_QUADS);

	qglTexCoord2i(0, 0);
	qglVertex2i(0, height);

	qglTexCoord2i(width, 0);
	qglVertex2i(width, height);

	qglTexCoord2i(width, height);
	qglVertex2i(width, 0);

	qglTexCoord2i(0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

static void RB_BloomCombine( void ) {
	GLenum target;
	int width, height;
	GLint loc;

	GL_SelectTexture(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	target = GL_TEXTURE_RECTANGLE_ARB;

	width = tr.bloomWidth;
	height = tr.bloomHeight;

	qglBindTexture(target, tr.bloomTexture);
	qglCopyTexSubImage2D(target, 0, 0, 0, 0, glConfig.vidHeight - height, width, height);

	qglUseProgramObjectARB(tr.combineSp);

	qglBindTexture(target, tr.backBufferTexture);
	loc = qglGetUniformLocationARB(tr.combineSp, "backBufferTex");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get backBufferTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 0);

	GL_SelectTexture(1);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);

	qglBindTexture(target, tr.bloomTexture);
	loc = qglGetUniformLocationARB(tr.combineSp, "bloomTex");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get bloomTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 1);

	loc = qglGetUniformLocationARB(tr.combineSp, "p_bloomsaturation");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get p_bloomsaturation", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_BloomSaturation->value);

	loc = qglGetUniformLocationARB(tr.combineSp, "p_scenesaturation");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get p_scenesaturation", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_BloomSceneSaturation->value);

	loc = qglGetUniformLocationARB(tr.combineSp, "p_bloomintensity");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get p_bloomintensity", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_BloomIntensity->value);

	loc = qglGetUniformLocationARB(tr.combineSp, "p_sceneintensity");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get p_sceneintensity", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_BloomSceneIntensity->value);


	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

    qglBegin(GL_QUADS);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, 0, 0);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, 0, 0);
	qglVertex2i(0, height);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, width, 0);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, tr.bloomWidth, 0);
	qglVertex2i(width, height);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, width, height);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, tr.bloomWidth, tr.bloomHeight);
	qglVertex2i(width, 0);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, 0, height);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, 0, tr.bloomHeight);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	GL_SelectTexture(1);
	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(0);
	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

static void RB_Bloom( void ) {
	int i;

	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	RB_SetGL2D();
	GL_State(GLS_DEPTHTEST_DISABLE);

	for (i = 0;  i < r_BloomPasses->integer;  i++) {
		RB_BloomDownSample();
		RB_BloomBrightness();
		if (r_BloomDebug->integer < 2) {
			RB_BloomBlurHorizontal();
			RB_BloomBlurVertical();
		}
		if (r_BloomDebug->integer == 0  ||  r_BloomDebug->integer == 3) {
			RB_BloomCombine();
		}
	}
}

void RB_ColorCorrect( void ) {
	GLint loc;
	GLenum target;
	int width, height;
	int shift;
	float mul;

	if ( !r_enablePostProcess->integer || !r_enableColorCorrect->integer || !glsl ) {
		return;
	}

	GL_SelectTexture(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);

	target = GL_TEXTURE_RECTANGLE_ARB;

	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

	qglBindTexture(target, tr.backBufferTexture);
	qglCopyTexSubImage2D(target, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight);

	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	RB_SetGL2D();
	GL_State( GLS_DEPTHTEST_DISABLE );

    qglUseProgramObjectARB(tr.colorCorrectSp);
	loc = qglGetUniformLocationARB(tr.colorCorrectSp, "p_gammaRecip");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get p_gammaRecip", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)(1.0 / r_gamma->value));

	//mul = r_overBrightBitsValue->value;
	mul = r_overBrightBits->value;
	if (mul < 0.0) {
		mul = 0.0;
	}
	shift = tr.overbrightBits;

	loc = qglGetUniformLocationARB(tr.colorCorrectSp, "p_overbright");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get p_overbright", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)((float)(1 << shift) * mul));

	loc = qglGetUniformLocationARB(tr.colorCorrectSp, "p_contrast");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get p_contrast", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_contrast->value);

	loc = qglGetUniformLocationARB(tr.colorCorrectSp, "backBufferTex");
	if (loc < 0) {
		Com_Error(ERR_DROP, "%s() couldn't get backBufferTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 0);

	qglBegin(GL_QUADS);

	qglTexCoord2i(0, 0);
	qglVertex2i(0, height);

	qglTexCoord2i(width, 0);
	qglVertex2i(width, height);

	qglTexCoord2i(width, height);
	qglVertex2i(width, 0);

	qglTexCoord2i(0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

void RB_PostProcessing( void ) {
	//GLenum target;
	//int width, height;

	//Com_Printf("count %d\n", tr.drawSurfsCount);
	if ( tr.drawSurfsCount != 0 ) {
		//return;
	}

	if ( !r_enablePostProcess->integer || !glsl ) {
		return;
	}

	if ( !r_enableBloom->integer ) {
		return;
	}

	//qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// since bloom is using readpixels()
	if (tr.usingMultiSample) {
		qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.frameBufferMultiSample);
		qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.frameBuffer);
		if (tr.usingFboStencil) {
			// packed depth stencil, but apparently some cards have a problem
			// if GL_STENCIL_BUFFER_BIT used
			qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		} else {
			qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		//qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.frameBuffer);
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tr.frameBuffer);
	}

	RB_Bloom();

	// anti alias 2d
	if (tr.usingMultiSample) {
		qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.frameBufferMultiSample);
		qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.frameBuffer);
		if (0) {  //(tr.usingFboStencil) {
			// packed depth stencil, but apparently some cards have a problem
			// if GL_STENCIL_BUFFER_BIT used
			qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		} else {
			qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		//qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.frameBuffer);
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tr.frameBufferMultiSample);
	}

}
#endif //USE_RENDERER_GLSLFBO
