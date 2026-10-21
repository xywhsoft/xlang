#include "editor/documents.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expression) do { if(!(expression)) { fprintf(stderr,"document_test:%d: %s; %s\n",__LINE__,#expression,error.message); failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,strlen(text)}); }
static wchar_t* wide(const char* text)
{
    int length=MultiByteToWideChar(CP_UTF8,0,text,-1,NULL,0); wchar_t* result=malloc((size_t)length*sizeof(*result));
    if(result) MultiByteToWideChar(CP_UTF8,0,text,-1,result,length);
    return result;
}
int main(int argc,char** argv)
{
    int failed=0; xui_context context=NULL; xide_document_store store={0}; xide_model_error error={0};
    char *root=NULL,*source=NULL,*alias=NULL,*otherPath=NULL,*copyPath=NULL,*wrong=NULL,*cache=NULL,*stagePath=NULL,*stageBody=NULL,*stageCopy=NULL; wchar_t *sourceWide=NULL,*aliasWide=NULL; HANDLE locked=INVALID_HANDLE_VALUE;
    xide_task_stage* stage=NULL;
    xide_text_file content={0};
    CHECK(argc==2); root=xidePathResolve(argv[1],"中文 编辑文件"); CHECK(root && xrtDirCreateAll(root));
    source=xidePathResolve(root,"main.xl"); alias=xidePathResolve(root,"MAIN-ALIAS.xl"); otherPath=xidePathResolve(root,"native.c"); copyPath=xidePathResolve(root,"saved.h"); wrong=xidePathResolve(root,"invalid.txt");
    CHECK(source && alias && otherPath && copyPath && wrong);
    const char* original="\xef\xbb\xbf" "func main(...) as int\r\n    // 中文\r\n    return 0\r\nend\r\n";
    CHECK(writeText(source,original) && writeText(otherPath,"int value = 1;\n"));
    CHECK(xuiCreate(&context)==XUI_OK); xideDocumentsInit(&store,context,NULL,NULL,NULL);
    xide_document* document=xideDocumentOpen(&store,source,&error); CHECK(document && !document->dirty && !strcmp(document->language,"xlang"));
    CHECK(document->baseline.bom && document->baseline.eol==XUI_CODE_EOL_CRLF);
    CHECK(!strchr(xuiCodeEditGetText(document->editor),'\r'));
    CHECK(xideDocumentOpen(&store,source,&error)==document && store.count==1);
    sourceWide=wide(source); aliasWide=wide(alias); DeleteFileW(aliasWide); CHECK(CreateHardLinkW(aliasWide,sourceWide,NULL));
    CHECK(xideDocumentOpen(&store,alias,&error)==document && store.count==1);
    xide_document* other=xideDocumentOpen(&store,otherPath,&error); CHECK(other && other!=document && !strcmp(other->language,"c"));
    xui_code_document core=xuiCodeEditGetDocument(document->editor);
    CHECK(xuiCodeFoldStateFoldAll(xuiCodeEditGetFoldState(document->editor))==XUI_OK);
    int end=xuiCodeDocumentGetLength(core); CHECK(xuiCodeDocumentInsert(core,end,"// 新内容 😀\n")==XUI_OK && document->dirty);
    int visible=1; CHECK(xuiCodeFoldStateIsLineVisible(xuiCodeEditGetFoldState(document->editor),1,&visible)==XUI_OK && !visible);
    CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(document->editor),core,2,6)==XUI_OK);
    uint32_t version=xuiCodeDocumentGetVersion(core);
    CHECK(xideDocumentSetLanguage(document,"c",&error) && xideDocumentSetLanguage(document,"text",&error) && xideDocumentSetLanguage(document,"xlang",&error));
    int start=0,finish=0; CHECK(xuiCodeSelectionGetRange(xuiCodeEditGetSelection(document->editor),&start,&finish)==XUI_OK && start==2 && finish==6);
    CHECK(xuiCodeDocumentGetVersion(core)==version && xuiCodeDocumentCanUndo(core));
    CHECK(xuiCodeDocumentUndo(core)==XUI_OK && !document->dirty && xuiCodeDocumentRedo(core)==XUI_OK && document->dirty);
    CHECK(!xideDocumentClose(document,0,&error) && error.code==XIDE_MODEL_CONFLICT && document->open);
    CHECK(!xideDocumentSave(document,otherPath,XIDE_DOCUMENT_OVERWRITE_TARGET,&error) && error.code==XIDE_MODEL_CONFLICT);
    locked=CreateFileW(sourceWide,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); CHECK(locked!=INVALID_HANDLE_VALUE);
    CHECK(!xideDocumentSave(document,NULL,0,&error) && error.code==XIDE_MODEL_IO && document->dirty);
    CHECK(xideTextFileRead(source,&content,&error) && content.rawLength==strlen(original) && !memcmp(content.raw,original,content.rawLength));
    CloseHandle(locked); locked=INVALID_HANDLE_VALUE;
    CHECK(xideDocumentSave(document,NULL,0,&error) && !document->dirty && xuiCodeDocumentCanUndo(core));
    CHECK(xideTextFileRead(source,&content,&error) && content.bom && content.eol==XUI_CODE_EOL_CRLF && strstr((char*)content.raw,"新内容 😀\r\n"));
    CHECK(xuiCodeDocumentUndo(core)==XUI_OK && document->dirty && xuiCodeDocumentRedo(core)==XUI_OK && !document->dirty);
    CHECK(writeText(source,"// 外部编辑\n"));
    CHECK(xideDocumentCheckExternal(document) && !xideDocumentSave(document,NULL,0,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(xuiCodeDocumentInsert(core,0,"// 本地\n")==XUI_OK && document->dirty);
    CHECK(!xideDocumentReload(document,0,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(xideDocumentReload(document,1,&error) && !document->dirty && xuiCodeDocumentCanUndo(core) && !strcmp(xuiCodeEditGetText(document->editor),"// 外部编辑\n"));
    CHECK(xuiCodeDocumentUndo(core)==XUI_OK && document->dirty && strstr(xuiCodeEditGetText(document->editor),"// 本地\n"));
    CHECK(xuiCodeDocumentRedo(core)==XUI_OK && !document->dirty);
    CHECK(!xideDocumentCheckExternal(document));
    /* Worker snapshots require no UI-thread disk read and remain owned on refusal. */
    const char* oldText="// a\n// 中文😀 middle\n// tail\n";
    const char* newText="// longer first line\n// 更新😀 middle\n// tail changed\n";
    CHECK(xideTextFileDecode((const unsigned char*)oldText,strlen(oldText),&content,&error));
    uint64_t documentId=document->id; char* identity=document->sessionId;
    CHECK(xideDocumentAcceptExternal(document,&content,0,&error) && !content.text && !content.raw && document->id==documentId && document->sessionId==identity);
    xui_code_selection_model selections=xuiCodeEditGetSelection(document->editor);
    CHECK(xuiCodeSelectionSetRange(selections,core,20,8)==XUI_OK);
    xui_code_selection_t extra={.iSize=sizeof(extra),.iAnchorOffset=31,.iCaretOffset=27}; CHECK(xuiCodeSelectionAdd(selections,&extra)==XUI_OK);
    CHECK(xideTextFileDecode((const unsigned char*)newText,strlen(newText),&content,&error));
    CHECK(xideDocumentAcceptExternal(document,&content,0,&error) && !document->dirty && xuiCodeSelectionGetCount(selections)==2);
    xui_code_selection_t selected={0}; CHECK(xuiCodeSelectionGetAt(selections,0,&selected)==XUI_OK && selected.iAnchorOffset==36 && selected.iCaretOffset==24);
    CHECK(xuiCodeDocumentUndo(core)==XUI_OK && document->dirty && !strcmp(xuiCodeEditGetText(document->editor),oldText));
    CHECK(xideTextFileDecode((const unsigned char*)"refused\n",8,&content,&error));
    CHECK(!xideDocumentAcceptExternal(document,&content,0,&error) && error.code==XIDE_MODEL_CONFLICT && content.raw && !strcmp(content.text,"refused\n")); xideTextFileClear(&content);
    CHECK(xuiCodeDocumentRedo(core)==XUI_OK && !document->dirty && !strcmp(xuiCodeEditGetText(document->editor),newText));
    CHECK(xideDocumentSetLanguage(document,"c",&error));
    CHECK(xideTextFileDecode((const unsigned char*)oldText,strlen(oldText),&content,&error));
    CHECK(xideDocumentAcceptExternal(document,&content,0,&error) && !strcmp(document->language,"c") && document->languageOverride);
    CHECK(xideDocumentSetLanguage(document,"xlang",&error));
    CHECK(xuiCodeDocumentInsert(core,0,"// 明确覆盖\n")==XUI_OK);
    CHECK(writeText(source,"another external\n"));
    CHECK(xideDocumentSave(document,NULL,XIDE_DOCUMENT_OVERWRITE_EXTERNAL,&error) && !document->dirty);
    CHECK(xideDocumentSave(document,copyPath,XIDE_DOCUMENT_OVERWRITE_TARGET,&error) && !strcmp(document->language,"xlang"));
    CHECK(!strcmp(document->path,copyPath) && xidePathExists(source) && xuiCodeDocumentCanUndo(core));
    xide_document* unnamed=xideDocumentNew(&store,"xlang",&error); CHECK(unnamed && !unnamed->path);
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(unnamed->editor),0,"int saved = 3;\n")==XUI_OK && unnamed->dirty);
    char* newPath=xidePathResolve(root,"automatic.c"); CHECK(newPath);
    int result=xideDocumentSave(unnamed,newPath,XIDE_DOCUMENT_OVERWRITE_TARGET,&error); free(newPath);
    CHECK(result && !strcmp(unnamed->language,"c") && xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(unnamed->editor)));
    CHECK(writeText(wrong,"\xff\xfe")); CHECK(!xideDocumentOpen(&store,wrong,&error) && error.code==XIDE_MODEL_INVALID);
    CHECK(writeText(wrong,"abc\0")); /* Explicit bytes below verify embedded NUL rejection. */
    CHECK(xrtFileWriteAtomic(wrong,(xbytesview){(unsigned char*)"a\0b",3}) && !xideDocumentOpen(&store,wrong,&error));
    CHECK(xideDocumentClose(document,0,&error)); size_t slots=store.count;
    for(int i=0;i<80;++i) { xide_document* temporary=xideDocumentNew(&store,"text",&error); CHECK(temporary && temporary==document); CHECK(xideDocumentClose(temporary,0,&error)); }
    CHECK(store.count==slots);
    for(int i=0;i<96;++i) CHECK(xideDocumentNew(&store,i%2 ? "c" : "xlang",&error));
    CHECK(store.count>=98 && !xideDocumentsDirty(&store));
    cache=xidePathResolveLexical(root,".xlang-ide-build"); stagePath=cache ? xidePathResolveLexical(cache,"01234567-89ab-cdef-0123-456789abcdef") : NULL;
    stageBody=stagePath ? xidePathResolveLexical(stagePath,"request.xlp") : NULL; stageCopy=xidePathResolveLexical(root,"cache-copy.c");
    CHECK(cache && stagePath && stageBody && stageCopy && (stage=xideTaskStageBegin(stagePath,&error)));
    CHECK(writeText(stageBody,"int cached = 3;\n") && xideTaskStageComplete(stage,&error)); xideTaskStageRelease(stage); stage=NULL;
    FILETIME time={0}; GetSystemTimeAsFileTime(&time); uint64_t future=(((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime)+XIDE_TASK_STAGE_GRACE_TICKS+UINT64_C(86400)*10000000; xide_task_stage_stats stats={0};
    xide_document* cached=xideDocumentOpen(&store,stageBody,&error); CHECK(cached && cached->stageReference);
    CHECK(xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xidePathExists(stageBody));
    CHECK(xideDocumentSave(cached,stageCopy,0,&error) && !cached->stageReference && !strcmp(cached->language,"c"));
    CHECK(xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && stats.deletedStages==1 && !xidePathExists(stagePath) && xidePathExists(stageCopy));
    free(stagePath); free(stageBody); stagePath=xidePathResolveLexical(cache,"11234567-89ab-cdef-0123-456789abcdef"); stageBody=stagePath ? xidePathResolveLexical(stagePath,"request.xlp") : NULL;
    CHECK(stagePath && stageBody && (stage=xideTaskStageBegin(stagePath,&error)) && writeText(stageBody,"int recovered = 1;\n") && xideTaskStageComplete(stage,&error)); xideTaskStageRelease(stage); stage=NULL;
    CHECK(xideTextFileRead(stageBody,&content,&error));
    cached=xideDocumentRecover(&store,stageBody,"request.xlp","c",&content,"int recovered = 2;\n",&error); CHECK(cached && cached->stageReference && cached->dirty);
    CHECK(xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xidePathExists(stageBody));
    CHECK(xideDocumentClose(cached,1,&error) && xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && stats.deletedStages==1 && !xidePathExists(stagePath));
    puts("document_test passed: physical-file deduplication, C/xlang/text without undo or caret loss, save-point dirty state, BOM/CRLF, atomic failure, external conflicts, reload, Save As, invalid encodings, dynamic/reused documents and cache-stage leases for open/recovery/Save As/close");
cleanup:
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    xideTextFileClear(&content); xideDocumentsClear(&store); if(context) xuiDestroy(context);
    xideTaskStageRelease(stage); free(cache); free(stagePath); free(stageBody); free(stageCopy);
    free(root); free(source); free(alias); free(otherPath); free(copyPath); free(wrong); free(sourceWide); free(aliasWide); return failed;
}
