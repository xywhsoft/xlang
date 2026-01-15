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
#include "src/xl_tcc.h"




// ============================================================================
// 运行模式
// ============================================================================

// 编译器类型枚举
typedef enum XL_Compiler {
	XL_COMPILER_TCC = 0,    // 默认：TCC
	XL_COMPILER_GCC,        // GCC
	XL_COMPILER_CLANG,      // Clang
	XL_COMPILER_VC          // Visual C++
} XL_Compiler;


typedef enum XL_Mode {
	XL_MODE_RUN = 0,        // 默认：TCC 运行
	XL_MODE_COMPILE,        // 编译为 EXE
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
	XL_Compiler compiler;   // 编译器类型（仅编译模式有效）
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
	printf("  -r, --run              TCC 运行模式（默认）\n");
	printf("  -c, --compile          编译为 EXE（默认使用 TCC）\n");
	printf("  -c=<编译器>            指定编译器（tcc/gcc/clang/vc）\n");
	printf("  --compile=<编译器>     同上\n");
	printf("  --dump-tokens          打印 Token 列表\n");
	printf("  --dump-ast             打印 AST 结构\n");
	printf("  --dump-sem             打印语义信息（符号表、类型）\n");
	printf("  --emit-c               生成 C 代码\n");
	printf("  -o <文件>              指定输出文件\n");
	printf("  -h, --help             显示帮助信息\n");
	printf("  -v, --version          显示版本信息\n");
	printf("\n");
	printf("编译器选项:\n");
	printf("  tcc    - Tiny C Compiler（默认，快速编译）\n");
	printf("  gcc    - GNU C Compiler（优化编译）\n");
	printf("  clang  - LLVM Clang\n");
	printf("  vc     - Visual C++（仅 Windows）\n");
	printf("\n");
	printf("示例:\n");
	printf("  xlang hello.xl              TCC 运行 hello.xl\n");
	printf("  xlang -r hello.xl           同上\n");
	printf("  xlang -c hello.xl           TCC 编译为 EXE\n");
	printf("  xlang -c=gcc hello.xl       GCC 编译为 EXE\n");
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

// 解析编译器名称
static XL_Compiler xlParseCompiler( const char* name )
{
	if ( (strcmp(name, "tcc") == 0) || (strcmp(name, "TCC") == 0) ) {
		return XL_COMPILER_TCC;
	}
	if ( (strcmp(name, "gcc") == 0) || (strcmp(name, "GCC") == 0) ) {
		return XL_COMPILER_GCC;
	}
	if ( (strcmp(name, "clang") == 0) || (strcmp(name, "CLANG") == 0) || (strcmp(name, "Clang") == 0) ) {
		return XL_COMPILER_CLANG;
	}
	if ( (strcmp(name, "vc") == 0) || (strcmp(name, "VC") == 0) || (strcmp(name, "msvc") == 0) || (strcmp(name, "MSVC") == 0) ) {
		return XL_COMPILER_VC;
	}
	fprintf(stderr, "警告: 未知编译器 '%s'，使用默认 TCC\n", name);
	return XL_COMPILER_TCC;
}



static XL_Config xlParseArgs( int argc, char* argv[] )
{
	XL_Config config = {0};
	config.mode = XL_MODE_RUN;
	config.compiler = XL_COMPILER_TCC;

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

		if ( (strcmp(arg, "-r") == 0) || (strcmp(arg, "--run") == 0) ) {
			config.mode = XL_MODE_RUN;
			continue;
		}

		// -c 或 --compile（默认 TCC）
		if ( (strcmp(arg, "-c") == 0) || (strcmp(arg, "--compile") == 0) ) {
			config.mode = XL_MODE_COMPILE;
			continue;
		}

		// -c=xxx 或 --compile=xxx
		if ( strncmp(arg, "-c=", 3) == 0 ) {
			config.mode = XL_MODE_COMPILE;
			config.compiler = xlParseCompiler(arg + 3);
			continue;
		}
		if ( strncmp(arg, "--compile=", 10) == 0 ) {
			config.mode = XL_MODE_COMPILE;
			config.compiler = xlParseCompiler(arg + 10);
			continue;
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
			// TCC 运行模式
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
										// 使用 TCC 运行
										iExitCode = xlRunWithTCC(pszCode, config.pszInputFile);
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

		case XL_MODE_COMPILE: {
			// 编译为 EXE
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
										// 生成路径：AppPath 下的 gen 目录
										str sAppDir = (str)xCore.AppPath;
										
										// 确保 gen 目录存在
										str sGenDir = xrtPathJoin(2, sAppDir, "gen");
										if ( !xrtDirExists(sGenDir) ) {
											xrtDirCreate(sGenDir);
										}
										
										// 生成临时文件和输出文件路径
										#ifdef _WIN32
										str sTmpFile = xrtPathJoin(2, sGenDir, "xlang_temp.c");
										str sDefaultExe = xrtPathJoin(2, sGenDir, "xlang_temp.exe");
										#else
										str sTmpFile = xrtPathJoin(2, sGenDir, "xlang_temp.c");
										str sDefaultExe = xrtPathJoin(2, sGenDir, "xlang_temp");
										#endif
										
										// xrt.c 路径：使用 tcc/xrt/xrt.c
										str sXrtPath = xrtPathJoin(4, sAppDir, "tcc", "xrt", "xrt.c");
										if ( !xrtFileExists(sXrtPath) ) {
											fprintf(stderr, "错误: 找不到运行时文件 %s\n", sXrtPath);
											xrtFree(sXrtPath);
											xrtFree(sGenDir);
											xrtFree(sTmpFile);
											xrtFree(sDefaultExe);
											free(pszCode);
											xlCodegenDestroy(pGen);
											xlSemanticDestroy(pSem);
											xlParserDestroy(pParser);
											xlLexerDestroy(pLexer);
											free(pszSource);
											xrtUnit();
											return 1;
										}
										
										// include 路径：使用 tcc 目录
										str sIncludeDir = xrtPathJoin(2, sAppDir, "tcc");

										// 输出文件
										str sOutFile = config.pszOutputFile ? (str)config.pszOutputFile : sDefaultExe;

										FILE *fp = fopen(sTmpFile, "w");
										if ( fp ) {
											fputs(pszCode, fp);
											fclose(fp);

											char szCmd[4096];
											switch ( config.compiler ) {
												case XL_COMPILER_TCC:
													#ifdef _WIN32
													snprintf(szCmd, sizeof(szCmd),
														"tcc \"%s\" \"%s\" -I\"%s\" -lshlwapi -lgdi32 -lws2_32 -lIPHLPAPI -o \"%s\"",
														sTmpFile, sXrtPath, sIncludeDir, sOutFile);
													#else
													snprintf(szCmd, sizeof(szCmd),
														"tcc \"%s\" \"%s\" -I\"%s\" -o \"%s\"",
														sTmpFile, sXrtPath, sIncludeDir, sOutFile);
													#endif
													break;
												case XL_COMPILER_GCC:
													#ifdef _WIN32
													snprintf(szCmd, sizeof(szCmd),
														"gcc \"%s\" \"%s\" -I\"%s\" -lshlwapi -lgdi32 -lws2_32 -lIPHLPAPI -o \"%s\"",
														sTmpFile, sXrtPath, sIncludeDir, sOutFile);
													#else
													snprintf(szCmd, sizeof(szCmd),
														"gcc \"%s\" \"%s\" -I\"%s\" -o \"%s\"",
														sTmpFile, sXrtPath, sIncludeDir, sOutFile);
													#endif
													break;
												case XL_COMPILER_CLANG:
													#ifdef _WIN32
													snprintf(szCmd, sizeof(szCmd),
														"clang \"%s\" \"%s\" -I\"%s\" -lshlwapi -lgdi32 -lws2_32 -lIPHLPAPI -o \"%s\"",
														sTmpFile, sXrtPath, sIncludeDir, sOutFile);
													#else
													snprintf(szCmd, sizeof(szCmd),
														"clang \"%s\" \"%s\" -I\"%s\" -o \"%s\"",
														sTmpFile, sXrtPath, sIncludeDir, sOutFile);
													#endif
													break;
												case XL_COMPILER_VC:
													#ifdef _WIN32
													snprintf(szCmd, sizeof(szCmd),
														"cl /Fe\"%s\" \"%s\" \"%s\" /I\"%s\" shlwapi.lib gdi32.lib ws2_32.lib iphlpapi.lib",
														sOutFile, sTmpFile, sXrtPath, sIncludeDir);
													#else
													fprintf(stderr, "错误: VC 编译器仅支持 Windows\n");
													iExitCode = 1;
													szCmd[0] = '\0';  // 清空命令
													#endif
													break;
											}

											if ( szCmd[0] != '\0' ) {
												iExitCode = system(szCmd);
												if ( iExitCode == 0 ) {
													printf("编译成功: %s\n", sOutFile);
												}
											}
										} else {
											fprintf(stderr, "错误: 无法创建临时文件 %s\n", sTmpFile);
											iExitCode = 1;
										}

										xrtFree(sTmpFile);
										xrtFree(sDefaultExe);
										xrtFree(sXrtPath);
										xrtFree(sGenDir);
										xrtFree(sIncludeDir);
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
