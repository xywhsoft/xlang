#include "xui_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define XUI_FILE_DIALOG_MAGIC 0x58464447u
#define XUI_FILE_DIALOG_ENTRY_GROW 32
#define XUI_FILE_DIALOG_FILTER_GROW 8
#define XUI_FILE_DIALOG_DOUBLE_SELECT_SECONDS 0.55
#define XUI_FILE_DIALOG_ICON_FOLDER "file_dialog_folder"
#define XUI_FILE_DIALOG_ICON_FILE "file_dialog_file"
#define XUI_FILE_DIALOG_ICON_DOCUMENT "file_dialog_document"
#define XUI_FILE_DIALOG_ICON_IMAGE "file_dialog_image"
#define XUI_FILE_DIALOG_ICON_SCRIPT "file_dialog_script"
#define XUI_FILE_DIALOG_ICON_UP "file_dialog_up"
#define XUI_FILE_DIALOG_ICON_REFRESH "file_dialog_refresh"

typedef struct xui_file_dialog_entry_t {
	char* sName;
	char* sPath;
	int bDir;
} xui_file_dialog_entry_t;

struct xui_file_dialog_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_widget pWindow;
	xui_widget pPathRow;
	xui_widget pListRow;
	xui_widget pFieldRow;
	xui_widget pButtonRow;
	xui_widget pButtonSpacer;
	xui_widget pPathBar;
	xui_widget pPathBreadcrumb;
	xui_widget pRootList;
	xui_widget pFileList;
	xui_widget pPathInput;
	xui_widget pNameInput;
	xui_widget pFilterCombo;
	xui_widget pUpButton;
	xui_widget pRefreshButton;
	xui_widget pOkButton;
	xui_widget pCancelButton;
	xui_msgbox pOverwriteBox;
	xui_widget pPathLabel;
	xui_widget pNameLabel;
	xui_widget pFilterLabel;
	xui_msgtip pPathTip;
	xui_font pFont;
	xui_widget_event_proc onPathInputBlur;
	void* pPathInputBlurUser;
	xui_file_dialog_result_proc onResult;
	void* pResultUser;
	char* sCurrentDir;
	char* sFileName;
	char* sFilter;
	char* sResultPath;
	char* sPendingSavePath;
	char** arrFilterNames;
	char** arrFilterPatterns;
	int iFilterCount;
	int iFilterCapacity;
	int iSelectedFilter;
	char** arrRootItems;
	int iRootCount;
	xui_file_dialog_entry_t* arrEntries;
	char** arrEntryItems;
	int iEntryCount;
	int iEntryCapacity;
	int iSelectedEntry;
	int iLastSelectEntry;
	int iMode;
	int iResult;
	int bOpen;
	int bPathEditMode;
	int bPathApplying;
	int bInternalNameChange;
	float fWidth;
	float fHeight;
	double fLastSelectTime;
};

static int __xuiFileDialogValid(xui_file_dialog pDialog)
{
	return (pDialog != NULL) && (pDialog->iMagic == XUI_FILE_DIALOG_MAGIC);
}

static char* __xuiFileDialogDup(const char* sText)
{
	char* sCopy;
	size_t iLen;

	if ( sText == NULL ) {
		sText = "";
	}
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static int __xuiFileDialogSetString(char** psDst, const char* sText)
{
	char* sNew;

	if ( psDst == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sNew = __xuiFileDialogDup(sText);
	if ( sNew == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( *psDst != NULL ) {
		xrtFree(*psDst);
	}
	*psDst = sNew;
	return XUI_OK;
}

static char* __xuiFileDialogDupRange(const char* sText, size_t iLen)
{
	char* sCopy;

	if ( sText == NULL ) {
		return __xuiFileDialogDup("");
	}
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iLen);
	sCopy[iLen] = 0;
	return sCopy;
}

static int __xuiFileDialogIsSlash(char c)
{
	return c == '/' || c == '\\';
}

static int __xuiFileDialogPathRootLen(const char* sPath)
{
	size_t iLen;
	size_t iPos;
	int iSlashCount;

	if ( sPath == NULL || sPath[0] == 0 ) {
		return 0;
	}
	iLen = strlen(sPath);
	if ( iLen >= 2u && sPath[1] == ':' ) {
		return (iLen >= 3u && __xuiFileDialogIsSlash(sPath[2])) ? 3 : 2;
	}
	if ( __xuiFileDialogIsSlash(sPath[0]) ) {
		if ( iLen >= 2u && __xuiFileDialogIsSlash(sPath[1]) ) {
			iSlashCount = 0;
			for ( iPos = 0u; iPos < iLen; iPos++ ) {
				if ( __xuiFileDialogIsSlash(sPath[iPos]) ) {
					iSlashCount++;
					if ( iSlashCount >= 4 ) return (int)(iPos + 1u);
				}
			}
		}
		return 1;
	}
	return 0;
}

static int __xuiFileDialogModeValid(int iMode)
{
	return (iMode == XUI_FILE_DIALOG_MODE_OPEN_FILE) ||
	       (iMode == XUI_FILE_DIALOG_MODE_SAVE_FILE) ||
	       (iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER);
}

static int __xuiFileDialogDescValid(const xui_file_dialog_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iMode != 0) && !__xuiFileDialogModeValid(pDesc->iMode) ) {
		return 0;
	}
	if ( (pDesc->fWidth < 0.0f) || (pDesc->fHeight < 0.0f) ) {
		return 0;
	}
	return 1;
}

static const char* __xuiFileDialogDefaultTitle(xui_context pContext, int iMode)
{
	if ( iMode == XUI_FILE_DIALOG_MODE_SAVE_FILE ) return xuiTranslate(pContext, XUI_TR_FILE_SAVE_FILE);
	if ( iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER ) return xuiTranslate(pContext, XUI_TR_FILE_SELECT_FOLDER);
	return xuiTranslate(pContext, XUI_TR_FILE_OPEN_FILE);
}

static const char* __xuiFileDialogOkText(xui_context pContext, int iMode)
{
	if ( iMode == XUI_FILE_DIALOG_MODE_SAVE_FILE ) return xuiTranslate(pContext, XUI_TR_FILE_SAVE);
	if ( iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER ) return xuiTranslate(pContext, XUI_TR_FILE_SELECT);
	return xuiTranslate(pContext, XUI_TR_FILE_OPEN);
}

static int __xuiFileDialogTextEquals(const char* sLeft, const char* sRight, size_t iRightSize)
{
	size_t i;

	if ( sLeft == NULL || sRight == NULL ) {
		return 0;
	}
	if ( strlen(sLeft) != iRightSize ) {
		return 0;
	}
	for ( i = 0; i < iRightSize; i++ ) {
		if ( tolower((unsigned char)sLeft[i]) != tolower((unsigned char)sRight[i]) ) {
			return 0;
		}
	}
	return 1;
}

static int __xuiFileDialogExtIn(const char* sExt, const char* const* arrExts, int iCount)
{
	int i;

	if ( sExt == NULL || arrExts == NULL || iCount <= 0 ) {
		return 0;
	}
	if ( sExt[0] == '.' ) {
		sExt++;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( __xuiFileDialogTextEquals(sExt, arrExts[i], strlen(arrExts[i])) ) {
			return 1;
		}
	}
	return 0;
}

static void __xuiFileDialogTrimSpan(const char** psText, size_t* pLen)
{
	const char* sText;
	size_t iLen;

	if ( psText == NULL || pLen == NULL || *psText == NULL ) {
		return;
	}
	sText = *psText;
	iLen = *pLen;
	while ( iLen > 0u && isspace((unsigned char)sText[0]) ) {
		sText++;
		iLen--;
	}
	while ( iLen > 0u && isspace((unsigned char)sText[iLen - 1u]) ) {
		iLen--;
	}
	*psText = sText;
	*pLen = iLen;
}

static const char* __xuiFileDialogIconAsset(const xui_file_dialog_entry_t* pEntry)
{
	static const char* const arrDocumentExts[] = {
		"txt", "text", "md", "markdown", "json", "xson", "xml", "yaml", "yml",
		"ini", "cfg", "conf", "log", "csv", "tsv", "rtf", "doc", "docx",
		"odt", "pdf", "html", "htm", "xhtml", "css", "scss", "less"
	};
	static const char* const arrImageExts[] = {
		"bmp", "dib", "jpg", "jpeg", "jfif", "png", "apng", "gif", "webp",
		"ico", "cur", "svg", "tif", "tiff", "tga", "psd", "avif", "heic",
		"heif", "raw", "dds", "exr", "hdr"
	};
	static const char* const arrScriptExts[] = {
		"c", "h", "cc", "cpp", "cxx", "hpp", "hxx", "m", "mm", "cs", "java",
		"js", "jsx", "ts", "tsx", "py", "pyw", "lua", "xs", "go", "rs",
		"php", "rb", "pl", "pm", "sh", "bash", "zsh", "ps1", "bat", "cmd",
		"sql", "swift", "kt", "kts", "scala", "r", "vue", "svelte", "glsl",
		"hlsl", "metal", "asm", "s"
	};
	const char* sExt;

	if ( pEntry == NULL ) {
		return XUI_FILE_DIALOG_ICON_FILE;
	}
	if ( pEntry->bDir ) {
		return XUI_FILE_DIALOG_ICON_FOLDER;
	}
	sExt = strrchr(pEntry->sName != NULL ? pEntry->sName : "", '.');
	if ( sExt != NULL ) {
		if ( __xuiFileDialogExtIn(sExt, arrDocumentExts, (int)(sizeof(arrDocumentExts) / sizeof(arrDocumentExts[0]))) ) {
			return XUI_FILE_DIALOG_ICON_DOCUMENT;
		}
		if ( __xuiFileDialogExtIn(sExt, arrImageExts, (int)(sizeof(arrImageExts) / sizeof(arrImageExts[0]))) ) {
			return XUI_FILE_DIALOG_ICON_IMAGE;
		}
		if ( __xuiFileDialogExtIn(sExt, arrScriptExts, (int)(sizeof(arrScriptExts) / sizeof(arrScriptExts[0]))) ) {
			return XUI_FILE_DIALOG_ICON_SCRIPT;
		}
	}
	return XUI_FILE_DIALOG_ICON_FILE;
}

static float __xuiFileDialogMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiFileDialogSetButtonIcon(xui_file_dialog pDialog, xui_widget pButton, const char* sAsset)
{
	xui_surface pAtlas;
	xui_rect_t tSrc;
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) || pButton == NULL || sAsset == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiBuiltinAssetGetAtlas(pDialog->pContext, &pAtlas);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiBuiltinAssetGetRect(sAsset, &tSrc);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiButtonSetIcon(pButton, pAtlas, tSrc);
	if ( iRet == XUI_OK ) iRet = xuiButtonSetIconLayout(pButton, XUI_BUTTON_ICON_LEFT, 16.0f, 0.0f);
	if ( iRet == XUI_OK ) iRet = xuiButtonSetIconColor(pButton, XUI_COLOR_WHITE);
	return iRet;
}

static int __xuiFileDialogDrawFileItem(xui_widget pWidget, int iIndex, xui_draw_context pDraw, xui_rect_t tRow, int iState, void* pUser)
{
	xui_file_dialog pDialog;
	xui_file_dialog_entry_t* pEntry;
	xui_proxy pProxy;
	xui_surface pAtlas;
	xui_rect_t tFill;
	xui_rect_t tIconSrc;
	xui_rect_t tIconDst;
	xui_rect_t tText;
	const char* sAsset;
	uint32_t iTextColor;
	int iRet;

	pDialog = (xui_file_dialog)pUser;
	if ( !__xuiFileDialogValid(pDialog) || pWidget == NULL || pDraw == NULL || iIndex < 0 || iIndex >= pDialog->iEntryCount ) {
		return 0;
	}
	pEntry = &pDialog->arrEntries[iIndex];
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL || pProxy->drawSurface == NULL || pProxy->drawText == NULL ||
	     pProxy->drawRectFill == NULL ) {
		return 0;
	}
	iRet = xuiBuiltinAssetGetAtlas(xuiWidgetGetContext(pWidget), &pAtlas);
	if ( iRet != XUI_OK || pAtlas == NULL ) {
		return 0;
	}
	sAsset = __xuiFileDialogIconAsset(pEntry);
	iRet = xuiBuiltinAssetGetRect(sAsset, &tIconSrc);
	if ( iRet != XUI_OK ) {
		return 0;
	}
	if ( (iState & XUI_LIST_ITEM_SELECTED) != 0 ) {
		tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiFileDialogMaxFloat(1.0f, tRow.fW - 6.0f), __xuiFileDialogMaxFloat(1.0f, tRow.fH - 4.0f)});
		iRet = pProxy->drawRectFill(pProxy, pDraw, tFill, XUI_COLOR_RGBA(47, 128, 237, 255));
		if ( iRet != XUI_OK ) return iRet;
	} else if ( (iState & XUI_LIST_ITEM_HOVER) != 0 ) {
		tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiFileDialogMaxFloat(1.0f, tRow.fW - 6.0f), __xuiFileDialogMaxFloat(1.0f, tRow.fH - 4.0f)});
		iRet = pProxy->drawRectFill(pProxy, pDraw, tFill, XUI_COLOR_RGBA(231, 243, 253, 255));
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( ((iState & XUI_LIST_ITEM_FOCUS) != 0) &&
	     (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) &&
	     ((iState & XUI_LIST_ITEM_DISABLED) == 0) &&
	     (pProxy->drawRectStroke != NULL) ) {
		tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiFileDialogMaxFloat(1.0f, tRow.fW - 6.0f), __xuiFileDialogMaxFloat(1.0f, tRow.fH - 4.0f)});
		iRet = pProxy->drawRectStroke(pProxy, pDraw, tFill, 1.0f, XUI_COLOR_RGBA(47, 128, 237, 255));
		if ( iRet != XUI_OK ) return iRet;
	}
	tIconDst = xuiInternalSnapRect((xui_rect_t){tRow.fX + 8.0f, tRow.fY + (tRow.fH - 16.0f) * 0.5f, 16.0f, 16.0f});
	iRet = pProxy->drawSurface(pProxy, pDraw, pAtlas, tIconSrc, tIconDst, XUI_COLOR_WHITE, 0);
	if ( iRet != XUI_OK ) return iRet;
	iTextColor = ((iState & XUI_LIST_ITEM_DISABLED) != 0) ? XUI_COLOR_RGBA(132, 146, 162, 210) : XUI_COLOR_RGBA(31, 50, 73, 255);
	if ( (iState & XUI_LIST_ITEM_SELECTED) != 0 ) {
		iTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	}
	if ( pDialog->pFont != NULL ) {
		tText = xuiInternalSnapRect((xui_rect_t){tRow.fX + 30.0f, tRow.fY, __xuiFileDialogMaxFloat(1.0f, tRow.fW - 38.0f), tRow.fH});
		iRet = pProxy->drawText(pProxy, pDraw, pDialog->pFont, pEntry->sName != NULL ? pEntry->sName : "", tText, iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	return 1;
}

static void __xuiFileDialogEntryFree(xui_file_dialog_entry_t* pEntry)
{
	if ( pEntry == NULL ) {
		return;
	}
	if ( pEntry->sName != NULL ) {
		xrtFree(pEntry->sName);
	}
	if ( pEntry->sPath != NULL ) {
		xrtFree(pEntry->sPath);
	}
	memset(pEntry, 0, sizeof(*pEntry));
}

static void __xuiFileDialogClearEntries(xui_file_dialog pDialog)
{
	int i;

	if ( pDialog == NULL ) {
		return;
	}
	for ( i = 0; i < pDialog->iEntryCount; i++ ) {
		__xuiFileDialogEntryFree(&pDialog->arrEntries[i]);
		if ( pDialog->arrEntryItems != NULL && pDialog->arrEntryItems[i] != NULL ) {
			xrtFree(pDialog->arrEntryItems[i]);
			pDialog->arrEntryItems[i] = NULL;
		}
	}
	pDialog->iEntryCount = 0;
	pDialog->iSelectedEntry = -1;
}

static void __xuiFileDialogFreeRootItems(xui_file_dialog pDialog)
{
	int i;

	if ( pDialog == NULL ) {
		return;
	}
	for ( i = 0; i < pDialog->iRootCount; i++ ) {
		if ( pDialog->arrRootItems[i] != NULL ) {
			xrtFree(pDialog->arrRootItems[i]);
		}
	}
	if ( pDialog->arrRootItems != NULL ) {
		xrtFree(pDialog->arrRootItems);
	}
	pDialog->arrRootItems = NULL;
	pDialog->iRootCount = 0;
}

static void __xuiFileDialogFreeFilters(xui_file_dialog pDialog)
{
	int i;

	if ( pDialog == NULL ) {
		return;
	}
	for ( i = 0; i < pDialog->iFilterCount; i++ ) {
		if ( pDialog->arrFilterNames != NULL && pDialog->arrFilterNames[i] != NULL ) {
			xrtFree(pDialog->arrFilterNames[i]);
		}
		if ( pDialog->arrFilterPatterns != NULL && pDialog->arrFilterPatterns[i] != NULL ) {
			xrtFree(pDialog->arrFilterPatterns[i]);
		}
	}
	if ( pDialog->arrFilterNames != NULL ) xrtFree(pDialog->arrFilterNames);
	if ( pDialog->arrFilterPatterns != NULL ) xrtFree(pDialog->arrFilterPatterns);
	pDialog->arrFilterNames = NULL;
	pDialog->arrFilterPatterns = NULL;
	pDialog->iFilterCount = 0;
	pDialog->iFilterCapacity = 0;
	pDialog->iSelectedFilter = -1;
}

static int __xuiFileDialogEnsureFilterCapacity(xui_file_dialog pDialog, int iNeed)
{
	char** arrNames;
	char** arrPatterns;
	int iNewCapacity;

	if ( pDialog == NULL || iNeed < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeed <= pDialog->iFilterCapacity ) {
		return XUI_OK;
	}
	iNewCapacity = pDialog->iFilterCapacity > 0 ? pDialog->iFilterCapacity : XUI_FILE_DIALOG_FILTER_GROW;
	while ( iNewCapacity < iNeed ) {
		iNewCapacity += XUI_FILE_DIALOG_FILTER_GROW;
	}
	arrNames = (char**)xrtRealloc(pDialog->arrFilterNames, sizeof(char*) * (size_t)iNewCapacity);
	if ( arrNames == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDialog->arrFilterNames = arrNames;
	arrPatterns = (char**)xrtRealloc(pDialog->arrFilterPatterns, sizeof(char*) * (size_t)iNewCapacity);
	if ( arrPatterns == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDialog->arrFilterPatterns = arrPatterns;
	memset(pDialog->arrFilterNames + pDialog->iFilterCapacity, 0, sizeof(char*) * (size_t)(iNewCapacity - pDialog->iFilterCapacity));
	memset(pDialog->arrFilterPatterns + pDialog->iFilterCapacity, 0, sizeof(char*) * (size_t)(iNewCapacity - pDialog->iFilterCapacity));
	pDialog->iFilterCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiFileDialogAddFilterRange(xui_file_dialog pDialog, const char* sName, size_t iNameLen, const char* sPattern, size_t iPatternLen)
{
	char* sNewName;
	char* sNewPattern;
	int iRet;

	if ( pDialog == NULL || sPattern == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFileDialogTrimSpan(&sName, &iNameLen);
	__xuiFileDialogTrimSpan(&sPattern, &iPatternLen);
	if ( iPatternLen == 0u ) {
		return XUI_OK;
	}
	if ( sName == NULL || iNameLen == 0u ) {
		sName = sPattern;
		iNameLen = iPatternLen;
	}
	iRet = __xuiFileDialogEnsureFilterCapacity(pDialog, pDialog->iFilterCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	sNewName = __xuiFileDialogDupRange(sName, iNameLen);
	sNewPattern = __xuiFileDialogDupRange(sPattern, iPatternLen);
	if ( sNewName == NULL || sNewPattern == NULL ) {
		if ( sNewName != NULL ) xrtFree(sNewName);
		if ( sNewPattern != NULL ) xrtFree(sNewPattern);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDialog->arrFilterNames[pDialog->iFilterCount] = sNewName;
	pDialog->arrFilterPatterns[pDialog->iFilterCount] = sNewPattern;
	pDialog->iFilterCount++;
	return XUI_OK;
}

static int __xuiFileDialogAddFilter(xui_file_dialog pDialog, const char* sName, const char* sPattern)
{
	return __xuiFileDialogAddFilterRange(pDialog, sName, sName != NULL ? strlen(sName) : 0u, sPattern, sPattern != NULL ? strlen(sPattern) : 0u);
}

static int __xuiFileDialogParsePipeFilters(xui_file_dialog pDialog, const char* sSpec)
{
	const char* p;
	const char* sName;
	const char* sPattern;
	size_t iNameLen;
	size_t iPatternLen;
	int iRet;

	p = sSpec;
	while ( p != NULL && *p != 0 ) {
		sName = p;
		while ( *p != 0 && *p != '|' ) p++;
		iNameLen = (size_t)(p - sName);
		if ( *p != '|' ) break;
		p++;
		sPattern = p;
		while ( *p != 0 && *p != '|' ) p++;
		iPatternLen = (size_t)(p - sPattern);
		iRet = __xuiFileDialogAddFilterRange(pDialog, sName, iNameLen, sPattern, iPatternLen);
		if ( iRet != XUI_OK ) return iRet;
		if ( *p == '|' ) p++;
	}
	return XUI_OK;
}

static int __xuiFileDialogSetFilters(xui_file_dialog pDialog, const char* sSpec)
{
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFileDialogFreeFilters(pDialog);
	if ( sSpec != NULL && sSpec[0] != 0 ) {
		iRet = __xuiFileDialogParsePipeFilters(pDialog, sSpec);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pDialog->iFilterCount == 0 ) {
		iRet = __xuiFileDialogAddFilter(pDialog, "All Files (*.*)", "*.*");
		if ( iRet != XUI_OK ) return iRet;
	}
	pDialog->iSelectedFilter = 0;
	return __xuiFileDialogSetString(&pDialog->sFilter, pDialog->arrFilterPatterns[0]);
}

static int __xuiFileDialogEnsureEntryCapacity(xui_file_dialog pDialog, int iNeed)
{
	xui_file_dialog_entry_t* pEntries;
	char** pItems;
	int iNewCapacity;

	if ( pDialog == NULL || iNeed < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeed <= pDialog->iEntryCapacity ) {
		return XUI_OK;
	}
	iNewCapacity = pDialog->iEntryCapacity > 0 ? pDialog->iEntryCapacity : XUI_FILE_DIALOG_ENTRY_GROW;
	while ( iNewCapacity < iNeed ) {
		iNewCapacity += XUI_FILE_DIALOG_ENTRY_GROW;
	}
	pEntries = (xui_file_dialog_entry_t*)xrtRealloc(pDialog->arrEntries, sizeof(xui_file_dialog_entry_t) * (size_t)iNewCapacity);
	if ( pEntries == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDialog->arrEntries = pEntries;
	memset(pDialog->arrEntries + pDialog->iEntryCapacity, 0, sizeof(xui_file_dialog_entry_t) * (size_t)(iNewCapacity - pDialog->iEntryCapacity));
	pItems = (char**)xrtRealloc(pDialog->arrEntryItems, sizeof(char*) * (size_t)iNewCapacity);
	if ( pItems == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDialog->arrEntryItems = pItems;
	pDialog->iEntryCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiFileDialogFilterMatch(xui_file_dialog pDialog, const char* sName)
{
	const char* sFilter;
	const char* p;
	size_t iTokenLen;

	if ( pDialog == NULL || sName == NULL ) {
		return 0;
	}
	sFilter = pDialog->sFilter;
	if ( sFilter == NULL || sFilter[0] == 0 || strcmp(sFilter, "*") == 0 || strcmp(sFilter, "*.*") == 0 ) {
		return 1;
	}
	p = sFilter;
	while ( *p != 0 ) {
		const char* sStart;
		while ( (*p == ';') || (*p == ',') || isspace((unsigned char)*p) ) p++;
		sStart = p;
		while ( (*p != 0) && (*p != ';') && (*p != ',') && !isspace((unsigned char)*p) ) p++;
		iTokenLen = (size_t)(p - sStart);
		if ( iTokenLen == 0u ) {
			continue;
		}
		if ( (iTokenLen == 1u && sStart[0] == '*') ||
		     (iTokenLen == 3u && sStart[0] == '*' && sStart[1] == '.' && sStart[2] == '*') ) {
			return 1;
		}
		if ( xrtStrLike((str)sName, 0u, (str)sStart, iTokenLen, TRUE) ) {
			return 1;
		}
	}
	return 0;
}

static int __xuiFileDialogAddEntry(xui_file_dialog pDialog, const char* sName, const char* sPath, int bDir)
{
	xui_file_dialog_entry_t* pEntry;
	int iRet;

	if ( pDialog == NULL || sName == NULL || sPath == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pDialog->iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER && !bDir ) {
		return XUI_OK;
	}
	if ( !bDir && !__xuiFileDialogFilterMatch(pDialog, sName) ) {
		return XUI_OK;
	}
	iRet = __xuiFileDialogEnsureEntryCapacity(pDialog, pDialog->iEntryCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pEntry = &pDialog->arrEntries[pDialog->iEntryCount];
	pEntry->sName = __xuiFileDialogDup(sName);
	pEntry->sPath = __xuiFileDialogDup(sPath);
	pEntry->bDir = bDir ? 1 : 0;
	if ( pEntry->sName == NULL || pEntry->sPath == NULL ) {
		__xuiFileDialogEntryFree(pEntry);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDialog->arrEntryItems[pDialog->iEntryCount] = __xuiFileDialogDup(sName);
	if ( pDialog->arrEntryItems[pDialog->iEntryCount] == NULL ) {
		__xuiFileDialogEntryFree(pEntry);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDialog->iEntryCount++;
	return XUI_OK;
}

static int __xuiFileDialogSetNameText(xui_file_dialog pDialog, const char* sText)
{
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) || pDialog->pNameInput == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pDialog->bInternalNameChange = 1;
	iRet = xuiInputSetText(pDialog->pNameInput, sText != NULL ? sText : "");
	pDialog->bInternalNameChange = 0;
	return iRet;
}

static int __xuiFileDialogRootProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	xui_file_dialog pDialog = (xui_file_dialog)Param;
	char** pItems;

	(void)iSize;
	(void)bDir;
	(void)pData;
	if ( !__xuiFileDialogValid(pDialog) ) {
		return TRUE;
	}
	pItems = (char**)xrtRealloc(pDialog->arrRootItems, sizeof(char*) * (size_t)(pDialog->iRootCount + 1));
	if ( pItems == NULL ) {
		return TRUE;
	}
	pDialog->arrRootItems = pItems;
	pDialog->arrRootItems[pDialog->iRootCount] = __xuiFileDialogDup((const char*)sPath);
	if ( pDialog->arrRootItems[pDialog->iRootCount] == NULL ) {
		return TRUE;
	}
	pDialog->iRootCount++;
	return FALSE;
}

static int __xuiFileDialogScanProc(str sDir, size_t iDirSize, str sName, size_t iNameSize, str sPath, size_t iPathSize, int bDir, ptr pData, ptr Param)
{
	xui_file_dialog pDialog = (xui_file_dialog)Param;
	(void)sDir;
	(void)iDirSize;
	(void)iNameSize;
	(void)iPathSize;
	(void)pData;

	if ( pDialog == NULL ) {
		return TRUE;
	}
	if ( bDir == 2 ) {
		return FALSE;
	}
	return __xuiFileDialogAddEntry(pDialog, (const char*)sName, (const char*)sPath, bDir == 1) != XUI_OK;
}

static int __xuiFileDialogPathBarRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_proxy pProxy;
	xui_rect_t tRect;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( pWidget == NULL || pDraw == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	tRect = xuiInternalSnapRect(xuiWidgetGetContentRect(pWidget));
	if ( pProxy->drawRectFill != NULL ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, XUI_COLOR_RGBA(255, 255, 255, 255));
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pProxy->drawRectStroke != NULL ) {
		iRet = pProxy->drawRectStroke(pProxy, pDraw, tRect, 1.0f, XUI_COLOR_RGBA(166, 182, 202, 255));
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiFileDialogBreadcrumbHit(xui_widget pBreadcrumb, float fX, float fY)
{
	xui_rect_t tWorld;
	xui_rect_t tItem;
	float fLocalX;
	float fLocalY;
	int i;
	int iCount;

	if ( pBreadcrumb == NULL ) {
		return -1;
	}
	tWorld = xuiWidgetGetWorldRect(pBreadcrumb);
	fLocalX = fX - tWorld.fX;
	fLocalY = fY - tWorld.fY;
	iCount = xuiBreadcrumbGetItemCount(pBreadcrumb);
	for ( i = 0; i < iCount; i++ ) {
		tItem = xuiBreadcrumbGetItemRect(pBreadcrumb, i);
		if ( xuiBreadcrumbGetItemClickable(pBreadcrumb, i) &&
		     fLocalX >= tItem.fX && fLocalY >= tItem.fY &&
		     fLocalX < tItem.fX + tItem.fW && fLocalY < tItem.fY + tItem.fH ) {
			return i;
		}
	}
	return -1;
}

static int __xuiFileDialogSetPathEditMode(xui_file_dialog pDialog, int bEdit)
{
	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pDialog->bPathEditMode = bEdit ? 1 : 0;
	if ( pDialog->pPathInput != NULL ) {
		(void)xuiWidgetSetVisible(pDialog->pPathInput, pDialog->bPathEditMode);
	}
	if ( pDialog->pPathBreadcrumb != NULL ) {
		(void)xuiWidgetSetVisible(pDialog->pPathBreadcrumb, !pDialog->bPathEditMode);
	}
	if ( pDialog->bPathEditMode ) {
		(void)xuiInputSetText(pDialog->pPathInput, pDialog->sCurrentDir != NULL ? pDialog->sCurrentDir : "");
		(void)xuiInputSetError(pDialog->pPathInput, 0);
		(void)xuiSetFocusWidget(pDialog->pContext, pDialog->pPathInput);
	} else if ( !pDialog->bPathApplying && xuiGetFocusWidget(pDialog->pContext) == pDialog->pPathInput ) {
		(void)xuiSetFocusWidget(pDialog->pContext, NULL);
	}
	if ( pDialog->pPathBar != NULL ) {
		(void)xuiWidgetInvalidate(pDialog->pPathBar, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiFileDialogShowPathError(xui_file_dialog pDialog, const char* sPath)
{
	char sMessage[512];

	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)xuiInputSetError(pDialog->pPathInput, 1);
	snprintf(sMessage, sizeof(sMessage), xuiTranslate(pDialog->pContext, XUI_TR_FILE_PATH_NOT_EXIST_FMT), (sPath != NULL && sPath[0] != 0) ? sPath : xuiTranslate(pDialog->pContext, XUI_TR_FILE_EMPTY));
	if ( pDialog->pPathTip != NULL ) {
		(void)xuiMsgTipShow(pDialog->pPathTip, XUI_MSGTIP_ICON_ERROR, sMessage, 2.2f);
	}
	return XUI_OK;
}

static int __xuiFileDialogShowFolderError(xui_file_dialog pDialog)
{
	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)xuiInputSetError(pDialog->pNameInput, 1);
	if ( pDialog->pPathTip != NULL ) {
		(void)xuiMsgTipShow(pDialog->pPathTip, XUI_MSGTIP_ICON_ERROR, xuiTranslate(pDialog->pContext, XUI_TR_FILE_SELECT_FOLDER_FIRST), 2.2f);
	}
	return XUI_OK;
}

static int __xuiFileDialogUpdateBreadcrumb(xui_file_dialog pDialog)
{
	const char* sPath;
	char sLabel[512];
	size_t iLen;
	size_t iStart;
	size_t iEnd;
	int iRootLen;
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) || pDialog->pPathBreadcrumb == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiBreadcrumbClearItems(pDialog->pPathBreadcrumb);
	if ( iRet != XUI_OK ) return iRet;
	sPath = pDialog->sCurrentDir != NULL ? pDialog->sCurrentDir : "";
	if ( sPath[0] == 0 ) {
		return xuiBreadcrumbAddItem(pDialog->pPathBreadcrumb, "Root", 1, 0);
	}
	iLen = strlen(sPath);
	iRootLen = __xuiFileDialogPathRootLen(sPath);
	if ( iRootLen > 0 ) {
		size_t iCopy = (size_t)iRootLen;
		if ( iCopy >= sizeof(sLabel) ) iCopy = sizeof(sLabel) - 1u;
		memcpy(sLabel, sPath, iCopy);
		sLabel[iCopy] = 0;
		iRet = xuiBreadcrumbAddItem(pDialog->pPathBreadcrumb, sLabel, 1, iRootLen);
		if ( iRet != XUI_OK ) return iRet;
	}
	iStart = (size_t)iRootLen;
	while ( iStart < iLen && __xuiFileDialogIsSlash(sPath[iStart]) ) {
		iStart++;
	}
	while ( iStart < iLen && xuiBreadcrumbGetItemCount(pDialog->pPathBreadcrumb) < XUI_BREADCRUMB_MAX_ITEMS ) {
		iEnd = iStart;
		while ( iEnd < iLen && !__xuiFileDialogIsSlash(sPath[iEnd]) ) {
			iEnd++;
		}
		if ( iEnd > iStart ) {
			size_t iCopy = iEnd - iStart;
			if ( iCopy >= sizeof(sLabel) ) iCopy = sizeof(sLabel) - 1u;
			memcpy(sLabel, sPath + iStart, iCopy);
			sLabel[iCopy] = 0;
			iRet = xuiBreadcrumbAddItem(pDialog->pPathBreadcrumb, sLabel, 1, (int)iEnd);
			if ( iRet != XUI_OK ) return iRet;
		}
		iStart = iEnd;
		while ( iStart < iLen && __xuiFileDialogIsSlash(sPath[iStart]) ) {
			iStart++;
		}
	}
	if ( xuiBreadcrumbGetItemCount(pDialog->pPathBreadcrumb) == 0 ) {
		return xuiBreadcrumbAddItem(pDialog->pPathBreadcrumb, sPath, 1, (int)iLen);
	}
	return XUI_OK;
}

static int __xuiFileDialogSetCurrentDir(xui_file_dialog pDialog, const char* sDir)
{
	int iRet;

	iRet = __xuiFileDialogSetString(&pDialog->sCurrentDir, sDir != NULL ? sDir : "");
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	(void)xuiInputSetText(pDialog->pPathInput, pDialog->sCurrentDir);
	(void)xuiInputSetError(pDialog->pPathInput, 0);
	(void)xuiInputSetError(pDialog->pNameInput, 0);
	if ( pDialog->pPathBreadcrumb != NULL ) {
		iRet = __xuiFileDialogUpdateBreadcrumb(pDialog);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiFileDialogBuildResultPath(xui_file_dialog pDialog, const char* sNameOrPath)
{
	char* sPath;
	int iRet;

	if ( pDialog == NULL || sNameOrPath == NULL || sNameOrPath[0] == 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xrtPathIsAbs((str)sNameOrPath, 0u) || pDialog->sCurrentDir == NULL || pDialog->sCurrentDir[0] == 0 ) {
		return __xuiFileDialogSetString(&pDialog->sResultPath, sNameOrPath);
	}
	sPath = (char*)xrtPathJoin(2, pDialog->sCurrentDir, sNameOrPath);
	if ( sPath == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiFileDialogSetString(&pDialog->sResultPath, sPath);
	xrtFree(sPath);
	return iRet;
}

static int __xuiFileDialogBuildCurrentPath(xui_file_dialog pDialog, const char* sNameOrPath, char** psPath)
{
	if ( pDialog == NULL || sNameOrPath == NULL || sNameOrPath[0] == 0 || psPath == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*psPath = NULL;
	if ( xrtPathIsAbs((str)sNameOrPath, 0u) || pDialog->sCurrentDir == NULL || pDialog->sCurrentDir[0] == 0 ) {
		*psPath = __xuiFileDialogDup(sNameOrPath);
	} else {
		*psPath = (char*)xrtPathJoin(2, pDialog->sCurrentDir, sNameOrPath);
	}
	return (*psPath != NULL) ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiFileDialogNameHasExtension(const char* sNameOrPath)
{
	const char* sLastSep;
	const char* sDot;

	if ( sNameOrPath == NULL || sNameOrPath[0] == 0 ) {
		return 0;
	}
	sLastSep = strrchr(sNameOrPath, '/');
	if ( strrchr(sNameOrPath, '\\') != NULL && (sLastSep == NULL || strrchr(sNameOrPath, '\\') > sLastSep) ) {
		sLastSep = strrchr(sNameOrPath, '\\');
	}
	sDot = strrchr(sNameOrPath, '.');
	return (sDot != NULL) && (sLastSep == NULL || sDot > sLastSep);
}

static char* __xuiFileDialogDefaultSaveExtension(xui_file_dialog pDialog)
{
	const char* sPattern;
	const char* sStart;
	const char* sEnd;
	const char* sDot;
	size_t iLen;

	if ( !__xuiFileDialogValid(pDialog) || pDialog->iSelectedFilter < 0 || pDialog->iSelectedFilter >= pDialog->iFilterCount ) {
		return NULL;
	}
	sPattern = pDialog->arrFilterPatterns[pDialog->iSelectedFilter];
	if ( sPattern == NULL ) {
		return NULL;
	}
	sStart = sPattern;
	while ( *sStart != 0 ) {
		while ( *sStart == ';' || *sStart == ',' || isspace((unsigned char)*sStart) ) {
			sStart++;
		}
		sEnd = sStart;
		while ( *sEnd != 0 && *sEnd != ';' && *sEnd != ',' && !isspace((unsigned char)*sEnd) ) {
			sEnd++;
		}
		iLen = (size_t)(sEnd - sStart);
		if ( iLen > 2u && sStart[0] == '*' && sStart[1] == '.' ) {
			if ( iLen == 3u && sStart[2] == '*' ) {
				return NULL;
			}
			sDot = sStart + 1;
			return __xuiFileDialogDupRange(sDot, iLen - 1u);
		}
		sStart = sEnd;
	}
	return NULL;
}

static int __xuiFileDialogBuildSavePath(xui_file_dialog pDialog, const char* sNameOrPath, char** psPath)
{
	char* sNameWithExt;
	char* sExt;
	size_t iNameLen;
	size_t iExtLen;
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) || sNameOrPath == NULL || sNameOrPath[0] == 0 || psPath == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*psPath = NULL;
	if ( __xuiFileDialogNameHasExtension(sNameOrPath) ) {
		return __xuiFileDialogBuildCurrentPath(pDialog, sNameOrPath, psPath);
	}
	sExt = __xuiFileDialogDefaultSaveExtension(pDialog);
	if ( sExt == NULL || sExt[0] == 0 ) {
		if ( sExt != NULL ) xrtFree(sExt);
		return __xuiFileDialogBuildCurrentPath(pDialog, sNameOrPath, psPath);
	}
	iNameLen = strlen(sNameOrPath);
	iExtLen = strlen(sExt);
	sNameWithExt = (char*)xrtMalloc(iNameLen + iExtLen + 1u);
	if ( sNameWithExt == NULL ) {
		xrtFree(sExt);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memcpy(sNameWithExt, sNameOrPath, iNameLen);
	memcpy(sNameWithExt + iNameLen, sExt, iExtLen + 1u);
	iRet = __xuiFileDialogBuildCurrentPath(pDialog, sNameWithExt, psPath);
	xrtFree(sNameWithExt);
	xrtFree(sExt);
	return iRet;
}

static char* __xuiFileDialogParentDir(const char* sDir)
{
	size_t iLen;
	size_t iEnd;
	size_t iPos;
	size_t iRootLen;
	char* sParent;

	if ( sDir == NULL || sDir[0] == 0 ) {
		return __xuiFileDialogDup("");
	}
	iLen = strlen(sDir);
	iEnd = iLen;
	while ( iEnd > 0u && (sDir[iEnd - 1u] == '/' || sDir[iEnd - 1u] == '\\') ) {
		iEnd--;
	}
	if ( iEnd == 0u ) {
		return __xuiFileDialogDup("");
	}
	iRootLen = 0u;
	if ( iEnd >= 2u && sDir[1] == ':' ) {
		iRootLen = (iLen >= 3u && (sDir[2] == '/' || sDir[2] == '\\')) ? 3u : 2u;
	} else if ( sDir[0] == '/' || sDir[0] == '\\' ) {
		iRootLen = 1u;
	}
	if ( iEnd <= iRootLen ) {
		return __xuiFileDialogDup("");
	}
	iPos = iEnd;
	while ( iPos > 0u && sDir[iPos - 1u] != '/' && sDir[iPos - 1u] != '\\' ) {
		iPos--;
	}
	if ( iPos == 0u ) {
		return __xuiFileDialogDup("");
	}
	if ( iPos <= iRootLen ) {
		iPos = iRootLen;
	} else {
		iPos--;
	}
	sParent = (char*)xrtMalloc(iPos + 1u);
	if ( sParent == NULL ) {
		return NULL;
	}
	memcpy(sParent, sDir, iPos);
	sParent[iPos] = 0;
	return sParent;
}

static int __xuiFileDialogNotify(xui_file_dialog pDialog, int iResult)
{
	pDialog->iResult = iResult;
	if ( pDialog->onResult != NULL ) {
		pDialog->onResult(pDialog, iResult, pDialog->sResultPath != NULL ? pDialog->sResultPath : "", pDialog->pResultUser);
	}
	return XUI_OK;
}

static int __xuiFileDialogFinishWithPath(xui_file_dialog pDialog, const char* sPath)
{
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) || sPath == NULL || sPath[0] == 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiFileDialogSetString(&pDialog->sResultPath, sPath);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	(void)xuiFileDialogSetOpen(pDialog, 0);
	return __xuiFileDialogNotify(pDialog, XUI_FILE_DIALOG_RESULT_OK);
}

static void __xuiFileDialogOverwriteResult(xui_msgbox pBox, int iResult, void* pUser)
{
	xui_file_dialog pDialog;

	(void)pBox;
	pDialog = (xui_file_dialog)pUser;
	if ( !__xuiFileDialogValid(pDialog) ) {
		return;
	}
	if ( iResult == XUI_MSGBOX_RESULT_YES && pDialog->sPendingSavePath != NULL && pDialog->sPendingSavePath[0] != 0 ) {
		(void)__xuiFileDialogFinishWithPath(pDialog, pDialog->sPendingSavePath);
	}
}

static int __xuiFileDialogShowOverwriteConfirm(xui_file_dialog pDialog, const char* sPath)
{
	xui_msgbox_desc_t tDesc;
	char sMessage[768];
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) || sPath == NULL || sPath[0] == 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiFileDialogSetString(&pDialog->sPendingSavePath, sPath);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pDialog->pOverwriteBox != NULL ) {
		xuiMsgBoxDestroy(pDialog->pOverwriteBox);
		pDialog->pOverwriteBox = NULL;
	}
	snprintf(sMessage, sizeof(sMessage), xuiTranslate(pDialog->pContext, XUI_TR_FILE_OVERWRITE_MESSAGE_FMT), sPath);
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sTitle = xuiTranslate(pDialog->pContext, XUI_TR_FILE_CONFIRM_SAVE_AS);
	tDesc.sMessage = sMessage;
	tDesc.pFont = pDialog->pFont;
	tDesc.iType = XUI_MSGBOX_ICON_QUEST;
	tDesc.iButtons = XUI_MSGBOX_BUTTON_YES_NO;
	iRet = xuiMsgBoxCreate(pDialog->pContext, &pDialog->pOverwriteBox, &tDesc);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	(void)xuiMsgBoxSetResult(pDialog->pOverwriteBox, __xuiFileDialogOverwriteResult, pDialog);
	return xuiMsgBoxSetOpen(pDialog->pOverwriteBox, 1);
}

static int __xuiFileDialogCommitSavePath(xui_file_dialog pDialog, const char* sPath)
{
	if ( !__xuiFileDialogValid(pDialog) || sPath == NULL || sPath[0] == 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xrtFileExists((str)sPath) ) {
		return __xuiFileDialogShowOverwriteConfirm(pDialog, sPath);
	}
	return __xuiFileDialogFinishWithPath(pDialog, sPath);
}

static void __xuiFileDialogRootSelect(xui_widget pWidget, int iIndex, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;
	(void)pWidget;
	if ( __xuiFileDialogValid(pDialog) && iIndex >= 0 && iIndex < pDialog->iRootCount ) {
		(void)xuiFileDialogSetDirectory(pDialog, pDialog->arrRootItems[iIndex]);
	}
}

static void __xuiFileDialogFileSelect(xui_widget pWidget, int iIndex, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;
	double fNow;
	int bDoubleSelect;

	(void)pWidget;
	if ( !__xuiFileDialogValid(pDialog) || iIndex < 0 || iIndex >= pDialog->iEntryCount ) {
		return;
	}
	fNow = xrtTimer();
	bDoubleSelect = (pDialog->iLastSelectEntry == iIndex) &&
	                ((fNow - pDialog->fLastSelectTime) <= XUI_FILE_DIALOG_DOUBLE_SELECT_SECONDS);
	pDialog->iSelectedEntry = iIndex;
	pDialog->iLastSelectEntry = iIndex;
	pDialog->fLastSelectTime = fNow;
	(void)__xuiFileDialogSetNameText(pDialog, pDialog->arrEntries[iIndex].sName);
	if ( bDoubleSelect ) {
		if ( pDialog->iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER && pDialog->arrEntries[iIndex].bDir ) {
			(void)xuiFileDialogSetDirectory(pDialog, pDialog->arrEntries[iIndex].sPath);
		} else if ( pDialog->iMode != XUI_FILE_DIALOG_MODE_SAVE_FILE || pDialog->arrEntries[iIndex].bDir ) {
			(void)xuiFileDialogCommit(pDialog);
		}
	}
}

static void __xuiFileDialogPathChange(xui_widget pWidget, const char* sText, void* pUser)
{
	(void)sText;
	(void)pUser;
	(void)xuiInputSetError(pWidget, 0);
}

static void __xuiFileDialogNameChange(xui_widget pWidget, const char* sText, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;
	(void)sText;
	if ( __xuiFileDialogValid(pDialog) ) {
		if ( !pDialog->bInternalNameChange ) {
			pDialog->iSelectedEntry = -1;
			pDialog->iLastSelectEntry = -1;
			pDialog->fLastSelectTime = 0.0;
			(void)xuiListViewSetSelected(pDialog->pFileList, -1);
		}
	}
	(void)xuiInputSetError(pWidget, 0);
}

static int __xuiFileDialogSyncPathInput(xui_file_dialog pDialog)
{
	const char* sPath;
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !pDialog->bPathEditMode ) {
		return XUI_OK;
	}
	sPath = xuiInputGetText(pDialog->pPathInput);
	if ( sPath == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( sPath[0] == 0 || xrtDirExists((str)sPath) ) {
		if ( pDialog->sCurrentDir == NULL || strcmp(pDialog->sCurrentDir, sPath) != 0 ) {
			iRet = xuiFileDialogSetDirectory(pDialog, sPath);
			if ( iRet != XUI_OK ) return iRet;
		}
		(void)xuiInputSetError(pDialog->pPathInput, 0);
		(void)__xuiFileDialogSetPathEditMode(pDialog, 0);
		return XUI_OK;
	}
	(void)__xuiFileDialogShowPathError(pDialog, sPath);
	return XUI_ERROR_FILE_NOT_FOUND;
}

static void __xuiFileDialogUpClick(xui_widget pWidget, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;

	(void)pWidget;
	if ( __xuiFileDialogSyncPathInput(pDialog) == XUI_OK ) {
		(void)xuiFileDialogGoUp(pDialog);
	}
}

static int __xuiFileDialogCommitPathInput(xui_file_dialog pDialog)
{
	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiFileDialogSyncPathInput(pDialog);
}

static int __xuiFileDialogCommitNameInput(xui_file_dialog pDialog)
{
	const char* sName;
	char* sPath;
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pDialog->iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER ) {
		(void)xuiInputSetError(pDialog->pNameInput, 0);
		return xuiFileDialogCommit(pDialog);
	}
	sName = xuiInputGetText(pDialog->pNameInput);
	if ( sName == NULL || sName[0] == 0 ) {
		(void)xuiInputSetError(pDialog->pNameInput, 1);
		return XUI_OK;
	}
	sPath = NULL;
	iRet = __xuiFileDialogBuildCurrentPath(pDialog, sName, &sPath);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( xrtDirExists((str)sPath) && pDialog->iMode != XUI_FILE_DIALOG_MODE_SELECT_FOLDER ) {
		(void)xuiInputSetError(pDialog->pNameInput, 0);
		iRet = xuiFileDialogSetDirectory(pDialog, sPath);
		xrtFree(sPath);
		return iRet;
	}
	if ( pDialog->iMode == XUI_FILE_DIALOG_MODE_OPEN_FILE && !xrtFileExists((str)sPath) ) {
		(void)xuiInputSetError(pDialog->pNameInput, 1);
		xrtFree(sPath);
		return XUI_OK;
	}
	(void)xuiInputSetError(pDialog->pNameInput, 0);
	xrtFree(sPath);
	return xuiFileDialogCommit(pDialog);
}

static void __xuiFileDialogDefaultAction(xui_widget pWidget, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;
	(void)pWidget;
	if ( !__xuiFileDialogValid(pDialog) ) {
		return;
	}
	if ( xuiGetFocusWidget(pDialog->pContext) == pDialog->pPathInput ) {
		(void)__xuiFileDialogCommitPathInput(pDialog);
	} else {
		(void)__xuiFileDialogCommitNameInput(pDialog);
	}
}

static void __xuiFileDialogBreadcrumbClick(xui_widget pWidget, int iIndex, int iValue, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;
	char* sPath;
	size_t iLen;
	int iRet;

	(void)pWidget;
	(void)iIndex;
	if ( !__xuiFileDialogValid(pDialog) || iValue < 0 ) {
		return;
	}
	iLen = strlen(pDialog->sCurrentDir != NULL ? pDialog->sCurrentDir : "");
	if ( (size_t)iValue > iLen ) {
		return;
	}
	if ( iValue == 0 ) {
		(void)xuiFileDialogSetDirectory(pDialog, "");
		return;
	}
	sPath = (char*)xrtMalloc((size_t)iValue + 1u);
	if ( sPath == NULL ) {
		return;
	}
	memcpy(sPath, pDialog->sCurrentDir, (size_t)iValue);
	sPath[iValue] = 0;
	iRet = xuiFileDialogSetDirectory(pDialog, sPath);
	xrtFree(sPath);
	(void)iRet;
}

static void __xuiFileDialogFilterSelect(xui_widget pWidget, int iIndex, int iValue, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;

	(void)pWidget;
	(void)iValue;
	if ( !__xuiFileDialogValid(pDialog) || iIndex < 0 || iIndex >= pDialog->iFilterCount ) {
		return;
	}
	pDialog->iSelectedFilter = iIndex;
	if ( __xuiFileDialogSetString(&pDialog->sFilter, pDialog->arrFilterPatterns[iIndex]) == XUI_OK ) {
		(void)xuiFileDialogRefresh(pDialog);
	}
}

static int __xuiFileDialogPathBarEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;
	int bLeftButton;

	(void)pWidget;
	if ( !__xuiFileDialogValid(pDialog) || pEvent == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE &&
	     pEvent->iType == XUI_EVENT_POINTER_DOWN &&
	     bLeftButton &&
	     !pDialog->bPathEditMode &&
	     __xuiFileDialogBreadcrumbHit(pDialog->pPathBreadcrumb, pEvent->fX, pEvent->fY) < 0 ) {
		(void)__xuiFileDialogSetPathEditMode(pDialog, 1);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiFileDialogPathInputBlurEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) || pEvent == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pDialog->onPathInputBlur != NULL ) {
		iRet = pDialog->onPathInputBlur(pWidget, pEvent, pDialog->pPathInputBlurUser);
		if ( iRet < 0 ) return iRet;
	} else {
		iRet = XUI_OK;
	}
	if ( pEvent->iType == XUI_EVENT_BLUR && xuiWidgetGetVisible(pDialog->pPathInput) && !pDialog->bPathApplying ) {
		pDialog->bPathApplying = 1;
		(void)__xuiFileDialogCommitPathInput(pDialog);
		pDialog->bPathApplying = 0;
	}
	return iRet;
}

static int __xuiFileDialogWindowPointerEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;
	xui_rect_t tPath;
	int bLeftButton;
	int iRet;

	(void)pWidget;
	if ( !__xuiFileDialogValid(pDialog) || pEvent == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ||
	     pEvent->iType != XUI_EVENT_POINTER_DOWN ||
	     !bLeftButton ||
	     !xuiWidgetGetVisible(pDialog->pPathInput) ) {
		return XUI_OK;
	}
	tPath = xuiWidgetGetWorldRect(pDialog->pPathBar);
	if ( pEvent->fX >= tPath.fX && pEvent->fY >= tPath.fY &&
	     pEvent->fX < tPath.fX + tPath.fW && pEvent->fY < tPath.fY + tPath.fH ) {
		return XUI_OK;
	}
	pDialog->bPathApplying = 1;
	iRet = __xuiFileDialogCommitPathInput(pDialog);
	pDialog->bPathApplying = 0;
	if ( iRet != XUI_OK ) {
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static void __xuiFileDialogCancelAction(xui_widget pWidget, void* pUser)
{
	(void)pWidget;
	(void)xuiFileDialogCancel((xui_file_dialog)pUser);
}

static void __xuiFileDialogRefreshClick(xui_widget pWidget, void* pUser)
{
	xui_file_dialog pDialog = (xui_file_dialog)pUser;

	(void)pWidget;
	if ( __xuiFileDialogSyncPathInput(pDialog) == XUI_OK ) {
		(void)xuiFileDialogRefresh(pDialog);
	}
}

static void __xuiFileDialogOkClick(xui_widget pWidget, void* pUser)
{
	(void)pWidget;
	(void)__xuiFileDialogCommitNameInput((xui_file_dialog)pUser);
}

static void __xuiFileDialogCancelClick(xui_widget pWidget, void* pUser)
{
	(void)pWidget;
	(void)xuiFileDialogCancel((xui_file_dialog)pUser);
}

static void __xuiFileDialogWindowClose(xui_widget pWidget, void* pUser)
{
	(void)pWidget;
	(void)xuiFileDialogCancel((xui_file_dialog)pUser);
}

static int __xuiFileDialogLayoutInitial(xui_file_dialog pDialog)
{
	xui_vec2_t tViewport;
	float fW;
	float fH;
	float fX;
	float fY;

	if ( pDialog == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fW = pDialog->fWidth;
	fH = pDialog->fHeight;
	tViewport = xuiGetViewportSize(pDialog->pContext);
	if ( tViewport.fX > 0.0f ) {
		fW = (fW > tViewport.fX - 24.0f) ? __xuiFileDialogMaxFloat(560.0f, tViewport.fX - 24.0f) : fW;
	}
	if ( tViewport.fY > 0.0f ) {
		fH = (fH > tViewport.fY - 24.0f) ? __xuiFileDialogMaxFloat(430.0f, tViewport.fY - 24.0f) : fH;
	}
	fX = (tViewport.fX > fW) ? ((tViewport.fX - fW) * 0.5f) : 12.0f;
	fY = (tViewport.fY > fH) ? ((tViewport.fY - fH) * 0.5f) : 12.0f;
	(void)xuiWidgetSetRect(pDialog->pWindow, (xui_rect_t){fX, fY, fW, fH});
	return XUI_OK;
}

XUI_API int xuiFileDialogCreate(xui_context pContext, xui_file_dialog* ppDialog, const xui_file_dialog_desc_t* pDesc)
{
	xui_file_dialog pDialog;
	xui_window_desc_t tWindow;
	xui_label_desc_t tLabel;
	xui_input_desc_t tInput;
	xui_button_desc_t tButton;
	xui_list_view_desc_t tList;
	xui_combobox_desc_t tCombo;
	xui_breadcrumb_desc_t tBreadcrumb;
	xui_msgtip_desc_t tTip;
	xui_widget pClient;
	const char* sTitle;
	const char* sInitialDir;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || ppDialog == NULL || !__xuiFileDialogDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppDialog = NULL;
	pDialog = (xui_file_dialog)xrtCalloc(1, sizeof(*pDialog));
	if ( pDialog == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDialog->iMagic = XUI_FILE_DIALOG_MAGIC;
	pDialog->pContext = pContext;
	pDialog->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pDialog->iMode = (pDesc != NULL && pDesc->iMode != 0) ? pDesc->iMode : XUI_FILE_DIALOG_MODE_OPEN_FILE;
	pDialog->fWidth = (pDesc != NULL && pDesc->fWidth > 0.0f) ? pDesc->fWidth : 720.0f;
	pDialog->fHeight = (pDesc != NULL && pDesc->fHeight > 0.0f) ? pDesc->fHeight : 520.0f;
	pDialog->iSelectedEntry = -1;
	pDialog->iLastSelectEntry = -1;
	pDialog->iResult = XUI_FILE_DIALOG_RESULT_NONE;
	pDialog->onResult = (pDesc != NULL) ? pDesc->onResult : NULL;
	pDialog->pResultUser = (pDesc != NULL) ? pDesc->pResultUser : NULL;
	iRet = __xuiFileDialogSetFilters(pDialog, (pDesc != NULL) ? pDesc->sFilter : NULL);
	if ( iRet == XUI_OK ) iRet = __xuiFileDialogSetString(&pDialog->sFileName, (pDesc != NULL) ? pDesc->sFileName : NULL);
	if ( iRet != XUI_OK ) {
		xuiFileDialogDestroy(pDialog);
		return iRet;
	}
	memset(&tWindow, 0, sizeof(tWindow));
	tWindow.iSize = sizeof(tWindow);
	sTitle = (pDesc != NULL && pDesc->sTitle != NULL) ? pDesc->sTitle : __xuiFileDialogDefaultTitle(pContext, pDialog->iMode);
	tWindow.sTitle = sTitle;
	tWindow.pFont = pDialog->pFont;
	tWindow.bClosed = 1;
	tWindow.bTopMost = 1;
	tWindow.bHideCollapse = 1;
	tWindow.bHideMaximize = 1;
	tWindow.fMinWidth = 560.0f;
	tWindow.fMinHeight = 430.0f;
	iRet = xuiWindowCreate(pContext, &pDialog->pWindow, &tWindow);
	if ( iRet != XUI_OK ) {
		xuiFileDialogDestroy(pDialog);
		return iRet;
	}
	(void)xuiWindowSetClose(pDialog->pWindow, __xuiFileDialogWindowClose, pDialog);
	pClient = xuiWindowGetClientWidget(pDialog->pWindow);
	(void)xuiWidgetSetLayoutType(pClient, XUI_LAYOUT_COLUMN);
	(void)xuiWidgetSetSizeMode(pClient, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetPadding(pClient, (xui_thickness_t){14.0f, 12.0f, 14.0f, 14.0f});
	(void)xuiWidgetSetGap(pClient, 8.0f);
	memset(&tLabel, 0, sizeof(tLabel));
	tLabel.iSize = sizeof(tLabel);
	tLabel.pFont = pDialog->pFont;
	tLabel.iTextColor = XUI_COLOR_RGBA(40, 56, 76, 255);
	tLabel.iTextFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
	tLabel.sText = xuiTranslate(pContext, XUI_TR_FILE_PATH);
	iRet = xuiLabelCreate(pContext, &pDialog->pPathLabel, &tLabel);
	tLabel.sText = (pDialog->iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER) ? xuiTranslate(pContext, XUI_TR_FILE_FOLDER) : xuiTranslate(pContext, XUI_TR_FILE_FILE);
	if ( iRet == XUI_OK ) iRet = xuiLabelCreate(pContext, &pDialog->pNameLabel, &tLabel);
	tLabel.sText = xuiTranslate(pContext, XUI_TR_FILE_TYPE);
	if ( iRet == XUI_OK ) iRet = xuiLabelCreate(pContext, &pDialog->pFilterLabel, &tLabel);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(pContext, &pDialog->pPathRow);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(pContext, &pDialog->pListRow);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(pContext, &pDialog->pFieldRow);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(pContext, &pDialog->pButtonRow);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(pContext, &pDialog->pButtonSpacer);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(pContext, &pDialog->pPathBar);
	memset(&tBreadcrumb, 0, sizeof(tBreadcrumb));
	tBreadcrumb.iSize = sizeof(tBreadcrumb);
	tBreadcrumb.pFont = pDialog->pFont;
	tBreadcrumb.sSeparator = ">";
	tBreadcrumb.iTextColor = XUI_COLOR_RGBA(52, 76, 105, 255);
	tBreadcrumb.iHoverTextColor = XUI_COLOR_RGBA(31, 117, 214, 255);
	tBreadcrumb.iActiveTextColor = XUI_COLOR_RGBA(18, 83, 168, 255);
	tBreadcrumb.iDisabledTextColor = XUI_COLOR_RGBA(90, 105, 124, 255);
	tBreadcrumb.iSeparatorColor = XUI_COLOR_RGBA(150, 164, 181, 255);
	tBreadcrumb.fGap = 6.0f;
	tBreadcrumb.fPaddingX = 0.0f;
	tBreadcrumb.fPaddingY = 4.0f;
	if ( iRet == XUI_OK ) iRet = xuiBreadcrumbCreate(pContext, &pDialog->pPathBreadcrumb, &tBreadcrumb);
	memset(&tInput, 0, sizeof(tInput));
	tInput.iSize = sizeof(tInput);
	tInput.pFont = pDialog->pFont;
	tInput.bReadonly = 0;
	if ( iRet == XUI_OK ) iRet = xuiInputCreate(pContext, &pDialog->pPathInput, &tInput);
	tInput.bReadonly = 0;
	if ( iRet == XUI_OK ) iRet = xuiInputCreate(pContext, &pDialog->pNameInput, &tInput);
	memset(&tCombo, 0, sizeof(tCombo));
	tCombo.iSize = sizeof(tCombo);
	tCombo.arrItems = (const char**)pDialog->arrFilterNames;
	tCombo.iItemCount = pDialog->iFilterCount;
	tCombo.iSelected = pDialog->iSelectedFilter;
	tCombo.iMode = XUI_COMBOBOX_MODE_SELECT;
	tCombo.pFont = pDialog->pFont;
	tCombo.fPopupMaxHeight = 220.0f;
	if ( iRet == XUI_OK ) iRet = xuiComboBoxCreate(pContext, &pDialog->pFilterCombo, &tCombo);
	memset(&tButton, 0, sizeof(tButton));
	tButton.iSize = sizeof(tButton);
	tButton.pFont = pDialog->pFont;
	tButton.fBorderWidth = 1.0f;
	tButton.sText = "";
	if ( iRet == XUI_OK ) iRet = xuiButtonCreate(pContext, &pDialog->pUpButton, &tButton);
	tButton.sText = "";
	if ( iRet == XUI_OK ) iRet = xuiButtonCreate(pContext, &pDialog->pRefreshButton, &tButton);
	tButton.sText = __xuiFileDialogOkText(pContext, pDialog->iMode);
	if ( iRet == XUI_OK ) iRet = xuiButtonCreate(pContext, &pDialog->pOkButton, &tButton);
	tButton.sText = xuiTranslate(pContext, XUI_TR_FILE_CANCEL);
	if ( iRet == XUI_OK ) iRet = xuiButtonCreate(pContext, &pDialog->pCancelButton, &tButton);
	memset(&tList, 0, sizeof(tList));
	tList.iSize = sizeof(tList);
	tList.pFont = pDialog->pFont;
	tList.iSelectionMode = XUI_SELECTION_SINGLE;
	tList.fItemHeight = 24.0f;
	if ( iRet == XUI_OK ) iRet = xuiListViewCreate(pContext, &pDialog->pRootList, &tList);
	if ( iRet == XUI_OK ) iRet = xuiListViewCreate(pContext, &pDialog->pFileList, &tList);
	memset(&tTip, 0, sizeof(tTip));
	tTip.iSize = sizeof(tTip);
	tTip.pFont = pDialog->pFont;
	tTip.iType = XUI_MSGTIP_ICON_ERROR;
	tTip.sText = "";
	tTip.fDuration = 2.2f;
	if ( iRet == XUI_OK ) iRet = xuiMsgTipCreate(pContext, &pDialog->pPathTip, &tTip);
	if ( iRet != XUI_OK ) {
		xuiFileDialogDestroy(pDialog);
		return iRet;
	}
	(void)xuiWidgetSetLayoutType(pDialog->pPathRow, XUI_LAYOUT_ROW);
	(void)xuiWidgetSetLayoutType(pDialog->pListRow, XUI_LAYOUT_ROW);
	(void)xuiWidgetSetLayoutType(pDialog->pFieldRow, XUI_LAYOUT_ROW);
	(void)xuiWidgetSetLayoutType(pDialog->pButtonRow, XUI_LAYOUT_ROW);
	(void)xuiWidgetSetLayoutType(pDialog->pPathBar, XUI_LAYOUT_OVERLAY);
	(void)xuiWidgetSetGap(pDialog->pPathRow, 8.0f);
	(void)xuiWidgetSetGap(pDialog->pListRow, 8.0f);
	(void)xuiWidgetSetGap(pDialog->pFieldRow, 8.0f);
	(void)xuiWidgetSetGap(pDialog->pButtonRow, 8.0f);
	(void)xuiWidgetSetSizeMode(pDialog->pPathRow, XUI_SIZE_FILL, XUI_SIZE_FIXED);
	(void)xuiWidgetSetSizeMode(pDialog->pListRow, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetSizeMode(pDialog->pFieldRow, XUI_SIZE_FILL, XUI_SIZE_FIXED);
	(void)xuiWidgetSetSizeMode(pDialog->pButtonRow, XUI_SIZE_FILL, XUI_SIZE_FIXED);
	(void)xuiWidgetSetPreferredSize(pDialog->pPathRow, (xui_vec2_t){0.0f, 32.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pFieldRow, (xui_vec2_t){0.0f, 28.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pButtonRow, (xui_vec2_t){0.0f, 28.0f});
	(void)xuiWidgetSetFlex(pDialog->pListRow, 1.0f, 1.0f);
	(void)xuiWidgetSetMinSize(pDialog->pListRow, (xui_vec2_t){0.0f, 72.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pPathLabel, (xui_vec2_t){58.0f, 32.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pNameLabel, (xui_vec2_t){58.0f, 28.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pPathBar, (xui_vec2_t){0.0f, 32.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pRootList, (xui_vec2_t){132.0f, 0.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pFilterCombo, (xui_vec2_t){210.0f, 28.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pUpButton, (xui_vec2_t){32.0f, 32.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pRefreshButton, (xui_vec2_t){32.0f, 32.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pOkButton, (xui_vec2_t){78.0f, 28.0f});
	(void)xuiWidgetSetPreferredSize(pDialog->pCancelButton, (xui_vec2_t){74.0f, 28.0f});
	(void)xuiWidgetSetSizeMode(pDialog->pPathBar, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetSizeMode(pDialog->pPathBreadcrumb, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetSizeMode(pDialog->pPathInput, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetSizeMode(pDialog->pRootList, XUI_SIZE_FIXED, XUI_SIZE_FILL);
	(void)xuiWidgetSetSizeMode(pDialog->pFileList, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetSizeMode(pDialog->pNameInput, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetSizeMode(pDialog->pButtonSpacer, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetFlex(pDialog->pPathBar, 1.0f, 1.0f);
	(void)xuiWidgetSetFlex(pDialog->pFileList, 1.0f, 1.0f);
	(void)xuiWidgetSetFlex(pDialog->pNameInput, 1.0f, 1.0f);
	(void)xuiWidgetSetFlex(pDialog->pButtonSpacer, 1.0f, 1.0f);
	(void)xuiWidgetSetMargin(pDialog->pPathBreadcrumb, (xui_thickness_t){8.0f, 0.0f, 8.0f, 0.0f});
	(void)xuiWidgetSetFocusable(pDialog->pPathBar, 0);
	(void)xuiWidgetSetCacheRenderCallback(pDialog->pPathBar, __xuiFileDialogPathBarRender, pDialog);
	(void)xuiWidgetSetEventCallback(pDialog->pPathBar, __xuiFileDialogPathBarEvent, pDialog);
	(void)xuiWidgetAddChild(pDialog->pPathBar, pDialog->pPathBreadcrumb);
	(void)xuiWidgetAddChild(pDialog->pPathBar, pDialog->pPathInput);
	(void)xuiWidgetSetVisible(pDialog->pPathInput, 0);
	(void)xuiButtonSetClick(pDialog->pUpButton, __xuiFileDialogUpClick, pDialog);
	(void)xuiButtonSetClick(pDialog->pRefreshButton, __xuiFileDialogRefreshClick, pDialog);
	(void)__xuiFileDialogSetButtonIcon(pDialog, pDialog->pUpButton, XUI_FILE_DIALOG_ICON_UP);
	(void)__xuiFileDialogSetButtonIcon(pDialog, pDialog->pRefreshButton, XUI_FILE_DIALOG_ICON_REFRESH);
	(void)xuiButtonSetClick(pDialog->pOkButton, __xuiFileDialogOkClick, pDialog);
	(void)xuiButtonSetClick(pDialog->pCancelButton, __xuiFileDialogCancelClick, pDialog);
	(void)xuiWidgetSetEventHandler(pDialog->pWindow, XUI_EVENT_POINTER_DOWN, __xuiFileDialogWindowPointerEvent, pDialog);
	(void)xuiWidgetSetDefaultAction(pDialog->pWindow, __xuiFileDialogDefaultAction, pDialog);
	(void)xuiWidgetSetCancelAction(pDialog->pWindow, __xuiFileDialogCancelAction, pDialog);
	(void)xuiWidgetSetDefaultAction(pDialog->pPathInput, __xuiFileDialogDefaultAction, pDialog);
	(void)xuiWidgetSetCancelAction(pDialog->pPathInput, __xuiFileDialogCancelAction, pDialog);
	(void)xuiWidgetGetEventHandler(pDialog->pPathInput, XUI_EVENT_BLUR, &pDialog->onPathInputBlur, &pDialog->pPathInputBlurUser);
	(void)xuiWidgetSetEventHandler(pDialog->pPathInput, XUI_EVENT_BLUR, __xuiFileDialogPathInputBlurEvent, pDialog);
	(void)xuiWidgetSetDefaultAction(pDialog->pNameInput, __xuiFileDialogDefaultAction, pDialog);
	(void)xuiWidgetSetCancelAction(pDialog->pNameInput, __xuiFileDialogCancelAction, pDialog);
	(void)xuiInputSetChange(pDialog->pPathInput, __xuiFileDialogPathChange, pDialog);
	(void)xuiInputSetChange(pDialog->pNameInput, __xuiFileDialogNameChange, pDialog);
	(void)xuiBreadcrumbSetClick(pDialog->pPathBreadcrumb, __xuiFileDialogBreadcrumbClick, pDialog);
	(void)xuiComboBoxSetSelect(pDialog->pFilterCombo, __xuiFileDialogFilterSelect, pDialog);
	(void)xuiListViewSetSelect(pDialog->pRootList, __xuiFileDialogRootSelect, pDialog);
	(void)xuiListViewSetSelect(pDialog->pFileList, __xuiFileDialogFileSelect, pDialog);
	(void)xuiListViewSetItemRenderer(pDialog->pFileList, __xuiFileDialogDrawFileItem, pDialog);
	(void)xuiListViewSetNotifyRepeatSelect(pDialog->pFileList, 1);
	(void)xuiWindowAddChild(pDialog->pWindow, pDialog->pPathRow);
	(void)xuiWindowAddChild(pDialog->pWindow, pDialog->pListRow);
	(void)xuiWindowAddChild(pDialog->pWindow, pDialog->pFieldRow);
	(void)xuiWindowAddChild(pDialog->pWindow, pDialog->pButtonRow);
	(void)xuiWidgetAddChild(pDialog->pPathRow, pDialog->pPathLabel);
	(void)xuiWidgetAddChild(pDialog->pPathRow, pDialog->pPathBar);
	(void)xuiWidgetAddChild(pDialog->pPathRow, pDialog->pUpButton);
	(void)xuiWidgetAddChild(pDialog->pPathRow, pDialog->pRefreshButton);
	(void)xuiWidgetAddChild(pDialog->pListRow, pDialog->pRootList);
	(void)xuiWidgetAddChild(pDialog->pListRow, pDialog->pFileList);
	(void)xuiWidgetAddChild(pDialog->pFieldRow, pDialog->pNameLabel);
	(void)xuiWidgetAddChild(pDialog->pFieldRow, pDialog->pNameInput);
	(void)xuiWidgetAddChild(pDialog->pFieldRow, pDialog->pFilterCombo);
	(void)xuiWidgetAddChild(pDialog->pButtonRow, pDialog->pButtonSpacer);
	(void)xuiWidgetAddChild(pDialog->pButtonRow, pDialog->pOkButton);
	(void)xuiWidgetAddChild(pDialog->pButtonRow, pDialog->pCancelButton);
	if ( pDialog->iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER ) {
		(void)xuiWidgetSetVisible(pDialog->pFilterCombo, 0);
		(void)xuiInputSetReadonly(pDialog->pNameInput, 1);
	}
	(void)xuiWidgetSetVisible(pDialog->pFilterLabel, 0);
	iRet = __xuiFileDialogLayoutInitial(pDialog);
	sInitialDir = (pDesc != NULL && pDesc->sInitialDir != NULL) ? pDesc->sInitialDir : ".";
	if ( iRet == XUI_OK ) iRet = xuiFileDialogSetDirectory(pDialog, sInitialDir);
	if ( iRet == XUI_OK ) iRet = __xuiFileDialogSetPathEditMode(pDialog, 0);
	if ( iRet == XUI_OK ) {
		iRet = __xuiFileDialogSetNameText(
			pDialog,
			(pDialog->iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER) ? "" : (pDialog->sFileName != NULL ? pDialog->sFileName : "")
		);
	}
	if ( iRet != XUI_OK ) {
		xuiFileDialogDestroy(pDialog);
		return iRet;
	}
	*ppDialog = pDialog;
	return XUI_OK;
}

XUI_API void xuiFileDialogDestroy(xui_file_dialog pDialog)
{
	if ( !__xuiFileDialogValid(pDialog) ) {
		return;
	}
	(void)xuiFileDialogSetOpen(pDialog, 0);
	if ( pDialog->pPathTip != NULL ) {
		xuiMsgTipDestroy(pDialog->pPathTip);
		pDialog->pPathTip = NULL;
	}
	if ( pDialog->pOverwriteBox != NULL ) {
		xuiMsgBoxDestroy(pDialog->pOverwriteBox);
		pDialog->pOverwriteBox = NULL;
	}
	if ( pDialog->pWindow != NULL ) {
		xuiWidgetDestroy(pDialog->pWindow);
	}
	__xuiFileDialogFreeRootItems(pDialog);
	__xuiFileDialogClearEntries(pDialog);
	if ( pDialog->arrEntries != NULL ) xrtFree(pDialog->arrEntries);
	if ( pDialog->arrEntryItems != NULL ) xrtFree(pDialog->arrEntryItems);
	if ( pDialog->sCurrentDir != NULL ) xrtFree(pDialog->sCurrentDir);
	if ( pDialog->sFileName != NULL ) xrtFree(pDialog->sFileName);
	if ( pDialog->sFilter != NULL ) xrtFree(pDialog->sFilter);
	if ( pDialog->sResultPath != NULL ) xrtFree(pDialog->sResultPath);
	if ( pDialog->sPendingSavePath != NULL ) xrtFree(pDialog->sPendingSavePath);
	__xuiFileDialogFreeFilters(pDialog);
	pDialog->iMagic = 0;
	xrtFree(pDialog);
}

XUI_API int xuiOpenFileDialog(xui_context pContext, xui_file_dialog* ppDialog, const xui_file_dialog_desc_t* pDesc)
{
	xui_file_dialog_desc_t tDesc;
	int iRet;
	if ( pDesc != NULL ) {
		tDesc = *pDesc;
	} else {
		memset(&tDesc, 0, sizeof(tDesc));
	}
	tDesc.iSize = sizeof(tDesc);
	tDesc.iMode = XUI_FILE_DIALOG_MODE_OPEN_FILE;
	iRet = xuiFileDialogCreate(pContext, ppDialog, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	return xuiFileDialogSetOpen(*ppDialog, 1);
}

XUI_API int xuiSaveFileDialog(xui_context pContext, xui_file_dialog* ppDialog, const xui_file_dialog_desc_t* pDesc)
{
	xui_file_dialog_desc_t tDesc;
	int iRet;
	if ( pDesc != NULL ) tDesc = *pDesc; else memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.iMode = XUI_FILE_DIALOG_MODE_SAVE_FILE;
	iRet = xuiFileDialogCreate(pContext, ppDialog, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	return xuiFileDialogSetOpen(*ppDialog, 1);
}

XUI_API int xuiSelectFolderDialog(xui_context pContext, xui_file_dialog* ppDialog, const xui_file_dialog_desc_t* pDesc)
{
	xui_file_dialog_desc_t tDesc;
	int iRet;
	if ( pDesc != NULL ) tDesc = *pDesc; else memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.iMode = XUI_FILE_DIALOG_MODE_SELECT_FOLDER;
	iRet = xuiFileDialogCreate(pContext, ppDialog, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	return xuiFileDialogSetOpen(*ppDialog, 1);
}

XUI_API int xuiFileDialogSetOpen(xui_file_dialog pDialog, int bOpen)
{
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pDialog->bOpen = bOpen ? 1 : 0;
	if ( !pDialog->bOpen && pDialog->pOverwriteBox != NULL ) {
		(void)xuiMsgBoxSetOpen(pDialog->pOverwriteBox, 0);
	}
	iRet = xuiWindowSetOpen(pDialog->pWindow, pDialog->bOpen);
	if ( iRet != XUI_OK ) return iRet;
	if ( pDialog->bOpen && pDialog->iMode == XUI_FILE_DIALOG_MODE_SAVE_FILE && pDialog->pNameInput != NULL ) {
		(void)xuiSetFocusWidget(pDialog->pContext, pDialog->pNameInput);
		(void)xuiInputSelectAll(pDialog->pNameInput);
	}
	return XUI_OK;
}

XUI_API int xuiFileDialogIsOpen(xui_file_dialog pDialog)
{
	return __xuiFileDialogValid(pDialog) ? pDialog->bOpen : 0;
}

XUI_API int xuiFileDialogSetResult(xui_file_dialog pDialog, xui_file_dialog_result_proc onResult, void* pUser)
{
	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pDialog->onResult = onResult;
	pDialog->pResultUser = pUser;
	return XUI_OK;
}

XUI_API int xuiFileDialogSetFilter(xui_file_dialog pDialog, const char* sFilter)
{
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiFileDialogSetFilters(pDialog, sFilter);
	if ( iRet != XUI_OK ) return iRet;
	if ( pDialog->pFilterCombo != NULL ) {
		(void)xuiComboBoxSetItems(pDialog->pFilterCombo, (const char**)pDialog->arrFilterNames, pDialog->iFilterCount);
		(void)xuiComboBoxSetSelected(pDialog->pFilterCombo, pDialog->iSelectedFilter);
	}
	return xuiFileDialogRefresh(pDialog);
}

XUI_API const char* xuiFileDialogGetFilter(xui_file_dialog pDialog)
{
	return __xuiFileDialogValid(pDialog) && pDialog->sFilter != NULL ? pDialog->sFilter : "";
}

XUI_API int xuiFileDialogSetDirectory(xui_file_dialog pDialog, const char* sDir)
{
	const char* sUseDir;
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( sDir == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sUseDir = sDir;
	iRet = __xuiFileDialogSetCurrentDir(pDialog, sUseDir);
	if ( iRet != XUI_OK ) return iRet;
	return xuiFileDialogRefresh(pDialog);
}

XUI_API const char* xuiFileDialogGetDirectory(xui_file_dialog pDialog)
{
	return __xuiFileDialogValid(pDialog) && pDialog->sCurrentDir != NULL ? pDialog->sCurrentDir : "";
}

XUI_API int xuiFileDialogGoUp(xui_file_dialog pDialog)
{
	char* sParent;
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) || pDialog->sCurrentDir == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pDialog->sCurrentDir[0] == 0 ) {
		return XUI_OK;
	}
	sParent = __xuiFileDialogParentDir(pDialog->sCurrentDir);
	if ( sParent == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = xuiFileDialogSetDirectory(pDialog, sParent);
	xrtFree(sParent);
	return iRet;
}

XUI_API int xuiFileDialogRefresh(xui_file_dialog pDialog)
{
	int iRet;
	int iRootCount;

	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFileDialogFreeRootItems(pDialog);
	(void)xrtDirScan((str)"", FALSE, __xuiFileDialogRootProc, pDialog);
	(void)xuiListViewSetItems(pDialog->pRootList, (const char**)pDialog->arrRootItems, pDialog->iRootCount);
	iRootCount = pDialog->iRootCount;
	__xuiFileDialogClearEntries(pDialog);
	pDialog->iLastSelectEntry = -1;
	pDialog->fLastSelectTime = 0.0;
	if ( pDialog->sCurrentDir == NULL || pDialog->sCurrentDir[0] == 0 ) {
		for ( iRet = 0; iRet < iRootCount; iRet++ ) {
			(void)__xuiFileDialogAddEntry(pDialog, pDialog->arrRootItems[iRet], pDialog->arrRootItems[iRet], 1);
		}
	} else {
		(void)xrtDirScanEx((str)pDialog->sCurrentDir, FALSE, __xuiFileDialogScanProc, pDialog);
	}
	(void)xuiListViewSetItems(pDialog->pFileList, (const char**)pDialog->arrEntryItems, pDialog->iEntryCount);
	(void)xuiListViewSetSelected(pDialog->pFileList, -1);
	return XUI_OK;
}

XUI_API int xuiFileDialogSelectIndex(xui_file_dialog pDialog, int iIndex)
{
	if ( !__xuiFileDialogValid(pDialog) || iIndex < 0 || iIndex >= pDialog->iEntryCount ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pDialog->iSelectedEntry = iIndex;
	(void)xuiListViewSetSelected(pDialog->pFileList, iIndex);
	(void)__xuiFileDialogSetNameText(pDialog, pDialog->arrEntries[iIndex].sName);
	return XUI_OK;
}

XUI_API int xuiFileDialogCommit(xui_file_dialog pDialog)
{
	const char* sName;
	char* sPath;
	xui_file_dialog_entry_t* pEntry;
	int iRet;

	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pDialog->iSelectedEntry >= 0 && pDialog->iSelectedEntry < pDialog->iEntryCount ) {
		pEntry = &pDialog->arrEntries[pDialog->iSelectedEntry];
		if ( pEntry->bDir && pDialog->iMode != XUI_FILE_DIALOG_MODE_SELECT_FOLDER ) {
			return xuiFileDialogSetDirectory(pDialog, pEntry->sPath);
		}
		if ( pDialog->iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER ) {
			iRet = __xuiFileDialogSetString(&pDialog->sResultPath, pEntry->bDir ? pEntry->sPath : pDialog->sCurrentDir);
		} else if ( pDialog->iMode == XUI_FILE_DIALOG_MODE_SAVE_FILE ) {
			return __xuiFileDialogCommitSavePath(pDialog, pEntry->sPath);
		} else {
			iRet = __xuiFileDialogSetString(&pDialog->sResultPath, pEntry->sPath);
		}
	} else if ( pDialog->iMode == XUI_FILE_DIALOG_MODE_SELECT_FOLDER ) {
		if ( pDialog->sCurrentDir == NULL || pDialog->sCurrentDir[0] == 0 ) {
			return __xuiFileDialogShowFolderError(pDialog);
		}
		iRet = __xuiFileDialogSetString(&pDialog->sResultPath, pDialog->sCurrentDir != NULL ? pDialog->sCurrentDir : "");
	} else {
		sName = xuiInputGetText(pDialog->pNameInput);
		if ( pDialog->iMode == XUI_FILE_DIALOG_MODE_SAVE_FILE ) {
			sPath = NULL;
			iRet = __xuiFileDialogBuildSavePath(pDialog, sName, &sPath);
			if ( iRet != XUI_OK ) return iRet;
			iRet = __xuiFileDialogCommitSavePath(pDialog, sPath);
			xrtFree(sPath);
			return iRet;
		}
		iRet = __xuiFileDialogBuildResultPath(pDialog, sName);
	}
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	(void)xuiFileDialogSetOpen(pDialog, 0);
	return __xuiFileDialogNotify(pDialog, XUI_FILE_DIALOG_RESULT_OK);
}

XUI_API int xuiFileDialogCancel(xui_file_dialog pDialog)
{
	if ( !__xuiFileDialogValid(pDialog) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)xuiFileDialogSetOpen(pDialog, 0);
	return __xuiFileDialogNotify(pDialog, XUI_FILE_DIALOG_RESULT_CANCEL);
}

XUI_API int xuiFileDialogGetResult(xui_file_dialog pDialog)
{
	return __xuiFileDialogValid(pDialog) ? pDialog->iResult : XUI_FILE_DIALOG_RESULT_NONE;
}

XUI_API const char* xuiFileDialogGetResultPath(xui_file_dialog pDialog)
{
	return __xuiFileDialogValid(pDialog) && pDialog->sResultPath != NULL ? pDialog->sResultPath : "";
}

XUI_API xui_widget xuiFileDialogGetWindowWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pWindow : NULL; }
XUI_API xui_widget xuiFileDialogGetRootListWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pRootList : NULL; }
XUI_API xui_widget xuiFileDialogGetFileListWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pFileList : NULL; }
XUI_API xui_widget xuiFileDialogGetPathBreadcrumbWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pPathBreadcrumb : NULL; }
XUI_API xui_widget xuiFileDialogGetPathInputWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pPathInput : NULL; }
XUI_API xui_widget xuiFileDialogGetNameInputWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pNameInput : NULL; }
XUI_API xui_widget xuiFileDialogGetFilterComboWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pFilterCombo : NULL; }
XUI_API xui_widget xuiFileDialogGetUpButtonWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pUpButton : NULL; }
XUI_API xui_widget xuiFileDialogGetRefreshButtonWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pRefreshButton : NULL; }
XUI_API xui_widget xuiFileDialogGetOkButtonWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pOkButton : NULL; }
XUI_API xui_widget xuiFileDialogGetCancelButtonWidget(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pCancelButton : NULL; }
XUI_API xui_msgbox xuiFileDialogGetOverwriteMsgBox(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->pOverwriteBox : NULL; }
XUI_API int xuiFileDialogGetFilterCount(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->iFilterCount : 0; }

XUI_API const char* xuiFileDialogGetFilterName(xui_file_dialog pDialog, int iIndex)
{
	if ( !__xuiFileDialogValid(pDialog) || iIndex < 0 || iIndex >= pDialog->iFilterCount ) return "";
	return pDialog->arrFilterNames[iIndex] != NULL ? pDialog->arrFilterNames[iIndex] : "";
}

XUI_API const char* xuiFileDialogGetFilterPattern(xui_file_dialog pDialog, int iIndex)
{
	if ( !__xuiFileDialogValid(pDialog) || iIndex < 0 || iIndex >= pDialog->iFilterCount ) return "";
	return pDialog->arrFilterPatterns[iIndex] != NULL ? pDialog->arrFilterPatterns[iIndex] : "";
}

XUI_API int xuiFileDialogGetEntryCount(xui_file_dialog pDialog) { return __xuiFileDialogValid(pDialog) ? pDialog->iEntryCount : 0; }

XUI_API const char* xuiFileDialogGetEntryName(xui_file_dialog pDialog, int iIndex)
{
	if ( !__xuiFileDialogValid(pDialog) || iIndex < 0 || iIndex >= pDialog->iEntryCount ) return "";
	return pDialog->arrEntries[iIndex].sName;
}

XUI_API const char* xuiFileDialogGetEntryPath(xui_file_dialog pDialog, int iIndex)
{
	if ( !__xuiFileDialogValid(pDialog) || iIndex < 0 || iIndex >= pDialog->iEntryCount ) return "";
	return pDialog->arrEntries[iIndex].sPath;
}

XUI_API int xuiFileDialogEntryIsDir(xui_file_dialog pDialog, int iIndex)
{
	if ( !__xuiFileDialogValid(pDialog) || iIndex < 0 || iIndex >= pDialog->iEntryCount ) return 0;
	return pDialog->arrEntries[iIndex].bDir;
}
