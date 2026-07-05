#include "views/xide_views.h"
#include "lang/xide_xlang_language.h"

#include <string.h>

static int xideEditorRebuildFolds(xide_app_t* pApp)
{
	xui_code_language_t lang;
	xui_code_language_registry registry;
	xui_code_document document;
	xui_code_fold_state folds;
	int ret;
	if ( pApp == NULL || pApp->pEditor == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	registry = xuiCodeEditGetLanguageRegistry(pApp->pEditor);
	document = xuiCodeEditGetDocument(pApp->pEditor);
	folds = xuiCodeEditGetFoldState(pApp->pEditor);
	if ( registry == NULL || document == NULL || folds == NULL ) return XUI_OK;
	memset(&lang, 0, sizeof(lang));
	ret = xuiCodeLanguageRegistryFind(registry, "xlang", &lang);
	if ( ret != XUI_OK || lang.onFold == NULL ) return XUI_OK;
	return xuiCodeFoldStateBuildFromProvider(folds, document, lang.onFold, lang.pUser);
}

static void xideEditorDocumentChanged(xui_code_document_t* pDocument, xui_code_range_t tRange, uint32_t iVersion, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	(void)pDocument;
	(void)tRange;
	(void)iVersion;
	if ( pApp == NULL || pApp->pEditor == NULL ) return;
	(void)xideEditorRebuildFolds(pApp);
	(void)xuiWidgetInvalidate(pApp->pEditor, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

int xideCreateEditorView(xide_app_t* pApp, xui_widget* ppView)
{
	xui_code_edit_desc_t desc;
	const char* text =
		"#doc\n"
		"desc: IDE skeleton entry\n"
		"#end\n"
		"func main(...) as int\n"
		"    print(\"hello xlang IDE\")\n"
		"    return 0\n"
		"end\n";
	int ret;
	if ( pApp == NULL || ppView == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.pFont = pApp->pCodeFont != NULL ? pApp->pCodeFont : pApp->pUiFont;
	desc.sText = text;
	desc.bShowLineNumbers = 1;
	desc.bShowFoldMargin = 1;
	desc.bShowMarkerMargin = 1;
	desc.bShowDiagnosticMargin = 1;
	desc.iTabColumns = 4;
	desc.iIndentColumns = 4;
	ret = xuiCodeEditCreate(pApp->pContext, &pApp->pEditor, &desc);
	if ( ret != XUI_OK ) return ret;
	ret = xideXlangLanguageRegister(pApp->pEditor);
	if ( ret == XUI_OK ) ret = xuiCodeEditSetLanguage(pApp->pEditor, "xlang");
	if ( ret != XUI_OK ) return ret;
	(void)xuiCodeDocumentSetChangeCallback(xuiCodeEditGetDocument(pApp->pEditor), xideEditorDocumentChanged, pApp);
	ret = xideEditorRebuildFolds(pApp);
	if ( ret != XUI_OK ) return ret;
	if ( pApp->pFindScope != NULL ) (void)xuiCodeFindScopeAddEditorNamed(pApp->pFindScope, pApp->pEditor, "main.xl");
	*ppView = pApp->pEditor;
	return XUI_OK;
}
