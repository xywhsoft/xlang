/*
 * xlang - 程序执行器
 * 负责执行 AST 节点
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "xl_ast.h"

// 前向声明
static XL_Value* exec_expr(XL_Context* ctx, XL_Node* node);
static XL_Value* exec_stmt(XL_Context* ctx, XL_Node* node);

// ==================== 二元运算 ====================

static XL_Value* exec_binary(XL_Context* ctx, XL_Node* node) {
    XL_Value* left = exec_expr(ctx, node->binary.left);
    XL_Value* right = exec_expr(ctx, node->binary.right);
    XL_Value* result = NULL;
    
    XL_OpType op = node->binary.op;
    
    // 逻辑运算（短路求值已在上层处理）
    if (op == OP_AND) {
        result = xl_value_bool(xl_to_bool(left) && xl_to_bool(right));
    }
    else if (op == OP_OR) {
        result = xl_value_bool(xl_to_bool(left) || xl_to_bool(right));
    }
    // 字符串连接
    else if (op == OP_ADD && (left->type == VAL_STRING || right->type == VAL_STRING)) {
        char* ls = xl_to_string(left);
        char* rs = xl_to_string(right);
        char* concat = (char*)malloc(strlen(ls) + strlen(rs) + 1);
        strcpy(concat, ls);
        strcat(concat, rs);
        result = xl_value_string(concat);
        free(concat);
        free(ls);
        free(rs);
    }
    // 浮点数运算
    else if (left->type == VAL_FLOAT || right->type == VAL_FLOAT) {
        double l = xl_to_float(left);
        double r = xl_to_float(right);
        switch (op) {
            case OP_ADD: result = xl_value_float(l + r); break;
            case OP_SUB: result = xl_value_float(l - r); break;
            case OP_MUL: result = xl_value_float(l * r); break;
            case OP_DIV: result = xl_value_float(r != 0 ? l / r : 0); break;
            case OP_MOD: result = xl_value_float(fmod(l, r)); break;
            case OP_POW: result = xl_value_float(pow(l, r)); break;
            case OP_EQ:  result = xl_value_bool(l == r); break;
            case OP_NE:  result = xl_value_bool(l != r); break;
            case OP_LT:  result = xl_value_bool(l < r); break;
            case OP_LE:  result = xl_value_bool(l <= r); break;
            case OP_GT:  result = xl_value_bool(l > r); break;
            case OP_GE:  result = xl_value_bool(l >= r); break;
            default: result = xl_value_null(); break;
        }
    }
    // 整数运算
    else {
        long long l = xl_to_int(left);
        long long r = xl_to_int(right);
        switch (op) {
            case OP_ADD: result = xl_value_int(l + r); break;
            case OP_SUB: result = xl_value_int(l - r); break;
            case OP_MUL: result = xl_value_int(l * r); break;
            case OP_DIV: result = xl_value_int(r != 0 ? l / r : 0); break;
            case OP_MOD: result = xl_value_int(r != 0 ? l % r : 0); break;
            case OP_POW: result = xl_value_float(pow((double)l, (double)r)); break;
            case OP_EQ:  result = xl_value_bool(l == r); break;
            case OP_NE:  result = xl_value_bool(l != r); break;
            case OP_LT:  result = xl_value_bool(l < r); break;
            case OP_LE:  result = xl_value_bool(l <= r); break;
            case OP_GT:  result = xl_value_bool(l > r); break;
            case OP_GE:  result = xl_value_bool(l >= r); break;
            default: result = xl_value_null(); break;
        }
    }
    
    xl_value_unref(left);
    xl_value_unref(right);
    return result;
}

// ==================== 一元运算 ====================

static XL_Value* exec_unary(XL_Context* ctx, XL_Node* node) {
    XL_Value* operand = exec_expr(ctx, node->unary.operand);
    XL_Value* result = NULL;
    
    switch (node->unary.op) {
        case OP_NEG:
            if (operand->type == VAL_FLOAT) {
                result = xl_value_float(-operand->float_val);
            } else {
                result = xl_value_int(-xl_to_int(operand));
            }
            break;
            
        case OP_NOT:
            result = xl_value_bool(!xl_to_bool(operand));
            break;
            
        case OP_INC:
        case OP_DEC: {
            // ++/-- 操作需要修改变量
            long long val = xl_to_int(operand);
            long long new_val = (node->unary.op == OP_INC) ? val + 1 : val - 1;
            
            // 获取变量名
            if (node->unary.operand->type == NODE_IDENT) {
                xl_context_set_var(ctx, node->unary.operand->str_val, 
                                   xl_value_int(new_val));
            }
            
            // 前缀返回新值，后缀返回旧值
            if (node->unary.prefix) {
                result = xl_value_int(new_val);
            } else {
                result = xl_value_int(val);
            }
            break;
        }
            
        default:
            result = xl_value_null();
    }
    
    xl_value_unref(operand);
    return result;
}

// ==================== 赋值运算 ====================

static XL_Value* exec_assign(XL_Context* ctx, XL_Node* node) {
    XL_Value* value = exec_expr(ctx, node->assign.value);
    XL_Value* result = value;
    
    // 复合赋值
    if (node->assign.op != OP_ASSIGN) {
        XL_Value* old_val = exec_expr(ctx, node->assign.target);
        XL_Value* new_val = NULL;
        
        double l = xl_to_float(old_val);
        double r = xl_to_float(value);
        
        switch (node->assign.op) {
            case OP_ADDASSIGN:
                if (old_val->type == VAL_STRING || value->type == VAL_STRING) {
                    char* ls = xl_to_string(old_val);
                    char* rs = xl_to_string(value);
                    char* concat = (char*)malloc(strlen(ls) + strlen(rs) + 1);
                    strcpy(concat, ls);
                    strcat(concat, rs);
                    new_val = xl_value_string(concat);
                    free(concat);
                    free(ls);
                    free(rs);
                } else if (old_val->type == VAL_FLOAT || value->type == VAL_FLOAT) {
                    new_val = xl_value_float(l + r);
                } else {
                    new_val = xl_value_int(xl_to_int(old_val) + xl_to_int(value));
                }
                break;
            case OP_SUBASSIGN:
                new_val = (old_val->type == VAL_FLOAT || value->type == VAL_FLOAT)
                    ? xl_value_float(l - r)
                    : xl_value_int(xl_to_int(old_val) - xl_to_int(value));
                break;
            case OP_MULASSIGN:
                new_val = (old_val->type == VAL_FLOAT || value->type == VAL_FLOAT)
                    ? xl_value_float(l * r)
                    : xl_value_int(xl_to_int(old_val) * xl_to_int(value));
                break;
            case OP_DIVASSIGN:
                new_val = (old_val->type == VAL_FLOAT || value->type == VAL_FLOAT)
                    ? xl_value_float(r != 0 ? l / r : 0)
                    : xl_value_int(xl_to_int(value) != 0 
                        ? xl_to_int(old_val) / xl_to_int(value) : 0);
                break;
            default:
                new_val = value;
                xl_value_ref(new_val);
        }
        
        xl_value_unref(old_val);
        xl_value_unref(value);
        value = new_val;
        result = value;
    }
    
    // 执行赋值
    XL_Node* target = node->assign.target;
    if (target->type == NODE_IDENT) {
        xl_context_set_var(ctx, target->str_val, value);
    } else if (target->type == NODE_INDEX) {
        XL_Value* arr = exec_expr(ctx, target->index.array);
        XL_Value* idx = exec_expr(ctx, target->index.index);
        if (arr->type == VAL_ARRAY) {
            int i = (int)xl_to_int(idx);
            xl_array_set(arr, i, value);
        } else if (arr->type == VAL_TABLE) {
            char* key = xl_to_string(idx);
            xl_table_set(arr, key, value);
            free(key);
        }
        xl_value_unref(arr);
        xl_value_unref(idx);
    } else if (target->type == NODE_MEMBER) {
        XL_Value* obj = exec_expr(ctx, target->member.object);
        if (obj->type == VAL_TABLE) {
            xl_table_set(obj, target->member.member, value);
        }
        xl_value_unref(obj);
    }
    
    xl_value_ref(result);
    return result;
}

// ==================== 函数调用 ====================

static XL_Value* exec_call(XL_Context* ctx, XL_Node* node) {
    XL_Value* func = exec_expr(ctx, node->call.func);
    
    // 计算参数
    int argc = node->call.arg_count;
    XL_Value** args = NULL;
    if (argc > 0) {
        args = (XL_Value**)malloc(sizeof(XL_Value*) * argc);
        for (int i = 0; i < argc; i++) {
            args[i] = exec_expr(ctx, node->call.args[i]);
        }
    }
    
    XL_Value* result = xl_value_null();
    
    if (func->type == VAL_BUILTIN) {
        result = func->builtin.fn(ctx, args, argc);
    }
    else if (func->type == VAL_FUNCTION) {
        XL_Node* fn = func->func.node;
        
        // 创建新作用域
        xl_context_push_scope(ctx);
        
        // 绑定参数
        for (int i = 0; i < fn->func_def.param_count && i < argc; i++) {
            xl_context_define_var(ctx, fn->func_def.params[i], args[i]);
        }
        // 未提供的参数设为 null
        for (int i = argc; i < fn->func_def.param_count; i++) {
            xl_context_define_var(ctx, fn->func_def.params[i], xl_value_null());
        }
        
        // 执行函数体
        exec_stmt(ctx, fn->func_def.body);
        
        // 获取返回值
        if (ctx->return_flag) {
            result = ctx->return_value;
            ctx->return_value = NULL;
            ctx->return_flag = false;
        }
        
        xl_context_pop_scope(ctx);
    }
    
    // 释放参数
    for (int i = 0; i < argc; i++) {
        xl_value_unref(args[i]);
    }
    free(args);
    xl_value_unref(func);
    
    return result;
}

// ==================== 表达式执行 ====================

static XL_Value* exec_expr(XL_Context* ctx, XL_Node* node) {
    if (!node) return xl_value_null();
    
    switch (node->type) {
        case NODE_NULL:
            return xl_value_null();
            
        case NODE_BOOL:
            return xl_value_bool(node->bool_val);
            
        case NODE_INT:
            return xl_value_int(node->int_val);
            
        case NODE_FLOAT:
            return xl_value_float(node->float_val);
            
        case NODE_STRING:
            return xl_value_string(node->str_val);
            
        case NODE_IDENT: {
            XL_Value* val = xl_context_get_var(ctx, node->str_val);
            if (val) {
                xl_value_ref(val);
                return val;
            }
            return xl_value_null();
        }
            
        case NODE_ARRAY: {
            XL_Value* arr = xl_value_array();
            for (int i = 0; i < node->array.count; i++) {
                XL_Value* elem = exec_expr(ctx, node->array.elements[i]);
                xl_array_push(arr, elem);
                xl_value_unref(elem);
            }
            return arr;
        }
            
        case NODE_TABLE: {
            XL_Value* tbl = xl_value_table();
            for (int i = 0; i < node->table.count; i++) {
                XL_Value* val = exec_expr(ctx, node->table.values[i]);
                xl_table_set(tbl, node->table.keys[i], val);
                xl_value_unref(val);
            }
            return tbl;
        }
            
        case NODE_BINARY_OP:
            return exec_binary(ctx, node);
            
        case NODE_UNARY_OP:
            return exec_unary(ctx, node);
            
        case NODE_ASSIGN:
            return exec_assign(ctx, node);
            
        case NODE_CALL:
            return exec_call(ctx, node);
            
        case NODE_INDEX: {
            XL_Value* arr = exec_expr(ctx, node->index.array);
            XL_Value* idx = exec_expr(ctx, node->index.index);
            XL_Value* result;
            
            if (arr->type == VAL_ARRAY) {
                int i = (int)xl_to_int(idx);
                result = xl_array_get(arr, i);
                xl_value_ref(result);
            } else if (arr->type == VAL_TABLE) {
                char* key = xl_to_string(idx);
                result = xl_table_get(arr, key);
                xl_value_ref(result);
                free(key);
            } else if (arr->type == VAL_STRING) {
                // 字符串索引
                int i = (int)xl_to_int(idx);
                int len = strlen(arr->str_val);
                if (i >= 0 && i < len) {
                    char buf[2] = { arr->str_val[i], 0 };
                    result = xl_value_string(buf);
                } else {
                    result = xl_value_string("");
                }
            } else {
                result = xl_value_null();
            }
            
            xl_value_unref(arr);
            xl_value_unref(idx);
            return result;
        }
            
        case NODE_MEMBER: {
            XL_Value* obj = exec_expr(ctx, node->member.object);
            XL_Value* result;
            
            if (obj->type == VAL_TABLE) {
                result = xl_table_get(obj, node->member.member);
                xl_value_ref(result);
            } else {
                result = xl_value_null();
            }
            
            xl_value_unref(obj);
            return result;
        }
            
        default:
            return xl_value_null();
    }
}

// ==================== 语句执行 ====================

static XL_Value* exec_stmt(XL_Context* ctx, XL_Node* node) {
    if (!node) return xl_value_null();
    
    // 检查控制流标志
    if (ctx->break_flag || ctx->continue_flag || ctx->return_flag) {
        return xl_value_null();
    }
    
    switch (node->type) {
        case NODE_PROGRAM:
        case NODE_BLOCK: {
            XL_Value* result = xl_value_null();
            for (int i = 0; i < node->block.count; i++) {
                xl_value_unref(result);
                result = exec_stmt(ctx, node->block.stmts[i]);
                if (ctx->break_flag || ctx->continue_flag || ctx->return_flag) {
                    break;
                }
            }
            return result;
        }
            
        case NODE_VAR_DECL: {
            XL_Value* init = node->var_decl.init 
                ? exec_expr(ctx, node->var_decl.init) 
                : xl_value_null();
            xl_context_define_var(ctx, node->var_decl.name, init);
            xl_value_unref(init);
            return xl_value_null();
        }
            
        case NODE_IF_STMT: {
            XL_Value* cond = exec_expr(ctx, node->if_stmt.cond);
            bool is_true = xl_to_bool(cond);
            xl_value_unref(cond);
            
            if (is_true) {
                return exec_stmt(ctx, node->if_stmt.then_block);
            }
            
            // 检查 elseif
            for (int i = 0; i < node->if_stmt.elseif_count; i++) {
                cond = exec_expr(ctx, node->if_stmt.elseif_conds[i]);
                is_true = xl_to_bool(cond);
                xl_value_unref(cond);
                if (is_true) {
                    return exec_stmt(ctx, node->if_stmt.elseif_blocks[i]);
                }
            }
            
            // else 分支
            if (node->if_stmt.else_block) {
                return exec_stmt(ctx, node->if_stmt.else_block);
            }
            
            return xl_value_null();
        }
            
        case NODE_WHILE_STMT: {
            XL_Value* result = xl_value_null();
            while (1) {
                XL_Value* cond = exec_expr(ctx, node->while_stmt.cond);
                bool is_true = xl_to_bool(cond);
                xl_value_unref(cond);
                
                if (!is_true) break;
                
                xl_value_unref(result);
                result = exec_stmt(ctx, node->while_stmt.body);
                
                if (ctx->continue_flag) {
                    ctx->continue_flag = false;
                    continue;
                }
                if (ctx->break_flag) {
                    ctx->break_flag = false;
                    break;
                }
                if (ctx->return_flag) {
                    break;
                }
            }
            return result;
        }
            
        case NODE_FOR_STMT: {
            XL_Value* result = xl_value_null();
            
            // 初始化
            XL_Value* start_val = exec_expr(ctx, node->for_stmt.start);
            XL_Value* end_val = exec_expr(ctx, node->for_stmt.end);
            XL_Value* step_val = node->for_stmt.step 
                ? exec_expr(ctx, node->for_stmt.step)
                : xl_value_int(1);
            
            long long start = xl_to_int(start_val);
            long long end = xl_to_int(end_val);
            long long step = xl_to_int(step_val);
            
            xl_value_unref(start_val);
            xl_value_unref(end_val);
            xl_value_unref(step_val);
            
            if (step == 0) step = 1;
            
            // 创建循环变量
            xl_context_push_scope(ctx);
            
            for (long long i = start; 
                 (step > 0 ? i <= end : i >= end); 
                 i += step) {
                
                xl_context_set_var(ctx, node->for_stmt.var, xl_value_int(i));
                
                xl_value_unref(result);
                result = exec_stmt(ctx, node->for_stmt.body);
                
                if (ctx->continue_flag) {
                    ctx->continue_flag = false;
                    continue;
                }
                if (ctx->break_flag) {
                    ctx->break_flag = false;
                    break;
                }
                if (ctx->return_flag) {
                    break;
                }
            }
            
            xl_context_pop_scope(ctx);
            return result;
        }
            
        case NODE_BREAK_STMT:
            ctx->break_flag = true;
            return xl_value_null();
            
        case NODE_CONTINUE_STMT:
            ctx->continue_flag = true;
            return xl_value_null();
            
        case NODE_RETURN_STMT:
            ctx->return_flag = true;
            if (node->return_stmt.value) {
                ctx->return_value = exec_expr(ctx, node->return_stmt.value);
            } else {
                ctx->return_value = xl_value_null();
            }
            return xl_value_null();
            
        case NODE_FUNC_DEF: {
            XL_Value* func = xl_value_function(node, ctx);
            xl_context_set_var(ctx, node->func_def.name, func);
            xl_value_unref(func);
            return xl_value_null();
        }
            
        default:
            // 表达式语句
            return exec_expr(ctx, node);
    }
}

// ==================== 公共接口 ====================

XL_Value* xl_exec(XL_Context* ctx, XL_Node* node) {
    return exec_stmt(ctx, node);
}

XL_Value* xl_exec_program(XL_Context* ctx, XL_Node* program) {
    return exec_stmt(ctx, program);
}
