#ifndef XL6_LEXER_API_H
#define XL6_LEXER_API_H

#include "src/diag/xl6_diag.h"
#include "src/lexer/xl6_token.h"

typedef struct XL6_LexerExtra {
    XL6_Token current;
    size_t offset;
    size_t tokenStartOffset;
    int line;
    int column;
    int tokenStartLine;
    int tokenStartColumn;
    int parenDepth;
    int bracketDepth;
    int braceDepth;
    int commentDepth;
    int hadError;
    char errorText[128];
} XL6_LexerExtra;

typedef struct XL6_Lexer {
    void* scanner;
    void* buffer;
    XL6_LexerExtra extra;
} XL6_Lexer;

int xl6LexerOpen(XL6_Lexer* lexer, const char* sourceText);
void xl6LexerClose(XL6_Lexer* lexer);
XL6_Token xl6LexerNext(XL6_Lexer* lexer);
int xl6LexerDumpFile(const char* path, int jsonOutput);
int xl6LexerDumpFileTo(FILE* out, const char* path, int jsonOutput);

#endif
