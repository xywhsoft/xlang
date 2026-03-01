/**
 * xlang v4 词法分析工具
 * 
 * 用法: xllex [选项] <文件>
 * 
 * 选项:
 *   -o <文件>     输出到文件（默认标准输出）
 *   -j            输出 JSON 格式
 *   -v            显示详细信息
 *   -h            显示帮助
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Flex 生成的实现（包含类型定义和所有函数） */
#include "src/lexer/xl_lexer_impl.h"

/* Lexer 高级 API */
#include "src/lexer/xl_lexer_api.h"



/* === 命令行选项 === */
typedef struct Options {
	const char* sInputFile;
	const char* sOutputFile;
	int bJsonOutput;
	int bVerbose;
	int bHelp;
} Options;



/* === 帮助信息 === */
static void PrintHelp(const char* sProgName)
{
	printf("xlang v4 词法分析工具\n\n");
	printf("用法: %s [选项] <文件>\n\n", sProgName);
	printf("选项:\n");
	printf("  -o <文件>     输出到文件（默认标准输出）\n");
	printf("  -j            输出 JSON 格式\n");
	printf("  -v            显示详细信息\n");
	printf("  -h            显示帮助\n");
	printf("\n示例:\n");
	printf("  %s test.xl           扫描 test.xl 并输出 Token 列表\n", sProgName);
	printf("  %s -j test.xl        输出 JSON 格式的 Token 列表\n", sProgName);
}


/* === 解析命令行参数 === */
static int ParseArgs(int argc, char** argv, Options* pOpts)
{
	memset(pOpts, 0, sizeof(Options));
	
	for ( int i = 1; i < argc; i++ ) {
		if ( strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ) {
			pOpts->bHelp = 1;
		} else if ( strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--json") == 0 ) {
			pOpts->bJsonOutput = 1;
		} else if ( strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0 ) {
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



/* === 主程序 === */
int main(int argc, char** argv)
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
	
	/* 重定向输出 */
	FILE* fpOutput = stdout;
	if ( opts.sOutputFile ) {
		fpOutput = fopen(opts.sOutputFile, "w");
		if ( !fpOutput ) {
			fprintf(stderr, "错误: 无法打开输出文件 %s\n", opts.sOutputFile);
			return 1;
		}
	}
	
	/* 详细模式 */
	if ( opts.bVerbose ) {
		fprintf(stderr, "输入文件: %s\n", opts.sInputFile);
		if ( opts.sOutputFile ) {
			fprintf(stderr, "输出文件: %s\n", opts.sOutputFile);
		}
		fprintf(stderr, "输出格式: %s\n", opts.bJsonOutput ? "JSON" : "文本");
		fprintf(stderr, "\n");
	}
	
	/* 创建 Lexer */
	XL_Lexer* pLexer = xlLexerCreateFromFile(opts.sInputFile);
	if ( !pLexer ) {
		fprintf(stderr, "错误: 无法打开文件 %s\n", opts.sInputFile);
		if ( fpOutput != stdout ) {
			fclose(fpOutput);
		}
		return 1;
	}
	
	/* 输出 Token */
	if ( opts.bJsonOutput ) {
		xlLexerDumpTokensJson(pLexer);
	} else {
		xlLexerDumpTokens(pLexer);
	}
	
	/* 清理 */
	xlLexerDestroy(pLexer);
	
	if ( fpOutput != stdout ) {
		fclose(fpOutput);
	}
	
	return 0;
}
