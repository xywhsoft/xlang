#include "lang/languages.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expression) do { if (!(expression)) { fprintf(stderr,"language_test:%d: %s\n",__LINE__,#expression); failed=1; goto cleanup; } } while(0)
static int tokenKind(const char* text, const xui_code_token_t* tokens, int count, const char* span)
{
    const char* found = strstr(text, span);
    if (!found) return -1;
    int start = (int)(found-text), end = start+(int)strlen(span);
    for (int i=0; i<count; ++i) if (tokens[i].iStartOffset<=start && tokens[i].iEndOffset>=end) return tokens[i].iKind;
    return -1;
}
static int lex(xui_code_language_registry registry, xui_code_document document, const char* name,
               const char* text, int start, int end, xui_code_token_t* tokens, int capacity, int* count)
{
    xui_code_language_t language={0};
    int result=xuiCodeDocumentSetText(document,text);
    if (result==XUI_OK) result=xuiCodeLanguageRegistryFind(registry,name,&language);
    return result==XUI_OK ? xuiCodeLanguageLex(&language,document,start,end,tokens,capacity,count,NULL,0) : result;
}
int main(void)
{
    xui_context context=NULL; xui_widget editor=NULL;
    xui_code_document document=NULL; xui_code_language_registry registry=NULL;
    xui_code_token_t tokens[512], partial[2]; xui_code_fold_range_t folds[512];
    int failed=0,count=0,required=0,foldCount=0; char* deep=NULL;
    const char* source="#doc\r\n描述: func fake() end\r\n#end\r\n"
        "/* outer /* nested */ still comment */\r\n"
        "async func main(...) as int\r\n"
        "    string 中文 = rf\"result {f\"{1+2}\"}\"\r\n"
        "    bytes data = br\"raw\\path\"\r\n"
        "    string multi = \"\"\"first\r\nend class fake\r\nlast\"\"\"\r\n"
        "    string literal = `end /* ignored */`\r\n"
        "    var when = #2026-10-06#\r\n"
        "    int n = 0xFF + 0b10 + 0o77 + 1_000 + 3.5e-2\r\n"
        "    if n > 0 then\r\n        return n\r\n    end\r\nend\r\n"
        "protocol Runnable\r\nend\r\n";
    const char* c="#include <stdio.h>\n/* C comment */\nstatic int main(void)\n{\n    const char* msg = \"hi\\n\";\n    return 0xFF;\n}\n";
    CHECK(xuiCodeDocumentCreate(&document)==XUI_OK);
    CHECK(xuiCodeLanguageRegistryCreate(&registry)==XUI_OK);
    CHECK(xideRegisterLanguages(registry)==XUI_OK);
    CHECK(!strcmp(xideLanguageForPath("D:/目录/a.XL"),"xlang"));
    CHECK(!strcmp(xideLanguageForPath("D:/folder.xl/a.C"),"c"));
    CHECK(!strcmp(xideLanguageForPath("D:/a.h"),"c"));
    CHECK(!strcmp(xideLanguageForPath("D:/folder.xl/README"),"text"));
    CHECK(!strcmp(xideLanguageForPath("file.xlp"),"text"));
    CHECK(lex(registry,document,"xlang",source,0,-1,tokens,512,&count)==XUI_OK);
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"#doc")==XUI_CODE_TOKEN_COMMENT);
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"nested")==XUI_CODE_TOKEN_COMMENT);
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"async")==XUI_CODE_TOKEN_KEYWORD);
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"func main")==-1); /* independent lexical tokens */
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"protocol")==XUI_CODE_TOKEN_KEYWORD);
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"中文")==XUI_CODE_TOKEN_IDENTIFIER);
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"rf\"result")==XUI_CODE_TOKEN_STRING);
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"br\"raw")==XUI_CODE_TOKEN_STRING);
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"end class fake")==XUI_CODE_TOKEN_STRING);
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"#2026-10-06#")==XUI_CODE_TOKEN_NUMBER);
    CHECK(tokenKind(xuiCodeDocumentGetText(document),tokens,count,"3.5e-2")==XUI_CODE_TOKEN_NUMBER);
    CHECK(lex(registry,document,"xlang",source,0,-1,NULL,0,&required)==XUI_OK && required==count);
    CHECK(lex(registry,document,"xlang",source,0,-1,partial,2,&required)==XUI_OK && required==count);
    CHECK(lex(registry,document,"xlang",source,8,8,tokens,512,&count)==XUI_OK && count==1);
    CHECK(tokens[0].iKind==XUI_CODE_TOKEN_STRING); /* relex inside multiline literal */
    CHECK(lex(registry,document,"xlang",source,1,1,tokens,512,&count)==XUI_OK && count==1);
    CHECK(tokens[0].iKind==XUI_CODE_TOKEN_COMMENT); /* inside #doc */
    xui_code_language_t language={0}; CHECK(xuiCodeLanguageRegistryFind(registry,"xlang",&language)==XUI_OK);
    CHECK(xuiCodeLanguageFold(&language,document,folds,512,&foldCount)==XUI_OK && foldCount==5);
    CHECK(lex(registry,document,"xlang","1.member 2e+ foo from derive c_callback",0,-1,tokens,512,&count)==XUI_OK);
    CHECK(tokens[0].iEndOffset==1 && tokens[2].iKind==XUI_CODE_TOKEN_IDENTIFIER);
    CHECK(tokenKind("1.member 2e+ foo from derive c_callback",tokens,count,"from")==XUI_CODE_TOKEN_KEYWORD);
    CHECK(lex(registry,document,"xlang","\"unfinished\nfunc valid()\nend",0,-1,tokens,512,&count)==XUI_OK);
    CHECK(tokens[0].iKind==XUI_CODE_TOKEN_ERROR && tokens[1].iKind==XUI_CODE_TOKEN_KEYWORD);
    CHECK(lex(registry,document,"c",c,0,-1,tokens,512,&count)==XUI_OK);
    CHECK(tokenKind(c,tokens,count,"#include")==XUI_CODE_TOKEN_PREPROCESSOR);
    CHECK(tokenKind(c,tokens,count,"C comment")==XUI_CODE_TOKEN_COMMENT);
    CHECK(tokenKind(c,tokens,count,"return")==XUI_CODE_TOKEN_KEYWORD);
    CHECK(xuiCodeLanguageRegistryFind(registry,"c",&language)==XUI_OK);
    CHECK(xuiCodeLanguageFold(&language,document,folds,512,&foldCount)==XUI_OK && foldCount>=1);
    /* More than the old IDE's fixed 256-level fold stack. */
    deep=calloc(12000,1); CHECK(deep!=NULL);
    for(int i=0;i<300;++i) strcat(deep,"if true then\n");
    for(int i=0;i<300;++i) strcat(deep,"end\n");
    CHECK(xuiCodeDocumentSetText(document,deep)==XUI_OK);
    CHECK(xuiCodeLanguageRegistryFind(registry,"xlang",&language)==XUI_OK);
    CHECK(xuiCodeLanguageFold(&language,document,folds,512,&foldCount)==XUI_OK && foldCount==300);
    CHECK(xuiCreate(&context)==XUI_OK && xuiCodeEditCreate(context,&editor,NULL)==XUI_OK);
    CHECK(xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(editor))==XUI_OK);
    CHECK(xuiCodeEditSetText(editor,"func sample() as int\nreturn 0\nend\n")==XUI_OK);
    CHECK(xideConfigureEditorLanguage(editor,"xlang")==XUI_OK);
    CHECK(xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),tokens,512,&count)==XUI_OK);
    CHECK(tokens[0].iKind==XUI_CODE_TOKEN_KEYWORD); /* real editor cache, not just standalone lexer */
    CHECK(xideConfigureEditorLanguage(editor,"c")==XUI_OK);
    CHECK(xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),tokens,512,&count)==XUI_OK && tokens[0].iKind==XUI_CODE_TOKEN_IDENTIFIER);
    CHECK(xideConfigureEditorLanguage(editor,"text")==XUI_OK);
    CHECK(xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),tokens,512,&count)==XUI_OK && count==1 && tokens[0].iKind==XUI_CODE_TOKEN_TEXT);
    puts("language_test passed: C/xlang/text, UTF-8/CRLF, nested comments/strings, partial lexing, bounded buffers, 300 nested folds and actual editor cache switching");
cleanup:
    free(deep); if(context) xuiDestroy(context);
    if(document) xuiCodeDocumentDestroy(document);
    if(registry) xuiCodeLanguageRegistryDestroy(registry);
    return failed;
}