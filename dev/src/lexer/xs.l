%option reentrant
%option noyywrap
%option nounput
%option noinput
%option outfile="xs_lex.c"
%option header-file="xs_lex.h"
%option prefix="xs_"



%top{
	/* 固定在生成文件顶部的代码块 */
}



%{
	/* OPTIONS:
		reentrant		生成可重入式的扫描器
		noyywrap		不调用 yywrap
		nodefault		禁止扫描到不符合规则的输入时输出到 stdout 的行为
		nounput			
		noinput			
		outfile			输出文件名
		header-file		生成头文件名
		prefix			修改函数前缀
	*/
	
	#include "xs_define.h"
	
	
	
	//#define DebugMode
	
	
	
	#ifdef DebugMode
		#define XS_DEBUGPRINT printf
	#else
		#define XS_DEBUGPRINT
	#endif
	
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
String				\'[^\']*\'
EscapeString		\"[^\"]*\"
TempletString		`[^`]*`

/* 标识符 */
Identifier			[a-zA-Z_][a-zA-Z0-9_]*



%%

 /* 空白字符和注释 */
{WhiteSpace}		{ XS_DEBUGPRINT("Lexer - WhiteSpace : %s\n", yytext); }
{Linenr}			{ XS_DEBUGPRINT("Lexer - Linenr : %d\n", iLine); iLine++; }
{LineComment}		{ XS_DEBUGPRINT("Lexer - LineComment : %s\n", yytext); }
{BlockComment}		{ XS_DEBUGPRINT("Lexer - BlockComment : %s\n", yytext); }

 /* 变量声明 */
null				{ XS_DEBUGPRINT("Lexer - null : %s\n", yytext); return XLTK_NULL; }
true				{ XS_DEBUGPRINT("Lexer - true : %s\n", yytext); return XLTK_TRUE; }
false				{ XS_DEBUGPRINT("Lexer - false : %s\n", yytext); return XLTK_FALSE; }

 /* 条件分支语句 */
if					{ XS_DEBUGPRINT("Lexer - if : %s\n", yytext); return XLTK_IF; }
then				{ XS_DEBUGPRINT("Lexer - then : %s\n", yytext); return XLTK_THEN; }
elseif				{ XS_DEBUGPRINT("Lexer - elseif : %s\n", yytext); return XLTK_ELSEIF; }
else				{ XS_DEBUGPRINT("Lexer - else : %s\n", yytext); return XLTK_ELSE; }

 /* 循环语句 */
do					{ XS_DEBUGPRINT("Lexer - do : %s\n", yytext); return XLTK_DO; }
while				{ XS_DEBUGPRINT("Lexer - while : %s\n", yytext); return XLTK_WHILE; }
for					{ XS_DEBUGPRINT("Lexer - for : %s\n", yytext); return XLTK_FOR; }
in					{ XS_DEBUGPRINT("Lexer - in : %s\n", yytext); return XLTK_IN; }
to					{ XS_DEBUGPRINT("Lexer - in : %s\n", yytext); return XLTK_TO; }

 /* 代码块结束、跳出符号 */
end					{ XS_DEBUGPRINT("Lexer - end : %s\n", yytext); return XLTK_END; }
break				{ XS_DEBUGPRINT("Lexer - break : %s\n", yytext); return XLTK_BREAK; }
continue			{ XS_DEBUGPRINT("Lexer - continue : %s\n", yytext); return XLTK_CONTINUE; }

 /* 函数声明 */
function			{ XS_DEBUGPRINT("Lexer - function : %s\n", yytext); return XLTK_FUNCTION; }
return				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_RETURN; }

 /* 结构体声明 */
struct				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_STRUCT; }
union				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_UNION; }

 /* 对象声明 */
object				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_OBJECT; }
get					{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_GET; }
set					{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_SET; }
meta				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_META; }

 /* 对象创建与释放 */
new					{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_NEW; }
delete				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_DELETE; }

 /* 预处理语句 */
#A					{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_ASTR; }
#W					{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_WSTR; }
#U					{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_USTR; }
#T					{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_TSTR; }
#define				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_DEFINE; }
#undef				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_UNDEF; }
#if					{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_IF; }
#ifdef				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_IFDEF; }
#ifndef				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_IFNDEF; }
#else				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_ELSE; }
#endif				{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_ENDIF; }
#include			{ XS_DEBUGPRINT("Lexer - return : %s\n", yytext); return XLTK_PRE_INCLUDE; }



 /* 括号 */
\(					{ XS_DEBUGPRINT("Lexer - parentheses left : %s\n", yytext); return XLTK_PARANTHESES_L; }
\)					{ XS_DEBUGPRINT("Lexer - parentheses right : %s\n", yytext); return XLTK_PARANTHESES_R; }
\{					{ XS_DEBUGPRINT("Lexer - braces left : %s\n", yytext); return XLTK_BRACES_L; }
\}					{ XS_DEBUGPRINT("Lexer - braces right : %s\n", yytext); return XLTK_BRACES_R; }
\[					{ XS_DEBUGPRINT("Lexer - brackets left : %s\n", yytext); return XLTK_BRACKETS_L; }
\]					{ XS_DEBUGPRINT("Lexer - brackets right : %s\n", yytext); return XLTK_BRACKETS_R; }

 /* 比较运算符 */
[>]					{ XS_DEBUGPRINT("Lexer - opera [>] : %s\n", yytext); return XLTK_OP_MORE; }
[<]					{ XS_DEBUGPRINT("Lexer - opera [<] : %s\n", yytext); return XLTK_OP_LESS; }
[>][=]				{ XS_DEBUGPRINT("Lexer - opera [>=] : %s\n", yytext); return XLTK_OP_MORE_EQUAL; }
[<][=]				{ XS_DEBUGPRINT("Lexer - opera [<=] : %s\n", yytext); return XLTK_OP_LESS_EQUAL; }
[=][=]				{ XS_DEBUGPRINT("Lexer - opera [==] : %s\n", yytext); return XLTK_OP_EQUAL; }
[!][=]				{ XS_DEBUGPRINT("Lexer - opera [!=] : %s\n", yytext); return XLTK_OP_UNEQUAL; }
[~][=]				{ XS_DEBUGPRINT("Lexer - opera [!=] : %s\n", yytext); return XLTK_OP_APPREQUAL; }

 /* 数学运算符 */
[+]					{ XS_DEBUGPRINT("Lexer - opera [+] : %s\n", yytext); return XLTK_OP_ADD; }
[-]					{ XS_DEBUGPRINT("Lexer - opera [-] : %s\n", yytext); return XLTK_OP_SUB; }
[*]					{ XS_DEBUGPRINT("Lexer - opera [*] : %s\n", yytext); return XLTK_OP_MUL; }
[/]					{ XS_DEBUGPRINT("Lexer - opera [/] : %s\n", yytext); return XLTK_OP_DIV; }
[%]					{ XS_DEBUGPRINT("Lexer - opera [%%] : %s\n", yytext); return XLTK_OP_MOD; }
[*][*]				{ XS_DEBUGPRINT("Lexer - opera [**] : %s\n", yytext); return XLTK_OP_POW; }

 /* 位运算符 */
[&]					{ XS_DEBUGPRINT("Lexer - opera [&] : %s\n", yytext); return XLTK_OP_BINAND; }
[|]					{ XS_DEBUGPRINT("Lexer - opera [|] : %s\n", yytext); return XLTK_OP_BINOR; }
[!]					{ XS_DEBUGPRINT("Lexer - opera [!] : %s\n", yytext); return XLTK_OP_BINNOT; }
[\^]				{ XS_DEBUGPRINT("Lexer - opera [^] : %s\n", yytext); return XLTK_OP_BINXOR; }
[>][>]				{ XS_DEBUGPRINT("Lexer - opera [>>] : %s\n", yytext); return XLTK_OP_BINRSH; }
[<][<]				{ XS_DEBUGPRINT("Lexer - opera [<<] : %s\n", yytext); return XLTK_OP_BINLSH; }

 /* 逻辑运算符 */
not					{ XS_DEBUGPRINT("Lexer - opera [not] : %s\n", yytext); return XLTK_OP_NOT; }
and					{ XS_DEBUGPRINT("Lexer - opera [and] : %s\n", yytext); return XLTK_OP_AND; }
or					{ XS_DEBUGPRINT("Lexer - opera [or] : %s\n", yytext); return XLTK_OP_OR; }

 /* 赋值运算符 */
[=]					{ XS_DEBUGPRINT("Lexer - opera [=] : %s\n", yytext); return XLTK_OP_SET; }
[+][=]				{ XS_DEBUGPRINT("Lexer - opera [+=] : %s\n", yytext); return XLTK_OP_SET_ADD; }
[-][=]				{ XS_DEBUGPRINT("Lexer - opera [-=] : %s\n", yytext); return XLTK_OP_SET_SUB; }
[*][=]				{ XS_DEBUGPRINT("Lexer - opera [*=] : %s\n", yytext); return XLTK_OP_SET_MUL; }
[/][=]				{ XS_DEBUGPRINT("Lexer - opera [/=] : %s\n", yytext); return XLTK_OP_SET_DIV; }
[+][+]				{ XS_DEBUGPRINT("Lexer - opera [++] : %s\n", yytext); return XLTK_OP_ADDONE; }
[-][-]				{ XS_DEBUGPRINT("Lexer - opera [--] : %s\n", yytext); return XLTK_OP_SUBONE; }

 /* 三目运算符 */
[\?]				{ XS_DEBUGPRINT("Lexer - opera [?] : %s\n", yytext); return XLTK_OP_QUEST; }
[:]					{ XS_DEBUGPRINT("Lexer - opera [:] : %s\n", yytext); return XLTK_OP_COLON; }

 /* 为空赋值运算符 */
 [\?][\?]			{ XS_DEBUGPRINT("Lexer - opera [??] : %s\n", yytext); return XLTK_OP_QUESTDET; }
 [\?][\?][=]		{ XS_DEBUGPRINT("Lexer - opera [??=] : %s\n", yytext); return XLTK_OP_QUESTSET; }

 /* 访问符号 */
[\.]				{ XS_DEBUGPRINT("Lexer - symbol [.] : %s\n", yytext); return XLTK_OP_DOT; }
[\.][\?]			{ XS_DEBUGPRINT("Lexer - symbol [.?] : %s\n", yytext); return XLTK_OP_QUESTDOT; }

 /* 数据类型判断运算符 */
is					{ XS_DEBUGPRINT("Lexer - opera [is] : %s\n", yytext); return XLTK_OP_IS; }

 /* 分行符号 */
[;]					{ XS_DEBUGPRINT("Lexer - symbol [;] : %s\n", yytext); return XLTK_BRANCH; }

 /* 动态参数符号 */
[\.][\.][\.]		{ XS_DEBUGPRINT("Lexer - symbol [...] : %s\n", yytext); return XLTK_MOREPARAM; }

 /* 其他符号 */
[,]					{ XS_DEBUGPRINT("Lexer - symbol [,] : %s\n", yytext); return XLTK_COMMA; }

 /* 标识符 */
{Identifier}		{ XS_DEBUGPRINT("Lexer - Identifier : %s\n", yytext); return XLTK_IDENT; }

 /* 立即值 */
{Integer}			{ XS_DEBUGPRINT("Lexer - Integer : %s\n", yytext); return XLTK_INTEGER; }
{Hex}				{ XS_DEBUGPRINT("Lexer - Hex : %s\n", yytext); return XLTK_HEX; }
{Number}			{ XS_DEBUGPRINT("Lexer - Number : %s\n", yytext); return XLTK_NUMBER; }
{String}			{ XS_DEBUGPRINT("Lexer - String [\"] : %s\n", yytext); return XLTK_STRING; }
{EscapeString}		{ XS_DEBUGPRINT("Lexer - EscapeString ['] : %s\n", yytext); return XLTK_ESCAPESTRING; }
{TempletString}		{ XS_DEBUGPRINT("Lexer - TempletString ['] : %s\n", yytext); return XLTK_TEMPLETSTRING; }

%%



// 用户自定义代码段


