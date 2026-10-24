/* Compiled in the original scanner TU; reuse its exact position advance. */
int xlsFrontendLexerResume(XL6_Lexer* lexer,const char* source,size_t offset,int paren,int bracket,int brace)
{
    size_t size=source ? strlen(source) : 0;
    if(!lexer || !source || offset>size || size>INT_MAX-2u || (offset<size && ((unsigned char)source[offset]&0xc0)==0x80))return 0;
    xl6LexerClose(lexer);memset(lexer,0,sizeof(*lexer));
    lexer->extra.sourceText=source;lexer->extra.sourceSize=size;lexer->extra.line=lexer->extra.column=1;
    xl6AdvancePosition(&lexer->extra,source,(int)offset);
    lexer->extra.parenDepth=paren;lexer->extra.bracketDepth=bracket;lexer->extra.braceDepth=brace;
    if(yylex_init_extra(&lexer->extra,(yyscan_t*)&lexer->scanner))return 0;
    lexer->buffer=yy_scan_bytes(source+offset,(int)(size-offset),(yyscan_t)lexer->scanner);
    return lexer->buffer!=NULL;
}
