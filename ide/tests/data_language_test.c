#include "lang/languages.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"data_language_test:%d: %s\n",__LINE__,#e);failed=1;goto cleanup;}}while(0)
static int kind(const char* text,const xui_code_token_t* tokens,int count,const char* needle)
{const char* p=strstr(text,needle);if(!p)return -1;int offset=(int)(p-text);/* CodeDocument normalizes CRLF to LF. */for(const char* s=text;s<p;++s)if(s[0]=='\r' && s[1]=='\n')--offset;for(int i=0;i<count;++i)if(tokens[i].iStartOffset<=offset && tokens[i].iEndOffset>=offset+(int)strlen(needle))return tokens[i].iKind;return -1;}
static int lex(xui_code_language_registry registry,xui_code_document doc,const char* mode,const char* text,int first,int last,xui_code_token_t* tokens,int capacity,int* count)
{xui_code_language_t language={0};int r=xuiCodeDocumentSetText(doc,text);if(r==XUI_OK)r=xuiCodeLanguageRegistryFind(registry,mode,&language);return r==XUI_OK?xuiCodeLanguageLex(&language,doc,first,last,tokens,capacity,count,NULL,0):r;}
int main(void)
{
    int failed=0,count=0,total=0,foldCount=0;xui_code_language_registry registry=NULL;xui_code_document doc=NULL;xui_context context=NULL;xui_widget editor=NULL;xui_code_token_t tokens[512],small[2];xui_code_fold_range_t folds[512];
    const char* modes[]={"json","xlon","jsonl","xlonl"};const char* paths[]={"D:/中文/a.JSON","D:/dir.json/a.XLON","D:\\文档\\a.JsOnL","a.XLONL"};
    const char* json="{\r\n \"键\": [true,false,null,-12,0.5,6e+8,\"UTF-8 中文😀\\u4F60\\n\\\"\"],\r\n \"nested\": {\"braces\":\"{[]}\"}\r\n}";
    const char* xlon="{\n\"b\":bytes(\"AAE=\"),\"t\":time(\"2026-11-08T00:00:00Z\"),\"f\":float(\"nan\"),\n\"s\":set[1,2],\"i\":intmap{-5: true},\"c\":app.id-v2(\"42\")\n}";
    CHECK(xuiCodeLanguageRegistryCreate(&registry)==XUI_OK && xideRegisterLanguages(registry)==XUI_OK && xideRegisterLanguages(registry)==XUI_OK && xuiCodeLanguageRegistryGetCount(registry)==7);
    CHECK(xuiCodeDocumentCreate(&doc)==XUI_OK);
    for(int k=0;k<4;++k){
        xui_code_language_t language={0};CHECK(!strcmp(xideLanguageForPath(paths[k]),modes[k]) && xideLanguageSupported(modes[k]));
        CHECK(xuiCodeLanguageRegistryFind(registry,modes[k],&language)==XUI_OK && language.onLex && language.onFold && language.iExtensionCount==1 && !strcmp(language.arrExtensions[0],modes[k]));
        CHECK(lex(registry,doc,modes[k],json,0,-1,tokens,512,&count)==XUI_OK && count>15);
        if(kind(json,tokens,count,"true")!=XUI_CODE_TOKEN_KEYWORD || kind(json,tokens,count,"null")!=XUI_CODE_TOKEN_KEYWORD){printf("mode %s: %d tokens true=%d null=%d\n",modes[k],count,kind(json,tokens,count,"true"),kind(json,tokens,count,"null"));for(int n=0;n<count;++n)printf("%d..%d kind=%d\n",tokens[n].iStartOffset,tokens[n].iEndOffset,tokens[n].iKind);}
        CHECK(kind(json,tokens,count,"true")==XUI_CODE_TOKEN_KEYWORD && kind(json,tokens,count,"null")==XUI_CODE_TOKEN_KEYWORD);
        CHECK(kind(json,tokens,count,"-12")==XUI_CODE_TOKEN_NUMBER && kind(json,tokens,count,"6e+8")==XUI_CODE_TOKEN_NUMBER && kind(json,tokens,count,"0.5")==XUI_CODE_TOKEN_NUMBER);
        CHECK(kind(json,tokens,count,"中文😀")==XUI_CODE_TOKEN_STRING && kind(json,tokens,count,"{[]}")==XUI_CODE_TOKEN_STRING);
        CHECK(lex(registry,doc,modes[k],json,0,-1,NULL,0,&total)==XUI_OK && total==count);
        CHECK(lex(registry,doc,modes[k],json,0,-1,small,2,&total)==XUI_OK && total==count && !memcmp(small,tokens,sizeof(small)));
        CHECK(lex(registry,doc,modes[k],json,99,100,tokens,512,&count)==XUI_OK && count==0);
        CHECK(xuiCodeLanguageRegistryFind(registry,modes[k],&language)==XUI_OK);
        CHECK(lex(registry,doc,modes[k],json,0,-1,tokens,512,&count)==XUI_OK && xuiCodeLanguageFold(&language,doc,folds,512,&foldCount)==XUI_OK);
        CHECK((k<2 && foldCount==1) || (k>=2 && foldCount==0));
    }
    CHECK(!xideLanguageSupported(NULL) && !xideLanguageSupported("bogus") && !strcmp(xideLanguageForPath("folder.json/noextension"),"text"));
    for(int k=0;k<4;++k){
        CHECK(lex(registry,doc,modes[k],xlon,0,-1,tokens,512,&count)==XUI_OK);
        int tag=(k==1 || k==3)?XUI_CODE_TOKEN_TYPE:XUI_CODE_TOKEN_ERROR;
        CHECK(kind(xlon,tokens,count,"bytes")==tag && kind(xlon,tokens,count,"set")==tag && kind(xlon,tokens,count,"intmap")==tag && kind(xlon,tokens,count,"app.id-v2")==tag);
        CHECK(kind(xlon,tokens,count,"-5")==XUI_CODE_TOKEN_NUMBER);
    }
    const char* invalid[]={"01","+1","1e+","1.","0xFF","1_000","-.5","\"bad\\q\"","\"bad\\u12XY\"","\"bad\tstring\"","\v","\f"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i)for(int k=0;k<4;++k){CHECK(lex(registry,doc,modes[k],invalid[i],0,-1,tokens,512,&count)==XUI_OK && count>=1 && tokens[0].iKind==XUI_CODE_TOKEN_ERROR);}
    const char* comments="/* first\r\n{ fake [\r\nlast */\r\n{\"ok\":true}\r\n";
    CHECK(lex(registry,doc,"xlon",comments,1,1,tokens,512,&count)==XUI_OK && count==1 && tokens[0].iKind==XUI_CODE_TOKEN_COMMENT && tokens[0].iStartOffset==0);
    CHECK(lex(registry,doc,"json",comments,1,1,tokens,512,&count)==XUI_OK && count==1 && tokens[0].iKind==XUI_CODE_TOKEN_ERROR);
    const char* records="\"broken\r\n{\"ok\":true}\r\n/* broken\n{\"last\":false}\n";
    for(int k=2;k<4;++k){CHECK(lex(registry,doc,modes[k],records,1,1,tokens,512,&count)==XUI_OK && tokens[0].iStartOffset==8 && kind(records,tokens,count,"true")==XUI_CODE_TOKEN_KEYWORD);CHECK(lex(registry,doc,modes[k],records,3,3,tokens,512,&count)==XUI_OK && kind(records,tokens,count,"false")==XUI_CODE_TOKEN_KEYWORD);}
    /* Real CodeEdit cache, edits, undo and structural folds use the same rules. */
    CHECK(xuiCreate(&context)==XUI_OK && xuiCodeEditCreate(context,&editor,NULL)==XUI_OK && xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(editor))==XUI_OK);
    for(int k=0;k<4;++k){CHECK(xuiCodeEditSetText(editor,k&1?xlon:json)==XUI_OK && xideConfigureEditorLanguage(editor,modes[k])==XUI_OK);CHECK(xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),tokens,512,&count)==XUI_OK && count>15);}
    CHECK(xuiCodeEditSetText(editor,"{\n\"array\": [\n{\"s\":\"[not a fold]\"}\n]\n}\n")==XUI_OK && xideConfigureEditorLanguage(editor,"json")==XUI_OK && xuiCodeFoldStateGetCount(xuiCodeEditGetFoldState(editor))==2);
    CHECK(xideConfigureEditorLanguage(editor,"jsonl")==XUI_OK && xuiCodeFoldStateGetCount(xuiCodeEditGetFoldState(editor))==0);
cleanup:
    if(context)xuiDestroy(context);
    if(doc)xuiCodeDocumentDestroy(doc);
    if(registry)xuiCodeLanguageRegistryDestroy(registry);
    if(!failed)printf("data_language_test passed: %d checks; 4 modes/extensions, strict strings/numbers, XLON tags/comments, line recovery, Unicode/CRLF, bounded partial tokens, fold and CodeEdit cache\n",checks);
    return failed;
}
