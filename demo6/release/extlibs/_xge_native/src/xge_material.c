int xgeShaderCreate(xge_shader pShader, const char* sVertexSource, const char* sFragmentSource)
{
	GLuint iVS;
	GLuint iFS;
	GLint bSuccess;
	char arrLog[512];

	if ( (pShader == NULL) || (sVertexSource == NULL) || (sFragmentSource == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	memset(pShader, 0, sizeof(*pShader));
	iVS = __xgeCompileShader(GL_VERTEX_SHADER, sVertexSource);
	iFS = __xgeCompileShader(GL_FRAGMENT_SHADER, sFragmentSource);
	if ( (iVS == 0) || (iFS == 0) ) {
		if ( iVS != 0 ) {
			glDeleteShader(iVS);
		}
		if ( iFS != 0 ) {
			glDeleteShader(iFS);
		}
		return XGE_ERROR_GPU_FAILED;
	}
	pShader->iProgram = (uint32_t)glCreateProgram();
	glAttachShader((GLuint)pShader->iProgram, iVS);
	glAttachShader((GLuint)pShader->iProgram, iFS);
	glLinkProgram((GLuint)pShader->iProgram);
	glGetProgramiv((GLuint)pShader->iProgram, GL_LINK_STATUS, &bSuccess);
	glDeleteShader(iVS);
	glDeleteShader(iFS);
	if ( bSuccess == 0 ) {
		glGetProgramInfoLog((GLuint)pShader->iProgram, 512, NULL, arrLog);
		xrtSetError(arrLog, false);
		__xgeLogFormat(XGE_LOG_ERROR, "graphics", "material program link failed: %s", arrLog);
		glDeleteProgram((GLuint)pShader->iProgram);
		memset(pShader, 0, sizeof(*pShader));
		return XGE_ERROR_GPU_FAILED;
	}
	pShader->iRefCount = 1;
	pShader->iLocResolution = glGetUniformLocation((GLuint)pShader->iProgram, "uResolution");
	pShader->iLocTexture = glGetUniformLocation((GLuint)pShader->iProgram, "uTexture");
	pShader->iLocTexture2 = glGetUniformLocation((GLuint)pShader->iProgram, "uTexture2");
	pShader->iLocTexture3 = glGetUniformLocation((GLuint)pShader->iProgram, "uTexture3");
	pShader->iLocColor = glGetUniformLocation((GLuint)pShader->iProgram, "uColor");
	return XGE_OK;
}

int xgeShaderAddRef(xge_shader pShader)
{
	if ( pShader == NULL ) {
		return 0;
	}
	if ( pShader->iRefCount < INT32_MAX ) {
		pShader->iRefCount++;
	}
	return pShader->iRefCount;
}

void xgeShaderFree(xge_shader pShader)
{
	if ( pShader == NULL ) {
		return;
	}
	if ( pShader->iRefCount > 1 ) {
		pShader->iRefCount--;
		return;
	}
	if ( (pShader->iProgram != 0) && (g_xge.bSokolRunning != 0) && (glDeleteProgram != NULL) ) {
		glDeleteProgram((GLuint)pShader->iProgram);
	}
	memset(pShader, 0, sizeof(*pShader));
}

static int __xgeShaderUniformLocation(xge_shader pShader, const char* sName, GLint* pLocation)
{
	if ( (pShader == NULL) || (sName == NULL) || (pLocation == NULL) || (pShader->iProgram == 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (g_xge.bSokolRunning == 0) || (glGetUniformLocation == NULL) ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	*pLocation = glGetUniformLocation((GLuint)pShader->iProgram, sName);
	if ( *pLocation < 0 ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	glUseProgram((GLuint)pShader->iProgram);
	return XGE_OK;
}

int xgeShaderUniform1i(xge_shader pShader, const char* sName, int iX)
{
	GLint iLoc;
	int iRet;

	iRet = __xgeShaderUniformLocation(pShader, sName, &iLoc);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	glUniform1i(iLoc, iX);
	glUseProgram(0);
	return XGE_OK;
}

int xgeShaderUniform1f(xge_shader pShader, const char* sName, float fX)
{
	GLint iLoc;
	int iRet;

	iRet = __xgeShaderUniformLocation(pShader, sName, &iLoc);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	glUniform1f(iLoc, fX);
	glUseProgram(0);
	return XGE_OK;
}

int xgeShaderUniform2f(xge_shader pShader, const char* sName, float fX, float fY)
{
	GLint iLoc;
	int iRet;

	iRet = __xgeShaderUniformLocation(pShader, sName, &iLoc);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	glUniform2f(iLoc, fX, fY);
	glUseProgram(0);
	return XGE_OK;
}

int xgeShaderUniform3f(xge_shader pShader, const char* sName, float fX, float fY, float fZ)
{
	GLint iLoc;
	int iRet;

	iRet = __xgeShaderUniformLocation(pShader, sName, &iLoc);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	glUniform3f(iLoc, fX, fY, fZ);
	glUseProgram(0);
	return XGE_OK;
}

int xgeShaderUniform4f(xge_shader pShader, const char* sName, float fX, float fY, float fZ, float fW)
{
	GLint iLoc;
	int iRet;

	iRet = __xgeShaderUniformLocation(pShader, sName, &iLoc);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	glUniform4f(iLoc, fX, fY, fZ, fW);
	glUseProgram(0);
	return XGE_OK;
}

static char* __xgeShaderTextCopy(const char* sText)
{
	char* sRet;
	size_t iLen;

	if ( sText == NULL ) {
		return NULL;
	}
	iLen = strlen(sText);
	if ( iLen >= (SIZE_MAX - 1u) ) {
		return NULL;
	}
	sRet = (char*)xrtMalloc(iLen + 1u);
	if ( sRet == NULL ) {
		return NULL;
	}
	memcpy(sRet, sText, iLen + 1u);
	return sRet;
}

static int __xgeShaderDefineNameValid(const char* sName)
{
	int i;

	if ( (sName == NULL) || (sName[0] == 0) ) {
		return 0;
	}
	if ( !(((sName[0] >= 'A') && (sName[0] <= 'Z')) || ((sName[0] >= 'a') && (sName[0] <= 'z')) || (sName[0] == '_')) ) {
		return 0;
	}
	for ( i = 1; sName[i] != 0; i++ ) {
		if ( i >= XGE_SHADER_DEFINE_NAME_MAX ) {
			return 0;
		}
		if ( !(((sName[i] >= 'A') && (sName[i] <= 'Z')) || ((sName[i] >= 'a') && (sName[i] <= 'z')) || ((sName[i] >= '0') && (sName[i] <= '9')) || (sName[i] == '_')) ) {
			return 0;
		}
	}
	return 1;
}

static size_t __xgeShaderVariantPrefixSize(uint32_t iKey, const xge_shader_define_t* pDefines, int iDefineCount)
{
	char arrLine[128];
	size_t iSize;
	int i;

	iSize = (size_t)snprintf(arrLine, sizeof(arrLine), "#define XGE_VARIANT_KEY %u\n", iKey);
	for ( i = 0; i < iDefineCount; i++ ) {
		iSize += (size_t)snprintf(arrLine, sizeof(arrLine), "#define %s %d\n", pDefines[i].sName, pDefines[i].iValue);
	}
	return iSize;
}

static char* __xgeShaderVariantBuildSource(const char* sSource, uint32_t iKey, const xge_shader_define_t* pDefines, int iDefineCount)
{
	char arrLine[128];
	char* sRet;
	char* sOut;
	const char* sTail;
	const char* sLineEnd;
	size_t iHeadSize;
	size_t iTailSize;
	size_t iPrefixSize;
	size_t iLineSize;
	int i;

	if ( sSource == NULL ) {
		return NULL;
	}
	iHeadSize = 0;
	if ( strncmp(sSource, "#version", 8) == 0 ) {
		sLineEnd = strchr(sSource, '\n');
		if ( sLineEnd != NULL ) {
			iHeadSize = (size_t)(sLineEnd - sSource) + 1u;
		}
	}
	sTail = sSource + iHeadSize;
	iTailSize = strlen(sTail);
	iPrefixSize = __xgeShaderVariantPrefixSize(iKey, pDefines, iDefineCount);
	if ( (iHeadSize > SIZE_MAX - iPrefixSize) || ((iHeadSize + iPrefixSize) > SIZE_MAX - iTailSize) || ((iHeadSize + iPrefixSize + iTailSize) >= SIZE_MAX) ) {
		return NULL;
	}
	sRet = (char*)xrtMalloc(iHeadSize + iPrefixSize + iTailSize + 1u);
	if ( sRet == NULL ) {
		return NULL;
	}
	sOut = sRet;
	if ( iHeadSize > 0 ) {
		memcpy(sOut, sSource, iHeadSize);
		sOut += iHeadSize;
	}
	iLineSize = (size_t)snprintf(arrLine, sizeof(arrLine), "#define XGE_VARIANT_KEY %u\n", iKey);
	memcpy(sOut, arrLine, iLineSize);
	sOut += iLineSize;
	for ( i = 0; i < iDefineCount; i++ ) {
		iLineSize = (size_t)snprintf(arrLine, sizeof(arrLine), "#define %s %d\n", pDefines[i].sName, pDefines[i].iValue);
		memcpy(sOut, arrLine, iLineSize);
		sOut += iLineSize;
	}
	memcpy(sOut, sTail, iTailSize + 1u);
	return sRet;
}

int xgeShaderVariantSetInit(xge_shader_variant_set pSet, const char* sVertexSource, const char* sFragmentSource)
{
	if ( (pSet == NULL) || (sVertexSource == NULL) || (sFragmentSource == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pSet, 0, sizeof(*pSet));
	pSet->sVertexSource = __xgeShaderTextCopy(sVertexSource);
	pSet->sFragmentSource = __xgeShaderTextCopy(sFragmentSource);
	if ( (pSet->sVertexSource == NULL) || (pSet->sFragmentSource == NULL) ) {
		xgeShaderVariantSetFree(pSet);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	return XGE_OK;
}

void xgeShaderVariantSetFree(xge_shader_variant_set pSet)
{
	xge_shader_variant pVariant;
	xge_shader_variant pNext;

	if ( pSet == NULL ) {
		return;
	}
	pVariant = pSet->pVariants;
	while ( pVariant != NULL ) {
		pNext = pVariant->pNext;
		xgeShaderFree(&pVariant->tShader);
		xrtFree(pVariant);
		pVariant = pNext;
	}
	if ( pSet->sVertexSource != NULL ) {
		xrtFree(pSet->sVertexSource);
	}
	if ( pSet->sFragmentSource != NULL ) {
		xrtFree(pSet->sFragmentSource);
	}
	memset(pSet, 0, sizeof(*pSet));
}

int xgeShaderVariantGet(xge_shader_variant_set pSet, uint32_t iKey, const xge_shader_define_t* pDefines, int iDefineCount, xge_shader* ppShader)
{
	xge_shader_variant pVariant;
	char* sVertexSource;
	char* sFragmentSource;
	int iRet;
	int i;

	if ( (pSet == NULL) || (ppShader == NULL) || (pSet->sVertexSource == NULL) || (pSet->sFragmentSource == NULL) || (iDefineCount < 0) || (iDefineCount > XGE_SHADER_DEFINE_MAX) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iDefineCount > 0) && (pDefines == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iDefineCount; i++ ) {
		if ( !__xgeShaderDefineNameValid(pDefines[i].sName) ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	pVariant = pSet->pVariants;
	while ( pVariant != NULL ) {
		if ( pVariant->iKey == iKey ) {
			*ppShader = &pVariant->tShader;
			return XGE_OK;
		}
		pVariant = pVariant->pNext;
	}
	pVariant = (xge_shader_variant)xrtMalloc(sizeof(*pVariant));
	if ( pVariant == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memset(pVariant, 0, sizeof(*pVariant));
	pVariant->iKey = iKey;
	pVariant->iDefineCount = iDefineCount;
	for ( i = 0; i < iDefineCount; i++ ) {
		pVariant->arrDefines[i] = pDefines[i];
	}
	sVertexSource = __xgeShaderVariantBuildSource(pSet->sVertexSource, iKey, pDefines, iDefineCount);
	sFragmentSource = __xgeShaderVariantBuildSource(pSet->sFragmentSource, iKey, pDefines, iDefineCount);
	if ( (sVertexSource == NULL) || (sFragmentSource == NULL) ) {
		if ( sVertexSource != NULL ) {
			xrtFree(sVertexSource);
		}
		if ( sFragmentSource != NULL ) {
			xrtFree(sFragmentSource);
		}
		xrtFree(pVariant);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = xgeShaderCreate(&pVariant->tShader, sVertexSource, sFragmentSource);
	xrtFree(sVertexSource);
	xrtFree(sFragmentSource);
	if ( iRet != XGE_OK ) {
		xrtFree(pVariant);
		return iRet;
	}
	pVariant->pNext = pSet->pVariants;
	pSet->pVariants = pVariant;
	pSet->iCount++;
	*ppShader = &pVariant->tShader;
	return XGE_OK;
}

void xgeMaterialInit(xge_material pMaterial)
{
	if ( pMaterial == NULL ) {
		return;
	}
	memset(pMaterial, 0, sizeof(*pMaterial));
	pMaterial->iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
	pMaterial->tPipeline.iBlend = XGE_MATERIAL_DEFAULT_BLEND;
}

void xgeMaterialFree(xge_material pMaterial)
{
	if ( pMaterial == NULL ) {
		return;
	}
	if ( pMaterial->pTexture != NULL ) {
		xgeTextureFree(pMaterial->pTexture);
	}
	if ( pMaterial->pTexture2 != NULL ) {
		xgeTextureFree(pMaterial->pTexture2);
	}
	if ( pMaterial->pTexture3 != NULL ) {
		xgeTextureFree(pMaterial->pTexture3);
	}
	memset(pMaterial, 0, sizeof(*pMaterial));
}

void xgeMaterialSetShader(xge_material pMaterial, xge_shader pShader)
{
	if ( pMaterial != NULL ) {
		pMaterial->pShader = pShader;
	}
}

void xgeMaterialSetTexture(xge_material pMaterial, xge_texture pTexture)
{
	if ( pMaterial == NULL ) {
		return;
	}
	if ( pMaterial->pTexture == pTexture ) {
		return;
	}
	if ( pMaterial->pTexture != NULL ) {
		xgeTextureFree(pMaterial->pTexture);
	}
	pMaterial->pTexture = pTexture;
	if ( pTexture != NULL ) {
		xgeTextureAddRef(pTexture);
	}
}

void xgeMaterialSetTexture2(xge_material pMaterial, xge_texture pTexture)
{
	if ( pMaterial == NULL ) {
		return;
	}
	if ( pMaterial->pTexture2 == pTexture ) {
		return;
	}
	if ( pMaterial->pTexture2 != NULL ) {
		xgeTextureFree(pMaterial->pTexture2);
	}
	pMaterial->pTexture2 = pTexture;
	if ( pTexture != NULL ) {
		xgeTextureAddRef(pTexture);
	}
}

void xgeMaterialSetTexture3(xge_material pMaterial, xge_texture pTexture)
{
	if ( pMaterial == NULL ) {
		return;
	}
	if ( pMaterial->pTexture3 == pTexture ) {
		return;
	}
	if ( pMaterial->pTexture3 != NULL ) {
		xgeTextureFree(pMaterial->pTexture3);
	}
	pMaterial->pTexture3 = pTexture;
	if ( pTexture != NULL ) {
		xgeTextureAddRef(pTexture);
	}
}

void xgeMaterialSetColor(xge_material pMaterial, uint32_t iColor)
{
	if ( pMaterial != NULL ) {
		pMaterial->iColor = iColor;
	}
}

void xgeMaterialSetBlend(xge_material pMaterial, int iBlend)
{
	if ( pMaterial != NULL ) {
		pMaterial->tPipeline.iBlend = iBlend;
	}
}

static void __xgeDrawBuildVertices10(const xge_draw_t* pDraw, xge_texture pTexture, float* pVertices, uint32_t iColor)
{
	float fSrcX;
	float fSrcY;
	float fSrcW;
	float fSrcH;
	float fU0;
	float fV0;
	float fU1;
	float fV1;
	float fLocalX[4];
	float fLocalY[4];
	float fU[4];
	float fV[4];
	float fCos;
	float fSin;
	float fR;
	float fG;
	float fB;
	float fA;
	int i;
	xge_vec2_t tScreen;
	xge_vec2_t tWorld;

	fSrcX = pDraw->tSrc.fX;
	fSrcY = pDraw->tSrc.fY;
	fSrcW = pDraw->tSrc.fW;
	fSrcH = pDraw->tSrc.fH;
	if ( fSrcW == 0.0f ) {
		fSrcW = (float)pTexture->iWidth;
	}
	if ( fSrcH == 0.0f ) {
		fSrcH = (float)pTexture->iHeight;
	}
	fU0 = fSrcX / (float)pTexture->iWidth;
	fV0 = fSrcY / (float)pTexture->iHeight;
	fU1 = (fSrcX + fSrcW) / (float)pTexture->iWidth;
	fV1 = (fSrcY + fSrcH) / (float)pTexture->iHeight;
	if ( (pDraw->iFlags & XGE_DRAW_FLIP_X) != 0 ) {
		float fTmp = fU0;
		fU0 = fU1;
		fU1 = fTmp;
	}
	if ( (pDraw->iFlags & XGE_DRAW_FLIP_Y) != 0 ) {
		float fTmp = fV0;
		fV0 = fV1;
		fV1 = fTmp;
	}
	fU[0] = fU0; fV[0] = fV0;
	fU[1] = fU1; fV[1] = fV0;
	fU[2] = fU0; fV[2] = fV1;
	fU[3] = fU1; fV[3] = fV1;
	fLocalX[0] = -pDraw->tOrigin.fX;
	fLocalY[0] = -pDraw->tOrigin.fY;
	fLocalX[1] = pDraw->tDst.fW - pDraw->tOrigin.fX;
	fLocalY[1] = -pDraw->tOrigin.fY;
	fLocalX[2] = -pDraw->tOrigin.fX;
	fLocalY[2] = pDraw->tDst.fH - pDraw->tOrigin.fY;
	fLocalX[3] = pDraw->tDst.fW - pDraw->tOrigin.fX;
	fLocalY[3] = pDraw->tDst.fH - pDraw->tOrigin.fY;
	fCos = cosf(pDraw->fRotation);
	fSin = sinf(pDraw->fRotation);
	__xgeColorToFloat(iColor, &fR, &fG, &fB, &fA);
	for ( i = 0; i < 4; i++ ) {
		tWorld.fX = pDraw->tDst.fX + (fLocalX[i] * fCos) - (fLocalY[i] * fSin);
		tWorld.fY = pDraw->tDst.fY + (fLocalX[i] * fSin) + (fLocalY[i] * fCos);
		if ( (pDraw->iFlags & XGE_DRAW_SCREEN_SPACE) != 0 ) {
			tScreen = tWorld;
		} else {
			tScreen = xgeWorldToScreen(tWorld);
		}
		pVertices[(i * 10) + 0] = tScreen.fX;
		pVertices[(i * 10) + 1] = tScreen.fY;
		pVertices[(i * 10) + 2] = 0.0f;
		pVertices[(i * 10) + 3] = 1.0f;
		pVertices[(i * 10) + 4] = fU[i];
		pVertices[(i * 10) + 5] = fV[i];
		pVertices[(i * 10) + 6] = fR;
		pVertices[(i * 10) + 7] = fG;
		pVertices[(i * 10) + 8] = fB;
		pVertices[(i * 10) + 9] = fA;
	}
}

void xgeMaterialDraw(xge_material pMaterial, const xge_draw_t* pDraw)
{
	float arrVertices[40];
	xge_shader pShader;
	xge_texture pTexture;
	int iOldBlend;

	if ( (pMaterial == NULL) || (pDraw == NULL) || (pMaterial->pShader == NULL) ) {
		return;
	}
	pShader = pMaterial->pShader;
	pTexture = pMaterial->pTexture ? pMaterial->pTexture : pDraw->pTexture;
	if ( (pTexture == NULL) || (pTexture->iBackendId == 0) || (pShader->iProgram == 0) ) {
		return;
	}
	if ( (pMaterial->pTexture2 != NULL) && (pMaterial->pTexture2->iBackendId == 0) ) {
		return;
	}
	if ( (pMaterial->pTexture3 != NULL) && (pMaterial->pTexture3->iBackendId == 0) ) {
		return;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return;
	}
	if ( __xgeQuad3DRendererInit() != XGE_OK ) {
		return;
	}
	if ( pMaterial->tPipeline.iBlend != XGE_MATERIAL_DEFAULT_BLEND ) {
		iOldBlend = xgeBlendGet();
		xgeBlendSet(pMaterial->tPipeline.iBlend);
	} else {
		iOldBlend = XGE_MATERIAL_DEFAULT_BLEND;
	}
	__xgeDrawBuildVertices10(pDraw, pTexture, arrVertices, pMaterial->iColor);
	glUseProgram((GLuint)pShader->iProgram);
	if ( pShader->iLocResolution >= 0 ) {
		glUniform2f(pShader->iLocResolution, (float)g_xge.iWidth, (float)g_xge.iHeight);
	}
	if ( pShader->iLocTexture >= 0 ) {
		glUniform1i(pShader->iLocTexture, 0);
	}
	if ( pShader->iLocTexture2 >= 0 ) {
		glUniform1i(pShader->iLocTexture2, 1);
	}
	if ( pShader->iLocTexture3 >= 0 ) {
		glUniform1i(pShader->iLocTexture3, 2);
	}
	if ( pShader->iLocColor >= 0 ) {
		float fR, fG, fB, fA;
		__xgeColorToFloat(pMaterial->iColor, &fR, &fG, &fB, &fA);
		glUniform4f(pShader->iLocColor, fR, fG, fB, fA);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, (GLuint)pTexture->iBackendId);
	if ( pMaterial->pTexture2 != NULL ) {
		glActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_2D, (GLuint)pMaterial->pTexture2->iBackendId);
		glActiveTexture(GL_TEXTURE0);
	}
	if ( pMaterial->pTexture3 != NULL ) {
		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, (GLuint)pMaterial->pTexture3->iBackendId);
		glActiveTexture(GL_TEXTURE0);
	}
	glBindVertexArray(g_xgeQuad3DRenderer.iVAO);
	glBindBuffer(GL_ARRAY_BUFFER, g_xgeQuad3DRenderer.iVBO);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(4 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(arrVertices), arrVertices);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	if ( pMaterial->pTexture2 != NULL ) {
		glActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
	}
	if ( pMaterial->pTexture3 != NULL ) {
		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	if ( iOldBlend != XGE_MATERIAL_DEFAULT_BLEND ) {
		xgeBlendSet(iOldBlend);
	}
	__xgeFrameStatsAddDrawCall();
}
