%{
%}



 /* 立即值 */
%token			XLTK_INTEGER
%token			XLTK_HEX
%token			XLTK_NUMBER
%token			XLTK_STRING
%token			XLTK_CHAR

 /* 关键字 */
%token			XLTK_VAR
%token			XLTK_NULL
%token			XLTK_TRUE
%token			XLTK_FALSE
%token			XLTK_IF
%token			XLTK_THEN
%token			XLTK_ELSE
%token			XLTK_ELSEIF
%token			XLTK_SELECT
%token			XLTK_CASE
%token			XLTK_DO
%token			XLTK_WHILE
%token			XLTK_FOR
%token			XLTK_IN
%token			XLTK_END
%token			XLTK_BREAK
%token			XLTK_CONTINUE
%token			XLTK_FUNCTION
%token			XLTK_RETURN

 /* 括号 */
%token			XLTK_PARANTHESES_L
%token			XLTK_PARANTHESES_R
%token			XLTK_BRACES_L
%token			XLTK_BRACES_R
%token			XLTK_BRACKETS_L
%token			XLTK_BRACKETS_R

 /* 运算符 */
%token			XLTK_OP_MORE
%token			XLTK_OP_LESS
%token			XLTK_OP_MORE_EQUAL
%token			XLTK_OP_LESS_EQUAL
%token			XLTK_OP_UNEQUAL
%token			XLTK_OP_EQUAL
%token			XLTK_OP_ADD
%token			XLTK_OP_SUB
%token			XLTK_OP_MUL
%token			XLTK_OP_DIV
%token			XLTK_OP_MOD
%token			XLTK_OP_POW
%token			XLTK_OP_BINAND
%token			XLTK_OP_BINOR
%token			XLTK_OP_BINNOT
%token			XLTK_OP_BINXOR
%token			XLTK_OP_BINRSH
%token			XLTK_OP_BINLSH
%token			XLTK_OP_NOT
%token			XLTK_OP_AND
%token			XLTK_OP_OR
%token			XLTK_OP_SET
%token			XLTK_OP_SET_ADD
%token			XLTK_OP_SET_SUB
%token			XLTK_OP_SET_MUL
%token			XLTK_OP_SET_DIV
%token			XLTK_OP_ADDONE
%token			XLTK_OP_SUBONE
%token			XLTK_OP_IS



%%

 /* 表达式 */
expression:

%%


