#include "lang/languages.h"
#include "lang/xlang_literal_span.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <xrt.h>

/* Keywords follow demo6/src/lexer/xl6_lexer.l. This is lexical highlighting,
 * not a parser or a replacement for compiler diagnostics. All offsets are UTF-8 bytes. */
static const char* const keywords[] = {
    "null", "true", "false", "func", "const", "var", "auto", "typedef", "as",
    "if", "then", "else", "elseif", "end", "for", "in", "is", "while", "do",
    "break", "continue", "return", "async", "await", "yield", "try", "catch",
    "finally", "throw", "assert", "import", "from", "export", "extern", "include",
    "define", "class", "protocol", "enum", "extends", "union", "derive", "get",
    "let", "public", "private", "protected", "static", "override", "final",
    "virtual", "native", "c_vararg", "c_callback", "match", "case", "and", "or", "not"
};
static const char* const types[] = {
    "void", "bool", "int", "int8", "uint8", "int16", "uint16", "int32", "uint32",
    "int64", "uint64", "float", "float32", "float64", "string", "bytes", "time",
    "point", "type", "array", "list", "set", "dict", "object", "task",
    "c_char", "c_schar", "c_uchar", "c_short", "c_ushort", "c_int", "c_uint",
    "c_long", "c_ulong", "c_longlong", "c_ulonglong", "c_size", "c_ssize",
    "c_ptrdiff", "c_intptr", "c_uintptr", "c_bool", "c_float", "c_double", "c_wchar"
};
static int identStart(unsigned char c) { return isalpha(c) || c == '_' || c >= 0x80; }
static int ident(unsigned char c) { return identStart(c) || isdigit(c); }
static int word(const char* text, int start, int end, const char* name)
{
    return end - start == (int)strlen(name) && memcmp(text + start, name, (size_t)(end - start)) == 0;
}
static int wordIn(const char* text, int start, int end, const char* const* words, size_t count)
{
    for (size_t i = 0; i < count; ++i) if (word(text, start, end, words[i])) return 1;
    return 0;
}
static int directive(const char* text, int length, int offset, const char* name)
{
    int i = offset;
    while (i > 0 && text[i - 1] != '\n' && text[i - 1] != '\r') {
        if (text[i - 1] != ' ' && text[i - 1] != '\t') return 0;
        --i;
    }
    i = offset + (int)strlen(name);
    if (i > length || memcmp(text + offset, name, strlen(name)) != 0) return 0;
    while (i < length && (text[i] == ' ' || text[i] == '\t')) ++i;
    return i == length || text[i] == '\r' || text[i] == '\n';
}
static void emit(xui_code_token_t* tokens, int capacity, int* count,
                 int start, int end, int kind, int rangeStart, int rangeEnd)
{
    if (end <= rangeStart || start >= rangeEnd || end <= start) return;
    if (tokens && *count < capacity) tokens[*count] = (xui_code_token_t){start, end, kind, kind, 0};
    ++*count;
}
static int numberEnd(const char* text, int length, int start)
{
    int i = start;
    if (length - i >= 2 && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'o' || text[i + 1] == 'b')) {
        int base = text[i + 1]; i += 2;
        while (i < length && (text[i] == '_' || (base == 'x' ? isxdigit((unsigned char)text[i]) :
               base == 'o' ? text[i] >= '0' && text[i] <= '7' : text[i] == '0' || text[i] == '1'))) ++i;
        return i;
    }
    while (i < length && (isdigit((unsigned char)text[i]) || text[i] == '_')) ++i;
    if (length - i >= 2 && text[i] == '.' && isdigit((unsigned char)text[i + 1])) {
        ++i; while (i < length && (isdigit((unsigned char)text[i]) || text[i] == '_')) ++i;
    }
    if (i < length && (text[i] == 'e' || text[i] == 'E')) {
        int exponent = i + 1;
        if (exponent < length && (text[exponent] == '+' || text[exponent] == '-')) ++exponent;
        if (exponent < length && isdigit((unsigned char)text[exponent])) {
            i = exponent + 1;
            while (i < length && (isdigit((unsigned char)text[i]) || text[i] == '_')) ++i;
        }
    }
    return i;
}
static int xlangLexInto(xui_code_document_t* document, int startLine, int endLine,
                    xui_code_token_t* tokens, int capacity, int* count, void* user, xarray* output)
{
    (void)user;
    if (!document || !count || capacity < 0) return XUI_ERROR_INVALID_ARGUMENT;
    const char* text = xuiCodeDocumentGetText(document);
    int length = xuiCodeDocumentGetLength(document), rangeStart, rangeEnd;
    int lines = xuiCodeDocumentGetLineCount(document);
    *count = 0;
    if (startLine < 0) startLine = 0;
    if (endLine < 0 || endLine >= lines) endLine = lines - 1;
    if (startLine > endLine || startLine >= lines) return XUI_OK;
    int result = xuiCodeDocumentGetLineRange(document, startLine, &rangeStart, NULL);
    if (result == XUI_OK) result = xuiCodeDocumentGetLineRange(document, endLine, NULL, &rangeEnd);
    if (result != XUI_OK) return result;
    /* Scan the preceding text too: partial relexing inside multiline strings,
     * nested comments and #doc must retain the original lexical state. */
    for (int i = 0; i < length && i < rangeEnd;) {
        int start = i, kind = XUI_CODE_TOKEN_OPERATOR;
        unsigned char c = (unsigned char)text[i];
        XL6_LiteralFrame opening;
        size_t content;
        if (isspace(c)) { ++i; continue; }
        if (c == '#' && directive(text, length, i, "#doc")) {
            i += 4;
            while (i < length && !(text[i] == '#' && directive(text, length, i, "#end"))) ++i;
            if (i < length) { i += 4; kind = XUI_CODE_TOKEN_COMMENT; }
            else kind = XUI_CODE_TOKEN_ERROR;
        } else if (c == '/' && length - i >= 2 && text[i + 1] == '/') {
            i += 2; while (i < length && text[i] != '\n' && text[i] != '\r') ++i;
            kind = XUI_CODE_TOKEN_COMMENT;
        } else if (c == '/' && length - i >= 2 && text[i + 1] == '*') {
            int depth = 1; i += 2;
            while (i < length && depth) {
                if (length - i >= 2 && text[i] == '/' && text[i + 1] == '*') { ++depth; i += 2; }
                else if (length - i >= 2 && text[i] == '*' && text[i + 1] == '/') { --depth; i += 2; }
                else ++i;
            }
            kind = depth ? XUI_CODE_TOKEN_ERROR : XUI_CODE_TOKEN_COMMENT;
        } else if (xl6LiteralOpening(text, (size_t)length, (size_t)i, &opening, &content)) {
            XL6_LiteralSpan span = xl6ScanLiteralSpan(text, (size_t)length, (size_t)i, 0);
            i = (int)span.end;
            if (i <= start) i = start + 1;
            kind = span.error ? XUI_CODE_TOKEN_ERROR : opening.close == '\'' ? XUI_CODE_TOKEN_CHAR : XUI_CODE_TOKEN_STRING;
        } else if (c == '#') {
            ++i; while (i < length && text[i] != '#' && text[i] != '\r' && text[i] != '\n') ++i;
            if (i < length && text[i] == '#') { ++i; kind = XUI_CODE_TOKEN_NUMBER; }
            else kind = XUI_CODE_TOKEN_ERROR;
        } else if (isdigit(c)) {
            i = numberEnd(text, length, i); kind = XUI_CODE_TOKEN_NUMBER;
        } else if (identStart(c)) {
            ++i; while (i < length && ident((unsigned char)text[i])) ++i;
            kind = wordIn(text, start, i, keywords, sizeof(keywords) / sizeof(*keywords)) ? XUI_CODE_TOKEN_KEYWORD :
                   wordIn(text, start, i, types, sizeof(types) / sizeof(*types)) ? XUI_CODE_TOKEN_TYPE : XUI_CODE_TOKEN_IDENTIFIER;
        } else {
            ++i; if (strchr("()[]{}", c)) kind = XUI_CODE_TOKEN_BRACE;
        }
        if(output && i > rangeStart && start < rangeEnd && i > start) {
            xui_code_token_t token={start,i,kind,kind,0};
            if(*count==INT_MAX || !xrtArrayAppend(output,&token,1)) return XUI_ERROR_OUT_OF_MEMORY;
            ++*count;
        } else emit(tokens, capacity, count, start, i, kind, rangeStart, rangeEnd);
    }
    return XUI_OK;
}
static int xlangLex(xui_code_document_t* document, int startLine, int endLine,
                    xui_code_token_t* tokens, int capacity, int* count, void* user)
{
    return xlangLexInto(document,startLine,endLine,tokens,capacity,count,user,NULL);
}
static int lineAt(const char* text, int* offset, int* line, int end)
{
    while (*offset < end) {
        if (text[*offset] == '\r') {
            ++*line;
            if (*offset + 1 < end && text[*offset + 1] == '\n') ++*offset;
        } else if (text[*offset] == '\n') ++*line;
        ++*offset;
    }
    return *line;
}
static void foldEmit(xui_code_fold_range_t* ranges, int capacity, int* count, int start, int end, int level, uint32_t flags)
{
    if (end <= start) return;
    if (ranges && *count < capacity) ranges[*count] = (xui_code_fold_range_t){sizeof(xui_code_fold_range_t), start, end, level, flags | XUI_CODE_FOLD_HEADER};
    ++*count;
}
/* Both the public provider and the editor refresh use this fold walk. The
 * editor supplies the just-produced tokens, so folding never re-lexes its text. */
static int xlangFoldTokens(xui_code_document document,const xui_code_token_t* tokens,int tokenCount,
                           xui_code_fold_range_t* ranges,int capacity,int* count,xarray* output)
{
    static const char* const blocks[] = {"func", "class", "protocol", "enum", "union", "if", "while", "for", "try", "match", "get", "let"};
    static const char* const modifiers[] = {"async", "public", "private", "protected", "static", "virtual", "override", "final", "export"};
    int result=XUI_OK,offset=0,line=0,depth=0,first=1; xarray stack;
    *count=0;
    if(!xrtArrayInit(&stack,sizeof(int))) return XUI_ERROR_OUT_OF_MEMORY;
    if(!xrtArrayReserve(&stack,(size_t)tokenCount)) {xrtArrayUnit(&stack);return XUI_ERROR_OUT_OF_MEMORY;}
    const char* text=xuiCodeDocumentGetText(document);
    for(int i=0;i<tokenCount && result==XUI_OK;++i) {
        const xui_code_token_t* token=tokens+i;int previous=line;
        int start=lineAt(text,&offset,&line,token->iStartOffset);
        if(previous!=line) first=1;
        int end=lineAt(text,&offset,&line,token->iEndOffset),foldStart=-1,foldEnd=end,level=depth;uint32_t flags=0;
        if(token->iKind==XUI_CODE_TOKEN_COMMENT || token->iKind==XUI_CODE_TOKEN_STRING || token->iKind==XUI_CODE_TOKEN_ERROR) {
            foldStart=start;flags=token->iKind==XUI_CODE_TOKEN_COMMENT ? XUI_CODE_FOLD_COMMENT : XUI_CODE_FOLD_CUSTOM;
            first=end>start;
        } else {
            if(first && word(text,token->iStartOffset,token->iEndOffset,"end")) {
                if(depth) {--depth;foldStart=((int*)stack.Data)[depth];foldEnd=start;level=depth;}
            } else if(first && wordIn(text,token->iStartOffset,token->iEndOffset,blocks,sizeof(blocks)/sizeof(*blocks))) {
                ((int*)stack.Data)[depth++]=start;
            } else if(first && wordIn(text,token->iStartOffset,token->iEndOffset,modifiers,sizeof(modifiers)/sizeof(*modifiers))) continue;
            first=0;
        }
        if(foldStart>=0 && foldEnd>foldStart) {
            if(output) {
                xui_code_fold_range_t range={sizeof(range),foldStart,foldEnd,level,flags|XUI_CODE_FOLD_HEADER};
                if(*count==INT_MAX || !xrtArrayAppend(output,&range,1)) result=XUI_ERROR_OUT_OF_MEMORY;
                else ++*count;
            } else foldEmit(ranges,capacity,count,foldStart,foldEnd,level,flags);
        }
    }
    xrtArrayUnit(&stack);return result;
}
static int xlangFold(xui_code_document_t* document, xui_code_fold_range_t* ranges, int capacity, int* count, void* user)
{
    (void)user;
    if(!document || !count || capacity<0) return XUI_ERROR_INVALID_ARGUMENT;
    *count=0;xarray tokens;if(!xrtArrayInit(&tokens,sizeof(xui_code_token_t))) return XUI_ERROR_OUT_OF_MEMORY;
    int tokenCount=0,result=xlangLexInto(document,0,-1,NULL,0,&tokenCount,NULL,&tokens);
    if(result==XUI_OK) result=xlangFoldTokens(document,(const xui_code_token_t*)tokens.Data,tokenCount,ranges,capacity,count,NULL);
    xrtArrayUnit(&tokens);return result;
}
const char* xideLanguageForPath(const char* path)
{
    if (!path) return "text";
    const char* slash = strrchr(path, '/'), *backslash = strrchr(path, '\\');
    if (!slash || (backslash && backslash > slash)) slash = backslash;
    const char* dot = strrchr(path, '.');
    if (!dot || (slash && dot < slash)) return "text";
    char extension[8]; size_t size = strlen(dot);
    if (size >= sizeof(extension)) return "text";
    for (size_t i = 0; i <= size; ++i) extension[i] = (char)tolower((unsigned char)dot[i]);
    if (strcmp(extension, ".xl") == 0) return "xlang";
    if (strcmp(extension, ".c") == 0 || strcmp(extension, ".h") == 0) return "c";
    return "text";
}
static int plainLex(xui_code_document_t* document, int startLine, int endLine,
                    xui_code_token_t* tokens, int capacity, int* count, void* user)
{
    (void)user;
    if (!document || !count || capacity < 0) return XUI_ERROR_INVALID_ARGUMENT;
    *count = 0;
    int lines = xuiCodeDocumentGetLineCount(document), start, end;
    if (startLine < 0) startLine = 0;
    if (endLine < 0 || endLine >= lines) endLine = lines - 1;
    if (startLine > endLine) return XUI_OK;
    int result = xuiCodeDocumentGetLineRange(document, startLine, &start, NULL);
    if (result == XUI_OK) result = xuiCodeDocumentGetLineRange(document, endLine, NULL, &end);
    if (result == XUI_OK) emit(tokens, capacity, count, start, end, XUI_CODE_TOKEN_TEXT, start, end);
    return result;
}
int xideRegisterLanguages(xui_code_language_registry registry)
{
    static const char* const extensions[] = {"xl"};
    xui_code_language_t language = {0};
    int result = xuiCodeLanguageRegistryLoadDefaults(registry);
    if (result != XUI_OK) return result;
    language.iSize = sizeof(language);
    language.sId = "xlang"; language.sName = "xlang";
    language.arrExtensions = extensions; language.iExtensionCount = 1;
    language.sLineComment = "//"; language.sBlockCommentStart = "/*"; language.sBlockCommentEnd = "*/";
    language.iLexerType = XUI_CODE_LEXER_CALLBACK; language.onLex = xlangLex; language.onFold = xlangFold;
    result = xuiCodeLanguageRegistryRegister(registry, &language);
    if (result != XUI_OK) return result;
    memset(&language, 0, sizeof(language)); language.iSize = sizeof(language);
    language.sId = "text"; language.sName = "Plain text"; language.iLexerType = XUI_CODE_LEXER_CALLBACK; language.onLex = plainLex;
    return xuiCodeLanguageRegistryRegister(registry, &language);
}
static int compareFold(const void* left,const void* right)
{
    const xui_code_fold_range_t *a=left,*b=right;
    if(a->iStartLine!=b->iStartLine) return a->iStartLine<b->iStartLine ? -1 : 1;
    if(a->iEndLine!=b->iEndLine) return a->iEndLine<b->iEndLine ? -1 : 1;
    if(a->iLevel!=b->iLevel) return a->iLevel<b->iLevel ? -1 : 1;
    return 0;
}
static int rebuildEditorFolds(xui_widget editor,const xui_code_token_t* lexical,int lexicalCount,int shared)
{
    xui_code_language_t language={0}; xui_code_fold_state state=xuiCodeEditGetFoldState(editor);
    int result=xuiCodeLanguageRegistryFind(xuiCodeEditGetLanguageRegistry(editor),xuiCodeEditGetLanguage(editor),&language);
    if(result!=XUI_OK) return result;
    int previousCount=xuiCodeFoldStateGetCount(state);
    xui_code_fold_range_t* previous=previousCount ? malloc((size_t)previousCount*sizeof(*previous)) : NULL;
    if(previousCount && !previous) return XUI_ERROR_OUT_OF_MEMORY;
    if(previousCount) result=xuiCodeFoldStateGetRanges(state,previous,previousCount,&previousCount);
    if(result!=XUI_OK) { free(previous); return result; }
    if(previousCount) qsort(previous,(size_t)previousCount,sizeof(*previous),compareFold);
    xuiCodeFoldStateClear(state);
    if(shared && language.onFold==xlangFold && language.onLex==xlangLex) {
        xarray folds;
        if(!xrtArrayInit(&folds,sizeof(xui_code_fold_range_t))) result=XUI_ERROR_OUT_OF_MEMORY;
        else {
            int count=0;result=xlangFoldTokens(xuiCodeEditGetDocument(editor),lexical,lexicalCount,NULL,0,&count,&folds);
            if(result==XUI_OK) result=xuiCodeFoldStateSetRanges(state,(const xui_code_fold_range_t*)folds.Data,count);
            xrtArrayUnit(&folds);
        }
    } else if(language.onFold) result=xuiCodeFoldStateBuildFromProvider(state,xuiCodeEditGetDocument(editor),language.onFold,language.pUser);
    int count=xuiCodeFoldStateGetCount(state);
    xui_code_fold_range_t* ranges=count ? malloc((size_t)count*sizeof(*ranges)) : NULL;
    if(count && !ranges) result=XUI_ERROR_OUT_OF_MEMORY;
    if(result==XUI_OK && count) result=xuiCodeFoldStateGetRanges(state,ranges,count,&count);
    if(result==XUI_OK && count && previousCount) {
        /* Keep collapse state for structurally unchanged ranges. Edits moving
         * a block's boundaries currently expand that block deliberately. */
        for(int i=0;i<count;++i) {
            const xui_code_fold_range_t* old=bsearch(&ranges[i],previous,(size_t)previousCount,sizeof(*previous),compareFold);
            if(old && (old->iFlags & XUI_CODE_FOLD_COLLAPSED)) ranges[i].iFlags|=XUI_CODE_FOLD_COLLAPSED;
        }
        result=xuiCodeFoldStateSetRanges(state,ranges,count);
    }
    free(previous); free(ranges); return result;
}
int xideRebuildEditorFolds(xui_widget editor)
{
    /* A token cache does not carry its language identity. Only an in-flight
     * refresh may share tokens; a standalone rebuild uses the real provider. */
    return rebuildEditorFolds(editor,NULL,0,0);
}
int xideRefreshEditorSyntax(xui_widget editor)
{
    xui_code_document document = xuiCodeEditGetDocument(editor);
    xui_code_language_t language = {0};
    int count = 0;
    int result = xuiCodeLanguageRegistryFind(xuiCodeEditGetLanguageRegistry(editor), xuiCodeEditGetLanguage(editor), &language);
    if (result != XUI_OK) return result;
    if(language.onLex==xlangLex) {
        xarray tokens;
        if(!xrtArrayInit(&tokens,sizeof(xui_code_token_t))) return XUI_ERROR_OUT_OF_MEMORY;
        result=xlangLexInto(document,0,-1,NULL,0,&count,NULL,&tokens);
        if(result==XUI_OK) result=xuiCodeTokenBufferSetRange(xuiCodeEditGetTokenBuffer(editor),(const xui_code_token_t*)tokens.Data,count,
                                  xuiCodeDocumentGetVersion(document),0,xuiCodeDocumentGetLength(document));
        if(result==XUI_OK) result=rebuildEditorFolds(editor,(const xui_code_token_t*)tokens.Data,count,1);
        xrtArrayUnit(&tokens);return result;
    }
    result = xuiCodeLanguageLex(&language, document, 0, -1, NULL, 0, &count, NULL, 0);
    if (result != XUI_OK) return result;
    xui_code_token_t* tokens = count ? malloc((size_t)count * sizeof(*tokens)) : NULL;
    if (count && !tokens) return XUI_ERROR_OUT_OF_MEMORY;
    if (count) result = xuiCodeLanguageLex(&language, document, 0, -1, tokens, count, &count, NULL, 0);
    /* This SDK's renderer falls back to C when its cache is absent. Populate
     * the cache explicitly so custom xlang/plain-text lexers are honored.
     * Keep the cache version and full byte range synchronized with the document. */
    if (result == XUI_OK) result = xuiCodeTokenBufferSetRange(xuiCodeEditGetTokenBuffer(editor), tokens, count,
                                  xuiCodeDocumentGetVersion(document), 0, xuiCodeDocumentGetLength(document));
    free(tokens);
    if (result == XUI_OK) result = xideRebuildEditorFolds(editor);
    return result;
}
int xideConfigureEditorLanguage(xui_widget editor, const char* language)
{
    int result = xuiCodeEditSetLanguage(editor, language);
    return result == XUI_OK ? xideRefreshEditorSyntax(editor) : result;
}