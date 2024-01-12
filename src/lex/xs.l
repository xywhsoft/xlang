%option noyywrap
%option nounput
%option noinput



%{
	/* OPTIONS:
		noyywrap    扫描单文件，不调用yywrap()
		nounput     unput将字符插入输入流
		noinput     input返回输入流的下一单个字符
		这些选项通常都开启，除非特殊使用
	*/
	
	#include "xs.tab.h"
	
	// 行号记录
	int iLine = 1;
	
%}



/* 空白字符 */
WhiteSpace			[ \t\r]*
Linenr				\n

/* 注释 */
LineComment			"//".*
BlockComment		"/*"[^(*/)]*"*/"

/* 数值立即值 */
Integer				[+-]?[0-9]+
Hex					0[xX][0-9a-fA-F]+
Number				[+-]?[0-9]+\.[0-9]+([Ee][+-]?[0-9]+)?
String				\"[^\"]*\"
Char				\'[^\']*\'

/* 标识符 */
Identifier			[a-zA-Z_][a-zA-Z0-9_]*



%%

 /* 空白字符和注释 */
{WhiteSpace}		{ printf("Lexer - WhiteSpace : %s\n", yytext); }
{Linenr}			{ iLine++; printf("Lexer - Linenr : %d\n", iLine); }
{LineComment}		{ printf("Lexer - LineComment : %s\n", yytext); }
{BlockComment}		{ printf("Lexer - BlockComment : %s\n", yytext); }

 /* 变量声明 */
var					{ printf("Lexer - var : %s\n", yytext); return XLTK_VAR; }
null				{ printf("Lexer - null : %s\n", yytext); return XLTK_NULL; }
true				{ printf("Lexer - true : %s\n", yytext); return XLTK_TRUE; }
false				{ printf("Lexer - false : %s\n", yytext); return XLTK_FALSE; }

 /* 条件分支语句 */
if					{ printf("Lexer - if : %s\n", yytext); return XLTK_IF; }
then				{ printf("Lexer - then : %s\n", yytext); return XLTK_THEN; }
else				{ printf("Lexer - else : %s\n", yytext); return XLTK_ELSE; }
elseif				{ printf("Lexer - elseif : %s\n", yytext); return XLTK_ELSEIF; }
select				{ printf("Lexer - select : %s\n", yytext); return XLTK_SELECT; }
case				{ printf("Lexer - case : %s\n", yytext); return XLTK_CASE; }

 /* 循环语句 */
do					{ printf("Lexer - do : %s\n", yytext); return XLTK_DO; }
while				{ printf("Lexer - while : %s\n", yytext); return XLTK_WHILE; }
for					{ printf("Lexer - for : %s\n", yytext); return XLTK_FOR; }
in					{ printf("Lexer - in : %s\n", yytext); return XLTK_IN; }

 /* 代码块结束、跳出符号 */
end					{ printf("Lexer - end : %s\n", yytext); return XLTK_END; }
break				{ printf("Lexer - break : %s\n", yytext); return XLTK_BREAK; }
continue			{ printf("Lexer - continue : %s\n", yytext); return XLTK_CONTINUE; }

 /* 函数声明 */
function			{ printf("Lexer - function : %s\n", yytext); return XLTK_FUNCTION; }
return				{ printf("Lexer - return : %s\n", yytext); return XLTK_RETURN; }

 /* 命名空间声明 */

 /* 括号 */
\(					{ printf("Lexer - parentheses left : %s\n", yytext); return XLTK_PARANTHESES_L; }
\)					{ printf("Lexer - parentheses right : %s\n", yytext); return XLTK_PARANTHESES_R; }
\{					{ printf("Lexer - braces left : %s\n", yytext); return XLTK_BRACES_L; }
\}					{ printf("Lexer - braces right : %s\n", yytext); return XLTK_BRACES_R; }
\[					{ printf("Lexer - brackets left : %s\n", yytext); return XLTK_BRACKETS_L; }
\]					{ printf("Lexer - brackets right : %s\n", yytext); return XLTK_BRACKETS_R; }

 /* 比较运算符 */
[>]					{ printf("Lexer - opera [>] : %s\n", yytext); return XLTK_OP_MORE; }
[<]					{ printf("Lexer - opera [<] : %s\n", yytext); return XLTK_OP_LESS; }
[>][=]				{ printf("Lexer - opera [>=] : %s\n", yytext); return XLTK_OP_MORE_EQUAL; }
[<][=]				{ printf("Lexer - opera [<=] : %s\n", yytext); return XLTK_OP_LESS_EQUAL; }
[!][=]				{ printf("Lexer - opera [!=] : %s\n", yytext); return XLTK_OP_UNEQUAL; }
[=][=]				{ printf("Lexer - opera [==] : %s\n", yytext); return XLTK_OP_EQUAL; }

 /* 数学运算符 */
[+]					{ printf("Lexer - opera [+] : %s\n", yytext); return XLTK_OP_ADD; }
[-]					{ printf("Lexer - opera [-] : %s\n", yytext); return XLTK_OP_SUB; }
[*]					{ printf("Lexer - opera [*] : %s\n", yytext); return XLTK_OP_MUL; }
[/]					{ printf("Lexer - opera [/] : %s\n", yytext); return XLTK_OP_DIV; }
[%]					{ printf("Lexer - opera [%%] : %s\n", yytext); return XLTK_OP_MOD; }
[*][*]				{ printf("Lexer - opera [**] : %s\n", yytext); return XLTK_OP_POW; }

 /* 位运算符 */
[&]					{ printf("Lexer - opera [&] : %s\n", yytext); return XLTK_OP_BINAND; }
[|]					{ printf("Lexer - opera [|] : %s\n", yytext); return XLTK_OP_BINOR; }
[~]					{ printf("Lexer - opera [~] : %s\n", yytext); return XLTK_OP_BINNOT; }
[\^]				{ printf("Lexer - opera [^] : %s\n", yytext); return XLTK_OP_BINXOR; }
[>][>]				{ printf("Lexer - opera [>>] : %s\n", yytext); return XLTK_OP_BINRSH; }
[<][<]				{ printf("Lexer - opera [<<] : %s\n", yytext); return XLTK_OP_BINLSH; }

 /* 逻辑运算符 */
[!]					{ printf("Lexer - opera [!] : %s\n", yytext); return XLTK_OP_NOT; }
[&][&]				{ printf("Lexer - opera [&&] : %s\n", yytext); return XLTK_OP_AND; }
[|][|]				{ printf("Lexer - opera [||] : %s\n", yytext); return XLTK_OP_OR; }

 /* 赋值运算符 */
[=]					{ printf("Lexer - opera [=] : %s\n", yytext); return XLTK_OP_SET; }
[+][=]				{ printf("Lexer - opera [+=] : %s\n", yytext); return XLTK_OP_SET_ADD; }
[-][=]				{ printf("Lexer - opera [-=] : %s\n", yytext); return XLTK_OP_SET_SUB; }
[*][=]				{ printf("Lexer - opera [*=] : %s\n", yytext); return XLTK_OP_SET_MUL; }
[/][=]				{ printf("Lexer - opera [/=] : %s\n", yytext); return XLTK_OP_SET_DIV; }
[+][+]				{ printf("Lexer - opera [++] : %s\n", yytext); return XLTK_OP_ADDONE; }
[-][-]				{ printf("Lexer - opera [--] : %s\n", yytext); return XLTK_OP_SUBONE; }
is					{ printf("Lexer - opera [is] : %s\n", yytext); return XLTK_OP_IS; }

 /* 指针运算符 */
[@]					{ printf("Lexer - opera [@] : %s\n", yytext); }
[#]					{ printf("Lexer - opera [#] : %s\n", yytext); }

 /* 三目运算符 */
[\?]				{ printf("Lexer - opera [?] : %s\n", yytext); }
[:]					{ printf("Lexer - opera [:] : %s\n", yytext); }

 /* 访问符号 */
[\.]				{ printf("Lexer - symbol [.] : %s\n", yytext); }

 /* 分行符号 */
[;]					{ printf("Lexer - symbol [;] : %s\n", yytext); }

 /* 动态参数符号 */
[\.][\.][\.]		{ printf("Lexer - symbol [...] : %s\n", yytext); }

 /* 其他符号 */
[,]					{ printf("Lexer - symbol [,] : %s\n", yytext); }

 /* 标识符 */
{Identifier}		{ printf("Lexer - Identifier : %s\n", yytext); }

 /* 立即值 */
{Integer}			{ printf("Lexer - Integer : %s\n", yytext); return XLTK_INTEGER; }
{Hex}				{ printf("Lexer - Hex : %s\n", yytext); return XLTK_HEX; }
{Number}			{ printf("Lexer - Number : %s\n", yytext); return XLTK_NUMBER; }
{String}			{ printf("Lexer - String [\"] : %s\n", yytext); return XLTK_STRING; }
{Char}				{ printf("Lexer - Char ['] : %s\n", yytext); return XLTK_CHAR; }

%%



// 用户自定义代码段


