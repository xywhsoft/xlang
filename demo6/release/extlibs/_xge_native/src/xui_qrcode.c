#include "xui_internal.h"

#include <stdlib.h>
#include <string.h>

#define XUI_QRCODE_MAX_SIZE		(21 + (XUI_QRCODE_MAX_VERSION - 1) * 4)
#define XUI_QRCODE_MAX_MODULES		(XUI_QRCODE_MAX_SIZE * XUI_QRCODE_MAX_SIZE)
#define XUI_QRCODE_MAX_CODEWORDS	346
#define XUI_QRCODE_MAX_BLOCKS		5
#define XUI_QRCODE_DEFAULT_SIZE		160.0f
#define XUI_QRCODE_DEFAULT_PADDING	8.0f

typedef struct xui_qrcode_matrix_t {
	uint8_t arrModules[XUI_QRCODE_MAX_MODULES];
	int iVersion;
	int iSize;
} xui_qrcode_matrix_t;

typedef struct xui_qrcode_data_t {
	char* sValue;
	int iValueCapacity;
	xui_surface pIconSurface;
	xui_rect_t tIconSrc;
	uint8_t arrModules[XUI_QRCODE_MAX_MODULES];
	uint32_t iForegroundColor;
	uint32_t iBackgroundColor;
	float fPadding;
	float fIconSize;
	int iMinVersion;
	int iMaxVersion;
	int iVersion;
	int iModuleCount;
	int iChangeCount;
} xui_qrcode_data_t;

static const int g_xuiQrCodeEccCodewordsPerBlock[XUI_QRCODE_MAX_VERSION + 1] = {
	0, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26
};

static const int g_xuiQrCodeNumEccBlocks[XUI_QRCODE_MAX_VERSION + 1] = {
	0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5
};

static int __xuiQrCodeVersionValid(int iVersion)
{
	return (iVersion >= XUI_QRCODE_MIN_VERSION) && (iVersion <= XUI_QRCODE_MAX_VERSION);
}

static int __xuiQrCodeDescValid(const xui_qrcode_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->fPadding < 0.0f) || (pDesc->fIconSize < 0.0f) ) {
		return 0;
	}
	if ( (pDesc->tIconSrc.fW < 0.0f) || (pDesc->tIconSrc.fH < 0.0f) ) {
		return 0;
	}
	if ( pDesc->iMinVersion != 0 && !__xuiQrCodeVersionValid(pDesc->iMinVersion) ) {
		return 0;
	}
	if ( pDesc->iMaxVersion != 0 && !__xuiQrCodeVersionValid(pDesc->iMaxVersion) ) {
		return 0;
	}
	if ( (pDesc->iMinVersion != 0) && (pDesc->iMaxVersion != 0) && (pDesc->iMinVersion > pDesc->iMaxVersion) ) {
		return 0;
	}
	return 1;
}

static int __xuiQrCodeStringSet(char** psText, int* pCapacity, const char* sText)
{
	char* sNew;
	int iNeed;

	if ( (psText == NULL) || (pCapacity == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( sText == NULL ) {
		sText = "";
	}
	iNeed = (int)strlen(sText) + 1;
	if ( iNeed > *pCapacity ) {
		sNew = (char*)xrtRealloc(*psText, (size_t)iNeed);
		if ( sNew == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		*psText = sNew;
		*pCapacity = iNeed;
	}
	memcpy(*psText, sText, (size_t)iNeed);
	return XUI_OK;
}

static int __xuiQrCodeAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static int __xuiQrCodeStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pColor == NULL) ) {
		return 0;
	}
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static int __xuiQrCodeStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pValue == NULL) ) {
		return 0;
	}
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && (tProperty.tValue.fFloat >= 0.0f) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static void __xuiQrCodeResolve(xui_widget pWidget, const xui_qrcode_data_t* pData, xui_qrcode_data_t* pResolved)
{
	*pResolved = *pData;
	(void)__xuiQrCodeStyleColor(pWidget, "qrcode.foreground_color", &pResolved->iForegroundColor);
	(void)__xuiQrCodeStyleColor(pWidget, "qrcode.background_color", &pResolved->iBackgroundColor);
	(void)__xuiQrCodeStyleFloat(pWidget, "qrcode.padding", &pResolved->fPadding);
	(void)__xuiQrCodeStyleFloat(pWidget, "qrcode.icon_size", &pResolved->fIconSize);
}

static xui_qrcode_data_t* __xuiQrCodeGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "qrcode");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_qrcode_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiQrCodeRawDataModules(int iVersion)
{
	int iResult;
	int iNumAlign;

	iResult = (16 * iVersion + 128) * iVersion + 64;
	if ( iVersion >= 2 ) {
		iNumAlign = iVersion / 7 + 2;
		iResult -= (25 * iNumAlign - 10) * iNumAlign - 55;
		if ( iVersion >= 7 ) {
			iResult -= 36;
		}
	}
	return iResult;
}

static int __xuiQrCodeRawCodewords(int iVersion)
{
	return __xuiQrCodeRawDataModules(iVersion) / 8;
}

static int __xuiQrCodeDataCodewords(int iVersion)
{
	return __xuiQrCodeRawCodewords(iVersion) -
	       g_xuiQrCodeEccCodewordsPerBlock[iVersion] * g_xuiQrCodeNumEccBlocks[iVersion];
}

static int __xuiQrCodeCharCountBits(int iVersion)
{
	return (iVersion <= 9) ? 8 : 16;
}

static int __xuiQrCodeAppendBits(uint8_t* pData, int iCapacityBits, int* pBitLen, unsigned int iValue, int iBitCount)
{
	int i;

	if ( (pData == NULL) || (pBitLen == NULL) || (iBitCount < 0) || ((*pBitLen + iBitCount) > iCapacityBits) ) {
		return XUI_ERROR_BUFFER_TOO_SMALL;
	}
	for ( i = iBitCount - 1; i >= 0; i-- ) {
		if ( ((iValue >> i) & 1u) != 0u ) {
			pData[*pBitLen >> 3] |= (uint8_t)(0x80u >> (*pBitLen & 7));
		}
		(*pBitLen)++;
	}
	return XUI_OK;
}

static int __xuiQrCodeEncodeData(const char* sValue, int iVersion, uint8_t* pData, int* pDataLen)
{
	const unsigned char* pBytes;
	int iPayloadLen;
	int iCapacity;
	int iBitLen;
	int iRet;
	int i;

	if ( (sValue == NULL) || (pData == NULL) || (pDataLen == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iPayloadLen = (int)strlen(sValue);
	iCapacity = __xuiQrCodeDataCodewords(iVersion);
	if ( (4 + __xuiQrCodeCharCountBits(iVersion) + iPayloadLen * 8) > iCapacity * 8 ) {
		return XUI_ERROR_BUFFER_TOO_SMALL;
	}
	memset(pData, 0, (size_t)iCapacity);
	iBitLen = 0;
	iRet = __xuiQrCodeAppendBits(pData, iCapacity * 8, &iBitLen, 0x4u, 4);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiQrCodeAppendBits(pData, iCapacity * 8, &iBitLen, (unsigned int)iPayloadLen, __xuiQrCodeCharCountBits(iVersion));
	if ( iRet != XUI_OK ) return iRet;
	pBytes = (const unsigned char*)sValue;
	for ( i = 0; i < iPayloadLen; i++ ) {
		iRet = __xuiQrCodeAppendBits(pData, iCapacity * 8, &iBitLen, pBytes[i], 8);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiQrCodeAppendBits(pData, iCapacity * 8, &iBitLen, 0, (iCapacity * 8 - iBitLen < 4) ? (iCapacity * 8 - iBitLen) : 4);
	if ( iRet != XUI_OK ) return iRet;
	while ( (iBitLen & 7) != 0 ) {
		iRet = __xuiQrCodeAppendBits(pData, iCapacity * 8, &iBitLen, 0, 1);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = iBitLen >> 3; i < iCapacity; i++ ) {
		pData[i] = (uint8_t)(((i - (iBitLen >> 3)) & 1) == 0 ? 0xec : 0x11);
	}
	*pDataLen = iCapacity;
	return XUI_OK;
}

static uint8_t __xuiQrCodeGfMultiply(uint8_t iX, uint8_t iY)
{
	unsigned int iZ;
	int i;

	iZ = 0;
	for ( i = 0; i < 8; i++ ) {
		if ( (iY & 1u) != 0u ) {
			iZ ^= iX;
		}
		iY = (uint8_t)(iY >> 1);
		iX = (uint8_t)((iX << 1) ^ (((iX >> 7) & 1u) * 0x1du));
	}
	return (uint8_t)iZ;
}

static void __xuiQrCodeRsGenerator(int iDegree, uint8_t* pResult)
{
	uint8_t iRoot;
	int i;
	int j;

	memset(pResult, 0, (size_t)iDegree);
	pResult[iDegree - 1] = 1;
	iRoot = 1;
	for ( i = 0; i < iDegree; i++ ) {
		for ( j = 0; j < iDegree; j++ ) {
			pResult[j] = __xuiQrCodeGfMultiply(pResult[j], iRoot);
			if ( j + 1 < iDegree ) {
				pResult[j] ^= pResult[j + 1];
			}
		}
		iRoot = __xuiQrCodeGfMultiply(iRoot, 0x02);
	}
}

static void __xuiQrCodeRsRemainder(const uint8_t* pData, int iDataLen, const uint8_t* pGenerator, int iDegree, uint8_t* pResult)
{
	uint8_t iFactor;
	int i;
	int j;

	memset(pResult, 0, (size_t)iDegree);
	for ( i = 0; i < iDataLen; i++ ) {
		iFactor = (uint8_t)(pData[i] ^ pResult[0]);
		memmove(pResult, pResult + 1, (size_t)(iDegree - 1));
		pResult[iDegree - 1] = 0;
		for ( j = 0; j < iDegree; j++ ) {
			pResult[j] ^= __xuiQrCodeGfMultiply(pGenerator[j], iFactor);
		}
	}
}

static int __xuiQrCodeAddEccAndInterleave(int iVersion, const uint8_t* pData, uint8_t* pResult, int* pResultLen)
{
	uint8_t arrGenerator[30];
	uint8_t arrBlockData[XUI_QRCODE_MAX_BLOCKS][XUI_QRCODE_MAX_CODEWORDS];
	uint8_t arrBlockEcc[XUI_QRCODE_MAX_BLOCKS][30];
	int arrBlockDataLen[XUI_QRCODE_MAX_BLOCKS];
	int iRawCodewords;
	int iBlockCount;
	int iEccLen;
	int iShortBlockDataLen;
	int iLongBlockCount;
	int iOffset;
	int iMaxDataLen;
	int iIndex;
	int iBlock;
	int i;

	if ( (pData == NULL) || (pResult == NULL) || (pResultLen == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRawCodewords = __xuiQrCodeRawCodewords(iVersion);
	iBlockCount = g_xuiQrCodeNumEccBlocks[iVersion];
	iEccLen = g_xuiQrCodeEccCodewordsPerBlock[iVersion];
	iShortBlockDataLen = iRawCodewords / iBlockCount - iEccLen;
	iLongBlockCount = iRawCodewords % iBlockCount;
	if ( iBlockCount > XUI_QRCODE_MAX_BLOCKS || iEccLen > 30 ) {
		return XUI_ERROR_BUFFER_TOO_SMALL;
	}
	__xuiQrCodeRsGenerator(iEccLen, arrGenerator);
	memset(arrBlockData, 0, sizeof(arrBlockData));
	memset(arrBlockEcc, 0, sizeof(arrBlockEcc));
	iOffset = 0;
	iMaxDataLen = 0;
	for ( iBlock = 0; iBlock < iBlockCount; iBlock++ ) {
		arrBlockDataLen[iBlock] = iShortBlockDataLen + (iBlock >= iBlockCount - iLongBlockCount ? 1 : 0);
		memcpy(arrBlockData[iBlock], pData + iOffset, (size_t)arrBlockDataLen[iBlock]);
		iOffset += arrBlockDataLen[iBlock];
		if ( arrBlockDataLen[iBlock] > iMaxDataLen ) {
			iMaxDataLen = arrBlockDataLen[iBlock];
		}
		__xuiQrCodeRsRemainder(arrBlockData[iBlock], arrBlockDataLen[iBlock], arrGenerator, iEccLen, arrBlockEcc[iBlock]);
	}
	iIndex = 0;
	for ( i = 0; i < iMaxDataLen; i++ ) {
		for ( iBlock = 0; iBlock < iBlockCount; iBlock++ ) {
			if ( i < arrBlockDataLen[iBlock] ) {
				pResult[iIndex++] = arrBlockData[iBlock][i];
			}
		}
	}
	for ( i = 0; i < iEccLen; i++ ) {
		for ( iBlock = 0; iBlock < iBlockCount; iBlock++ ) {
			pResult[iIndex++] = arrBlockEcc[iBlock][i];
		}
	}
	if ( iIndex != iRawCodewords ) {
		return XUI_ERROR;
	}
	*pResultLen = iIndex;
	return XUI_OK;
}

static int __xuiQrCodeIndex(int iSize, int iX, int iY)
{
	return iY * iSize + iX;
}

static void __xuiQrCodeSet(uint8_t* pModules, uint8_t* pFunction, int iSize, int iX, int iY, int bDark, int bFunction)
{
	int iIndex;

	if ( (iX < 0) || (iY < 0) || (iX >= iSize) || (iY >= iSize) ) {
		return;
	}
	iIndex = __xuiQrCodeIndex(iSize, iX, iY);
	pModules[iIndex] = (uint8_t)(bDark ? 1 : 0);
	if ( bFunction ) {
		pFunction[iIndex] = 1;
	}
}

static void __xuiQrCodeDrawFinder(uint8_t* pModules, uint8_t* pFunction, int iSize, int iCenterX, int iCenterY)
{
	int iDx;
	int iDy;
	int iDist;

	for ( iDy = -4; iDy <= 4; iDy++ ) {
		for ( iDx = -4; iDx <= 4; iDx++ ) {
			iDist = abs(iDx);
			if ( abs(iDy) > iDist ) iDist = abs(iDy);
			__xuiQrCodeSet(pModules, pFunction, iSize, iCenterX + iDx, iCenterY + iDy, (iDist != 2 && iDist != 4), 1);
		}
	}
}

static int __xuiQrCodeAlignmentPositions(int iVersion, int* pPositions)
{
	int iNumAlign;
	int iStep;
	int iPos;
	int i;

	if ( iVersion == 1 ) {
		return 0;
	}
	iNumAlign = iVersion / 7 + 2;
	iStep = (iVersion == 32) ? 26 : ((iVersion * 4 + iNumAlign * 2 + 1) / (iNumAlign * 2 - 2)) * 2;
	pPositions[0] = 6;
	iPos = 21 + (iVersion - 1) * 4 - 7;
	for ( i = iNumAlign - 1; i >= 1; i-- ) {
		pPositions[i] = iPos;
		iPos -= iStep;
	}
	return iNumAlign;
}

static void __xuiQrCodeDrawAlignment(uint8_t* pModules, uint8_t* pFunction, int iSize, int iCenterX, int iCenterY)
{
	int iDx;
	int iDy;
	int iDist;

	for ( iDy = -2; iDy <= 2; iDy++ ) {
		for ( iDx = -2; iDx <= 2; iDx++ ) {
			iDist = abs(iDx);
			if ( abs(iDy) > iDist ) iDist = abs(iDy);
			__xuiQrCodeSet(pModules, pFunction, iSize, iCenterX + iDx, iCenterY + iDy, iDist != 1, 1);
		}
	}
}

static int __xuiQrCodeVersionBits(int iVersion)
{
	int iRem;
	int i;

	iRem = iVersion;
	for ( i = 0; i < 12; i++ ) {
		iRem = (iRem << 1) ^ (((iRem >> 11) & 1) * 0x1f25);
	}
	return (iVersion << 12) | (iRem & 0xfff);
}

static int __xuiQrCodeFormatBits(int iMask)
{
	int iData;
	int iRem;
	int i;

	iData = iMask; /* Error correction level M uses format bits 00. */
	iRem = iData;
	for ( i = 0; i < 10; i++ ) {
		iRem = (iRem << 1) ^ (((iRem >> 9) & 1) * 0x537);
	}
	return ((iData << 10) | (iRem & 0x3ff)) ^ 0x5412;
}

static void __xuiQrCodeDrawFormatBits(uint8_t* pModules, uint8_t* pFunction, int iSize, int iMask)
{
	int iBits;
	int i;

	iBits = __xuiQrCodeFormatBits(iMask);
	for ( i = 0; i <= 5; i++ ) {
		__xuiQrCodeSet(pModules, pFunction, iSize, 8, i, (iBits >> i) & 1, 1);
	}
	__xuiQrCodeSet(pModules, pFunction, iSize, 8, 7, (iBits >> 6) & 1, 1);
	__xuiQrCodeSet(pModules, pFunction, iSize, 8, 8, (iBits >> 7) & 1, 1);
	__xuiQrCodeSet(pModules, pFunction, iSize, 7, 8, (iBits >> 8) & 1, 1);
	for ( i = 9; i < 15; i++ ) {
		__xuiQrCodeSet(pModules, pFunction, iSize, 14 - i, 8, (iBits >> i) & 1, 1);
	}
	for ( i = 0; i < 8; i++ ) {
		__xuiQrCodeSet(pModules, pFunction, iSize, iSize - 1 - i, 8, (iBits >> i) & 1, 1);
	}
	for ( i = 8; i < 15; i++ ) {
		__xuiQrCodeSet(pModules, pFunction, iSize, 8, iSize - 15 + i, (iBits >> i) & 1, 1);
	}
	__xuiQrCodeSet(pModules, pFunction, iSize, 8, iSize - 8, 1, 1);
}

static void __xuiQrCodeDrawVersionBits(uint8_t* pModules, uint8_t* pFunction, int iSize, int iVersion)
{
	int iBits;
	int i;
	int iX;
	int iY;

	if ( iVersion < 7 ) {
		return;
	}
	iBits = __xuiQrCodeVersionBits(iVersion);
	for ( i = 0; i < 18; i++ ) {
		iX = iSize - 11 + i % 3;
		iY = i / 3;
		__xuiQrCodeSet(pModules, pFunction, iSize, iX, iY, (iBits >> i) & 1, 1);
		__xuiQrCodeSet(pModules, pFunction, iSize, iY, iX, (iBits >> i) & 1, 1);
	}
}

static void __xuiQrCodeDrawFunctionPatterns(uint8_t* pModules, uint8_t* pFunction, int iVersion)
{
	int arrAlign[8];
	int iSize;
	int iNumAlign;
	int i;
	int j;

	iSize = 21 + (iVersion - 1) * 4;
	__xuiQrCodeDrawFinder(pModules, pFunction, iSize, 3, 3);
	__xuiQrCodeDrawFinder(pModules, pFunction, iSize, iSize - 4, 3);
	__xuiQrCodeDrawFinder(pModules, pFunction, iSize, 3, iSize - 4);
	for ( i = 0; i < iSize; i++ ) {
		if ( pFunction[__xuiQrCodeIndex(iSize, 6, i)] == 0 ) {
			__xuiQrCodeSet(pModules, pFunction, iSize, 6, i, (i & 1) == 0, 1);
		}
		if ( pFunction[__xuiQrCodeIndex(iSize, i, 6)] == 0 ) {
			__xuiQrCodeSet(pModules, pFunction, iSize, i, 6, (i & 1) == 0, 1);
		}
	}
	iNumAlign = __xuiQrCodeAlignmentPositions(iVersion, arrAlign);
	for ( i = 0; i < iNumAlign; i++ ) {
		for ( j = 0; j < iNumAlign; j++ ) {
			if ( pFunction[__xuiQrCodeIndex(iSize, arrAlign[j], arrAlign[i])] == 0 ) {
				__xuiQrCodeDrawAlignment(pModules, pFunction, iSize, arrAlign[j], arrAlign[i]);
			}
		}
	}
	__xuiQrCodeDrawFormatBits(pModules, pFunction, iSize, 0);
	__xuiQrCodeDrawVersionBits(pModules, pFunction, iSize, iVersion);
}

static int __xuiQrCodeMask(int iMask, int iX, int iY)
{
	int iProduct;

	iProduct = iX * iY;
	switch ( iMask ) {
	case 0: return ((iX + iY) & 1) == 0;
	case 1: return (iY & 1) == 0;
	case 2: return (iX % 3) == 0;
	case 3: return ((iX + iY) % 3) == 0;
	case 4: return (((iX / 3) + (iY / 2)) & 1) == 0;
	case 5: return ((iProduct % 2) + (iProduct % 3)) == 0;
	case 6: return (((iProduct % 2) + (iProduct % 3)) & 1) == 0;
	case 7: return ((((iX + iY) & 1) + (iProduct % 3)) & 1) == 0;
	default: return 0;
	}
}

static void __xuiQrCodeDrawCodewords(uint8_t* pModules, const uint8_t* pFunction, int iSize, const uint8_t* pCodewords, int iCodewordLen)
{
	int iRight;
	int iVert;
	int iJ;
	int iX;
	int iY;
	int iBitIndex;
	int bUpward;
	int bDark;

	iBitIndex = 0;
	bUpward = 1;
	for ( iRight = iSize - 1; iRight >= 1; iRight -= 2 ) {
		if ( iRight == 6 ) {
			iRight--;
		}
		for ( iVert = 0; iVert < iSize; iVert++ ) {
			iY = bUpward ? (iSize - 1 - iVert) : iVert;
			for ( iJ = 0; iJ < 2; iJ++ ) {
				iX = iRight - iJ;
				if ( pFunction[__xuiQrCodeIndex(iSize, iX, iY)] == 0 ) {
					bDark = 0;
					if ( iBitIndex < iCodewordLen * 8 ) {
						bDark = ((pCodewords[iBitIndex >> 3] >> (7 - (iBitIndex & 7))) & 1) != 0;
					}
					pModules[__xuiQrCodeIndex(iSize, iX, iY)] = (uint8_t)bDark;
					iBitIndex++;
				}
			}
		}
		bUpward = !bUpward;
	}
}

static void __xuiQrCodeApplyMask(const uint8_t* pBase, const uint8_t* pFunction, uint8_t* pOut, int iSize, int iMask)
{
	int iX;
	int iY;
	int iIndex;

	memcpy(pOut, pBase, (size_t)(iSize * iSize));
	for ( iY = 0; iY < iSize; iY++ ) {
		for ( iX = 0; iX < iSize; iX++ ) {
			iIndex = __xuiQrCodeIndex(iSize, iX, iY);
			if ( pFunction[iIndex] == 0 && __xuiQrCodeMask(iMask, iX, iY) ) {
				pOut[iIndex] ^= 1;
			}
		}
	}
	__xuiQrCodeDrawFormatBits(pOut, (uint8_t*)pFunction, iSize, iMask);
}

static int __xuiQrCodePenaltyRuns(const uint8_t* pModules, int iSize, int bVertical)
{
	int iPenalty;
	int iOuter;
	int iInner;
	int iRunColor;
	int iRunLen;
	int iColor;

	iPenalty = 0;
	for ( iOuter = 0; iOuter < iSize; iOuter++ ) {
		iRunColor = -1;
		iRunLen = 0;
		for ( iInner = 0; iInner < iSize; iInner++ ) {
			iColor = bVertical ? pModules[__xuiQrCodeIndex(iSize, iOuter, iInner)] :
			                     pModules[__xuiQrCodeIndex(iSize, iInner, iOuter)];
			if ( iColor == iRunColor ) {
				iRunLen++;
				if ( iRunLen == 5 ) {
					iPenalty += 3;
				} else if ( iRunLen > 5 ) {
					iPenalty++;
				}
			} else {
				iRunColor = iColor;
				iRunLen = 1;
			}
		}
	}
	return iPenalty;
}

static int __xuiQrCodePenaltyFinder(const uint8_t* pModules, int iSize, int bVertical)
{
	int iPenalty;
	int iOuter;
	int iInner;
	int iBits;
	int iColor;

	iPenalty = 0;
	for ( iOuter = 0; iOuter < iSize; iOuter++ ) {
		iBits = 0;
		for ( iInner = 0; iInner < iSize; iInner++ ) {
			iColor = bVertical ? pModules[__xuiQrCodeIndex(iSize, iOuter, iInner)] :
			                     pModules[__xuiQrCodeIndex(iSize, iInner, iOuter)];
			iBits = ((iBits << 1) & 0x7ff) | (iColor ? 1 : 0);
			if ( iInner >= 10 && (iBits == 0x5d0 || iBits == 0x05d) ) {
				iPenalty += 40;
			}
		}
	}
	return iPenalty;
}

static int __xuiQrCodePenalty(const uint8_t* pModules, int iSize)
{
	int iPenalty;
	int iX;
	int iY;
	int iDark;
	int iTotal;
	int iK;

	iPenalty = __xuiQrCodePenaltyRuns(pModules, iSize, 0) + __xuiQrCodePenaltyRuns(pModules, iSize, 1);
	for ( iY = 0; iY < iSize - 1; iY++ ) {
		for ( iX = 0; iX < iSize - 1; iX++ ) {
			iK = pModules[__xuiQrCodeIndex(iSize, iX, iY)];
			if ( iK == pModules[__xuiQrCodeIndex(iSize, iX + 1, iY)] &&
			     iK == pModules[__xuiQrCodeIndex(iSize, iX, iY + 1)] &&
			     iK == pModules[__xuiQrCodeIndex(iSize, iX + 1, iY + 1)] ) {
				iPenalty += 3;
			}
		}
	}
	iPenalty += __xuiQrCodePenaltyFinder(pModules, iSize, 0);
	iPenalty += __xuiQrCodePenaltyFinder(pModules, iSize, 1);
	iDark = 0;
	iTotal = iSize * iSize;
	for ( iK = 0; iK < iTotal; iK++ ) {
		if ( pModules[iK] != 0 ) {
			iDark++;
		}
	}
	iK = abs(iDark * 20 - iTotal * 10) / iTotal;
	iPenalty += iK * 10;
	return iPenalty;
}

static int __xuiQrCodeEncodeMatrix(const char* sValue, int iMinVersion, int iMaxVersion, xui_qrcode_matrix_t* pMatrix)
{
	uint8_t arrData[XUI_QRCODE_MAX_CODEWORDS];
	uint8_t arrCodewords[XUI_QRCODE_MAX_CODEWORDS];
	uint8_t arrBase[XUI_QRCODE_MAX_MODULES];
	uint8_t arrFunction[XUI_QRCODE_MAX_MODULES];
	uint8_t arrCandidate[XUI_QRCODE_MAX_MODULES];
	uint8_t arrBest[XUI_QRCODE_MAX_MODULES];
	int iVersion;
	int iDataLen;
	int iCodewordLen;
	int iSize;
	int iMask;
	int iPenalty;
	int iBestPenalty;
	int iBestMask;
	int iRet;

	if ( (sValue == NULL) || (pMatrix == NULL) || !__xuiQrCodeVersionValid(iMinVersion) || !__xuiQrCodeVersionValid(iMaxVersion) || iMinVersion > iMaxVersion ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( iVersion = iMinVersion; iVersion <= iMaxVersion; iVersion++ ) {
		iRet = __xuiQrCodeEncodeData(sValue, iVersion, arrData, &iDataLen);
		if ( iRet == XUI_OK ) {
			break;
		}
		if ( iRet != XUI_ERROR_BUFFER_TOO_SMALL ) {
			return iRet;
		}
	}
	if ( iVersion > iMaxVersion ) {
		return XUI_ERROR_BUFFER_TOO_SMALL;
	}
	iRet = __xuiQrCodeAddEccAndInterleave(iVersion, arrData, arrCodewords, &iCodewordLen);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iSize = 21 + (iVersion - 1) * 4;
	memset(arrBase, 0, sizeof(arrBase));
	memset(arrFunction, 0, sizeof(arrFunction));
	__xuiQrCodeDrawFunctionPatterns(arrBase, arrFunction, iVersion);
	__xuiQrCodeDrawCodewords(arrBase, arrFunction, iSize, arrCodewords, iCodewordLen);
	iBestPenalty = 0x7fffffff;
	iBestMask = 0;
	for ( iMask = 0; iMask < 8; iMask++ ) {
		__xuiQrCodeApplyMask(arrBase, arrFunction, arrCandidate, iSize, iMask);
		iPenalty = __xuiQrCodePenalty(arrCandidate, iSize);
		if ( iPenalty < iBestPenalty ) {
			iBestPenalty = iPenalty;
			iBestMask = iMask;
			memcpy(arrBest, arrCandidate, (size_t)(iSize * iSize));
		}
	}
	(void)iBestMask;
	memset(pMatrix, 0, sizeof(*pMatrix));
	pMatrix->iVersion = iVersion;
	pMatrix->iSize = iSize;
	memcpy(pMatrix->arrModules, arrBest, (size_t)(iSize * iSize));
	return XUI_OK;
}

static void __xuiQrCodeApplyMatrix(xui_qrcode_data_t* pData, const xui_qrcode_matrix_t* pMatrix)
{
	if ( (pData == NULL) || (pMatrix == NULL) ) {
		return;
	}
	pData->iVersion = pMatrix->iVersion;
	pData->iModuleCount = pMatrix->iSize;
	memcpy(pData->arrModules, pMatrix->arrModules, sizeof(pData->arrModules));
}

static int __xuiQrCodeRebuild(xui_widget pWidget, xui_qrcode_data_t* pData)
{
	xui_qrcode_matrix_t tMatrix;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiQrCodeEncodeMatrix((pData->sValue != NULL) ? pData->sValue : "", pData->iMinVersion, pData->iMaxVersion, &tMatrix);
	if ( iRet != XUI_OK ) {
		pData->iVersion = 0;
		pData->iModuleCount = 0;
		memset(pData->arrModules, 0, sizeof(pData->arrModules));
		return iRet;
	}
	__xuiQrCodeApplyMatrix(pData, &tMatrix);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiQrCodeContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	float fSize;

	(void)pWidget;
	(void)pUser;
	if ( pSize == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fSize = XUI_QRCODE_DEFAULT_SIZE;
	if ( (tConstraint.fX > 0.0f) && (tConstraint.fX < XUI_LAYOUT_UNBOUNDED) &&
	     (tConstraint.fY > 0.0f) && (tConstraint.fY < XUI_LAYOUT_UNBOUNDED) ) {
		fSize = (tConstraint.fX < tConstraint.fY) ? tConstraint.fX : tConstraint.fY;
	}
	pSize->fX = fSize;
	pSize->fY = fSize;
	return XUI_OK;
}

static int __xuiQrCodeSurfaceSource(xui_widget pWidget, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t* pOut)
{
	xui_surface_desc_t tDesc;
	xui_proxy pProxy;

	if ( pOut == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (tSrc.fW > 0.0f) && (tSrc.fH > 0.0f) ) {
		*pOut = tSrc;
		return XUI_OK;
	}
	if ( pSurface == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy == NULL) || (pProxy->surfaceGetDesc == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	if ( pProxy->surfaceGetDesc(pProxy, pSurface, &tDesc) != XUI_OK ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pOut = (xui_rect_t){0.0f, 0.0f, (float)tDesc.iWidth, (float)tDesc.iHeight};
	return XUI_OK;
}

static int __xuiQrCodeCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_qrcode_data_t* pData;
	xui_qrcode_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tContent;
	xui_rect_t tField;
	xui_rect_t tModule;
	xui_rect_t tIcon;
	xui_rect_t tIconSrc;
	float fAvailable;
	float fModuleSize;
	float fQrSize;
	float fIconSize;
	int iX;
	int iY;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiQrCodeGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy == NULL) || (pProxy->drawRectFill == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiQrCodeResolve(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	tContent = xuiInternalSnapRect(tContent);
	if ( __xuiQrCodeAlpha(tResolved.iBackgroundColor) != 0 ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, tContent, tResolved.iBackgroundColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( tResolved.iModuleCount <= 0 || __xuiQrCodeAlpha(tResolved.iForegroundColor) == 0 ) {
		return XUI_OK;
	}
	fAvailable = (tContent.fW < tContent.fH) ? tContent.fW : tContent.fH;
	fAvailable -= tResolved.fPadding * 2.0f;
	if ( fAvailable <= 0.0f ) {
		return XUI_OK;
	}
	fModuleSize = (float)((int)(fAvailable / (float)tResolved.iModuleCount));
	if ( fModuleSize < 1.0f ) {
		fModuleSize = fAvailable / (float)tResolved.iModuleCount;
	}
	fQrSize = fModuleSize * (float)tResolved.iModuleCount;
	tField = xuiInternalSnapRect((xui_rect_t){
		tContent.fX + (tContent.fW - fQrSize) * 0.5f,
		tContent.fY + (tContent.fH - fQrSize) * 0.5f,
		fQrSize,
		fQrSize});
	for ( iY = 0; iY < tResolved.iModuleCount; iY++ ) {
		for ( iX = 0; iX < tResolved.iModuleCount; iX++ ) {
			if ( tResolved.arrModules[__xuiQrCodeIndex(tResolved.iModuleCount, iX, iY)] != 0 ) {
				tModule = xuiInternalSnapRect((xui_rect_t){
					tField.fX + (float)iX * fModuleSize,
					tField.fY + (float)iY * fModuleSize,
					fModuleSize + 0.01f,
					fModuleSize + 0.01f});
				iRet = pProxy->drawRectFill(pProxy, pDraw, tModule, tResolved.iForegroundColor);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
	}
	if ( tResolved.pIconSurface != NULL && pProxy->drawSurface != NULL ) {
		fIconSize = tResolved.fIconSize;
		if ( fIconSize <= 0.0f ) {
			fIconSize = fQrSize * 0.22f;
		}
		if ( fIconSize > fQrSize * 0.32f ) {
			fIconSize = fQrSize * 0.32f;
		}
		if ( fIconSize > 0.0f && __xuiQrCodeSurfaceSource(pWidget, tResolved.pIconSurface, tResolved.tIconSrc, &tIconSrc) == XUI_OK ) {
			tIcon = xuiInternalSnapRect((xui_rect_t){
				tField.fX + (tField.fW - fIconSize) * 0.5f,
				tField.fY + (tField.fH - fIconSize) * 0.5f,
				fIconSize,
				fIconSize});
			if ( __xuiQrCodeAlpha(tResolved.iBackgroundColor) != 0 ) {
				iRet = pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect((xui_rect_t){tIcon.fX - 4.0f, tIcon.fY - 4.0f, tIcon.fW + 8.0f, tIcon.fH + 8.0f}), tResolved.iBackgroundColor);
				if ( iRet != XUI_OK ) return iRet;
			}
			iRet = pProxy->drawSurface(pProxy, pDraw, tResolved.pIconSurface, tIconSrc, tIcon, XUI_COLOR_WHITE, 0);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiQrCodeInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_qrcode_data_t* pData;
	const xui_qrcode_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_qrcode_data_t*)pTypeData;
	pDesc = (const xui_qrcode_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiQrCodeDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	pData->iForegroundColor = XUI_COLOR_RGBA(0, 0, 0, 255);
	pData->iBackgroundColor = XUI_COLOR_WHITE;
	pData->fPadding = XUI_QRCODE_DEFAULT_PADDING;
	pData->iMinVersion = XUI_QRCODE_MIN_VERSION;
	pData->iMaxVersion = XUI_QRCODE_MAX_VERSION;
	if ( pDesc != NULL ) {
		if ( pDesc->iForegroundColor != 0 ) pData->iForegroundColor = pDesc->iForegroundColor;
		if ( pDesc->iBackgroundColor != 0 ) pData->iBackgroundColor = pDesc->iBackgroundColor;
		if ( pDesc->fPadding > 0.0f ) pData->fPadding = pDesc->fPadding;
		pData->fIconSize = pDesc->fIconSize;
		pData->pIconSurface = pDesc->pIconSurface;
		pData->tIconSrc = pDesc->tIconSrc;
		if ( pDesc->iMinVersion != 0 ) pData->iMinVersion = pDesc->iMinVersion;
		if ( pDesc->iMaxVersion != 0 ) pData->iMaxVersion = pDesc->iMaxVersion;
	}
	iRet = __xuiQrCodeStringSet(&pData->sValue, &pData->iValueCapacity, (pDesc != NULL) ? pDesc->sValue : "");
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	return __xuiQrCodeRebuild(pWidget, pData);
}

static void __xuiQrCodeDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_qrcode_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_qrcode_data_t*)pTypeData;
	if ( pData == NULL ) {
		return;
	}
	if ( pData->sValue != NULL ) {
		xrtFree(pData->sValue);
	}
	memset(pData, 0, sizeof(*pData));
}

static void __xuiQrCodeDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = 0;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiQrCodeDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static void __xuiQrCodeRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) {
		return;
	}
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiQrCodeRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiQrCodeRegisterStyleProperty(pContext, pType, "qrcode.foreground_color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiQrCodeRegisterStyleProperty(pContext, pType, "qrcode.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiQrCodeRegisterStyleProperty(pContext, pType, "qrcode.padding", XUI_STYLE_VALUE_FLOAT, iPaintDirty);
	__xuiQrCodeRegisterStyleProperty(pContext, pType, "qrcode.icon_size", XUI_STYLE_VALUE_FLOAT, iPaintDirty);
}

XUI_API xui_widget_type xuiQrCodeGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "qrcode");
	if ( pType != NULL ) {
		__xuiQrCodeRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "qrcode";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_qrcode_data_t);
	tDesc.onInit = __xuiQrCodeInit;
	tDesc.onDestroy = __xuiQrCodeDestroy;
	tDesc.onContentMeasure = __xuiQrCodeContentMeasure;
	tDesc.onCacheRender = __xuiQrCodeCacheRender;
	__xuiQrCodeDefaultLayout(&tDesc.tLayout);
	__xuiQrCodeDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiQrCodeRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiQrCodeCreate(xui_context pContext, xui_widget* ppWidget, const xui_qrcode_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiQrCodeDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiQrCodeGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiQrCodeSetValue(xui_widget pWidget, const char* sValue)
{
	xui_qrcode_data_t* pData;
	xui_qrcode_matrix_t tMatrix;
	int iRet;

	pData = __xuiQrCodeGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sValue == NULL ) sValue = "";
	iRet = __xuiQrCodeEncodeMatrix(sValue, pData->iMinVersion, pData->iMaxVersion, &tMatrix);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiQrCodeStringSet(&pData->sValue, &pData->iValueCapacity, sValue);
	if ( iRet != XUI_OK ) return iRet;
	__xuiQrCodeApplyMatrix(pData, &tMatrix);
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiQrCodeGetValue(xui_widget pWidget)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	return (pData != NULL && pData->sValue != NULL) ? pData->sValue : "";
}

XUI_API int xuiQrCodeSetColors(xui_widget pWidget, uint32_t iForegroundColor, uint32_t iBackgroundColor)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iForegroundColor = iForegroundColor;
	pData->iBackgroundColor = iBackgroundColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiQrCodeSetForegroundColor(xui_widget pWidget, uint32_t iColor)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iForegroundColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiQrCodeGetForegroundColor(xui_widget pWidget)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	return (pData != NULL) ? pData->iForegroundColor : 0;
}

XUI_API int xuiQrCodeSetBackgroundColor(xui_widget pWidget, uint32_t iColor)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiQrCodeGetBackgroundColor(xui_widget pWidget)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	return (pData != NULL) ? pData->iBackgroundColor : 0;
}

XUI_API int xuiQrCodeSetPadding(xui_widget pWidget, float fPadding)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	if ( pData == NULL || fPadding < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fPadding = fPadding;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiQrCodeGetPadding(xui_widget pWidget)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	return (pData != NULL) ? pData->fPadding : 0.0f;
}

XUI_API int xuiQrCodeSetIcon(xui_widget pWidget, xui_surface pIconSurface, xui_rect_t tIconSrc, float fIconSize)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	if ( pData == NULL || tIconSrc.fW < 0.0f || tIconSrc.fH < 0.0f || fIconSize < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pIconSurface = pIconSurface;
	pData->tIconSrc = tIconSrc;
	pData->fIconSize = fIconSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_surface xuiQrCodeGetIcon(xui_widget pWidget)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	return (pData != NULL) ? pData->pIconSurface : NULL;
}

XUI_API xui_rect_t xuiQrCodeGetIconSource(xui_widget pWidget)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	return (pData != NULL) ? pData->tIconSrc : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API float xuiQrCodeGetIconSize(xui_widget pWidget)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	return (pData != NULL) ? pData->fIconSize : 0.0f;
}

XUI_API int xuiQrCodeSetVersionRange(xui_widget pWidget, int iMinVersion, int iMaxVersion)
{
	xui_qrcode_data_t* pData;
	xui_qrcode_matrix_t tMatrix;
	int iRet;

	pData = __xuiQrCodeGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iMinVersion == 0 ) iMinVersion = XUI_QRCODE_MIN_VERSION;
	if ( iMaxVersion == 0 ) iMaxVersion = XUI_QRCODE_MAX_VERSION;
	if ( !__xuiQrCodeVersionValid(iMinVersion) || !__xuiQrCodeVersionValid(iMaxVersion) || iMinVersion > iMaxVersion ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiQrCodeEncodeMatrix((pData->sValue != NULL) ? pData->sValue : "", iMinVersion, iMaxVersion, &tMatrix);
	if ( iRet != XUI_OK ) return iRet;
	pData->iMinVersion = iMinVersion;
	pData->iMaxVersion = iMaxVersion;
	__xuiQrCodeApplyMatrix(pData, &tMatrix);
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiQrCodeGetVersion(xui_widget pWidget)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	return (pData != NULL) ? pData->iVersion : 0;
}

XUI_API int xuiQrCodeGetModuleCount(xui_widget pWidget)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	return (pData != NULL) ? pData->iModuleCount : 0;
}

XUI_API int xuiQrCodeGetModule(xui_widget pWidget, int iX, int iY)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	if ( pData == NULL || iX < 0 || iY < 0 || iX >= pData->iModuleCount || iY >= pData->iModuleCount ) {
		return -1;
	}
	return pData->arrModules[__xuiQrCodeIndex(pData->iModuleCount, iX, iY)] != 0;
}

XUI_API int xuiQrCodeGetChangeCount(xui_widget pWidget)
{
	xui_qrcode_data_t* pData;

	pData = __xuiQrCodeGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
