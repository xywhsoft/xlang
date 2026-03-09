/**
 * xlang v4 词法分析器 API
 * 
 * 对 Flex 生成的词法分析器的高级封装
 * Token 类型定义在 xl_tokens.h 中（由 main.c 先 include）
 */

#ifndef XL_LEXER_API_H
#define XL_LEXER_API_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* === Lexer 句柄 === */
typedef struct XL_Lexer {
	yyscan_t scanner;
	XL_LexerExtra extra;
	char* source;
	size_t sourceLen;
	int bOwnsSource;
} XL_Lexer;



/* === Lexer 创建与销毁 === */

/* 从字符串创建 Lexer */
static XL_Lexer* xlLexerCreate(const char* sSource, size_t iLen, const char* sFilename)
{
	XL_Lexer* pLexer = (XL_Lexer*)malloc(sizeof(XL_Lexer));
	if ( !pLexer ) {
		return NULL;
	}
	
	memset(pLexer, 0, sizeof(XL_Lexer));
	
	/* 复制源码 */
	pLexer->source = (char*)malloc(iLen + 2);
	if ( !pLexer->source ) {
		free(pLexer);
		return NULL;
	}
	memcpy(pLexer->source, sSource, iLen);
	pLexer->source[iLen] = '\0';
	pLexer->source[iLen + 1] = '\0';  /* Flex 需要双 NUL 结尾 */
	pLexer->sourceLen = iLen;
	pLexer->bOwnsSource = 1;
	
	/* 初始化额外数据 */
	pLexer->extra.filename = sFilename ? sFilename : "<string>";
	pLexer->extra.column = 1;
	pLexer->extra.prevColumn = 1;
	pLexer->extra.errorMsg = NULL;
	
	/* 创建 Flex scanner */
	xllex_init_extra(&pLexer->extra, &pLexer->scanner);
	
	/* 设置输入缓冲区 */
	YY_BUFFER_STATE buf = xl_scan_buffer(pLexer->source, iLen + 2, pLexer->scanner);
	xl_switch_to_buffer(buf, pLexer->scanner);
	
	/* 设置初始行号（必须在设置缓冲区之后） */
	xlset_lineno(1, pLexer->scanner);
	
	return pLexer;
}

/* 从文件创建 Lexer */
static XL_Lexer* xlLexerCreateFromFile(const char* sFilepath)
{
	FILE* fp = fopen(sFilepath, "rb");
	if ( !fp ) {
		return NULL;
	}
	
	/* 获取文件大小 */
	fseek(fp, 0, SEEK_END);
	long iSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	if ( iSize < 0 ) {
		fclose(fp);
		return NULL;
	}
	
	/* 读取文件内容 */
	char* sContent = (char*)malloc(iSize + 1);
	if ( !sContent ) {
		fclose(fp);
		return NULL;
	}
	
	size_t iRead = fread(sContent, 1, iSize, fp);
	fclose(fp);
	sContent[iRead] = '\0';
	
	/* 创建 Lexer */
	XL_Lexer* pLexer = xlLexerCreate(sContent, iRead, sFilepath);
	free(sContent);
	
	return pLexer;
}

/* 销毁 Lexer */
static void xlLexerDestroy(XL_Lexer* pLexer)
{
	if ( !pLexer ) {
		return;
	}
	
	if ( pLexer->scanner ) {
		xllex_destroy(pLexer->scanner);
	}
	
	if ( pLexer->bOwnsSource && pLexer->source ) {
		free(pLexer->source);
	}
	
	free(pLexer);
}



/* === Token 操作 === */

/* 获取下一个 Token */
static int xlLexerNextToken(XL_Lexer* pLexer, XL_Token* pToken)
{
	if ( !pLexer || !pToken ) {
		return XL_TOK_ERROR;
	}
	
	int iType = xllex(pLexer->scanner);
	
	/* 填充 Token 信息 */
	pToken->type = (XL_TokenType)iType;
	pToken->text = xlget_text(pLexer->scanner);
	pToken->length = xlget_leng(pLexer->scanner);
	pToken->line = xlget_lineno(pLexer->scanner);
	pToken->column = pLexer->extra.prevColumn;
	
	/* 复制解析后的值（整数/浮点数在 lexer 规则中通过 xlMakeToken 设置） */
	pToken->iVal = pLexer->extra.currentToken.iVal;
	
	return iType;
}

/* 获取当前行号 */
static int xlLexerGetLine(XL_Lexer* pLexer)
{
	if ( !pLexer ) {
		return 0;
	}
	return xlget_lineno(pLexer->scanner);
}

/* 获取当前列号 */
static int xlLexerGetColumn(XL_Lexer* pLexer)
{
	if ( !pLexer ) {
		return 0;
	}
	return pLexer->extra.column;
}

/* 获取错误信息 */
static const char* xlLexerGetError(XL_Lexer* pLexer)
{
	if ( !pLexer ) {
		return NULL;
	}
	return pLexer->extra.errorMsg;
}

/* 检查是否有错误 */
static int xlLexerHasError(XL_Lexer* pLexer)
{
	if ( !pLexer ) {
		return 0;
	}
	return pLexer->extra.errorMsg != NULL;
}



/* === 调试辅助 === */

/* 打印 Token */
static void xlTokenPrint(XL_Token* pToken)
{
	if ( !pToken ) {
		return;
	}
	
	printf("%3d:%-3d  %-20s  ", pToken->line, pToken->column, xlTokenTypeName(pToken->type));
	
	/* 打印 Token 文本（处理特殊字符） */
	if ( pToken->length > 0 ) {
		printf("\"");
		for ( size_t i = 0; i < pToken->length && i < 40; i++ ) {
			char c = pToken->text[i];
			if ( c == '\n' ) {
				printf("\\n");
			} else if ( c == '\t' ) {
				printf("\\t");
			} else if ( c == '\r' ) {
				printf("\\r");
			} else {
				printf("%c", c);
			}
		}
		if ( pToken->length > 40 ) {
			printf("...");
		}
		printf("\"");
	}
	
	printf("\n");
}

/* 打印所有 Token */
static void xlLexerDumpTokens(XL_Lexer* pLexer)
{
	if ( !pLexer ) {
		return;
	}
	
	printf("=== Token 列表 ===\n");
	printf("行:列    类型                  文本\n");
	printf("----------------------------------------------\n");
	
	XL_Token token;
	int iType;
	int iCount = 0;
	
	while ( (iType = xlLexerNextToken(pLexer, &token)) != XL_TOK_EOF ) {
		/* 跳过换行（可选） */
		if ( iType == XL_TOK_NEWLINE ) {
			continue;
		}
		
		xlTokenPrint(&token);
		iCount++;
		
		if ( iType == XL_TOK_ERROR ) {
			printf("错误: 无法识别的字符\n");
		}
	}
	
	printf("----------------------------------------------\n");
	printf("共 %d 个 Token\n", iCount);
}

/* JSON 格式输出 Token */
static void xlTokenPrintJson(XL_Token* pToken, int bFirst)
{
	if ( !pToken ) {
		return;
	}
	
	if ( !bFirst ) {
		printf(",\n");
	}
	
	printf("  {\n");
	printf("    \"type\": \"%s\",\n", xlTokenTypeName(pToken->type));
	printf("    \"line\": %d,\n", pToken->line);
	printf("    \"column\": %d,\n", pToken->column);
	printf("    \"text\": \"");
	
	/* 转义 JSON 字符串 */
	for ( size_t i = 0; i < pToken->length; i++ ) {
		char c = pToken->text[i];
		switch ( c ) {
			case '"':  printf("\\\""); break;
			case '\\': printf("\\\\"); break;
			case '\n': printf("\\n"); break;
			case '\r': printf("\\r"); break;
			case '\t': printf("\\t"); break;
			default:   printf("%c", c); break;
		}
	}
	
	printf("\"\n  }");
}

/* JSON 格式输出所有 Token */
static void xlLexerDumpTokensJson(XL_Lexer* pLexer)
{
	if ( !pLexer ) {
		return;
	}
	
	printf("[\n");
	
	XL_Token token;
	int iType;
	int bFirst = 1;
	
	while ( (iType = xlLexerNextToken(pLexer, &token)) != XL_TOK_EOF ) {
		/* 跳过换行 */
		if ( iType == XL_TOK_NEWLINE ) {
			continue;
		}
		
		xlTokenPrintJson(&token, bFirst);
		bFirst = 0;
	}
	
	printf("\n]\n");
}

#endif /* XL_LEXER_API_H */
