%name xlParse
%token_prefix XLPG_
%token_type { XL_Token }
%default_type { XL_AstNode* }
%extra_argument { XL_ParseContext* pCtx }

%include {
#include "src/ast/xl_ast.h"
#include "src/diag/xl_diag.h"

typedef struct XL_ParseContext {
	XL_AstNode* pRoot;
	XL_DiagContext* pDiag;
	int bHadError;
	int iErrorCount;
} XL_ParseContext;

static XL_AstNode* xlPList(void)
{
	return xlAstCreateList();
}
}

%syntax_error {
	pCtx->bHadError = 1;
	pCtx->iErrorCount += 1;
	if ( pCtx->pDiag != NULL ) {
		xlDiagEmit(
			pCtx->pDiag,
			XL_DIAG_LEVEL_ERROR,
			"parser",
			__FILE__,
			__LINE__,
			"syntax error near %s",
			xlTokenKindName(TOKEN.eKind)
		);
	}
}

%parse_failure {
	if ( pCtx->pDiag != NULL ) {
		xlDiagEmit(pCtx->pDiag, XL_DIAG_LEVEL_ERROR, "parser", __FILE__, __LINE__, "parse failure");
	}
}

%stack_overflow {
	if ( pCtx->pDiag != NULL ) {
		xlDiagEmit(pCtx->pDiag, XL_DIAG_LEVEL_ERROR, "parser", __FILE__, __LINE__, "parser stack overflow");
	}
}

%type async_opt { int }
%type then_opt { int }
%type for_bindings { XL_AstNode* }

%left OP_SHL OP_SHR.
%left OP_PLUS OP_MINUS.
%left OP_STAR OP_SLASH OP_PERCENT.
%right OP_POWER.
%right UMINUS KW_NOT OP_BANG KW_AWAIT.
%left OP_DOT OP_SAFE_DOT TK_LBRACKET OP_SAFE_LBRACKET TK_LPAREN.

program ::= stmt_list(L). {
	XL_AstNode* pProgram = xlAstCreateProgram();
	xlAstAppendChildren(pProgram, L);
	pCtx->pRoot = pProgram;
}

stmt_list(A) ::= . {
	A = xlPList();
}
stmt_list(A) ::= stmt_items(L) opt_stmt_sep. {
	A = L;
}

stmt_items(A) ::= stmt(S). {
	A = xlPList();
	if ( S != NULL ) {
		xlAstAppendChild(A, S);
	}
}
stmt_items(A) ::= stmt_items(L) stmt_sep stmt(S). {
	A = L;
	if ( S != NULL ) {
		xlAstAppendChild(A, S);
	}
}

stmt_sep ::= STMT_END.
stmt_sep ::= stmt_sep STMT_END.
opt_stmt_sep ::= .
opt_stmt_sep ::= stmt_sep.

stmt(A) ::= error. {
	A = NULL;
}
stmt(A) ::= func_decl(F). { A = F; }
stmt(A) ::= const_decl(C). { A = C; }
stmt(A) ::= if_stmt(I). { A = I; }
stmt(A) ::= try_stmt(T). { A = T; }
stmt(A) ::= for_in_stmt(F). { A = F; }
stmt(A) ::= import_stmt(I). { A = I; }
stmt(A) ::= export_stmt(E). { A = E; }
stmt(A) ::= KW_RETURN(T) expr_opt(E). { A = xlAstCreateReturn(&T, E); }
stmt(A) ::= KW_BREAK(T) INT(N). { A = xlAstCreateBreak(&T, (int)N.iIntValue); }
stmt(A) ::= KW_BREAK(T). { A = xlAstCreateBreak(&T, 1); }
stmt(A) ::= KW_CONTINUE(T) INT(N). { A = xlAstCreateContinue(&T, (int)N.iIntValue); }
stmt(A) ::= KW_CONTINUE(T). { A = xlAstCreateContinue(&T, 1); }
stmt(A) ::= KW_THROW(T) expr(E). { A = xlAstCreateThrow(&T, E); }
stmt(A) ::= KW_ASSERT(T) expr(E) assert_message(M). { A = xlAstCreateAssert(&T, E, M); }
stmt(A) ::= KW_YIELD(T) expr_opt(E). { A = xlAstCreateYield(&T, E); }
stmt(A) ::= expr(E). { A = xlAstCreateExprStmt(E); }

assert_message(A) ::= TK_COMMA expr(E). { A = E; }
assert_message(A) ::= . { A = NULL; }

async_opt(A) ::= KW_ASYNC. { A = 1; }
async_opt(A) ::= . { A = 0; }

func_decl(A) ::= async_opt(B) KW_FUNC TOK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN ret_type(R) opt_stmt_sep block(BODY) KW_END. {
	A = xlAstCreateFuncDecl(&N, B, P, R, BODY);
}

param_list(A) ::= . {
	A = xlPList();
}
param_list(A) ::= params(P). {
	A = P;
}

params(A) ::= param(P). {
	A = xlPList();
	xlAstAppendChild(A, P);
}
params(A) ::= params(L) TK_COMMA param(P). {
	A = L;
	xlAstAppendChild(A, P);
}

param(A) ::= TOK_IDENT(N). {
	A = xlAstCreateParam(&N, NULL, NULL);
}
param(A) ::= TOK_IDENT(T) TOK_IDENT(N). {
	A = xlAstCreateParam(&N, xlAstCreateTypeRef(&T), NULL);
}
param(A) ::= TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateParam(&N, NULL, E);
}
param(A) ::= TOK_IDENT(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateParam(&N, xlAstCreateTypeRef(&T), E);
}

ret_type(A) ::= . { A = NULL; }
ret_type(A) ::= KW_AS TOK_IDENT(T). { A = xlAstCreateTypeRef(&T); }

const_decl(A) ::= KW_CONST TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateConstDecl(&N, NULL, E);
}
const_decl(A) ::= KW_CONST TOK_IDENT(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateConstDecl(&N, xlAstCreateTypeRef(&T), E);
}

if_stmt(A) ::= KW_IF(T) expr(C) then_opt opt_stmt_sep block(B) elseif_list(EI) else_opt(EL) KW_END. {
	A = xlAstCreateIf(&T, C, B, EI, EL);
}

try_stmt(A) ::= KW_TRY(T) opt_stmt_sep block(B) KW_CATCH TOK_IDENT(N) opt_stmt_sep block(C) KW_END. {
	A = xlAstCreateTry(&T, B, xlAstCreateIdent(&N), C, NULL);
}
try_stmt(A) ::= KW_TRY(T) opt_stmt_sep block(B) KW_CATCH TOK_IDENT(N) opt_stmt_sep block(C) KW_FINALLY opt_stmt_sep block(F) KW_END. {
	A = xlAstCreateTry(&T, B, xlAstCreateIdent(&N), C, F);
}
try_stmt(A) ::= KW_TRY(T) opt_stmt_sep block(B) KW_FINALLY opt_stmt_sep block(F) KW_END. {
	A = xlAstCreateTry(&T, B, NULL, NULL, F);
}

for_in_stmt(A) ::= KW_FOR(T) for_bindings(BS) KW_IN expr(E) opt_stmt_sep block(B) KW_END. {
	A = xlAstCreateForIn(&T, BS, E, B);
}

for_bindings(A) ::= TOK_IDENT(N). {
	A = xlPList();
	xlAstAppendChild(A, xlAstCreateIdent(&N));
}
for_bindings(A) ::= for_bindings(L) TK_COMMA TOK_IDENT(N). {
	A = L;
	xlAstAppendChild(A, xlAstCreateIdent(&N));
}

then_opt(A) ::= KW_THEN. { A = 1; }
then_opt(A) ::= . { A = 0; }

elseif_list(A) ::= . {
	A = xlPList();
}
elseif_list(A) ::= elseif_list(L) KW_ELSEIF(T) expr(C) then_opt opt_stmt_sep block(B). {
	A = L;
	xlAstAppendChild(A, xlAstCreateElseIf(&T, C, B));
}

else_opt(A) ::= . { A = NULL; }
else_opt(A) ::= KW_ELSE opt_stmt_sep block(B). { A = B; }

block(A) ::= stmt_list(L). {
	A = xlAstCreateBlock(NULL);
	xlAstAppendChildren(A, L);
}

import_stmt(A) ::= KW_IMPORT(T) import_target(P). {
	A = xlAstCreateImport(&T, P);
}

import_target(A) ::= TOK_IDENT(N). {
	A = xlAstCreateIdent(&N);
}
import_target(A) ::= import_target(B) OP_DOT(T) TOK_IDENT(N). {
	A = xlAstCreateMember(B, &N, 0, &T);
}

export_stmt(A) ::= KW_EXPORT(T) expr(E). {
	A = xlAstCreateExport(&T, E);
}

expr_opt(A) ::= . { A = NULL; }
expr_opt(A) ::= expr(E). { A = E; }

arg_list(A) ::= . {
	A = xlPList();
}
arg_list(A) ::= args(L). {
	A = L;
}

args(A) ::= expr(E). {
	A = xlPList();
	xlAstAppendChild(A, E);
}
args(A) ::= args(L) TK_COMMA expr(E). {
	A = L;
	xlAstAppendChild(A, E);
}
args(A) ::= TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlPList();
	xlAstAppendChild(A, xlAstCreateNamedArg(&N, E));
}
args(A) ::= args(L) TK_COMMA TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = L;
	xlAstAppendChild(A, xlAstCreateNamedArg(&N, E));
}

expr(A) ::= null_expr(E). {
	A = E;
}

null_expr(A) ::= logical_or_expr(E). {
	A = E;
}
null_expr(A) ::= null_expr(L) OP_NULL_COALESCE(T) logical_or_expr(R). {
	A = xlAstCreateBinary(&T, "??", L, R);
}

logical_or_expr(A) ::= logical_and_expr(E). {
	A = E;
}
logical_or_expr(A) ::= logical_or_expr(L) KW_OR(T) logical_and_expr(R). {
	A = xlAstCreateBinary(&T, "or", L, R);
}

logical_and_expr(A) ::= bit_or_expr(E). {
	A = E;
}
logical_and_expr(A) ::= logical_and_expr(L) KW_AND(T) bit_or_expr(R). {
	A = xlAstCreateBinary(&T, "and", L, R);
}

bit_or_expr(A) ::= bit_xor_expr(E). {
	A = E;
}
bit_or_expr(A) ::= bit_or_expr(L) OP_PIPE(T) bit_xor_expr(R). {
	A = xlAstCreateBinary(&T, "|", L, R);
}

bit_xor_expr(A) ::= bit_and_expr(E). {
	A = E;
}
bit_xor_expr(A) ::= bit_xor_expr(L) OP_CARET(T) bit_and_expr(R). {
	A = xlAstCreateBinary(&T, "^", L, R);
}

bit_and_expr(A) ::= equality_expr(E). {
	A = E;
}
bit_and_expr(A) ::= bit_and_expr(L) OP_AMP(T) equality_expr(R). {
	A = xlAstCreateBinary(&T, "&", L, R);
}

equality_expr(A) ::= compare_expr(E). {
	A = E;
}
equality_expr(A) ::= equality_expr(L) OP_EQ(T) compare_expr(R). {
	A = xlAstCreateBinary(&T, "==", L, R);
}
equality_expr(A) ::= equality_expr(L) OP_NE(T) compare_expr(R). {
	A = xlAstCreateBinary(&T, "!=", L, R);
}
equality_expr(A) ::= equality_expr(L) OP_APPROX(T) compare_expr(R). {
	A = xlAstCreateBinary(&T, "~=", L, R);
}

compare_expr(A) ::= shift_expr(E). {
	A = E;
}
compare_expr(A) ::= compare_expr(L) OP_LT(T) shift_expr(R). {
	A = xlAstCreateBinary(&T, "<", L, R);
}
compare_expr(A) ::= compare_expr(L) OP_LE(T) shift_expr(R). {
	A = xlAstCreateBinary(&T, "<=", L, R);
}
compare_expr(A) ::= compare_expr(L) OP_GT(T) shift_expr(R). {
	A = xlAstCreateBinary(&T, ">", L, R);
}
compare_expr(A) ::= compare_expr(L) OP_GE(T) shift_expr(R). {
	A = xlAstCreateBinary(&T, ">=", L, R);
}
compare_expr(A) ::= compare_expr(L) KW_IN(T) shift_expr(R). {
	A = xlAstCreateBinary(&T, "in", L, R);
}

shift_expr(A) ::= add_expr(E). [OP_SHL] {
	A = E;
}
shift_expr(A) ::= shift_expr(L) OP_SHL(T) add_expr(R). {
	A = xlAstCreateBinary(&T, "<<", L, R);
}
shift_expr(A) ::= shift_expr(L) OP_SHR(T) add_expr(R). {
	A = xlAstCreateBinary(&T, ">>", L, R);
}

add_expr(A) ::= mul_expr(E). {
	A = E;
}
add_expr(A) ::= add_expr(L) OP_PLUS(T) mul_expr(R). {
	A = xlAstCreateBinary(&T, "+", L, R);
}
add_expr(A) ::= add_expr(L) OP_MINUS(T) mul_expr(R). {
	A = xlAstCreateBinary(&T, "-", L, R);
}

mul_expr(A) ::= power_expr(E). {
	A = E;
}
mul_expr(A) ::= mul_expr(L) OP_STAR(T) power_expr(R). {
	A = xlAstCreateBinary(&T, "*", L, R);
}
mul_expr(A) ::= mul_expr(L) OP_SLASH(T) power_expr(R). {
	A = xlAstCreateBinary(&T, "/", L, R);
}
mul_expr(A) ::= mul_expr(L) OP_PERCENT(T) power_expr(R). {
	A = xlAstCreateBinary(&T, "%", L, R);
}

power_expr(A) ::= unary_expr(E). {
	A = E;
}
power_expr(A) ::= unary_expr(L) OP_POWER(T) power_expr(R). {
	A = xlAstCreateBinary(&T, "**", L, R);
}

unary_expr(A) ::= postfix_expr(E). [KW_AWAIT] {
	A = E;
}
unary_expr(A) ::= OP_MINUS(T) unary_expr(E). {
	A = xlAstCreateUnary(&T, "-", E);
}
unary_expr(A) ::= KW_NOT(T) unary_expr(E). {
	A = xlAstCreateUnary(&T, "not", E);
}
unary_expr(A) ::= OP_BANG(T) unary_expr(E). {
	A = xlAstCreateUnary(&T, "!", E);
}
unary_expr(A) ::= KW_AWAIT(T) unary_expr(E). {
	A = xlAstCreateUnary(&T, "await", E);
}

postfix_expr(A) ::= primary_expr(E). {
	A = E;
}
postfix_expr(A) ::= postfix_expr(C) TK_LPAREN arg_list(ARGS) TK_RPAREN(T). {
	A = xlAstCreateCall(C, ARGS, &T);
}
postfix_expr(A) ::= postfix_expr(B) OP_DOT(T) TOK_IDENT(N). {
	A = xlAstCreateMember(B, &N, 0, &T);
}
postfix_expr(A) ::= postfix_expr(B) OP_SAFE_DOT(T) TOK_IDENT(N). {
	A = xlAstCreateMember(B, &N, 1, &T);
}
postfix_expr(A) ::= postfix_expr(B) TK_LBRACKET(T) expr(E) TK_RBRACKET. {
	A = xlAstCreateIndex(B, E, 0, &T);
}
postfix_expr(A) ::= postfix_expr(B) OP_SAFE_LBRACKET(T) expr(E) TK_RBRACKET. {
	A = xlAstCreateIndex(B, E, 1, &T);
}

primary_expr(A) ::= literal(L). {
	A = L;
}
primary_expr(A) ::= TOK_IDENT(N). {
	A = xlAstCreateIdent(&N);
}
primary_expr(A) ::= TK_LPAREN expr(E) TK_RPAREN. {
	A = E;
}

literal(A) ::= INT(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= INT_HEX(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= INT_OCT(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= INT_BIN(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= FLOAT(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= STRING(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= STRING_RAW(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= TEMPLATE(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= TIME(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= KW_TRUE(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= KW_FALSE(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= KW_NULL(T). { A = xlAstCreateLiteralFromToken(&T); }
