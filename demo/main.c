/**
 * xlang - X Language Compiler/Interpreter
 * 
 * xlang 是一种脚本+编译式语言，基于 C 语言开发。
 * 它可以像脚本语言一样解释执行（依赖 libtcc），
 * 也可以编译为独立的可执行程序。
 */

// ============================================================================
// 预编译定义
// ============================================================================

#define XRT_IMPLEMENT

// ============================================================================
// 标准库引用
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// 第三方库引用
// ============================================================================

#include "lib/xrt/xrt.h"
#include "lib/libtcc.h"

// ============================================================================
// xlang 模块引用
// ============================================================================

#include "src/xl_common.h"
#include "src/xl_error.h"
#include "src/xl_lexer.h"
#include "src/xl_ast.h"
#include "src/xl_parser.h"
#include "src/xl_semantic.h"
#include "src/xl_codegen.h"
#include "src/xl_backend.h"




// ============================================================================
// 运行模式
// ============================================================================

typedef enum XL_Mode {
	XL_MODE_RUN = 0,        // 默认：编译并运行
	XL_MODE_DUMP_TOKENS,    // --dump-tokens
	XL_MODE_DUMP_AST,       // --dump-ast
	XL_MODE_DUMP_SEM,       // --dump-sem
	XL_MODE_EMIT_C,         // --emit-c
	XL_MODE_HELP,           // --help
	XL_MODE_VERSION         // --version
} XL_Mode;


// 命令行配置
typedef struct XL_Config {
	XL_Mode mode;           // 运行模式
	str pszInputFile;       // 输入文件
	str pszOutputFile;      // 输出文件（--emit-c 或 -o）
} XL_Config;




// ============================================================================
// 帮助与版本信息
// ============================================================================

static void xlPrintUsage( void )
{
	printf("xlang - X Language Compiler/Interpreter v0.3.0\n");
	printf("\n");
	printf("用法: xlang [选项] <文件.xl>\n");
	printf("\n");
	printf("选项:\n");
	printf("  --dump-tokens     打印 Token 列表\n");
	printf("  --dump-ast        打印 AST 结构\n");
	printf("  --dump-sem        打印语义信息（符号表、类型）\n");
	printf("  --emit-c          生成 C 代码\n");
	printf("  --run             编译并运行（默认）\n");
	printf("  -o <文件>         指定输出文件\n");
	printf("  -h, --help        显示帮助信息\n");
	printf("  -v, --version     显示版本信息\n");
	printf("\n");
	printf("示例:\n");
	printf("  xlang hello.xl                运行 hello.xl\n");
	printf("  xlang --dump-tokens hello.xl  打印 Token 列表\n");
	printf("  xlang --emit-c -o out.c hello.xl  生成 C 代码到 out.c\n");
}


static void xlPrintVersion( void )
{
	printf("xlang v0.3.0 (xlang 第三版 Demo)\n");
	printf("基于 xrt 运行时库和 libtcc 编译器\n");
}




// ============================================================================
// 命令行参数解析
// ============================================================================

static XL_Config xlParseArgs( int argc, char* argv[] )
{
	XL_Config config = {0};
	config.mode = XL_MODE_RUN;

	for ( int i = 1; i < argc; i++ ) {
		char* arg = argv[i];

		if ( (strcmp(arg, "-h") == 0) || (strcmp(arg, "--help") == 0) ) {
			config.mode = XL_MODE_HELP;
			return config;
		}

		if ( (strcmp(arg, "-v") == 0) || (strcmp(arg, "--version") == 0) ) {
			config.mode = XL_MODE_VERSION;
			return config;
		}

		if ( strcmp(arg, "--dump-tokens") == 0 ) {
			config.mode = XL_MODE_DUMP_TOKENS;
			continue;
		}

		if ( strcmp(arg, "--dump-ast") == 0 ) {
			config.mode = XL_MODE_DUMP_AST;
			continue;
		}

		if ( strcmp(arg, "--dump-sem") == 0 ) {
			config.mode = XL_MODE_DUMP_SEM;
			continue;
		}

		if ( strcmp(arg, "--emit-c") == 0 ) {
			config.mode = XL_MODE_EMIT_C;
			continue;
		}

		if ( strcmp(arg, "--run") == 0 ) {
			config.mode = XL_MODE_RUN;
			continue;
		}

		if ( strcmp(arg, "-o") == 0 ) {
			if ( i + 1 < argc ) {
				config.pszOutputFile = (str)argv[++i];
			} else {
				fprintf(stderr, "错误: -o 选项需要指定输出文件\n");
			}
			continue;
		}

		// 输入文件
		if ( arg[0] != '-' ) {
			config.pszInputFile = (str)arg;
			continue;
		}

		fprintf(stderr, "未知选项: %s\n", arg);
	}

	return config;
}




// ============================================================================
// 读取文件内容
// ============================================================================

static str xlReadFile( str pszPath, size_t *pSize )
{
	FILE *fp = fopen((char*)pszPath, "rb");
	if ( !fp ) {
		fprintf(stderr, "无法打开文件: %s\n", (char*)pszPath);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	size_t iSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	str pszContent = (str)malloc(iSize + 1);
	if ( !pszContent ) {
		fclose(fp);
		return NULL;
	}

	fread(pszContent, 1, iSize, fp);
	pszContent[iSize] = '\0';
	fclose(fp);

	if ( pSize ) *pSize = iSize;
	return pszContent;
}




// ============================================================================
// 主函数
// ============================================================================

int main( int argc, char* argv[] )
{
	// Windows 平台设置控制台输出为 UTF-8
	#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	#endif

	// 初始化 xrt 运行时
	xrtInit();

	// 解析命令行参数
	XL_Config config = xlParseArgs(argc, argv);

	// 处理帮助和版本
	if ( config.mode == XL_MODE_HELP ) {
		xlPrintUsage();
		xrtUnit();
		return 0;
	}

	if ( config.mode == XL_MODE_VERSION ) {
		xlPrintVersion();
		xrtUnit();
		return 0;
	}

	// 检查输入文件
	if ( !config.pszInputFile ) {
		fprintf(stderr, "错误: 未指定输入文件\n");
		fprintf(stderr, "使用 'xlang --help' 查看帮助\n");
		xrtUnit();
		return 1;
	}

	// 读取源文件
	size_t iSize = 0;
	str pszSource = xlReadFile(config.pszInputFile, &iSize);
	if ( !pszSource ) {
		xrtUnit();
		return 1;
	}

	int iExitCode = 0;

	// 根据模式执行
	switch ( config.mode ) {
		case XL_MODE_DUMP_TOKENS: {
			// 词法分析
			XL_Lexer *pLexer = xlLexerCreate(pszSource, iSize, config.pszInputFile);
			if ( pLexer ) {
				xlLexerDumpTokens(pLexer);
				xlLexerDestroy(pLexer);
			} else {
				fprintf(stderr, "错误: 创建词法分析器失败\n");
				iExitCode = 1;
			}
			break;
		}

		case XL_MODE_DUMP_AST: {
			// 语法分析
			XL_Lexer *pLexer = xlLexerCreate(pszSource, iSize, config.pszInputFile);
			if ( pLexer ) {
				XL_Parser *pParser = xlParserCreate(pLexer);
				if ( pParser ) {
					xvalue xModule = xlParserParse(pParser, config.pszInputFile);
					if ( xModule && !pParser->bHadError ) {
						xlAstPrint(xModule);
					} else {
						xlErrorListPrint(pParser->pErrors);
						iExitCode = 1;
					}
					// xvalue 由 xrt 管理内存
					xlParserDestroy(pParser);
				} else {
					fprintf(stderr, "错误: 创建语法分析器失败\n");
					iExitCode = 1;
				}
				xlLexerDestroy(pLexer);
			} else {
				fprintf(stderr, "错误: 创建词法分析器失败\n");
				iExitCode = 1;
			}
			break;
		}

		case XL_MODE_DUMP_SEM: {
			// 语义分析
			XL_Lexer *pLexer = xlLexerCreate(pszSource, iSize, config.pszInputFile);
			if ( pLexer ) {
				XL_Parser *pParser = xlParserCreate(pLexer);
				if ( pParser ) {
					xvalue xModule = xlParserParse(pParser, config.pszInputFile);
					if ( xModule && !pParser->bHadError ) {
						XL_Semantic *pSem = xlSemanticCreate(pParser->pErrors);
						if ( pSem ) {
							bool bOk = xlSemanticAnalyze(pSem, xModule);
							if ( bOk ) {
								printf("=== 语义分析通过 ===\n");
								xlAstPrint(xModule);
							} else {
								xlErrorListPrint(pSem->pErrors);
								iExitCode = 1;
							}
							xlSemanticDestroy(pSem);
						} else {
							fprintf(stderr, "错误: 创建语义分析器失败\n");
							iExitCode = 1;
						}
					} else {
						xlErrorListPrint(pParser->pErrors);
						iExitCode = 1;
					}
					xlParserDestroy(pParser);
				} else {
					fprintf(stderr, "错误: 创建语法分析器失败\n");
					iExitCode = 1;
				}
				xlLexerDestroy(pLexer);
			} else {
				fprintf(stderr, "错误: 创建词法分析器失败\n");
				iExitCode = 1;
			}
			break;
		}

		case XL_MODE_EMIT_C: {
			// 代码生成
			XL_Lexer *pLexer = xlLexerCreate(pszSource, iSize, config.pszInputFile);
			if ( pLexer ) {
				XL_Parser *pParser = xlParserCreate(pLexer);
				if ( pParser ) {
					xvalue xModule = xlParserParse(pParser, config.pszInputFile);
					if ( xModule && !pParser->bHadError ) {
						XL_Semantic *pSem = xlSemanticCreate(pParser->pErrors);
						if ( pSem ) {
							bool bOk = xlSemanticAnalyze(pSem, xModule);
							if ( bOk ) {
								XL_Codegen *pGen = xlCodegenCreate(pParser->pErrors);
								if ( pGen ) {
									xlCodegenGenerate(pGen, xModule);
									char *pszCode = xlCodegenGetCode(pGen);
									if ( pszCode ) {
										if ( config.pszOutputFile ) {
											// 写入文件
											FILE *fp = fopen((char*)config.pszOutputFile, "w");
											if ( fp ) {
												fputs(pszCode, fp);
												fclose(fp);
												printf("C 代码已生成: %s\n", (char*)config.pszOutputFile);
											} else {
												fprintf(stderr, "错误: 无法创建输出文件 %s\n", (char*)config.pszOutputFile);
												iExitCode = 1;
											}
										} else {
											// 输出到标准输出
											printf("%s", pszCode);
										}
										free(pszCode);
									}
									xlCodegenDestroy(pGen);
								} else {
									fprintf(stderr, "错误: 创建代码生成器失败\n");
									iExitCode = 1;
								}
							} else {
								xlErrorListPrint(pSem->pErrors);
								iExitCode = 1;
							}
							xlSemanticDestroy(pSem);
						} else {
							fprintf(stderr, "错误: 创建语义分析器失败\n");
							iExitCode = 1;
						}
					} else {
						xlErrorListPrint(pParser->pErrors);
						iExitCode = 1;
					}
					xlParserDestroy(pParser);
				} else {
					fprintf(stderr, "错误: 创建语法分析器失败\n");
					iExitCode = 1;
				}
				xlLexerDestroy(pLexer);
			} else {
				fprintf(stderr, "错误: 创建词法分析器失败\n");
				iExitCode = 1;
			}
			break;
		}

		case XL_MODE_RUN: {
			// 完整编译运行
			XL_Lexer *pLexer = xlLexerCreate(pszSource, iSize, config.pszInputFile);
			if ( pLexer ) {
				XL_Parser *pParser = xlParserCreate(pLexer);
				if ( pParser ) {
					xvalue xModule = xlParserParse(pParser, config.pszInputFile);
					if ( xModule && !pParser->bHadError ) {
						XL_Semantic *pSem = xlSemanticCreate(pParser->pErrors);
						if ( pSem ) {
							bool bOk = xlSemanticAnalyze(pSem, xModule);
							if ( bOk ) {
								XL_Codegen *pGen = xlCodegenCreate(pParser->pErrors);
								if ( pGen ) {
									xlCodegenGenerate(pGen, xModule);
									char *pszCode = xlCodegenGetCode(pGen);
									if ( pszCode ) {
										// 使用 xrt 路径函数获取程序所在目录
										str sExeDir = xrtPathGetDir((str)xCore.AppPath, 0);
										
										// 如果在 release 子目录，向上一级到项目根目录
										if ( strstr(sExeDir, "release") ) {
											str sParent = xrtPathGetDir(sExeDir, 0);
											xrtFree(sExeDir);
											sExeDir = sParent;
										}

										// 使用 xrtPathJoin 生成路径（自动处理跨平台分隔符）
										#ifdef _WIN32
										str sTmpFile = xrtPathJoin(3, sExeDir, "gen", "xlang_temp.c");
										str sTmpExe = xrtPathJoin(3, sExeDir, "gen", "xlang_temp.exe");
										#else
										str sTmpFile = xrtPathJoin(3, sExeDir, "gen", "xlang_temp.c");
										str sTmpExe = xrtPathJoin(3, sExeDir, "gen", "xlang_temp");
										#endif
										str sXrtPath = xrtPathJoin(4, sExeDir, "lib", "xrt", "xrt.c");

										FILE *fp = fopen(sTmpFile, "w");
										if ( fp ) {
											fputs(pszCode, fp);
											fclose(fp);

											// 使用外部编译器编译运行
											#ifdef _WIN32
											char szCmd[2048];
											snprintf(szCmd, sizeof(szCmd),
											         "gcc \"%s\" \"%s\" -I\"%s\" -lshlwapi -lgdi32 -lws2_32 -lIPHLPAPI -o \"%s\" && \"%s\"",
											         sTmpFile, sXrtPath, sExeDir, sTmpExe, sTmpExe);
											iExitCode = system(szCmd);
											#else
											char szCmd[2048];
											snprintf(szCmd, sizeof(szCmd),
											         "gcc \"%s\" \"%s\" -I\"%s\" -o \"%s\" && \"%s\"",
											         sTmpFile, sXrtPath, sExeDir, sTmpExe, sTmpExe);
											iExitCode = system(szCmd);
											#endif
										} else {
											fprintf(stderr, "错误: 无法创建临时文件 %s\n", sTmpFile);
											iExitCode = 1;
										}
										xrtFree(sTmpFile);
										xrtFree(sTmpExe);
										xrtFree(sXrtPath);
										xrtFree(sExeDir);
										free(pszCode);
									}
									xlCodegenDestroy(pGen);
								} else {
									fprintf(stderr, "错误: 创建代码生成器失败\n");
									iExitCode = 1;
								}
							} else {
								xlErrorListPrint(pSem->pErrors);
								iExitCode = 1;
							}
							xlSemanticDestroy(pSem);
						} else {
							fprintf(stderr, "错误: 创建语义分析器失败\n");
							iExitCode = 1;
						}
					} else {
						xlErrorListPrint(pParser->pErrors);
						iExitCode = 1;
					}
					xlParserDestroy(pParser);
				} else {
					fprintf(stderr, "错误: 创建语法分析器失败\n");
					iExitCode = 1;
				}
				xlLexerDestroy(pLexer);
			} else {
				fprintf(stderr, "错误: 创建词法分析器失败\n");
				iExitCode = 1;
			}
			break;
		}

		default:
			break;
	}

	// 释放源文件
	free(pszSource);

	// 释放 xrt 运行时
	xrtUnit();

	return iExitCode;
}
