#ifndef XL_PARSER_API_H
#define XL_PARSER_API_H

#ifndef XL_PARSE_CONTEXT_DEFINED
#define XL_PARSE_CONTEXT_DEFINED
typedef struct XL_ParseContext {
	xvalue xAst;
	int bHadError;
	int iErrorCount;
	const char *pszFilename;
} XL_ParseContext;
#endif

typedef struct XL_Parser {
	XL_Lexer* pLexer;
	XL_ParseContext ctx;
	XL_Token bufferedToken;
	int bHasBufferedToken;
	int iPendingSyntheticEnd;
	int iSingleLineIfDepth;
	int iLastLine;
	int iLastColumn;
	int bHadError;
	int iErrorCount;
} XL_Parser;

XL_Parser* xlParserCreate( XL_Lexer* pLexer );
void xlParserDestroy( XL_Parser* pParser );
xvalue xlParserParse( XL_Parser* pParser, const char* pszFilename );

#endif /* XL_PARSER_API_H */
