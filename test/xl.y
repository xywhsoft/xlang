/*
 * xlang - 语法分析器定义文件
 * 使用 lemon 生成语法分析器
 */

%include {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "xl_token.h"
#include "xl_ast.h"

// 解析结果
static XL_Node* parse_result = 0;

// 设置解析结果
void xl_set_parse_result(XL_Node* node) {
    parse_result = node;
}

// 获取解析结果
XL_Node* xl_get_parse_result(void) {
    return parse_result;
}

// 错误处理
static int parse_error = 0;
int xl_get_parse_error(void) { return parse_error; }
}

%token_type { XL_Token* }
%default_type { XL_Node* }

%extra_argument { XL_Node** result }

%syntax_error {
    parse_error = 1;
    fprintf(stderr, "Syntax error!\n");
}

%parse_failure {
    parse_error = 1;
    fprintf(stderr, "Parse failure!\n");
}

%parse_accept {
    if (result && *result) {
        xl_set_parse_result(*result);
    }
}

// 运算符优先级（从低到高）
%left TK_OR.
%left TK_AND.
%right TK_NOT.
%left TK_EQ TK_NE.
%left TK_LT TK_LE TK_GT TK_GE.
%left TK_PLUS TK_MINUS.
%left TK_MUL TK_DIV TK_MOD.
%right TK_POW.
%right TK_UMINUS.

// 程序入口
program(P) ::= stmt_list(L). { 
    P = L; 
    if (result) *result = P;
}

// 语句列表
stmt_list(L) ::= . { L = xl_node_program(); }
stmt_list(L) ::= stmt_list(S) stmt(T). { 
    L = S; 
    if (T) xl_node_add_stmt(L, T); 
}

// 语句
stmt(S) ::= var_decl(V). { S = V; }
stmt(S) ::= expr_stmt(E). { S = E; }
stmt(S) ::= if_stmt(I). { S = I; }
stmt(S) ::= while_stmt(W). { S = W; }
stmt(S) ::= for_stmt(F). { S = F; }
stmt(S) ::= func_def(D). { S = D; }
stmt(S) ::= return_stmt(R). { S = R; }
stmt(S) ::= break_stmt(B). { S = B; }
stmt(S) ::= continue_stmt(C). { S = C; }
stmt(S) ::= TK_SEMI. { S = 0; }

// 变量声明
var_decl(V) ::= TK_VAR TK_IDENT(N). { 
    V = xl_node_var_decl(N->text, 0);
}
var_decl(V) ::= TK_VAR TK_IDENT(N) TK_ASSIGN expr(E). { 
    V = xl_node_var_decl(N->text, E); 
}

// 表达式语句
expr_stmt(S) ::= expr(E). { S = E; }

// if 语句
if_stmt(I) ::= TK_IF expr(C) then_block(T) elseif_list(EL) else_block(E) TK_END. {
    I = xl_node_if(C, T);
    // 添加 elseif
    if (EL) {
        for (int i = 0; i < EL->block.count; i += 2) {
            xl_node_add_elseif(I, EL->block.stmts[i], EL->block.stmts[i+1]);
        }
        xl_node_free(EL);
    }
    if (E) xl_node_set_else(I, E);
}

// if 语句 - 单行形式
if_stmt(I) ::= TK_IF expr(C) TK_THEN expr(E). {
    XL_Node* block = xl_node_block();
    xl_node_add_stmt(block, E);
    I = xl_node_if(C, block);
}

then_block(B) ::= stmt_list(L). { B = L; }
then_block(B) ::= TK_THEN stmt_list(L). { B = L; }

elseif_list(L) ::= . { L = 0; }
elseif_list(L) ::= elseif_list(EL) TK_ELSEIF expr(C) then_block(B). {
    if (EL == 0) {
        L = xl_node_block();
    } else {
        L = EL;
    }
    xl_node_add_stmt(L, C);
    xl_node_add_stmt(L, B);
}

else_block(E) ::= . { E = 0; }
else_block(E) ::= TK_ELSE stmt_list(L). { E = L; }

// while 语句
while_stmt(W) ::= TK_WHILE expr(C) stmt_list(B) TK_END. {
    W = xl_node_while(C, B);
}

while_stmt(W) ::= TK_DO stmt_list(B) TK_WHILE expr(C) TK_END. {
    // do-while 转换为 while
    W = xl_node_while(C, B);
    // 注意：真正的 do-while 需要先执行一次，这里简化处理
}

// for 语句
for_stmt(F) ::= TK_FOR TK_IDENT(V) TK_ASSIGN expr(S) TK_TO expr(E) stmt_list(B) TK_END. {
    F = xl_node_for(V->text, S, E, 0, B);
}

for_stmt(F) ::= TK_FOR TK_IDENT(V) TK_ASSIGN expr(S) TK_TO expr(E) TK_STEP expr(ST) stmt_list(B) TK_END. {
    F = xl_node_for(V->text, S, E, ST, B);
}

// break 和 continue
break_stmt(B) ::= TK_BREAK. { B = xl_node_break(); }
continue_stmt(C) ::= TK_CONTINUE. { C = xl_node_continue(); }

// return 语句
return_stmt(R) ::= TK_RETURN. { R = xl_node_return(0); }
return_stmt(R) ::= TK_RETURN expr(E). { R = xl_node_return(E); }

// 函数定义
func_def(D) ::= TK_FUNCTION TK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN stmt_list(B) TK_END. {
    D = xl_node_func_def(N->text, P ? (char**)P->block.stmts : NULL, 
                         P ? P->block.count : 0, B);
    if (P) free(P);
}

param_list(L) ::= . { L = 0; }
param_list(L) ::= param_list_ne(P). { L = P; }

param_list_ne(L) ::= TK_IDENT(N). { 
    L = xl_node_block();
    xl_node_add_stmt(L, (XL_Node*)strdup(N->text));
}
param_list_ne(L) ::= param_list_ne(P) TK_COMMA TK_IDENT(N). {
    L = P;
    xl_node_add_stmt(L, (XL_Node*)strdup(N->text));
}

// 表达式
expr(E) ::= or_expr(O). { E = O; }

// 赋值表达式 - 直接使用 postfix_expr 作为左值
expr(E) ::= postfix_expr(L) TK_ASSIGN expr(R). { 
    E = xl_node_assign(OP_ASSIGN, L, R); 
}
expr(E) ::= postfix_expr(L) TK_PLUSEQ expr(R). { 
    E = xl_node_assign(OP_ADDASSIGN, L, R); 
}
expr(E) ::= postfix_expr(L) TK_MINUSEQ expr(R). { 
    E = xl_node_assign(OP_SUBASSIGN, L, R); 
}
expr(E) ::= postfix_expr(L) TK_MULEQ expr(R). { 
    E = xl_node_assign(OP_MULASSIGN, L, R); 
}
expr(E) ::= postfix_expr(L) TK_DIVEQ expr(R). { 
    E = xl_node_assign(OP_DIVASSIGN, L, R); 
}

// 逻辑或
or_expr(E) ::= and_expr(A). { E = A; }
or_expr(E) ::= or_expr(L) TK_OR and_expr(R). { 
    E = xl_node_binary(OP_OR, L, R); 
}

// 逻辑与
and_expr(E) ::= not_expr(N). { E = N; }
and_expr(E) ::= and_expr(L) TK_AND not_expr(R). { 
    E = xl_node_binary(OP_AND, L, R); 
}

// 逻辑非
not_expr(E) ::= cmp_expr(C). { E = C; }
not_expr(E) ::= TK_NOT not_expr(N). { 
    E = xl_node_unary(OP_NOT, N, true); 
}

// 比较表达式
cmp_expr(E) ::= add_expr(A). { E = A; }
cmp_expr(E) ::= cmp_expr(L) TK_EQ add_expr(R). { 
    E = xl_node_binary(OP_EQ, L, R); 
}
cmp_expr(E) ::= cmp_expr(L) TK_NE add_expr(R). { 
    E = xl_node_binary(OP_NE, L, R); 
}
cmp_expr(E) ::= cmp_expr(L) TK_LT add_expr(R). { 
    E = xl_node_binary(OP_LT, L, R); 
}
cmp_expr(E) ::= cmp_expr(L) TK_LE add_expr(R). { 
    E = xl_node_binary(OP_LE, L, R); 
}
cmp_expr(E) ::= cmp_expr(L) TK_GT add_expr(R). { 
    E = xl_node_binary(OP_GT, L, R); 
}
cmp_expr(E) ::= cmp_expr(L) TK_GE add_expr(R). { 
    E = xl_node_binary(OP_GE, L, R); 
}

// 加减法
add_expr(E) ::= mul_expr(M). { E = M; }
add_expr(E) ::= add_expr(L) TK_PLUS mul_expr(R). { 
    E = xl_node_binary(OP_ADD, L, R); 
}
add_expr(E) ::= add_expr(L) TK_MINUS mul_expr(R). { 
    E = xl_node_binary(OP_SUB, L, R); 
}

// 乘除法
mul_expr(E) ::= pow_expr(P). { E = P; }
mul_expr(E) ::= mul_expr(L) TK_MUL pow_expr(R). { 
    E = xl_node_binary(OP_MUL, L, R); 
}
mul_expr(E) ::= mul_expr(L) TK_DIV pow_expr(R). { 
    E = xl_node_binary(OP_DIV, L, R); 
}
mul_expr(E) ::= mul_expr(L) TK_MOD pow_expr(R). { 
    E = xl_node_binary(OP_MOD, L, R); 
}

// 乘方
pow_expr(E) ::= unary_expr(U). { E = U; }
pow_expr(E) ::= unary_expr(L) TK_POW pow_expr(R). { 
    E = xl_node_binary(OP_POW, L, R); 
}

// 一元表达式
unary_expr(E) ::= postfix_expr(P). { E = P; }
unary_expr(E) ::= TK_MINUS unary_expr(U). [TK_UMINUS] { 
    E = xl_node_unary(OP_NEG, U, true); 
}
unary_expr(E) ::= TK_INC postfix_expr(L). { 
    E = xl_node_unary(OP_INC, L, true); 
}
unary_expr(E) ::= TK_DEC postfix_expr(L). { 
    E = xl_node_unary(OP_DEC, L, true); 
}

// 后缀表达式
postfix_expr(E) ::= primary_expr(P). { E = P; }
postfix_expr(E) ::= postfix_expr(F) TK_LPAREN arg_list(A) TK_RPAREN. {
    E = xl_node_call(F, A ? (XL_Node**)A->block.stmts : 0, 
                     A ? A->block.count : 0);
    if (A) free(A);
}
postfix_expr(E) ::= postfix_expr(A) TK_LBRACKET expr(I) TK_RBRACKET. {
    E = xl_node_index(A, I);
}
postfix_expr(E) ::= postfix_expr(O) TK_DOT TK_IDENT(N). {
    E = xl_node_member(O, N->text);
}
postfix_expr(E) ::= postfix_expr(L) TK_INC. {
    E = xl_node_unary(OP_INC, L, false);
}
postfix_expr(E) ::= postfix_expr(L) TK_DEC. {
    E = xl_node_unary(OP_DEC, L, false);
}

// 参数列表
arg_list(L) ::= . { L = 0; }
arg_list(L) ::= arg_list_ne(A). { L = A; }

arg_list_ne(L) ::= expr(E). {
    L = xl_node_block();
    xl_node_add_stmt(L, E);
}
arg_list_ne(L) ::= arg_list_ne(A) TK_COMMA expr(E). {
    L = A;
    xl_node_add_stmt(L, E);
}

// 基础表达式
primary_expr(E) ::= TK_NIL. { E = xl_node_null(); }
primary_expr(E) ::= TK_TRUE. { E = xl_node_bool(true); }
primary_expr(E) ::= TK_FALSE. { E = xl_node_bool(false); }
primary_expr(E) ::= TK_INTEGER(T). { E = xl_node_int(T->ival); }
primary_expr(E) ::= TK_NUMBER(T). { E = xl_node_float(T->fval); }
primary_expr(E) ::= TK_STRING(T). { E = xl_node_string(T->sval); }
primary_expr(E) ::= TK_IDENT(T). { E = xl_node_ident(T->text); }
primary_expr(E) ::= TK_LPAREN expr(X) TK_RPAREN. { E = X; }
primary_expr(E) ::= array_literal(A). { E = A; }
primary_expr(E) ::= table_literal(T). { E = T; }

// 数组字面量
array_literal(A) ::= TK_LBRACKET array_elements(E) TK_RBRACKET. {
    A = xl_node_array(E ? (XL_Node**)E->block.stmts : 0,
                      E ? E->block.count : 0);
    if (E) free(E);
}

array_elements(E) ::= . { E = 0; }
array_elements(E) ::= array_elements_ne(L). { E = L; }

array_elements_ne(L) ::= expr(E). {
    L = xl_node_block();
    xl_node_add_stmt(L, E);
}
array_elements_ne(L) ::= array_elements_ne(A) TK_COMMA expr(E). {
    L = A;
    xl_node_add_stmt(L, E);
}

// 表字面量
table_literal(T) ::= TK_LBRACE table_entries(E) TK_RBRACE. {
    if (E) {
        // 从临时结构中提取 keys 和 values
        int count = E->block.count / 2;
        char** keys = (char**)malloc(sizeof(char*) * count);
        XL_Node** values = (XL_Node**)malloc(sizeof(XL_Node*) * count);
        for (int i = 0; i < count; i++) {
            keys[i] = (char*)E->block.stmts[i * 2];
            values[i] = E->block.stmts[i * 2 + 1];
        }
        T = xl_node_table(keys, values, count);
        free(E);
    } else {
        T = xl_node_table(0, 0, 0);
    }
}

table_entries(E) ::= . { E = 0; }
table_entries(E) ::= table_entries_ne(L). { E = L; }

table_entries_ne(L) ::= TK_IDENT(K) TK_ASSIGN expr(V). {
    L = xl_node_block();
    xl_node_add_stmt(L, (XL_Node*)strdup(K->text));
    xl_node_add_stmt(L, V);
}
table_entries_ne(L) ::= table_entries_ne(T) TK_COMMA TK_IDENT(K) TK_ASSIGN expr(V). {
    L = T;
    xl_node_add_stmt(L, (XL_Node*)strdup(K->text));
    xl_node_add_stmt(L, V);
}

%code {
// 额外的代码
}
