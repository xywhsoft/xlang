/**
 * xlang v4 - Lemon LALR(1) 语法定义
 * 
 * 从手写递归下降 Parser 迁移到 Lemon 语法生成器
 * 所有语义动作调用 xl_ast.h 中的 AST 构建函数
 * 
 * 设计说明：
 * - 容器字面量使用统一项列表，语义动作中判断 dict/set/array/list
 * - do-while 语法: do ... end while expr（end 显式终结块）
 * - 单行 if 由 Token Filter 注入合成 KW_END 处理
 * - 不支持 IDENT IDENT 变量声明（软关键字需用 var 前缀）
 */

/* Lemon 配置 */
%name xlParse
%token_prefix XLTP_
%token_type { XL_Token }
%default_type { xvalue }
%extra_argument { XL_ParseContext *pCtx }

%include {
/* 前向声明 - 实际类型由 main.c 的 include 链提供 */

#ifndef XL_PARSE_CONTEXT_DEFINED
#define XL_PARSE_CONTEXT_DEFINED
typedef struct XL_ParseContext {
	xvalue xAst;
	int bHadError;
	int iErrorCount;
	const char *pszFilename;
} XL_ParseContext;
#endif

/* 辅助函数：判断 brace_items 是否全部为 DictEntry */
static int xlBraceIsDictList( xvalue xItems )
{
	int iLen = xlAstArrayLen(xItems);
	if ( iLen == 0 ) return 1;
	for ( int i = 0; i < iLen; i++ ) {
		xvalue xItem = xlAstArrayGet(xItems, i);
		if ( xlAstGetKind(xItem) != AST_DICT_ENTRY ) return 0;
	}
	return 1;
}
}

%syntax_error {
	pCtx->bHadError = 1;
	pCtx->iErrorCount++;
	fprintf(stderr, "[%d:%d] 语法错误: 意外的 %s\n",
		TOKEN.line, TOKEN.column, xlTokenTypeName(TOKEN.type));
}

%parse_failure {
	fprintf(stderr, "致命: 语法分析失败\n");
}

%stack_overflow {
	fprintf(stderr, "致命: 语法分析栈溢出\n");
}

/* ================================================================
 * 优先级声明（从低到高）
 * ================================================================ */

%right OP_ASSIGN OP_PLUS_ASSIGN OP_MINUS_ASSIGN OP_STAR_ASSIGN
       OP_SLASH_ASSIGN OP_PERCENT_ASSIGN OP_POWER_ASSIGN OP_NULL_ASSIGN.
%right OP_QUESTION OP_COLON.
%left OP_RANGE.
%left OP_NULL_COALESCE.
%left KW_OR.
%left KW_AND.
%left OP_PIPE.
%left OP_CARET.
%left OP_AMP.
%left OP_EQ OP_NE OP_APPROX.
%left OP_LT OP_LE OP_GT OP_GE KW_IN.
%left OP_SHL OP_SHR.
%left OP_PLUS OP_MINUS.
%left OP_STAR OP_SLASH OP_PERCENT.
%right OP_POWER.
%right UMINUS KW_NOT OP_BANG.
%left OP_DOT OP_SAFE_DOT TOK_LBRACKET OP_SAFE_BRACKET TOK_LPAREN OP_INC OP_DEC.


/* ================================================================
 * 程序结构
 * ================================================================ */

program ::= stmt_list(L). {
	xvalue xModule = xlAstCreateModule(pCtx->pszFilename);
	int iLen = xlAstArrayLen(L);
	for ( int i = 0; i < iLen; i++ ) {
		xlAstModuleAddStmt(xModule, xlAstArrayGet(L, i));
	}
	pCtx->xAst = xModule;
}

%type stmt_list { xvalue }
stmt_list(A) ::= stmt_list(L) stmt(S). {
	A = L;
	if ( S ) xlAstArrayAppend(A, S);
}
stmt_list(A) ::= . {
	A = xlAstCreateArray();
}


/* ================================================================
 * 语句
 * ================================================================ */

%type stmt { xvalue }

/* --- 变量声明 --- */

/* var/auto name [= expr] */
stmt(A) ::= KW_VAR(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(N.text, (int)N.length, NULL, 0, E, 0, T.line, T.column);
}
stmt(A) ::= KW_VAR(T) TOK_IDENT(N). {
	A = xlAstCreateVarDecl(N.text, (int)N.length, NULL, 0, 0, 0, T.line, T.column);
}
stmt(A) ::= KW_AUTO(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(N.text, (int)N.length, NULL, 0, E, 0, T.line, T.column);
}
stmt(A) ::= KW_AUTO(T) TOK_IDENT(N). {
	A = xlAstCreateVarDecl(N.text, (int)N.length, NULL, 0, 0, 0, T.line, T.column);
}

/* 类型名变量声明: Type name [= expr] */
stmt(A) ::= type_kw(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(N.text, (int)N.length, T.text, (int)T.length, E, 0, T.line, T.column);
}
stmt(A) ::= type_kw(T) TOK_IDENT(N). {
	A = xlAstCreateVarDecl(N.text, (int)N.length, T.text, (int)T.length, 0, 0, T.line, T.column);
}

/* --- 常量声明 --- */

stmt(A) ::= KW_CONST(C) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(N.text, (int)N.length, NULL, 0, E, 1, C.line, C.column);
}
stmt(A) ::= KW_CONST(C) type_kw(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(N.text, (int)N.length, T.text, (int)T.length, E, 1, C.line, C.column);
}
stmt(A) ::= KW_CONST(C) KW_VAR TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(N.text, (int)N.length, NULL, 0, E, 1, C.line, C.column);
}
stmt(A) ::= KW_CONST(C) KW_AUTO TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(N.text, (int)N.length, NULL, 0, E, 1, C.line, C.column);
}

/* --- if 语句 --- */

stmt(A) ::= KW_IF(I) expr(COND) opt_then block(THEN) elseif_list(EIS) opt_else(ELSE) KW_END. {
	A = xlAstCreateIfStmt(COND, THEN, EIS, ELSE, I.line, I.column);
}

opt_then ::= KW_THEN.
opt_then ::= .

%type elseif_list { xvalue }
elseif_list(A) ::= elseif_list(L) KW_ELSEIF expr(COND) opt_then block(BODY). {
	A = L;
	xlAstArrayAppend(A, xlAstCreateElseIf(COND, BODY, 0, 0));
}
elseif_list(A) ::= . {
	A = xlAstCreateArray();
}

%type opt_else { xvalue }
opt_else(A) ::= KW_ELSE block(B). { A = B; }
opt_else(A) ::= . { A = 0; }

/* --- while 循环 --- */

stmt(A) ::= KW_WHILE(W) expr(COND) KW_DO block(BODY) KW_END. {
	A = xlAstCreateWhileStmt(COND, BODY, W.line, W.column);
}

/* --- do-while 循环: do ... end while expr --- */

stmt(A) ::= KW_DO(D) block(BODY) KW_END KW_WHILE expr(COND). {
	A = xlAstCreateDoWhileStmt(BODY, COND, D.line, D.column);
}

/* --- for 循环 --- */

stmt(A) ::= KW_FOR(F) TOK_IDENT(V) KW_IN expr(ITER) block(BODY) KW_END. {
	A = xlAstCreateForInStmt(NULL, 0, V.text, (int)V.length, ITER, BODY, F.line, F.column);
}

stmt(A) ::= KW_FOR(F) TOK_IDENT(K) TOK_COMMA TOK_IDENT(V) KW_IN expr(ITER) block(BODY) KW_END. {
	A = xlAstCreateForInStmt(K.text, (int)K.length, V.text, (int)V.length, ITER, BODY, F.line, F.column);
}

stmt(A) ::= KW_FOR(F) TOK_IDENT(V) OP_ASSIGN expr(START) KW_TO expr(END) block(BODY) KW_END. {
	A = xlAstCreateForStmt(V.text, (int)V.length, START, END, 0, BODY, F.line, F.column);
}

stmt(A) ::= KW_FOR(F) TOK_IDENT(V) OP_ASSIGN expr(START) KW_TO expr(END) KW_STEP expr(STEP) block(BODY) KW_END. {
	A = xlAstCreateForStmt(V.text, (int)V.length, START, END, STEP, BODY, F.line, F.column);
}

/* --- return / break / continue --- */

stmt(A) ::= KW_RETURN(R) expr(E). {
	A = xlAstCreateReturnStmt(E, R.line, R.column);
}
stmt(A) ::= KW_RETURN(R). {
	A = xlAstCreateReturnStmt(0, R.line, R.column);
}

stmt(A) ::= KW_BREAK(B) TOK_INT(N). {
	A = xlAstCreateBreakStmt((int)N.iVal, B.line, B.column);
}
stmt(A) ::= KW_BREAK(B). {
	A = xlAstCreateBreakStmt(1, B.line, B.column);
}

stmt(A) ::= KW_CONTINUE(C) TOK_INT(N). {
	A = xlAstCreateContinueStmt((int)N.iVal, C.line, C.column);
}
stmt(A) ::= KW_CONTINUE(C). {
	A = xlAstCreateContinueStmt(1, C.line, C.column);
}

/* --- 函数声明 --- */

stmt(A) ::= KW_FUNC(F) TOK_IDENT(N) TOK_LPAREN param_list(P) TOK_RPAREN ret_type(R) block(BODY) KW_END. {
	A = xlAstCreateFuncDecl(N.text, (int)N.length, P, R.text, (int)R.length, BODY, F.line, F.column);
}

/* --- 表达式语句 --- */

stmt(A) ::= expr(E). {
	int iLine = (int)xlAstGetInt(E, "line");
	int iCol = (int)xlAstGetInt(E, "col");
	A = xlAstCreateExprStmt(E, iLine, iCol);
}


/* ================================================================
 * 块（stmt_list 的别名，用于结构清晰）
 * ================================================================ */

%type block { xvalue }
block(A) ::= block_stmts(L). {
	int iLine = 0, iCol = 0;
	if ( xlAstArrayLen(L) > 0 ) {
		xvalue xFirst = xlAstArrayGet(L, 0);
		if ( xFirst ) {
			iLine = (int)xlAstGetInt(xFirst, "line");
			iCol = (int)xlAstGetInt(xFirst, "col");
		}
	}
	A = xlAstCreateBlockStmt(iLine, iCol);
	int iLen = xlAstArrayLen(L);
	for ( int i = 0; i < iLen; i++ ) {
		xlAstBlockAddStmt(A, xlAstArrayGet(L, i));
	}
}

%type block_stmts { xvalue }
block_stmts(A) ::= block_stmts(L) stmt(S). {
	A = L;
	if ( S ) xlAstArrayAppend(A, S);
}
block_stmts(A) ::= . {
	A = xlAstCreateArray();
}


/* ================================================================
 * 参数列表
 * ================================================================ */

%type param_list { xvalue }
param_list(A) ::= params(L). { A = L; }
param_list(A) ::= . { A = xlAstCreateArray(); }
param_list(A) ::= OP_ELLIPSIS(E). {
	A = xlAstCreateArray();
	xvalue xP = xlAstCreate(AST_PARAM_DECL, E.line, E.column);
	xlAstSetStr(xP, "name", "...", 3);
	xlAstSetBool(xP, "isVariadic", 1);
	xlAstArrayAppend(A, xP);
}
param_list(A) ::= OP_ELLIPSIS_DICT(E). {
	A = xlAstCreateArray();
	xvalue xP = xlAstCreate(AST_PARAM_DECL, E.line, E.column);
	xlAstSetStr(xP, "name", "***", 3);
	xlAstSetBool(xP, "isVariadicDict", 1);
	xlAstArrayAppend(A, xP);
}

%type params { xvalue }
params(A) ::= params(L) TOK_COMMA param(P). {
	A = L;
	xlAstArrayAppend(A, P);
}
params(A) ::= param(P). {
	A = xlAstCreateArray();
	xlAstArrayAppend(A, P);
}

%type param { xvalue }
param(A) ::= type_kw(T) TOK_IDENT(N) OP_ASSIGN expr(D). {
	A = xlAstCreateParamDecl(N.text, (int)N.length, T.text, (int)T.length, D, T.line, T.column);
}
param(A) ::= type_kw(T) TOK_IDENT(N). {
	A = xlAstCreateParamDecl(N.text, (int)N.length, T.text, (int)T.length, 0, T.line, T.column);
}
param(A) ::= KW_VAR TOK_IDENT(N). {
	A = xlAstCreateParamDecl(N.text, (int)N.length, NULL, 0, 0, N.line, N.column);
}
param(A) ::= KW_AUTO TOK_IDENT(N). {
	A = xlAstCreateParamDecl(N.text, (int)N.length, NULL, 0, 0, N.line, N.column);
}
param(A) ::= TOK_IDENT(N) OP_ASSIGN expr(D). {
	A = xlAstCreateParamDecl(N.text, (int)N.length, NULL, 0, D, N.line, N.column);
}
param(A) ::= TOK_IDENT(N). {
	A = xlAstCreateParamDecl(N.text, (int)N.length, NULL, 0, 0, N.line, N.column);
}


/* ================================================================
 * 返回类型
 * ================================================================ */

%type ret_type { XL_Token }
ret_type(A) ::= KW_AS type_kw(T). { A = T; }
ret_type(A) ::= KW_AS TOK_IDENT(T). { A = T; }
ret_type(A) ::= . { memset(&A, 0, sizeof(XL_Token)); }


/* ================================================================
 * 类型名关键字（不参与 expr 解析，仅用于声明语法）
 * ================================================================ */

%type type_kw { XL_Token }
type_kw(A) ::= KW_BOOL(T).   { A = T; }
type_kw(A) ::= KW_INT(T).    { A = T; }
type_kw(A) ::= KW_FLOAT(T).  { A = T; }
type_kw(A) ::= KW_STRING(T). { A = T; }
type_kw(A) ::= KW_TIME(T).   { A = T; }
type_kw(A) ::= KW_POINT(T).  { A = T; }
type_kw(A) ::= KW_ARRAY(T).  { A = T; }
type_kw(A) ::= KW_LIST(T).   { A = T; }
type_kw(A) ::= KW_SET(T).    { A = T; }
type_kw(A) ::= KW_DICT(T).   { A = T; }
type_kw(A) ::= KW_TYPE(T).   { A = T; }
type_kw(A) ::= KW_RANGE(T).  { A = T; }


/* ================================================================
 * 表达式
 * ================================================================ */

%type expr { xvalue }

/* --- 赋值 --- */
expr(A) ::= expr(L) OP_ASSIGN(O) expr(R).         { A = xlAstCreateAssignExpr("=", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_PLUS_ASSIGN(O) expr(R).    { A = xlAstCreateAssignExpr("+=", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_MINUS_ASSIGN(O) expr(R).   { A = xlAstCreateAssignExpr("-=", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_STAR_ASSIGN(O) expr(R).    { A = xlAstCreateAssignExpr("*=", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_SLASH_ASSIGN(O) expr(R).   { A = xlAstCreateAssignExpr("/=", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_PERCENT_ASSIGN(O) expr(R). { A = xlAstCreateAssignExpr("%=", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_POWER_ASSIGN(O) expr(R).   { A = xlAstCreateAssignExpr("**=", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_NULL_ASSIGN(O) expr(R).    { A = xlAstCreateAssignExpr("?=", L, R, O.line, O.column); }

/* --- 三元 --- */
expr(A) ::= expr(COND) OP_QUESTION(O) expr(THEN) OP_COLON expr(ELSE). {
	A = xlAstCreateTernaryExpr(COND, THEN, ELSE, O.line, O.column);
}

/* --- 二元运算符 --- */
expr(A) ::= expr(L) OP_RANGE(O) expr(R).          { A = xlAstCreateBinaryExpr("..", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_NULL_COALESCE(O) expr(R).  { A = xlAstCreateBinaryExpr("??", L, R, O.line, O.column); }
expr(A) ::= expr(L) KW_OR(O) expr(R).             { A = xlAstCreateBinaryExpr("or", L, R, O.line, O.column); }
expr(A) ::= expr(L) KW_AND(O) expr(R).            { A = xlAstCreateBinaryExpr("and", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_PIPE(O) expr(R).           { A = xlAstCreateBinaryExpr("|", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_CARET(O) expr(R).          { A = xlAstCreateBinaryExpr("^", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_AMP(O) expr(R).            { A = xlAstCreateBinaryExpr("&", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_EQ(O) expr(R).             { A = xlAstCreateBinaryExpr("==", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_NE(O) expr(R).             { A = xlAstCreateBinaryExpr("!=", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_APPROX(O) expr(R).         { A = xlAstCreateBinaryExpr("~=", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_LT(O) expr(R).             { A = xlAstCreateBinaryExpr("<", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_LE(O) expr(R).             { A = xlAstCreateBinaryExpr("<=", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_GT(O) expr(R).             { A = xlAstCreateBinaryExpr(">", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_GE(O) expr(R).             { A = xlAstCreateBinaryExpr(">=", L, R, O.line, O.column); }
expr(A) ::= expr(L) KW_IN(O) expr(R).             { A = xlAstCreateBinaryExpr("in", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_SHL(O) expr(R).            { A = xlAstCreateBinaryExpr("<<", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_SHR(O) expr(R).            { A = xlAstCreateBinaryExpr(">>", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_PLUS(O) expr(R).           { A = xlAstCreateBinaryExpr("+", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_MINUS(O) expr(R).          { A = xlAstCreateBinaryExpr("-", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_STAR(O) expr(R).           { A = xlAstCreateBinaryExpr("*", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_SLASH(O) expr(R).          { A = xlAstCreateBinaryExpr("/", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_PERCENT(O) expr(R).        { A = xlAstCreateBinaryExpr("%", L, R, O.line, O.column); }
expr(A) ::= expr(L) OP_POWER(O) expr(R).          { A = xlAstCreateBinaryExpr("**", L, R, O.line, O.column); }

/* --- 一元运算符 --- */
expr(A) ::= OP_MINUS(O) expr(R). [UMINUS]  { A = xlAstCreateUnaryExpr("-", R, 1, O.line, O.column); }
expr(A) ::= KW_NOT(O) expr(R).             { A = xlAstCreateUnaryExpr("not", R, 1, O.line, O.column); }
expr(A) ::= OP_BANG(O) expr(R).            { A = xlAstCreateUnaryExpr("!", R, 1, O.line, O.column); }

/* --- 后缀 --- */
expr(A) ::= expr(L) OP_INC(O).            { A = xlAstCreateUnaryExpr("++", L, 0, O.line, O.column); }
expr(A) ::= expr(L) OP_DEC(O).            { A = xlAstCreateUnaryExpr("--", L, 0, O.line, O.column); }

/* --- 成员访问 --- */
expr(A) ::= expr(L) OP_DOT(O) TOK_IDENT(M). {
	A = xlAstCreateMemberExpr(L, M.text, (int)M.length, 0, O.line, O.column);
}
expr(A) ::= expr(L) OP_SAFE_DOT(O) TOK_IDENT(M). {
	A = xlAstCreateMemberExpr(L, M.text, (int)M.length, 1, O.line, O.column);
}

/* --- 索引 --- */
expr(A) ::= expr(L) TOK_LBRACKET(O) expr(IDX) TOK_RBRACKET. {
	A = xlAstCreateIndexExpr(L, IDX, O.line, O.column);
}
expr(A) ::= expr(L) OP_SAFE_BRACKET(O) expr(IDX) TOK_RBRACKET. {
	xvalue xN = xlAstCreateIndexExpr(L, IDX, O.line, O.column);
	xlAstSetBool(xN, "isSafe", 1);
	A = xN;
}

/* --- 函数调用 --- */
expr(A) ::= expr(CALLEE) TOK_LPAREN(O) arg_list(ARGS) TOK_RPAREN. {
	A = xlAstCreateCallExpr(CALLEE, ARGS, O.line, O.column);
}

%type arg_list { xvalue }
arg_list(A) ::= arg_items(L). { A = L; }
arg_list(A) ::= . { A = xlAstCreateArray(); }

%type arg_items { xvalue }
arg_items(A) ::= arg_items(L) TOK_COMMA expr(E). { A = L; xlAstArrayAppend(A, E); }
arg_items(A) ::= expr(E). { A = xlAstCreateArray(); xlAstArrayAppend(A, E); }


/* ================================================================
 * 基础表达式
 * ================================================================ */

/* 整数 */
expr(A) ::= TOK_INT(T).     { A = xlAstCreateIntLit(T.iVal, T.line, T.column); }
expr(A) ::= TOK_INT_HEX(T). { A = xlAstCreateIntLit(T.iVal, T.line, T.column); }
expr(A) ::= TOK_INT_OCT(T). { A = xlAstCreateIntLit(T.iVal, T.line, T.column); }
expr(A) ::= TOK_INT_BIN(T). { A = xlAstCreateIntLit(T.iVal, T.line, T.column); }

/* 浮点 */
expr(A) ::= TOK_FLOAT(T). { A = xlAstCreateFloatLit(T.fVal, T.line, T.column); }

/* 字符串 */
expr(A) ::= TOK_STRING(T).          { A = xlAstCreateStringLit(T.text + 1, (int)T.length - 2, T.line, T.column); }
expr(A) ::= TOK_STRING_RAW(T).      { A = xlAstCreateStringLit(T.text + 1, (int)T.length - 2, T.line, T.column); }
expr(A) ::= TOK_STRING_MULTI(T).    { A = xlAstCreateStringLit(T.text + 3, (int)T.length - 6, T.line, T.column); }
expr(A) ::= TOK_STRING_MULTI_RAW(T). { A = xlAstCreateStringLit(T.text + 3, (int)T.length - 6, T.line, T.column); }

/* 时间 */
expr(A) ::= TOK_TIME(T). { A = xlAstCreateTimeLit(T.text + 1, (int)T.length - 2, T.line, T.column); }

/* 布尔 */
expr(A) ::= KW_TRUE(T).  { A = xlAstCreateBoolLit(1, T.line, T.column); }
expr(A) ::= KW_FALSE(T). { A = xlAstCreateBoolLit(0, T.line, T.column); }

/* null */
expr(A) ::= KW_NULL(T). { A = xlAstCreateNullLit(T.line, T.column); }

/* 标识符 */
expr(A) ::= TOK_IDENT(T). { A = xlAstCreateIdent(T.text, (int)T.length, T.line, T.column); }

/* arg / function 关键字作为标识符 */
expr(A) ::= KW_ARG(T).      { A = xlAstCreateIdent("arg", 3, T.line, T.column); }
expr(A) ::= KW_FUNCTION(T). { A = xlAstCreateIdent("function", 8, T.line, T.column); }

/* 类型名在表达式中作为标识符（用于类型转换如 int(3.14)） */
expr(A) ::= type_kw(T). { A = xlAstCreateIdent(T.text, (int)T.length, T.line, T.column); }

/* 括号表达式 */
expr(A) ::= TOK_LPAREN expr(E) TOK_RPAREN. { A = E; }


/* ================================================================
 * 容器字面量
 * ================================================================ */

/* --- 方括号 [] --- */

/* 空数组 */
expr(A) ::= TOK_LBRACKET(T) TOK_RBRACKET. {
	A = xlAstCreateArrayLit(xlAstCreateArray(), T.line, T.column);
}

/* 数组或列表: [items] */
expr(A) ::= TOK_LBRACKET(T) bracket_items(L) TOK_RBRACKET. {
	/* 检查是否包含 DictEntry → 列表(key:val)，否则 → 数组 */
	if ( xlBraceIsDictList(L) ) {
		A = xlAstCreateDictLit(L, T.line, T.column);
	} else {
		A = xlAstCreateArrayLit(L, T.line, T.column);
	}
}

%type bracket_items { xvalue }
bracket_items(A) ::= bracket_items(L) TOK_COMMA bracket_item(I). {
	A = L; xlAstArrayAppend(A, I);
}
bracket_items(A) ::= bracket_item(I). {
	A = xlAstCreateArray(); xlAstArrayAppend(A, I);
}

%type bracket_item { xvalue }
bracket_item(A) ::= expr(K) OP_COLON expr(V). {
	A = xlAstCreateDictEntry(K, V, 0, 0);
}
bracket_item(A) ::= expr(E). {
	A = E;
}


/* --- 花括号 {} --- */

/* 空字典 */
expr(A) ::= TOK_LBRACE(T) TOK_RBRACE. {
	A = xlAstCreateDictLit(xlAstCreateArray(), T.line, T.column);
}

/* 字典或集合: {items} */
expr(A) ::= TOK_LBRACE(T) brace_items(L) TOK_RBRACE. {
	if ( xlBraceIsDictList(L) ) {
		A = xlAstCreateDictLit(L, T.line, T.column);
	} else {
		A = xlAstCreateSetLit(L, T.line, T.column);
	}
}

%type brace_items { xvalue }
brace_items(A) ::= brace_items(L) TOK_COMMA brace_item(I). {
	A = L; xlAstArrayAppend(A, I);
}
brace_items(A) ::= brace_item(I). {
	A = xlAstCreateArray(); xlAstArrayAppend(A, I);
}

%type brace_item { xvalue }
/* 字典项: ident : expr 或 ident = expr */
brace_item(A) ::= TOK_IDENT(K) OP_COLON expr(V). {
	xvalue xKey = xlAstCreateStringLit(K.text, (int)K.length, K.line, K.column);
	A = xlAstCreateDictEntry(xKey, V, K.line, K.column);
}
brace_item(A) ::= TOK_IDENT(K) OP_ASSIGN expr(V). {
	xvalue xKey = xlAstCreateStringLit(K.text, (int)K.length, K.line, K.column);
	A = xlAstCreateDictEntry(xKey, V, K.line, K.column);
}
/* 字典项: string : expr */
brace_item(A) ::= TOK_STRING(K) OP_COLON expr(V). {
	xvalue xKey = xlAstCreateStringLit(K.text + 1, (int)K.length - 2, K.line, K.column);
	A = xlAstCreateDictEntry(xKey, V, K.line, K.column);
}
/* 集合项 */
brace_item(A) ::= expr(E). {
	A = E;
}
