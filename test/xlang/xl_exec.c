/*
 * xlang - 执行器实现
 * 负责 AST 节点创建、值操作和程序执行
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "xl_ast.h"

// ==================== AST 节点创建 ====================

XL_Node* xl_node_new(XL_NodeType type) {
    XL_Node* node = (XL_Node*)calloc(1, sizeof(XL_Node));
    node->type = type;
    return node;
}

XL_Node* xl_node_program(void) {
    XL_Node* node = xl_node_new(NODE_PROGRAM);
    node->block.stmts = NULL;
    node->block.count = 0;
    node->block.capacity = 0;
    return node;
}

XL_Node* xl_node_block(void) {
    XL_Node* node = xl_node_new(NODE_BLOCK);
    node->block.stmts = NULL;
    node->block.count = 0;
    node->block.capacity = 0;
    return node;
}

void xl_node_add_stmt(XL_Node* block, XL_Node* stmt) {
    if (!block || !stmt) return;
    
    if (block->block.count >= block->block.capacity) {
        int new_cap = block->block.capacity == 0 ? 8 : block->block.capacity * 2;
        block->block.stmts = (XL_Node**)realloc(block->block.stmts, 
                                                 sizeof(XL_Node*) * new_cap);
        block->block.capacity = new_cap;
    }
    block->block.stmts[block->block.count++] = stmt;
}

XL_Node* xl_node_var_decl(const char* name, XL_Node* init) {
    XL_Node* node = xl_node_new(NODE_VAR_DECL);
    node->var_decl.name = strdup(name);
    node->var_decl.init = init;
    return node;
}

XL_Node* xl_node_binary(XL_OpType op, XL_Node* left, XL_Node* right) {
    XL_Node* node = xl_node_new(NODE_BINARY_OP);
    node->binary.op = op;
    node->binary.left = left;
    node->binary.right = right;
    return node;
}

XL_Node* xl_node_unary(XL_OpType op, XL_Node* operand, bool prefix) {
    XL_Node* node = xl_node_new(NODE_UNARY_OP);
    node->unary.op = op;
    node->unary.operand = operand;
    node->unary.prefix = prefix;
    return node;
}

XL_Node* xl_node_assign(XL_OpType op, XL_Node* target, XL_Node* value) {
    XL_Node* node = xl_node_new(NODE_ASSIGN);
    node->assign.op = op;
    node->assign.target = target;
    node->assign.value = value;
    return node;
}

XL_Node* xl_node_if(XL_Node* cond, XL_Node* then_block) {
    XL_Node* node = xl_node_new(NODE_IF_STMT);
    node->if_stmt.cond = cond;
    node->if_stmt.then_block = then_block;
    node->if_stmt.elseif_conds = NULL;
    node->if_stmt.elseif_blocks = NULL;
    node->if_stmt.elseif_count = 0;
    node->if_stmt.else_block = NULL;
    return node;
}

void xl_node_add_elseif(XL_Node* if_node, XL_Node* cond, XL_Node* block) {
    int n = if_node->if_stmt.elseif_count;
    if_node->if_stmt.elseif_conds = (XL_Node**)realloc(
        if_node->if_stmt.elseif_conds, sizeof(XL_Node*) * (n + 1));
    if_node->if_stmt.elseif_blocks = (XL_Node**)realloc(
        if_node->if_stmt.elseif_blocks, sizeof(XL_Node*) * (n + 1));
    if_node->if_stmt.elseif_conds[n] = cond;
    if_node->if_stmt.elseif_blocks[n] = block;
    if_node->if_stmt.elseif_count++;
}

void xl_node_set_else(XL_Node* if_node, XL_Node* else_block) {
    if_node->if_stmt.else_block = else_block;
}

XL_Node* xl_node_while(XL_Node* cond, XL_Node* body) {
    XL_Node* node = xl_node_new(NODE_WHILE_STMT);
    node->while_stmt.cond = cond;
    node->while_stmt.body = body;
    return node;
}

XL_Node* xl_node_for(const char* var, XL_Node* start, XL_Node* end, 
                     XL_Node* step, XL_Node* body) {
    XL_Node* node = xl_node_new(NODE_FOR_STMT);
    node->for_stmt.var = strdup(var);
    node->for_stmt.start = start;
    node->for_stmt.end = end;
    node->for_stmt.step = step;
    node->for_stmt.body = body;
    return node;
}

XL_Node* xl_node_break(void) {
    return xl_node_new(NODE_BREAK_STMT);
}

XL_Node* xl_node_continue(void) {
    return xl_node_new(NODE_CONTINUE_STMT);
}

XL_Node* xl_node_return(XL_Node* value) {
    XL_Node* node = xl_node_new(NODE_RETURN_STMT);
    node->return_stmt.value = value;
    return node;
}

XL_Node* xl_node_func_def(const char* name, char** params, int param_count, 
                          XL_Node* body) {
    XL_Node* node = xl_node_new(NODE_FUNC_DEF);
    node->func_def.name = strdup(name);
    node->func_def.params = params;
    node->func_def.param_count = param_count;
    node->func_def.body = body;
    return node;
}

XL_Node* xl_node_call(XL_Node* func, XL_Node** args, int arg_count) {
    XL_Node* node = xl_node_new(NODE_CALL);
    node->call.func = func;
    node->call.args = args;
    node->call.arg_count = arg_count;
    return node;
}

XL_Node* xl_node_index(XL_Node* array, XL_Node* index) {
    XL_Node* node = xl_node_new(NODE_INDEX);
    node->index.array = array;
    node->index.index = index;
    return node;
}

XL_Node* xl_node_member(XL_Node* object, const char* member) {
    XL_Node* node = xl_node_new(NODE_MEMBER);
    node->member.object = object;
    node->member.member = strdup(member);
    return node;
}

XL_Node* xl_node_null(void) {
    return xl_node_new(NODE_NULL);
}

XL_Node* xl_node_bool(bool val) {
    XL_Node* node = xl_node_new(NODE_BOOL);
    node->bool_val = val;
    return node;
}

XL_Node* xl_node_int(long long val) {
    XL_Node* node = xl_node_new(NODE_INT);
    node->int_val = val;
    return node;
}

XL_Node* xl_node_float(double val) {
    XL_Node* node = xl_node_new(NODE_FLOAT);
    node->float_val = val;
    return node;
}

XL_Node* xl_node_string(const char* val) {
    XL_Node* node = xl_node_new(NODE_STRING);
    node->str_val = strdup(val);
    return node;
}

XL_Node* xl_node_ident(const char* name) {
    XL_Node* node = xl_node_new(NODE_IDENT);
    node->str_val = strdup(name);
    return node;
}

XL_Node* xl_node_array(XL_Node** elements, int count) {
    XL_Node* node = xl_node_new(NODE_ARRAY);
    node->array.elements = elements;
    node->array.count = count;
    return node;
}

XL_Node* xl_node_table(char** keys, XL_Node** values, int count) {
    XL_Node* node = xl_node_new(NODE_TABLE);
    node->table.keys = keys;
    node->table.values = values;
    node->table.count = count;
    return node;
}

void xl_node_free(XL_Node* node) {
    if (!node) return;
    // 简化实现，实际应该递归释放
    free(node);
}

// ==================== 值操作 ====================

XL_Value* xl_value_null(void) {
    XL_Value* val = (XL_Value*)calloc(1, sizeof(XL_Value));
    val->type = VAL_NULL;
    val->refcount = 1;
    return val;
}

XL_Value* xl_value_bool(bool b) {
    XL_Value* val = (XL_Value*)calloc(1, sizeof(XL_Value));
    val->type = VAL_BOOL;
    val->refcount = 1;
    val->bool_val = b;
    return val;
}

XL_Value* xl_value_int(long long i) {
    XL_Value* val = (XL_Value*)calloc(1, sizeof(XL_Value));
    val->type = VAL_INT;
    val->refcount = 1;
    val->int_val = i;
    return val;
}

XL_Value* xl_value_float(double f) {
    XL_Value* val = (XL_Value*)calloc(1, sizeof(XL_Value));
    val->type = VAL_FLOAT;
    val->refcount = 1;
    val->float_val = f;
    return val;
}

XL_Value* xl_value_string(const char* s) {
    XL_Value* val = (XL_Value*)calloc(1, sizeof(XL_Value));
    val->type = VAL_STRING;
    val->refcount = 1;
    val->str_val = strdup(s ? s : "");
    return val;
}

XL_Value* xl_value_array(void) {
    XL_Value* val = (XL_Value*)calloc(1, sizeof(XL_Value));
    val->type = VAL_ARRAY;
    val->refcount = 1;
    val->array.elements = NULL;
    val->array.count = 0;
    val->array.capacity = 0;
    return val;
}

XL_Value* xl_value_table(void) {
    XL_Value* val = (XL_Value*)calloc(1, sizeof(XL_Value));
    val->type = VAL_TABLE;
    val->refcount = 1;
    val->table.keys = NULL;
    val->table.values = NULL;
    val->table.count = 0;
    val->table.capacity = 0;
    return val;
}

XL_Value* xl_value_function(XL_Node* node, XL_Context* closure) {
    XL_Value* val = (XL_Value*)calloc(1, sizeof(XL_Value));
    val->type = VAL_FUNCTION;
    val->refcount = 1;
    val->func.node = node;
    val->func.closure = closure;
    return val;
}

XL_Value* xl_value_builtin(const char* name, 
                           XL_Value* (*fn)(XL_Context*, XL_Value**, int)) {
    XL_Value* val = (XL_Value*)calloc(1, sizeof(XL_Value));
    val->type = VAL_BUILTIN;
    val->refcount = 1;
    val->builtin.name = name;
    val->builtin.fn = fn;
    return val;
}

void xl_value_ref(XL_Value* val) {
    if (val) val->refcount++;
}

void xl_value_unref(XL_Value* val) {
    if (!val) return;
    val->refcount--;
    if (val->refcount <= 0) {
        switch (val->type) {
            case VAL_STRING:
                free(val->str_val);
                break;
            case VAL_ARRAY:
                for (int i = 0; i < val->array.count; i++) {
                    xl_value_unref(val->array.elements[i]);
                }
                free(val->array.elements);
                break;
            case VAL_TABLE:
                for (int i = 0; i < val->table.count; i++) {
                    free(val->table.keys[i]);
                    xl_value_unref(val->table.values[i]);
                }
                free(val->table.keys);
                free(val->table.values);
                break;
            default:
                break;
        }
        free(val);
    }
}

void xl_array_push(XL_Value* arr, XL_Value* val) {
    if (arr->type != VAL_ARRAY) return;
    if (arr->array.count >= arr->array.capacity) {
        int new_cap = arr->array.capacity == 0 ? 8 : arr->array.capacity * 2;
        arr->array.elements = (XL_Value**)realloc(arr->array.elements,
                                                   sizeof(XL_Value*) * new_cap);
        arr->array.capacity = new_cap;
    }
    xl_value_ref(val);
    arr->array.elements[arr->array.count++] = val;
}

XL_Value* xl_array_get(XL_Value* arr, int index) {
    if (arr->type != VAL_ARRAY) return xl_value_null();
    if (index < 0 || index >= arr->array.count) return xl_value_null();
    return arr->array.elements[index];
}

void xl_array_set(XL_Value* arr, int index, XL_Value* val) {
    if (arr->type != VAL_ARRAY) return;
    if (index < 0 || index >= arr->array.count) return;
    xl_value_unref(arr->array.elements[index]);
    xl_value_ref(val);
    arr->array.elements[index] = val;
}

void xl_table_set(XL_Value* tbl, const char* key, XL_Value* val) {
    if (tbl->type != VAL_TABLE) return;
    
    // 查找是否存在
    for (int i = 0; i < tbl->table.count; i++) {
        if (strcmp(tbl->table.keys[i], key) == 0) {
            xl_value_unref(tbl->table.values[i]);
            xl_value_ref(val);
            tbl->table.values[i] = val;
            return;
        }
    }
    
    // 添加新键值对
    if (tbl->table.count >= tbl->table.capacity) {
        int new_cap = tbl->table.capacity == 0 ? 8 : tbl->table.capacity * 2;
        tbl->table.keys = (char**)realloc(tbl->table.keys, sizeof(char*) * new_cap);
        tbl->table.values = (XL_Value**)realloc(tbl->table.values, 
                                                 sizeof(XL_Value*) * new_cap);
        tbl->table.capacity = new_cap;
    }
    tbl->table.keys[tbl->table.count] = strdup(key);
    xl_value_ref(val);
    tbl->table.values[tbl->table.count] = val;
    tbl->table.count++;
}

XL_Value* xl_table_get(XL_Value* tbl, const char* key) {
    if (tbl->type != VAL_TABLE) return xl_value_null();
    for (int i = 0; i < tbl->table.count; i++) {
        if (strcmp(tbl->table.keys[i], key) == 0) {
            return tbl->table.values[i];
        }
    }
    return xl_value_null();
}

// 类型转换
bool xl_to_bool(XL_Value* val) {
    if (!val) return false;
    switch (val->type) {
        case VAL_NULL: return false;
        case VAL_BOOL: return val->bool_val;
        case VAL_INT: return val->int_val != 0;
        case VAL_FLOAT: return val->float_val != 0.0;
        case VAL_STRING: return val->str_val && val->str_val[0];
        case VAL_ARRAY: return val->array.count > 0;
        case VAL_TABLE: return val->table.count > 0;
        default: return true;
    }
}

long long xl_to_int(XL_Value* val) {
    if (!val) return 0;
    switch (val->type) {
        case VAL_NULL: return 0;
        case VAL_BOOL: return val->bool_val ? 1 : 0;
        case VAL_INT: return val->int_val;
        case VAL_FLOAT: return (long long)val->float_val;
        case VAL_STRING: return strtoll(val->str_val, NULL, 10);
        default: return 0;
    }
}

double xl_to_float(XL_Value* val) {
    if (!val) return 0.0;
    switch (val->type) {
        case VAL_NULL: return 0.0;
        case VAL_BOOL: return val->bool_val ? 1.0 : 0.0;
        case VAL_INT: return (double)val->int_val;
        case VAL_FLOAT: return val->float_val;
        case VAL_STRING: return strtod(val->str_val, NULL);
        default: return 0.0;
    }
}

char* xl_to_string(XL_Value* val) {
    if (!val) return strdup("null");
    char buf[64];
    switch (val->type) {
        case VAL_NULL: return strdup("null");
        case VAL_BOOL: return strdup(val->bool_val ? "true" : "false");
        case VAL_INT: 
            snprintf(buf, sizeof(buf), "%lld", val->int_val);
            return strdup(buf);
        case VAL_FLOAT:
            snprintf(buf, sizeof(buf), "%g", val->float_val);
            return strdup(buf);
        case VAL_STRING: return strdup(val->str_val);
        case VAL_ARRAY:
            snprintf(buf, sizeof(buf), "[array:%d]", val->array.count);
            return strdup(buf);
        case VAL_TABLE:
            snprintf(buf, sizeof(buf), "{table:%d}", val->table.count);
            return strdup(buf);
        case VAL_FUNCTION: return strdup("[function]");
        case VAL_BUILTIN: return strdup("[builtin]");
        default: return strdup("[unknown]");
    }
}

// ==================== 上下文操作 ====================

XL_Context* xl_context_new(void) {
    XL_Context* ctx = (XL_Context*)calloc(1, sizeof(XL_Context));
    ctx->scope = (XL_Scope*)calloc(1, sizeof(XL_Scope));
    ctx->global = ctx->scope;
    ctx->output = stdout;
    return ctx;
}

void xl_context_free(XL_Context* ctx) {
    if (!ctx) return;
    // 释放所有作用域
    XL_Scope* s = ctx->scope;
    while (s) {
        XL_Var* v = s->vars;
        while (v) {
            XL_Var* next = v->next;
            free(v->name);
            xl_value_unref(v->value);
            free(v);
            v = next;
        }
        XL_Scope* parent = s->parent;
        free(s);
        s = parent;
    }
    free(ctx);
}

void xl_context_push_scope(XL_Context* ctx) {
    XL_Scope* s = (XL_Scope*)calloc(1, sizeof(XL_Scope));
    s->parent = ctx->scope;
    ctx->scope = s;
}

void xl_context_pop_scope(XL_Context* ctx) {
    if (ctx->scope == ctx->global) return;
    XL_Scope* s = ctx->scope;
    ctx->scope = s->parent;
    
    XL_Var* v = s->vars;
    while (v) {
        XL_Var* next = v->next;
        free(v->name);
        xl_value_unref(v->value);
        free(v);
        v = next;
    }
    free(s);
}

void xl_context_define_var(XL_Context* ctx, const char* name, XL_Value* val) {
    XL_Var* v = (XL_Var*)calloc(1, sizeof(XL_Var));
    v->name = strdup(name);
    v->value = val;
    xl_value_ref(val);
    v->next = ctx->scope->vars;
    ctx->scope->vars = v;
}

void xl_context_set_var(XL_Context* ctx, const char* name, XL_Value* val) {
    // 查找现有变量
    for (XL_Scope* s = ctx->scope; s; s = s->parent) {
        for (XL_Var* v = s->vars; v; v = v->next) {
            if (strcmp(v->name, name) == 0) {
                xl_value_unref(v->value);
                v->value = val;
                xl_value_ref(val);
                return;
            }
        }
    }
    // 不存在则定义新变量
    xl_context_define_var(ctx, name, val);
}

XL_Value* xl_context_get_var(XL_Context* ctx, const char* name) {
    for (XL_Scope* s = ctx->scope; s; s = s->parent) {
        for (XL_Var* v = s->vars; v; v = v->next) {
            if (strcmp(v->name, name) == 0) {
                return v->value;
            }
        }
    }
    return NULL;
}

// ==================== 内置函数 ====================

// print 函数
static XL_Value* builtin_print(XL_Context* ctx, XL_Value** args, int argc) {
    for (int i = 0; i < argc; i++) {
        char* s = xl_to_string(args[i]);
        fprintf(ctx->output, "%s", s);
        free(s);
        if (i < argc - 1) fprintf(ctx->output, " ");
    }
    fprintf(ctx->output, "\n");
    fflush(ctx->output);
    return xl_value_null();
}

// println 函数 (与 print 相同)
static XL_Value* builtin_println(XL_Context* ctx, XL_Value** args, int argc) {
    return builtin_print(ctx, args, argc);
}

// len 函数
static XL_Value* builtin_len(XL_Context* ctx, XL_Value** args, int argc) {
    if (argc < 1) return xl_value_int(0);
    XL_Value* val = args[0];
    switch (val->type) {
        case VAL_STRING: return xl_value_int(strlen(val->str_val));
        case VAL_ARRAY: return xl_value_int(val->array.count);
        case VAL_TABLE: return xl_value_int(val->table.count);
        default: return xl_value_int(0);
    }
}

// type 函数
static XL_Value* builtin_type(XL_Context* ctx, XL_Value** args, int argc) {
    if (argc < 1) return xl_value_string("null");
    switch (args[0]->type) {
        case VAL_NULL: return xl_value_string("null");
        case VAL_BOOL: return xl_value_string("bool");
        case VAL_INT: return xl_value_string("int");
        case VAL_FLOAT: return xl_value_string("float");
        case VAL_STRING: return xl_value_string("string");
        case VAL_ARRAY: return xl_value_string("array");
        case VAL_TABLE: return xl_value_string("table");
        case VAL_FUNCTION: return xl_value_string("function");
        case VAL_BUILTIN: return xl_value_string("builtin");
        default: return xl_value_string("unknown");
    }
}

// str 函数 - 转换为字符串
static XL_Value* builtin_str(XL_Context* ctx, XL_Value** args, int argc) {
    if (argc < 1) return xl_value_string("");
    char* s = xl_to_string(args[0]);
    XL_Value* result = xl_value_string(s);
    free(s);
    return result;
}

// int 函数 - 转换为整数
static XL_Value* builtin_int(XL_Context* ctx, XL_Value** args, int argc) {
    if (argc < 1) return xl_value_int(0);
    return xl_value_int(xl_to_int(args[0]));
}

// float 函数 - 转换为浮点数
static XL_Value* builtin_float(XL_Context* ctx, XL_Value** args, int argc) {
    if (argc < 1) return xl_value_float(0.0);
    return xl_value_float(xl_to_float(args[0]));
}

// push 函数 - 向数组添加元素
static XL_Value* builtin_push(XL_Context* ctx, XL_Value** args, int argc) {
    if (argc < 2 || args[0]->type != VAL_ARRAY) return xl_value_null();
    xl_array_push(args[0], args[1]);
    return args[0];
}

// 注册内置函数
void xl_register_builtins(XL_Context* ctx) {
    xl_context_set_var(ctx, "print", xl_value_builtin("print", builtin_print));
    xl_context_set_var(ctx, "println", xl_value_builtin("println", builtin_println));
    xl_context_set_var(ctx, "len", xl_value_builtin("len", builtin_len));
    xl_context_set_var(ctx, "type", xl_value_builtin("type", builtin_type));
    xl_context_set_var(ctx, "str", xl_value_builtin("str", builtin_str));
    xl_context_set_var(ctx, "int", xl_value_builtin("int", builtin_int));
    xl_context_set_var(ctx, "float", xl_value_builtin("float", builtin_float));
    xl_context_set_var(ctx, "push", xl_value_builtin("push", builtin_push));
}
