/*
 * xlang - 主程序入口
 * 
 * 用法: xlang [选项] [脚本文件]
 *   -e <代码>    执行一段代码
 *   -i           交互模式 (REPL)
 *   -h           显示帮助
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xl_token.h"
#include "xl_ast.h"

// 词法分析器和语法分析器声明
typedef void* yyscan_t;
extern int xl_lex_init(yyscan_t* scanner);
extern int xl_lex_destroy(yyscan_t scanner);
extern void xl__scan_string(const char* str, yyscan_t scanner);
extern void xl__switch_to_buffer(void* buffer, yyscan_t scanner);
extern void* xl__scan_buffer(char* base, size_t size, yyscan_t scanner);
extern int xl_lex(XL_Token** lval, yyscan_t scanner);
extern void xl__delete_buffer(void* buffer, yyscan_t scanner);
extern int xl_get_lineno(yyscan_t scanner);

// Lemon 解析器声明
void* ParseAlloc(void* (*mallocProc)(size_t));
void ParseFree(void* parser, void (*freeProc)(void*));
void Parse(void* parser, int tokenType, XL_Token* token, XL_Node** result);

// 解析结果获取
extern XL_Node* xl_get_parse_result(void);
extern int xl_get_parse_error(void);

// 读取文件内容
static char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 2);
    if (!content) {
        fclose(f);
        return NULL;
    }
    
    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';
    content[read_size + 1] = '\0';  // flex 需要双 NUL 结尾
    fclose(f);
    
    return content;
}

// 解析源代码
static XL_Node* parse_source(const char* source) {
    yyscan_t scanner;
    xl_lex_init(&scanner);
    
    // 需要为 flex 提供额外的结尾字符
    size_t len = strlen(source);
    char* buf = (char*)malloc(len + 2);
    memcpy(buf, source, len);
    buf[len] = 0;
    buf[len + 1] = 0;
    
    xl__scan_buffer(buf, len + 2, scanner);
    
    void* parser = ParseAlloc(malloc);
    XL_Node* result = NULL;
    XL_Token* token = NULL;
    int tok;
    
    while ((tok = xl_lex(&token, scanner)) > 0) {
        // 创建 token 副本（简化处理，lemon 会使用）
        Parse(parser, tok, token, &result);
        
        if (xl_get_parse_error()) {
            fprintf(stderr, "Parse error at line %d\n", xl_get_lineno(scanner));
            break;
        }
    }
    
    // 发送结束标记
    Parse(parser, 0, NULL, &result);
    
    ParseFree(parser, free);
    xl_lex_destroy(scanner);
    free(buf);
    
    return result ? result : xl_get_parse_result();
}

// 执行源代码
static int run_source(const char* source) {
    XL_Node* program = parse_source(source);
    if (!program) {
        return 1;
    }
    
    // 创建执行上下文
    XL_Context* ctx = xl_context_new();
    xl_register_builtins(ctx);
    
    // 执行程序
    XL_Value* result = xl_exec_program(ctx, program);
    
    // 清理
    xl_value_unref(result);
    xl_node_free(program);
    xl_context_free(ctx);
    
    return 0;
}

// 执行文件
static int run_file(const char* filename) {
    char* source = read_file(filename);
    if (!source) {
        return 1;
    }
    
    int result = run_source(source);
    free(source);
    return result;
}

// 交互模式 (REPL)
static void run_repl(void) {
    printf("xlang REPL - Type 'exit' to quit\n");
    printf(">>> ");
    
    XL_Context* ctx = xl_context_new();
    xl_register_builtins(ctx);
    
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        // 去除换行符
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        // 退出命令
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        
        // 空行跳过
        if (line[0] == '\0') {
            printf(">>> ");
            continue;
        }
        
        // 解析执行
        XL_Node* program = parse_source(line);
        if (program) {
            XL_Value* result = xl_exec_program(ctx, program);
            
            // 显示结果（如果不是 null）
            if (result && result->type != VAL_NULL) {
                char* s = xl_to_string(result);
                printf("= %s\n", s);
                free(s);
            }
            
            xl_value_unref(result);
            xl_node_free(program);
        }
        
        printf(">>> ");
    }
    
    printf("\nBye!\n");
    xl_context_free(ctx);
}

// 显示帮助
static void show_help(const char* prog) {
    printf("xlang - A Simple Scripting Language\n\n");
    printf("Usage: %s [options] [script_file]\n\n", prog);
    printf("Options:\n");
    printf("  -e <code>    Execute a code string\n");
    printf("  -i           Start interactive mode (REPL)\n");
    printf("  -h, --help   Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s script.xl         Run a script file\n", prog);
    printf("  %s -e \"print(1+2)\"   Execute code directly\n", prog);
    printf("  %s -i                Start REPL\n", prog);
}

// 主函数
int main(int argc, char* argv[]) {
    if (argc < 2) {
        // 无参数，进入交互模式
        run_repl();
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-i") == 0) {
            run_repl();
            return 0;
        }
        else if (strcmp(argv[i], "-e") == 0) {
            if (i + 1 < argc) {
                return run_source(argv[++i]);
            } else {
                fprintf(stderr, "Error: -e requires an argument\n");
                return 1;
            }
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            show_help(argv[0]);
            return 1;
        }
        else {
            // 假设是脚本文件
            return run_file(argv[i]);
        }
    }
    
    return 0;
}
