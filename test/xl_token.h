/*
 * xlang - Token 定义头文件
 * 
 * 这个文件定义了词法分析器和语法分析器共用的 token 结构
 */

#ifndef XL_TOKEN_H
#define XL_TOKEN_H

// Token 结构体 - 用于在词法分析器和语法分析器之间传递信息
typedef struct {
    int type;           // token 类型
    char* text;         // token 原始文本
    int len;            // 文本长度
    int line;           // 行号
    int col;            // 列号
    union {
        long long ival;     // 整数值
        double fval;        // 浮点数值
        char* sval;         // 字符串值 (requires free)
    };
} XL_Token;

// Token 类型映射宏 - 使用 TK_ 前缀避免与 C 关键字冲突
// 实际值由 xl_parser.h 定义，这里只是转发

// 关键字 tokens - 这些宏将在 include xl_parser.h 后定义
// xl.l 直接使用 xl_parser.h 中的定义

// 特殊 token
#define TK_EOF 0

#endif // XL_TOKEN_H
