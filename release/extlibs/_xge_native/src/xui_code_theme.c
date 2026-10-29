#include "../xui.h"

#include <string.h>

#define XUI_CODE_THEME_STYLE_CAPACITY 128
#define XUI_CODE_THEME_TOKEN_CAPACITY 128

struct xui_code_theme_t {
	xui_code_style_t arrStyles[XUI_CODE_THEME_STYLE_CAPACITY];
	int arrTokenStyle[XUI_CODE_THEME_TOKEN_CAPACITY];
};

static int __xuiCodeThemeStyleIndexValid(int iStyleId)
{
	return iStyleId >= 0 && iStyleId < XUI_CODE_THEME_STYLE_CAPACITY;
}

static int __xuiCodeThemeTokenIndexValid(int iTokenKind)
{
	return iTokenKind >= 0 && iTokenKind < XUI_CODE_THEME_TOKEN_CAPACITY;
}

XUI_API int xuiCodeThemeCreate(xui_code_theme* ppTheme)
{
	xui_code_theme pTheme;
	int iRet;

	if ( ppTheme == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppTheme = NULL;
	pTheme = (xui_code_theme)xrtMalloc(sizeof(*pTheme));
	if ( pTheme == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pTheme, 0, sizeof(*pTheme));
	iRet = xuiCodeThemeSetDefault(pTheme);
	if ( iRet != XUI_OK ) {
		xuiCodeThemeDestroy(pTheme);
		return iRet;
	}
	*ppTheme = pTheme;
	return XUI_OK;
}

XUI_API void xuiCodeThemeDestroy(xui_code_theme pTheme)
{
	xrtFree(pTheme);
}

XUI_API int xuiCodeThemeSetDefault(xui_code_theme pTheme)
{
	int i;

	if ( pTheme == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pTheme, 0, sizeof(*pTheme));
	for ( i = 0; i < XUI_CODE_THEME_STYLE_CAPACITY; i++ ) {
		pTheme->arrStyles[i].iSize = sizeof(pTheme->arrStyles[i]);
		pTheme->arrStyles[i].iForeground = XUI_COLOR_RGBA(36, 41, 47, 255);
		pTheme->arrStyles[i].iBackground = 0;
	}
	pTheme->arrStyles[XUI_CODE_STYLE_DEFAULT].iForeground = XUI_COLOR_RGBA(36, 41, 47, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_KEYWORD].iForeground = XUI_COLOR_RGBA(9, 105, 218, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_KEYWORD].iFlags = XUI_CODE_STYLE_BOLD;
	pTheme->arrStyles[XUI_CODE_STYLE_TYPE].iForeground = XUI_COLOR_RGBA(130, 80, 223, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_NUMBER].iForeground = XUI_COLOR_RGBA(17, 99, 41, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_STRING].iForeground = XUI_COLOR_RGBA(10, 91, 129, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_CHAR].iForeground = XUI_COLOR_RGBA(10, 91, 129, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_COMMENT].iForeground = XUI_COLOR_RGBA(110, 119, 129, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_COMMENT].iFlags = XUI_CODE_STYLE_ITALIC;
	pTheme->arrStyles[XUI_CODE_STYLE_PREPROCESSOR].iForeground = XUI_COLOR_RGBA(149, 56, 18, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_OPERATOR].iForeground = XUI_COLOR_RGBA(87, 96, 106, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_BRACE].iForeground = XUI_COLOR_RGBA(87, 96, 106, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_ERROR].iForeground = XUI_COLOR_RGBA(207, 34, 46, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_SELECTION].iBackground = XUI_COLOR_RGBA(173, 214, 255, 160);
	pTheme->arrStyles[XUI_CODE_STYLE_CURRENT_LINE].iBackground = XUI_COLOR_RGBA(246, 248, 250, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_SEARCH_RESULT].iBackground = XUI_COLOR_RGBA(255, 235, 128, 190);
	pTheme->arrStyles[XUI_CODE_STYLE_MATCHED_BRACE].iBackground = XUI_COLOR_RGBA(221, 244, 255, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_MARGIN].iForeground = XUI_COLOR_RGBA(87, 96, 106, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_MARGIN].iBackground = XUI_COLOR_RGBA(246, 248, 250, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_LINE_NUMBER].iForeground = XUI_COLOR_RGBA(110, 119, 129, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_FOLD_ICON].iForeground = XUI_COLOR_RGBA(87, 96, 106, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_MARKER].iForeground = XUI_COLOR_RGBA(9, 105, 218, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_DIAGNOSTIC_ERROR].iForeground = XUI_COLOR_RGBA(207, 34, 46, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_DIAGNOSTIC_WARNING].iForeground = XUI_COLOR_RGBA(154, 103, 0, 255);
	pTheme->arrStyles[XUI_CODE_STYLE_DIAGNOSTIC_INFO].iForeground = XUI_COLOR_RGBA(9, 105, 218, 255);
	for ( i = 0; i < XUI_CODE_THEME_TOKEN_CAPACITY; i++ ) pTheme->arrTokenStyle[i] = XUI_CODE_STYLE_DEFAULT;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_KEYWORD] = XUI_CODE_STYLE_KEYWORD;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_TYPE] = XUI_CODE_STYLE_TYPE;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_IDENTIFIER] = XUI_CODE_STYLE_IDENTIFIER;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_NUMBER] = XUI_CODE_STYLE_NUMBER;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_STRING] = XUI_CODE_STYLE_STRING;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_CHAR] = XUI_CODE_STYLE_CHAR;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_COMMENT] = XUI_CODE_STYLE_COMMENT;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_PREPROCESSOR] = XUI_CODE_STYLE_PREPROCESSOR;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_OPERATOR] = XUI_CODE_STYLE_OPERATOR;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_BRACE] = XUI_CODE_STYLE_BRACE;
	pTheme->arrTokenStyle[XUI_CODE_TOKEN_ERROR] = XUI_CODE_STYLE_ERROR;
	return XUI_OK;
}

XUI_API int xuiCodeThemeSetStyle(xui_code_theme pTheme, int iStyleId, const xui_code_style_t* pStyle)
{
	if ( (pTheme == NULL) || !__xuiCodeThemeStyleIndexValid(iStyleId) || (pStyle == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pTheme->arrStyles[iStyleId] = *pStyle;
	pTheme->arrStyles[iStyleId].iSize = sizeof(pTheme->arrStyles[iStyleId]);
	return XUI_OK;
}

XUI_API int xuiCodeThemeGetStyle(xui_code_theme pTheme, int iStyleId, xui_code_style_t* pStyle)
{
	if ( (pTheme == NULL) || !__xuiCodeThemeStyleIndexValid(iStyleId) || (pStyle == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pStyle = pTheme->arrStyles[iStyleId];
	pStyle->iSize = sizeof(*pStyle);
	return XUI_OK;
}

XUI_API int xuiCodeThemeMapTokenKind(xui_code_theme pTheme, int iTokenKind, int iStyleId)
{
	if ( (pTheme == NULL) || !__xuiCodeThemeTokenIndexValid(iTokenKind) || !__xuiCodeThemeStyleIndexValid(iStyleId) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pTheme->arrTokenStyle[iTokenKind] = iStyleId;
	return XUI_OK;
}

XUI_API int xuiCodeThemeGetTokenStyle(xui_code_theme pTheme, int iTokenKind, xui_code_style_t* pStyle)
{
	int iStyleId;

	if ( (pTheme == NULL) || !__xuiCodeThemeTokenIndexValid(iTokenKind) || (pStyle == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iStyleId = pTheme->arrTokenStyle[iTokenKind];
	return xuiCodeThemeGetStyle(pTheme, iStyleId, pStyle);
}

XUI_API int xuiCodeThemeCopy(xui_code_theme pDst, xui_code_theme pSrc)
{
	if ( pDst == NULL || pSrc == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memcpy(pDst, pSrc, sizeof(*pDst));
	return XUI_OK;
}
