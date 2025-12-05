/*
 * xlang - AST 节点定义和运行时结构
 */

#ifndef XL_AST_H
#define XL_AST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// 前置声明
typedef struct XL_Node XL_Node;
typedef struct XL_Value XL_Value;
typedef struct XL_Context XL_Context;

// ==================== AST 节点类型 ====================
typedef enum {
    // 程序结构
    NODE_PROGRAM,           // 程序（语句列表）
    NODE_BLOCK,             // 代码块
    
    // 语句
    NODE_VAR_DECL,          // 变量声明
    NODE_EXPR_STMT,         // 表达式语句
    NODE_IF_STMT,           // if 语句
    NODE_WHILE_STMT,        // while 循环
    NODE_FOR_STMT,          // for 循环
    NODE_BREAK_STMT,        // break 语句
    NODE_CONTINUE_STMT,     // continue 语句
    NODE_RETURN_STMT,       // return 语句
    NODE_FUNC_DEF,          // 函数定义
    
    // 表达式
    NODE_BINARY_OP,         // 二元运算
    NODE_UNARY_OP,          // 一元运算
    NODE_ASSIGN,            // 赋值
    NODE_CALL,              // 函数调用
    NODE_INDEX,             // 数组索引
    NODE_MEMBER,            // 成员访问
    
    // 字面量
    NODE_NULL,              // null
    NODE_BOOL,              // 布尔值
    NODE_INT,               // 整数
    NODE_FLOAT,             // 浮点数
    NODE_STRING,            // 字符串
    NODE_IDENT,             // 标识符
    NODE_ARRAY,             // 数组字面量
    NODE_TABLE              // 表字面量
} XL_NodeType;

// 运算符类型
typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_AND, OP_OR, OP_NOT,
    OP_NEG,                 // 负号
    OP_ASSIGN, OP_ADDASSIGN, OP_SUBASSIGN, OP_MULASSIGN, OP_DIVASSIGN,
    OP_INC, OP_DEC          // ++, --
} XL_OpType;

// ==================== AST 节点结构 ====================
struct XL_Node {
    XL_NodeType type;
    int line;               // 行号（调试用）
    
    union {
        // 程序/代码块
        struct {
            XL_Node** stmts;    // 语句数组
            int count;          // 语句数量
            int capacity;       // 容量
        } block;
        
        // 变量声明: var name = value
        struct {
            char* name;
            XL_Node* init;      // 初始值表达式（可为 NULL）
        } var_decl;
        
        // 二元运算
        struct {
            XL_OpType op;
            XL_Node* left;
            XL_Node* right;
        } binary;
        
        // 一元运算
        struct {
            XL_OpType op;
            XL_Node* operand;
            bool prefix;        // 前缀还是后缀
        } unary;
        
        // 赋值
        struct {
            XL_OpType op;       // =, +=, -=, *=, /=
            XL_Node* target;
            XL_Node* value;
        } assign;
        
        // if 语句
        struct {
            XL_Node* cond;
            XL_Node* then_block;
            XL_Node** elseif_conds;
            XL_Node** elseif_blocks;
            int elseif_count;
            XL_Node* else_block;
        } if_stmt;
        
        // while 语句
        struct {
            XL_Node* cond;
            XL_Node* body;
        } while_stmt;
        
        // for 语句: for i = start to end [step s]
        struct {
            char* var;
            XL_Node* start;
            XL_Node* end;
            XL_Node* step;      // 可为 NULL
            XL_Node* body;
        } for_stmt;
        
        // return 语句
        struct {
            XL_Node* value;     // 可为 NULL
        } return_stmt;
        
        // 函数定义
        struct {
            char* name;
            char** params;      // 参数名数组
            int param_count;
            XL_Node* body;
        } func_def;
        
        // 函数调用
        struct {
            XL_Node* func;      // 函数表达式
            XL_Node** args;     // 参数数组
            int arg_count;
        } call;
        
        // 数组索引
        struct {
            XL_Node* array;
            XL_Node* index;
        } index;
        
        // 成员访问
        struct {
            XL_Node* object;
            char* member;
        } member;
        
        // 字面量
        long long int_val;
        double float_val;
        bool bool_val;
        char* str_val;          // 字符串或标识符名
        
        // 数组字面量
        struct {
            XL_Node** elements;
            int count;
        } array;
        
        // 表字面量
        struct {
            char** keys;
            XL_Node** values;
            int count;
        } table;
    };
};

// ==================== 运行时值类型 ====================
typedef enum {
    VAL_NULL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_ARRAY,
    VAL_TABLE,
    VAL_FUNCTION,
    VAL_BUILTIN          // 内置函数
} XL_ValueType;

// 运行时值
struct XL_Value {
    XL_ValueType type;
    int refcount;           // 引用计数
    
    union {
        bool bool_val;
        long long int_val;
        double float_val;
        char* str_val;
        
        // 数组
        struct {
            XL_Value** elements;
            int count;
            int capacity;
        } array;
        
        // 表
        struct {
            char** keys;
            XL_Value** values;
            int count;
            int capacity;
        } table;
        
        // 函数
        struct {
            XL_Node* node;          // 函数定义节点
            XL_Context* closure;    // 闭包环境
        } func;
        
        // 内置函数
        struct {
            const char* name;
            XL_Value* (*fn)(XL_Context*, XL_Value**, int);
        } builtin;
    };
};

// ==================== 变量和作用域 ====================
typedef struct XL_Var {
    char* name;
    XL_Value* value;
    struct XL_Var* next;
} XL_Var;

typedef struct XL_Scope {
    XL_Var* vars;
    struct XL_Scope* parent;
} XL_Scope;

// 执行上下文
struct XL_Context {
    XL_Scope* scope;            // 当前作用域
    XL_Scope* global;           // 全局作用域
    bool break_flag;            // break 标志
    bool continue_flag;         // continue 标志
    bool return_flag;           // return 标志
    XL_Value* return_value;     // 返回值
    FILE* output;               // 输出流
};

// ==================== 函数声明 ====================

// AST 节点创建函数
XL_Node* xl_node_new(XL_NodeType type);
XL_Node* xl_node_program(void);
XL_Node* xl_node_block(void);
void xl_node_add_stmt(XL_Node* block, XL_Node* stmt);
XL_Node* xl_node_var_decl(const char* name, XL_Node* init);
XL_Node* xl_node_binary(XL_OpType op, XL_Node* left, XL_Node* right);
XL_Node* xl_node_unary(XL_OpType op, XL_Node* operand, bool prefix);
XL_Node* xl_node_assign(XL_OpType op, XL_Node* target, XL_Node* value);
XL_Node* xl_node_if(XL_Node* cond, XL_Node* then_block);
void xl_node_add_elseif(XL_Node* if_node, XL_Node* cond, XL_Node* block);
void xl_node_set_else(XL_Node* if_node, XL_Node* else_block);
XL_Node* xl_node_while(XL_Node* cond, XL_Node* body);
XL_Node* xl_node_for(const char* var, XL_Node* start, XL_Node* end, XL_Node* step, XL_Node* body);
XL_Node* xl_node_break(void);
XL_Node* xl_node_continue(void);
XL_Node* xl_node_return(XL_Node* value);
XL_Node* xl_node_func_def(const char* name, char** params, int param_count, XL_Node* body);
XL_Node* xl_node_call(XL_Node* func, XL_Node** args, int arg_count);
XL_Node* xl_node_index(XL_Node* array, XL_Node* index);
XL_Node* xl_node_member(XL_Node* object, const char* member);
XL_Node* xl_node_null(void);
XL_Node* xl_node_bool(bool val);
XL_Node* xl_node_int(long long val);
XL_Node* xl_node_float(double val);
XL_Node* xl_node_string(const char* val);
XL_Node* xl_node_ident(const char* name);
XL_Node* xl_node_array(XL_Node** elements, int count);
XL_Node* xl_node_table(char** keys, XL_Node** values, int count);

// AST 节点释放
void xl_node_free(XL_Node* node);

// 值操作函数
XL_Value* xl_value_null(void);
XL_Value* xl_value_bool(bool val);
XL_Value* xl_value_int(long long val);
XL_Value* xl_value_float(double val);
XL_Value* xl_value_string(const char* val);
XL_Value* xl_value_array(void);
XL_Value* xl_value_table(void);
XL_Value* xl_value_function(XL_Node* node, XL_Context* closure);
XL_Value* xl_value_builtin(const char* name, XL_Value* (*fn)(XL_Context*, XL_Value**, int));
void xl_value_ref(XL_Value* val);
void xl_value_unref(XL_Value* val);
XL_Value* xl_value_copy(XL_Value* val);

// 数组操作
void xl_array_push(XL_Value* arr, XL_Value* val);
XL_Value* xl_array_get(XL_Value* arr, int index);
void xl_array_set(XL_Value* arr, int index, XL_Value* val);

// 表操作
void xl_table_set(XL_Value* tbl, const char* key, XL_Value* val);
XL_Value* xl_table_get(XL_Value* tbl, const char* key);

// 类型转换
bool xl_to_bool(XL_Value* val);
long long xl_to_int(XL_Value* val);
double xl_to_float(XL_Value* val);
char* xl_to_string(XL_Value* val);

// 上下文操作
XL_Context* xl_context_new(void);
void xl_context_free(XL_Context* ctx);
void xl_context_push_scope(XL_Context* ctx);
void xl_context_pop_scope(XL_Context* ctx);
void xl_context_set_var(XL_Context* ctx, const char* name, XL_Value* val);
XL_Value* xl_context_get_var(XL_Context* ctx, const char* name);
void xl_context_define_var(XL_Context* ctx, const char* name, XL_Value* val);

// 执行器
XL_Value* xl_exec(XL_Context* ctx, XL_Node* node);
XL_Value* xl_exec_program(XL_Context* ctx, XL_Node* program);

// 内置函数注册
void xl_register_builtins(XL_Context* ctx);

#endif // XL_AST_H
