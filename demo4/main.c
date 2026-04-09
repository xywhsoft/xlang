/**
 * xlang v4 编译器工具
 * 
 * 用法: xlang [模式] [选项] <文件>
 * 
 * 模式:
 *   --lex         词法分析（输出 Token 列表）
 *   --ast         语法分析（输出 AST 树）
 * 
 * 选项:
 *   -o <文件>     输出到文件（默认标准输出）
 *   -j            输出 JSON 格式
 *   -v            显示详细信息
 *   -h            显示帮助
 */

#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 共享 Token 定义 */
#include "src/parse/xl_tokens.h"

/* Flex 生成的实现（作为头文件 include） */
#include "src/lexer/xl_lexer_impl.h"

/* Lexer 高级 API */
#include "src/lexer/xl_lexer_api.h"

/* AST 定义 */
#include "bak/xl_ast.h"

/* Parser */
#include "bak/xl_parser.h"



/* === 命令行选项 === */
typedef enum RunMode {
	MODE_LEX = 0,
	MODE_AST
} RunMode;

typedef struct Options {
	RunMode mode;
	const char* sInputFile;
	const char* sOutputFile;
	int bJsonOutput;
	int bVerbose;
	int bHelp;
} Options;



/* === 帮助信息 === */
static void PrintHelp( const char* sProgName )
{
	printf("xlang v4 编译器工具\n\n");
	printf("用法: %s [模式] [选项] <文件>\n\n", sProgName);
	printf("模式:\n");
	printf("  --lex         词法分析（输出 Token 列表）\n");
	printf("  --ast         语法分析（输出 AST 树，默认模式）\n");
	printf("\n选项:\n");
	printf("  -o <文件>     输出到文件（默认标准输出）\n");
	printf("  -j            输出 JSON 格式\n");
	printf("  -v            显示详细信息\n");
	printf("  -h            显示帮助\n");
	printf("\n示例:\n");
	printf("  %s test.xl             解析 test.xl 并输出 AST\n", sProgName);
	printf("  %s --lex test.xl       输出 Token 列表\n", sProgName);
	printf("  %s --lex -j test.xl    输出 JSON 格式的 Token 列表\n", sProgName);
	printf("  %s --ast test.xl       输出 AST 树\n", sProgName);
}


/* === 解析命令行参数 === */
static int ParseArgs( int argc, char** argv, Options* pOpts )
{
	memset(pOpts, 0, sizeof(Options));
	pOpts->mode = MODE_AST;  /* 默认 AST 模式 */
	
	for ( int i = 1; i < argc; i++ ) {
		if ( (strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0) ) {
			pOpts->bHelp = 1;
		} else if ( strcmp(argv[i], "--lex") == 0 ) {
			pOpts->mode = MODE_LEX;
		} else if ( strcmp(argv[i], "--ast") == 0 ) {
			pOpts->mode = MODE_AST;
		} else if ( (strcmp(argv[i], "-j") == 0) || (strcmp(argv[i], "--json") == 0) ) {
			pOpts->bJsonOutput = 1;
		} else if ( (strcmp(argv[i], "-v") == 0) || (strcmp(argv[i], "--verbose") == 0) ) {
			pOpts->bVerbose = 1;
		} else if ( strcmp(argv[i], "-o") == 0 ) {
			if ( i + 1 >= argc ) {
				fprintf(stderr, "错误: -o 选项需要指定输出文件\n");
				return -1;
			}
			pOpts->sOutputFile = argv[++i];
		} else if ( argv[i][0] == '-' ) {
			fprintf(stderr, "错误: 未知选项 %s\n", argv[i]);
			return -1;
		} else {
			if ( pOpts->sInputFile ) {
				fprintf(stderr, "错误: 只能指定一个输入文件\n");
				return -1;
			}
			pOpts->sInputFile = argv[i];
		}
	}
	
	return 0;
}



/* === 词法分析模式 === */
static int RunLexMode( Options* pOpts )
{
	XL_Lexer* pLexer = xlLexerCreateFromFile(pOpts->sInputFile);
	if ( !pLexer ) {
		fprintf(stderr, "错误: 无法打开文件 %s\n", pOpts->sInputFile);
		return 1;
	}
	
	if ( pOpts->bJsonOutput ) {
		xlLexerDumpTokensJson(pLexer);
	} else {
		xlLexerDumpTokens(pLexer);
	}
	
	xlLexerDestroy(pLexer);
	return 0;
}



/* === 语法分析模式 === */
static int RunAstMode( Options* pOpts )
{
	XL_Lexer* pLexer = xlLexerCreateFromFile(pOpts->sInputFile);
	if ( !pLexer ) {
		fprintf(stderr, "错误: 无法打开文件 %s\n", pOpts->sInputFile);
		return 1;
	}
	
	XL_Parser* pParser = xlParserCreate(pLexer);
	if ( !pParser ) {
		fprintf(stderr, "错误: 无法创建 Parser\n");
		xlLexerDestroy(pLexer);
		return 1;
	}
	
	xvalue xAST = xlParserParse(pParser, (const char*)pOpts->sInputFile);
	
	if ( pParser->bHadError ) {
		fprintf(stderr, "共 %d 个语法错误\n", pParser->iErrorCount);
	}
	
	if ( xAST ) {
		if ( pOpts->bJsonOutput ) {
			xvoPrintValue(xAST, 0, 0, 0, NULL);
			printf("\n");
		} else {
			xlAstPrint(xAST);
		}
	}
	
	int bHadError = pParser->bHadError;
	
	xlParserDestroy(pParser);
	xlLexerDestroy(pLexer);
	
	return bHadError ? 1 : 0;
}



/* === 主程序 === */
int main( int argc, char** argv )
{
	Options opts;
	
	/* 解析命令行参数 */
	if ( ParseArgs(argc, argv, &opts) < 0 ) {
		return 1;
	}
	
	/* 显示帮助 */
	if ( opts.bHelp ) {
		PrintHelp(argv[0]);
		return 0;
	}
	
	/* 检查输入文件 */
	if ( !opts.sInputFile ) {
		fprintf(stderr, "错误: 请指定输入文件\n");
		fprintf(stderr, "使用 -h 查看帮助\n");
		return 1;
	}
	
	/* 详细模式 */
	if ( opts.bVerbose ) {
		fprintf(stderr, "输入文件: %s\n", opts.sInputFile);
		fprintf(stderr, "模式: %s\n", opts.mode == MODE_LEX ? "词法分析" : "语法分析");
		if ( opts.sOutputFile ) {
			fprintf(stderr, "输出文件: %s\n", opts.sOutputFile);
		}
		fprintf(stderr, "\n");
	}
	
	/* 重定向输出 */
	if ( opts.sOutputFile ) {
		if ( !freopen(opts.sOutputFile, "w", stdout) ) {
			fprintf(stderr, "错误: 无法打开输出文件 %s\n", opts.sOutputFile);
			return 1;
		}
	}
	
	/* 初始化 xrt（AST 模式需要） */
	int iResult = 0;
	if ( opts.mode == MODE_AST ) {
		xrtInit();
	}
	
	/* 执行对应模式 */
	switch ( opts.mode ) {
		case MODE_LEX:
			iResult = RunLexMode(&opts);
			break;
		case MODE_AST:
			iResult = RunAstMode(&opts);
			break;
	}
	
	/* 清理 */
	if ( opts.mode == MODE_AST ) {
		xrtUnit();
	}
	
	return iResult;
}
