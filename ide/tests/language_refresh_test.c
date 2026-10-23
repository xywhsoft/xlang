#include "lang/languages.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t checks,bodyReads,arrayCalls,failAt;
static const char* readBody(xui_code_document document)
{++bodyReads;return xuiCodeDocumentGetText(document);}
static int rejectArray(void) {return ++arrayCalls==failAt;}
static bool arrayInit(xarray* array,size_t item)
{return !rejectArray() && xrtArrayInit(array,item);}
static bool arrayReserve(xarray* array,size_t count)
{return !rejectArray() && xrtArrayReserve(array,count);}
static bool arrayAppend(xarray* array,const void* items,size_t count)
{return !rejectArray() && xrtArrayAppend(array,items,count);}
/* Exercise the real refresh and count whole-body acquisition. Faults affect
 * only its XRT arrays, not the SDK context or our comparison oracle. */
#define xuiCodeDocumentGetText readBody
#define xrtArrayInit arrayInit
#define xrtArrayReserve arrayReserve
#define xrtArrayAppend arrayAppend
#include "../src/lang/languages.c"
#undef xuiCodeDocumentGetText
#undef xrtArrayInit
#undef xrtArrayReserve
#undef xrtArrayAppend
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"language_refresh:%d %s\n",__LINE__,#e);return 0;}}while(0)

static int equivalent(xui_widget editor)
{
    xui_code_document document=xuiCodeEditGetDocument(editor);xui_code_language_t language={0};
    CHECK(xuiCodeLanguageRegistryFind(xuiCodeEditGetLanguageRegistry(editor),xuiCodeEditGetLanguage(editor),&language)==XUI_OK);
    int count=0,actualCount=0;uint32_t version=0;int begin=-1,end=-1;
    CHECK(xuiCodeLanguageLex(&language,document,0,-1,NULL,0,&count,NULL,0)==XUI_OK && count>=0);
    xui_code_token_t *expected=calloc((size_t)count+1,sizeof(*expected)),*actual=calloc((size_t)count+1,sizeof(*actual));
    if(!expected || !actual) {free(expected);free(actual);return 0;}
    int okay=xuiCodeLanguageLex(&language,document,0,-1,expected,count,&actualCount,NULL,0)==XUI_OK && count==actualCount &&
        xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(document),actual,count,&actualCount)==XUI_OK && count==actualCount;
    for(int i=0;okay && i<count;++i) okay=expected[i].iStartOffset==actual[i].iStartOffset && expected[i].iEndOffset==actual[i].iEndOffset &&
        expected[i].iKind==actual[i].iKind && expected[i].iStyle==actual[i].iStyle && expected[i].iFlags==actual[i].iFlags;
    free(expected);free(actual);CHECK(okay);
    CHECK(xuiCodeTokenBufferGetRange(xuiCodeEditGetTokenBuffer(editor),&version,&begin,&end)==XUI_OK &&
          version==xuiCodeDocumentGetVersion(document) && begin==0 && end==xuiCodeDocumentGetLength(document));
    xui_code_fold_state fresh=NULL;CHECK(xuiCodeFoldStateCreate(&fresh)==XUI_OK);
    okay=!language.onFold || xuiCodeFoldStateBuildFromProvider(fresh,document,language.onFold,language.pUser)==XUI_OK;
    int folds=xuiCodeFoldStateGetCount(fresh),installed=xuiCodeFoldStateGetCount(xuiCodeEditGetFoldState(editor));
    xui_code_fold_range_t *left=calloc((size_t)folds+1,sizeof(*left)),*right=calloc((size_t)installed+1,sizeof(*right));
    if(!left || !right) {free(left);free(right);xuiCodeFoldStateDestroy(fresh);return 0;}
    okay=okay && folds==installed && xuiCodeFoldStateGetRanges(fresh,left,folds,&folds)==XUI_OK &&
         xuiCodeFoldStateGetRanges(xuiCodeEditGetFoldState(editor),right,installed,&installed)==XUI_OK;
    for(int i=0;okay && i<folds;++i) okay=left[i].iStartLine==right[i].iStartLine && left[i].iEndLine==right[i].iEndLine &&
        left[i].iLevel==right[i].iLevel && (left[i].iFlags&~XUI_CODE_FOLD_COLLAPSED)==(right[i].iFlags&~XUI_CODE_FOLD_COLLAPSED);
    free(left);free(right);xuiCodeFoldStateDestroy(fresh);CHECK(okay);return 1;
}
static int refreshCases(xui_widget editor)
{
    const char* cases[]={""," \t\r\n","func main(...) as int\n return 1\nend\n",
        "#doc\r\nfunc fake()\r\n#end\r\nasync func main(...) as int\r\n if true then\r\n return 2\r\n end\r\nend\r\n",
        "/* outer\r /* nested */\r end */\rfunc main()\r string 中文 = rf\"{f\"{1+2}\"}\"\r return #2026-10-23#\rend\r",
        "export class Item\n func value() as string\n return \"\"\"line\nend class fake\nlast\"\"\"\n end\nend\n",
        "func incomplete()\n string text = \"unfinished\n if true then\n", "/* unfinished\n func fake()\nend",
        "func main()\n var 中 = `end /* text */`\n return 0b101 + 3.5e-2\nend"};
    CHECK(xuiCodeEditSetLanguage(editor,"xlang")==XUI_OK);
    for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i) {
        CHECK(xuiCodeEditSetText(editor,cases[i])==XUI_OK);bodyReads=0;
        CHECK(xideRefreshEditorSyntax(editor)==XUI_OK && bodyReads==2 && equivalent(editor));
    }
    xstrbuf large;xrtStrBufInit(&large);
    for(int i=0;i<32768;++i) if(!xrtStrBufAppend(&large,xrtStrView("// 0123456789abcdef0123456789abcde\n"))) {xrtStrBufFree(&large);return 0;}
    int okay=xrtStrBufAppend(&large,xrtStrView("func main(...) as int\n return 0\nend\n")) && xuiCodeEditSetText(editor,large.Data)==XUI_OK;
    bodyReads=0;okay=okay && xideRefreshEditorSyntax(editor)==XUI_OK && bodyReads==2 && equivalent(editor);xrtStrBufFree(&large);CHECK(okay);
    return 1;
}
static int collapseAndSwitch(xui_widget editor)
{
    const char* text="func main(...) as int\n return 1\nend\n";CHECK(xuiCodeEditSetText(editor,text)==XUI_OK && xideConfigureEditorLanguage(editor,"xlang")==XUI_OK);
    xui_code_document document=xuiCodeEditGetDocument(editor);xui_code_fold_state state=xuiCodeEditGetFoldState(editor);
    CHECK(xuiCodeFoldStateToggleLine(state,0)==XUI_OK);
    int at=(int)(strstr(text,"1")-text);CHECK(xuiCodeDocumentReplace(document,at,at+1,"2")==XUI_OK && xideRefreshEditorSyntax(editor)==XUI_OK && equivalent(editor));
    xui_code_fold_range_t range={0};int count=0;CHECK(xuiCodeFoldStateGetRanges(state,&range,1,&count)==XUI_OK && count==1 && (range.iFlags&XUI_CODE_FOLD_COLLAPSED));
    CHECK(xuiCodeDocumentInsert(document,0,"// moved\n")==XUI_OK && xideRefreshEditorSyntax(editor)==XUI_OK && equivalent(editor));
    CHECK(xuiCodeFoldStateGetRanges(state,&range,1,&count)==XUI_OK && !(range.iFlags&XUI_CODE_FOLD_COLLAPSED));
    CHECK(xuiCodeEditSetText(editor,"int main(void) {\n return 0;\n}\n")==XUI_OK && xideConfigureEditorLanguage(editor,"xlang")==XUI_OK);
    CHECK(xuiCodeEditSetLanguage(editor,"c")==XUI_OK && xideRebuildEditorFolds(editor)==XUI_OK && xuiCodeFoldStateGetCount(state)==1);
    CHECK(xideRefreshEditorSyntax(editor)==XUI_OK && equivalent(editor));
    CHECK(xideConfigureEditorLanguage(editor,"text")==XUI_OK && equivalent(editor));
    CHECK(xideConfigureEditorLanguage(editor,"xlang")==XUI_OK && equivalent(editor));return 1;
}
static int allocationFailures(xui_widget editor)
{
    const char* text="func main(...) as int\n /* block\n comment */\n if true then\n return 1\n end\nend\n";
    CHECK(xuiCodeEditSetText(editor,text)==XUI_OK && xideConfigureEditorLanguage(editor,"xlang")==XUI_OK);
    arrayCalls=0;CHECK(xideRefreshEditorSyntax(editor)==XUI_OK);size_t total=arrayCalls;CHECK(total>10);
    xui_code_document document=xuiCodeEditGetDocument(editor);int at=(int)(strstr(text,"1")-text);
    for(size_t i=1;i<=total;++i) {
        CHECK(xuiCodeDocumentReplace(document,at,at+1,(i&1) ? "2" : "1")==XUI_OK);arrayCalls=0;failAt=i;
        int result=xideRefreshEditorSyntax(editor);failAt=0;CHECK(result==XUI_ERROR_OUT_OF_MEMORY);
        uint32_t cached=0;int start=0,end=0;
        if(xuiCodeTokenBufferGetRange(xuiCodeEditGetTokenBuffer(editor),&cached,&start,&end)==XUI_OK && cached==xuiCodeDocumentGetVersion(document))
            CHECK(start==0 && end==xuiCodeDocumentGetLength(document) && xuiCodeFoldStateGetCount(xuiCodeEditGetFoldState(editor))==0);
        CHECK(xideRefreshEditorSyntax(editor)==XUI_OK && equivalent(editor));
    }
    return 1;
}
static int customLex(xui_code_document document,int start,int end,xui_code_token_t* tokens,int capacity,int* count,void* user)
{
    (void)start;(void)end;(void)user;int length=xuiCodeDocumentGetLength(document);*count=length ? 1 : 0;
    if(length && tokens && capacity>0) tokens[0]=(xui_code_token_t){0,length,XUI_CODE_TOKEN_TEXT,XUI_CODE_TOKEN_TEXT,0};
    return XUI_OK;
}
static int customFold(xui_code_document document,xui_code_fold_range_t* ranges,int capacity,int* count,void* user)
{
    (void)user;*count=xuiCodeDocumentGetLineCount(document)>1 ? 1 : 0;
    if(*count && ranges && capacity>0) ranges[0]=(xui_code_fold_range_t){sizeof(*ranges),0,1,0,XUI_CODE_FOLD_CUSTOM|XUI_CODE_FOLD_HEADER};
    return XUI_OK;
}
static int customProvider(xui_widget editor)
{
    xui_code_language_t language={0};language.iSize=sizeof(language);language.sId="custom-refresh";language.sName="Custom refresh";
    language.iLexerType=XUI_CODE_LEXER_CALLBACK;language.onLex=customLex;language.onFold=customFold;
    CHECK(xuiCodeLanguageRegistryRegister(xuiCodeEditGetLanguageRegistry(editor),&language)==XUI_OK);
    CHECK(xuiCodeEditSetText(editor,"custom\nprovider\n")==XUI_OK && xideConfigureEditorLanguage(editor,"custom-refresh")==XUI_OK && equivalent(editor));return 1;
}
int main(void)
{
    xui_context context=NULL;xui_widget editor=NULL;int okay=xuiCreate(&context)==XUI_OK && xuiCodeEditCreate(context,&editor,NULL)==XUI_OK;
    if(okay) okay=xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(editor))==XUI_OK && refreshCases(editor) && collapseAndSwitch(editor) && allocationFailures(editor) && customProvider(editor);
    if(context) xuiDestroy(context);
    if(okay) printf("language_refresh_test passed: %zu checks; single lexical pass/body access bound, full-cache/provider equivalence, empty/Unicode/CRLF/CR/multiline/incomplete/1MiB, collapse preservation and moved expansion, raw language switch, custom providers, all XRT array-operation failures and recovery.\n",checks);
    return okay ? 0 : 1;
}
