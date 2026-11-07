#include "services/preferences.h"
#include "services/editor_font.h"
#include "services/ui_font.h"
#include "ui/catalog_view.h"
#include "ui/task_view.h"
#include "ui/reading_state.h"
#include "ui/language_view.h"
#include "ui/completion_view.h"
#include "platform/path.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define TRY(call) do {int r=(call);if(r!=XUI_OK)return r;}while(0)
int xidePreferencesStartDocument(xide_app* a)
{
    xide_model_error e={0};a->documentFontSize=a->preferences.documentFontSize ? a->preferences.documentFontSize : a->uiFontSize;
    a->documentFontPath=xidePathResolve(NULL,a->preferences.documentFont ? a->preferences.documentFont : a->uiFontPath);
    if(xideFontLoad(a,a->documentFontPath,a->documentFontSize,&a->documentFont,&e))return XUI_OK;
    free(a->documentFontPath);a->documentFontPath=xideUiFontDefaultPath();a->documentFontSize=16;
    return xideFontLoad(a,a->documentFontPath,16,&a->documentFont,&e) ? XUI_OK : XUI_ERROR;
}
int xidePreferencesEditor(xide_app* a,xui_widget editor,const xide_settings_options* o)
{
    (void)a;if(!editor)return XUI_OK;
    TRY(xuiCodeEditSetTabColumns(editor,o->tabColumns));TRY(xuiCodeEditSetIndentColumns(editor,o->indentColumns));
    TRY(xuiCodeEditSetExpandTabs(editor,o->expandTabs));TRY(xuiCodeEditSetWordWrap(editor,o->wordWrap));
    int lines=0,markers=0,fold=0,diagnostics=0;
    TRY(xuiCodeEditGetDefaultMargins(editor,&lines,&markers,&fold,&diagnostics));
    TRY(xuiCodeEditSetDefaultMargins(editor,o->lineNumbers,markers,o->foldMargin,diagnostics));
    int automatic=0,minimum=0,maximum=0;float delay=0;
    TRY(xuiCodeEditGetCompletionOptions(editor,&automatic,&minimum,&maximum,&delay));
    TRY(xuiCodeEditSetCompletionOptions(editor,o->completion,minimum,maximum,delay));
    if(!o->completion)TRY(xuiCodeEditCancelCompletion(editor));
    return xuiCodeEditSetMinimap(editor,o->minimap,96.0f);
}
static int editors(xide_app* a,const xide_settings_options* o)
{
    for(size_t i=0;i<a->documents.count;++i)TRY(xidePreferencesEditor(a,a->documents.items[i]->editor,o));
    for(int i=0;i<2;++i)TRY(xidePreferencesEditor(a,a->editors[i],o));
    return XUI_OK;
}
static int document(xide_app* a,xui_font font,int size)
{a->documentFont=font;a->documentFontSize=size;return xideCatalogViewDocumentFont(a);}
int xidePreferencesApply(xide_app* a,const xide_settings_draft* d,xide_model_error* e)
{
    if(e)memset(e,0,sizeof(*e));
    if(!a || !a->context || !d)return 0;
    const char* input[]={d->editorFont,d->uiFont,d->documentFont};
    const char* oldPaths[]={a->codeFontPath,a->uiFontPath,a->documentFontPath};
    int sizes[]={d->editorSize,d->uiSize,d->documentSize},oldSizes[]={a->codeFontSize,a->uiFontSize,a->documentFontSize};
    xui_font old[]={a->codeFont,a->uiFont,a->documentFont},fonts[]={a->codeFont,a->uiFont,a->documentFont};
    char* paths[3]={0};int changed[3]={0};char* compiler=NULL;xvalue* reading=NULL;int success=0;
    /* Prepare replacements without releasing any font still used by widgets. */
    for(int i=0;i<3;++i) {
        paths[i]=xidePathResolve(NULL,input[i]);if(!paths[i])goto cleanup;
        changed[i]=!oldPaths[i] || !xidePathEqual(paths[i],oldPaths[i]) || sizes[i]!=oldSizes[i];
        if(changed[i] && !xideFontLoad(a,paths[i],sizes[i],&fonts[i],e))goto cleanup;
    }
    if(d->compiler && *d->compiler){compiler=xidePathResolve(NULL,d->compiler);if(!compiler)goto cleanup;}
    reading=xideReadingCapture(a,NULL);if(!reading)goto cleanup;
    xide_settings_options oldOptions=*xideSettingsOptions(&a->preferences);int oldReady=a->preferences.optionsReady,oldTheme=a->theme;
    int languageChanged=oldOptions.automaticDiagnostics!=d->options.automaticDiagnostics || oldOptions.diagnosticDelay!=d->options.diagnosticDelay;
    const char* activeCompiler=xideTaskViewCompiler(a);
    if((compiler || activeCompiler) && (!compiler || !activeCompiler || !xidePathEqual(compiler,activeCompiler)))languageChanged=1;
    a->preferences.options=d->options;a->preferences.optionsReady=1;
    int result=xideUiFontInstall(a,old[1],fonts[1],sizes[1]);
    if(result==XUI_OK)result=xideEditorFontInstall(a,fonts[0]);
    if(result==XUI_OK)result=document(a,fonts[2],sizes[2]);
    if(result==XUI_OK)result=editors(a,&d->options);
    if(result==XUI_OK)result=xideWorkbenchApplyTheme(a,d->options.theme);
    /* The disk write is the commit point. Keep old resources for rollback. */
    if(result!=XUI_OK || !xideSettingsSaveDraft(&a->preferences,d,e)) {
        a->preferences.options=oldOptions;a->preferences.optionsReady=oldReady;
        int r=xideUiFontInstall(a,fonts[1],old[1],oldSizes[1]);
        if(r==XUI_OK)r=xideEditorFontInstall(a,old[0]);
        if(r==XUI_OK)r=document(a,old[2],oldSizes[2]);
        if(r==XUI_OK)r=editors(a,&oldOptions);
        if(r==XUI_OK)r=xideWorkbenchApplyTheme(a,oldTheme);
        if(r!=XUI_OK)a->frameError=r;
        if(result!=XUI_OK && e)snprintf(e->message,sizeof(e->message),"设置无法应用到工作台；已恢复原设置。");
        goto cleanup;
    }
    free(a->codeFontPath);a->codeFontPath=paths[0];paths[0]=NULL;a->codeFontSize=sizes[0];
    free(a->uiFontPath);a->uiFontPath=paths[1];paths[1]=NULL;
    free(a->documentFontPath);a->documentFontPath=paths[2];paths[2]=NULL;
    xideTaskViewAdoptCompiler(a,compiler);compiler=NULL;
    a->disableAutomaticDiagnostics=!d->options.automaticDiagnostics;
    if(languageChanged)xideLanguageViewSettingsChanged(a);
    xideCompletionViewStop(a);
    for(int i=0;i<3;++i)if(changed[i] && old[i] && old[i]!=a->uiFont && old[i]!=a->codeFont && old[i]!=a->documentFont){int seen=0;for(int j=0;j<i;++j)if(old[j]==old[i])seen=1;if(!seen)a->proxy.fontDestroy(&a->proxy,old[i]);}
    success=1;
cleanup:
    if(reading){xideReadingRestore(a,reading,1);xrtValueRelease(reading);}
    for(int i=0;i<3;++i){free(paths[i]);if(!success && changed[i] && fonts[i] && fonts[i]!=old[i] && !a->frameError)a->proxy.fontDestroy(&a->proxy,fonts[i]);}
    free(compiler);
    if(!success && e && !e->message[0]){e->code=XIDE_MODEL_MEMORY;snprintf(e->message,sizeof(e->message),"无法准备设置；未保存任何修改。");}
    xgeRenderRequest();return success;
}
