/** ANSI C ANTLR v3 grammar

根据Jim Idle的C输出目标改编-2007年4月。

由Terence Parr翻译自Jutta Degener 1995年的ANSI C yacc语法
2006年7月。词汇规则取自Java语法。

Jutta说：“1985年，Jeff Lee发表了他的Yacc语法（
与1985年4月30日的草案配套的Lex规范）
ANSI C标准的版本。Tom Stockfisch在
1987; 该原件，如对问题17.25的回答中所述
comp.lang.cFAQ，可从ftp.uu.net下载，
文件usenet/net.sources/ansi.c.grammar.Z。
我打算使这个版本尽可能接近当前的C标准语法
可能的如果你发现差异，请告诉我。Jutta Degener，1995年”

一般来说，您需要符号表信息来解析C；类型定义
定义类型，然后IDENTIFIERS是类型或纯ID。我在做
这里需要的最小值仅跟踪类型名称。这是一个很好的例子
全局范围的符号（称为符号）。声明其用法的每个规则
符号在堆栈上推送一个新副本，有效地创建一个新的
符号范围。还要注意，规则声明声明了一个规则范围
让任何被调用的规则看到isTypedef布尔值。这比
将该信息作为参数传递。非常干净。规则
direct_declarator可以很容易地确定IDENTIFIER
应声明为类型名。

我只在一个文件上测试过这个，尽管它有3500行。

此语法要求ANTLR v3（3.0b8或更高版本）

编程语言实现模式
2006年7月

*/
grammar C;

options 
{
    backtrack	= true;
    memoize	= true;
    k		= 2;
    language	= C;
}



scope Symbols 
{
    // 只有跟踪类型才能使解析器工作。Java示例使用Java.util.Set来跟踪这些。ANTLR3运行时有许多有用的“对象”，我们可以使用它们，它们的行为非常类似于Java哈希表、列表和向量。与Java程序员相比，您可以更好地控制它们，但它们有时会更“原始”一些。这里，对于每个作用域级别，我们都需要一组符号，因此我们可以使用ANTLR3运行时提供的哈希表，然后稍后我们将通过使用符号作为哈希表的键并查看该表是否包含该键来查看符号是否存储在任何级别
	
    pANTLR3_HASH_TABLE	    types;


}

// //虽然您可以实现自己的字符流等，但它们通常通过函数指针调用诸如LA（）之类的东西。通常，您将使用一个预先提供的输入流，您可以指示生成的代码直接访问输入指针。
//
// For  8 bit inputs            : #define ANTLR3_INLINE_INPUT_ASCII
// For 16 bit UTF16/UCS2 inputs : #define ANTLR3_INLINE_INPUT_UTF16
//
// 如果编译后的识别器可能从任何一个源获得输入，或者您已经编写了自己的字符输入流，则不要定义其中任何一个。
//
@lexer::header
{
#define	ANTLR3_INLINE_INPUT_ASCII
}

@parser::includes
{
	// 包括我们的noddy C++示例类
	//
	#include	<cpp_symbolpp.h>
}

// @header说明符在C目标中是有效的，但在这种情况下，在生成的代码上没有什么可添加的。在这里，您可以添加#defines，这可能是您的代码所依赖的。
//
// 在#include＜antlr3.h＞之前，对要显示在输出文件中的内容使用@preincludes
// @includes在#include<antlr3.h>之后
// @header表示在所有include之后应该出现的内容。

// @members 在C输出文件中插入函数（解析器没有其他限定。@lexer::members 在lexer中插入函数。
//
// 一般来说，不要过多地使用这个函数（可能放在奇怪的小函数中），而是将生成的头文件包含在自己的头文件中，并在包含支持函数的单独翻译单元中使用。
//
@members 
{

    void addTypeDef(pANTLR3_HASH_TABLE *types, pANTLR3_COMMON_TOKEN typeDef)
    {
	// By the time we are traversing tokens here, it
	// does not matter if we play with the input stream. Hence
	// rather than use text or getText() on a token and have the
	// huge overhead of creating pANTLR3_STRINGS, then we just
	// null terminate the string that the token is pointing to
	// and use it directly as a key.
	//
	*((pANTLR3_UINT8)(typeDef->stop) + 1) = '\0';

	// We only create a symbol hash table if we find any
	// symbols to record at this scope level
	//
	if (*types == NULL)
	{
		*types =	antlr3HashTableNew(11); 
	}
	(*types)->put(*types, (void *)typeDef->start, (void *)(typeDef->start), NULL);

    }

    // This is a function that is small enough to be kept in the
    // generated parser code (@lexer::members puts code in the lexer.
    //
    // Note a few useful MACROS in use here:
    //
    // SCOPE_SIZE     returns the number of levels on the stack (1 to n)
    //                for the named scope.
    // SCOPE_INSTANCE returns a pointer to Scope instance at the
    //                specified level.
    // SCOPE_TYPE     makes it easy to declare and cast the pointer to
    //                the structure typedef that the code generator declares.
    //
    // All functions (that need anything to do with the runtime, should
    // receive a parameter called ctx as the first parameter. ctx is a pointer
    // to the instance of the parser that is running and ensures thread safety
    // as well as easy access to all the parser elements etc. All MACROS assume
    // the presence of this parameter. This would be a pCLexer pointer if this
    // were a function to be called by the lexer (in which case this would be in
    // @lexer::members.
    //
    ANTLR3_BOOLEAN  isTypeName(pCParser ctx, pANTLR3_COMMON_TOKEN name)
    {
	int	i;

	// By the time we are traversing tokens here, it
	// does not matter if we play with the input stream. Hence
	// rather than use text or getText() on a token and have the
	// huge overhead of creating pANTLR3_STRINGS, then we just
	// null terminate the string that the token is pointing to
	// and use it directly as a key.
	//
	*((pANTLR3_UINT8)(name->stop) + 1) = '\0';

	for (i = (int)SCOPE_SIZE(Symbols)-1 ; i >= 0; i--)
	{
	    pANTLR3_HASH_TABLE  symtab;
	    pANTLR3_STRING		symbol;
	    SCOPE_TYPE(Symbols)	symScope;   // Aids in declaring the scope pointers

	    // Pick up the pointer to the scope structure at the current level
	    // We are descending from the inner most scope as that is how C type
	    // scoping works.
	    //
	    symScope	= (SCOPE_TYPE(Symbols))SCOPE_INSTANCE(Symbols, i);

	    // The pointer we have is an instance of the dynamic global scope
	    // called Symbols. Within there, as declared above, we have a pointer
	    // to an ANTLR3_HASH_TABLE. We should really check for NULL etc, like all good C code 
	    // should. But, this is example code...
	    //
	    symtab		= (pANTLR3_HASH_TABLE) symScope->types;

	    // The following call shows that you cannot add a NULL pointer as the entry for
	    // the hash table. You can always just add the pointer to the key and ignore it, but
	    // when you return from this call, you want to test for a NULL pointer, which means
	    // the entry was not found in the table.
	    //
	    symbol		= NULL;
	    if (symtab != NULL)
	    {
	        symbol		= (pANTLR3_STRING)	(symtab->get(symtab, (void *)(name->start)));
	    }

	    // Did we find the symbol in the type lists?
	    // This is generally used for semantic predicates, hence ANTLR3_TRUE or ANTLR3_FALSE
	    // for the return
	    //
	    if (symbol != NULL)
	    {
		    return ANTLR3_TRUE;
	    }
	}

	// We did not find the requested symbol in any of the scopes
	// that are currently in force.
	//
	return ANTLR3_FALSE;
    }

    // Because our dynamic scope contains an ANTLR3_HASH_TABLE, we need to free
    // it when it goes out of scope. When we create a new scope, we just set the
    // free pointer for the scope to point to this embedded function, which will be
    // called with a pointer to the scope instance that is to be freed,
    // from whence we take the table pointer, which we can then close :-)
    //
    void ANTLR3_CDECL freeTable(SCOPE_TYPE(Symbols) symtab)
    {
	// If we supplied an entry in the table with a free pointer,
	// then calling the table free function will call the free function
	// for each entry as it deletes it from the table. In this case however
	// we only stored things that were manufactured by internal factories, which
	// will be released anyway when the parser/lexer/etc are freed.
	//
	// As of release 3.1.2, we do not automatically have a new
	// scope table, so we only free it if we found symbols at this
	// scope level and therefore created the types map
	//
	if	(symtab->types != NULL)
	{
		symtab->types->free(symtab->types);
	}
    }
}

translation_unit

    scope Symbols; // The entire translation_unit (file) is a scope

    @init 
    {
		// The code in @init is executed before the rule starts. Note that this
		// is C, hence we cannot guarantee to be able to both declare and initialize
		// variables at the same time. If you need to declare variables as local
		// to a rule, use the @declarations section and then initialize locals
		// separately in this @init section.
		//
		$Symbols::types = NULL;	// Flag that we don't have the map yet (scopes are not memset to 0)

		SCOPE_TOP(Symbols)->free	= freeTable;	// This is called when the scope is popped
    }


    : external_declaration+
    ;

/** Either a function definition or any other kind of C decl/def.
 *  The LL(*) analysis algorithm fails to deal with this due to
 *  recursion in the declarator rules.  I'm putting in a
 *  manual predicate here so that we don't backtrack over
 *  the entire function.  Further, you get a better error
 *  as errors within the function itself don't make it fail
 *  to predict that it's a function.  Weird errors previously.
 *  Remember: the goal is to avoid backtrack like the plague
 *  because it makes debugging, actions, and errors harder.
 *
 *  Note that k=1 results in a much smaller predictor for the 
 *  fixed lookahead; k=2 made a few extra thousand lines. ;)
 *  I'll have to optimize that in the future.
 */
external_declaration
    options 
    {
		k=1;
    }
    : ( declaration_specifiers? declarator declaration* '{' )=> function_definition
    | declaration
    ;

function_definition

    scope Symbols; // put parameters and locals into same scope for now

    @init 
    {
      $Symbols::types = NULL; // Flag that we should create teh map if we find symbols to enter
      SCOPE_TOP(Symbols)->free	= freeTable;	// This is called when the scope is popped
    }
    :	declaration_specifiers? declarator
    	(	declaration+ compound_statement	// K&R style
	    |	compound_statement				// ANSI style
	)
    ;

declaration
    scope 
    {
		ANTLR3_BOOLEAN	    isTypedef;
    }
    @init 
    {
		$declaration::isTypedef = ANTLR3_FALSE;
    }
    : 'typedef' declaration_specifiers? 
		{
		    $declaration::isTypedef=ANTLR3_TRUE;
		}
		    init_declarator_list ';' // special case, looking for typedef	

    | declaration_specifiers init_declarator_list? ';'
    ;

declaration_specifiers
    :   (     storage_class_specifier
			| type_specifier
			| type_qualifier
        )+
    ;

init_declarator_list
    : init_declarator (',' init_declarator)*
    ;

init_declarator
    : declarator ('=' initializer)?
    ;

storage_class_specifier
    : 'extern'
    | 'static'
    | 'auto'
    | 'register'
    ;

type_specifier
    : 'void'
    | 'char'
    | 'short'
    | 'int'
    | 'long'
    | 'float'
    | 'double'
    | 'signed'
    | 'unsigned'
    | struct_or_union_specifier
    | enum_specifier
    | type_id
    ;

type_id
    :   {isTypeName(ctx, LT(1)) }?	// Note how we reference using C directly
	IDENTIFIER
	{
	    // In Java you can just use $xxx.text, which is of type String.
	    // In C, .text returns an ANTLR3 'object' of type pANTLR3_STRING.
	    // the pointer to the actual characters is contained in ->chars and
	    // the object has lots of methods to help you with strings, such as append and
	    // insert etc. pANTLR3_STRING is also auto managed by a string factory, which
	    // will be released when you ->free() the parser.
	    //
	    //   printf("'\%s' is a type", $IDENTIFIER.text->chars);
	}
    ;

struct_or_union_specifier
    options 
    {
		k=3;
    }
    scope Symbols; // structs are scopes
    @init 
    {
		$Symbols::types = NULL; // Will create this if find a symbol to record
		SCOPE_TOP(Symbols)->free	= freeTable;	// This is called when the scope is popped
    }
    : struct_or_union IDENTIFIER? '{' struct_declaration_list '}'
    | struct_or_union IDENTIFIER
    ;

struct_or_union
    : 'struct'
    | 'union'
    ;

struct_declaration_list
    : struct_declaration+
    ;

struct_declaration
    : specifier_qualifier_list struct_declarator_list ';'
    ;

specifier_qualifier_list
    : ( type_qualifier | type_specifier )+
    ;

struct_declarator_list
    : struct_declarator (',' struct_declarator)*
    ;

struct_declarator
    : declarator (':' constant_expression)?
    | ':' constant_expression
    ;

enum_specifier
    options 
    {
		k=3;
    }
    : 'enum' '{' enumerator_list '}'
    | 'enum' IDENTIFIER '{' enumerator_list '}'
    | 'enum' IDENTIFIER
    ;

enumerator_list
    : enumerator (',' enumerator)*
    ;

enumerator
    : IDENTIFIER ('=' constant_expression)?
    ;

type_qualifier
    : 'const'
    | 'volatile'
    ;

declarator
    : pointer? direct_declarator
    | pointer
    ;

direct_declarator
    :   (   IDENTIFIER
			{
	    		if (SCOPE_TOP(declaration) != NULL && $declaration::isTypedef)
				{
					pANTLR3_STRING idText;

					// Found a type, so we can enter it in the symbol tables
					// The called funciton will work out what the text is for the
					// identifier.
					//
					addTypeDef(&($Symbols::types), $IDENTIFIER);

#ifdef __cplusplus
	
	// If you compile the generated C source as C++, then you can embed 
	// C++ code in your actions. The runtime is still C based and everything
	// is tagged properly for linking and so on, but because you are using the
	// C++ compiler, it will happilly accept classes and so on for things like
	// scopes. This class is defined entirely in the header file C.h, if "compile
	// as C++ is set for CParser.c and CLexer.c" It is just a silly example
	// of course and I don't do anythign with this class, just create and delete it.
	//
	symbolpp		*mySymClass;

	mySymClass = new symbolpp($IDENTIFIER.line, $IDENTIFIER.text);

	delete mySymClass;


#endif

					// Note that we must escape the percent sign here from ANTLR expression
					// parsing. It is not seen in the generated C code.
					//
					printf("define type \%s\n", $IDENTIFIER.text->chars);
				}
			}
		  | '(' declarator ')'
	)

        declarator_suffix*
    ;

declarator_suffix
    :   '[' constant_expression ']'
    |   '[' ']'
    |   '(' parameter_type_list ')'
    |   '(' identifier_list ')'
    |   '(' ')'
    ;

pointer
    : '*' type_qualifier+ pointer?
    | '*' pointer
    | '*'
    ;

parameter_type_list
    : parameter_list (',' '...')?
    ;

parameter_list
    : parameter_declaration (',' parameter_declaration)*
    ;

parameter_declaration
    : declaration_specifiers (declarator | abstract_declarator)*
    ;

identifier_list
    : IDENTIFIER (',' IDENTIFIER)*
    ;

type_name
    : specifier_qualifier_list abstract_declarator?
    ;

abstract_declarator
    : pointer direct_abstract_declarator?
    | direct_abstract_declarator
    ;

direct_abstract_declarator
    :	( '(' abstract_declarator ')' | abstract_declarator_suffix ) abstract_declarator_suffix*
    ;

abstract_declarator_suffix
    :	'[' ']'
    |	'[' constant_expression ']'
    |	'(' ')'
    |	'(' parameter_type_list ')'
    ;
	
initializer
    : assignment_expression
    | '{' initializer_list ','? '}'
    ;

initializer_list
    : initializer (',' initializer)*
    ;

// E x p r e s s i o n s

argument_expression_list
    :   assignment_expression (',' assignment_expression)*
    ;

additive_expression
    : (multiplicative_expression) ('+' multiplicative_expression | '-' multiplicative_expression)*
    ;

multiplicative_expression
    : (cast_expression) ('*' cast_expression | '/' cast_expression | '%' cast_expression)*
    ;

cast_expression
    : '(' type_name ')' cast_expression
    | unary_expression
    ;

unary_expression
    : postfix_expression
    | '++' unary_expression
    | '--' unary_expression
    | unary_operator cast_expression
    | 'sizeof' unary_expression
    | 'sizeof' '(' type_name ')'
    ;

postfix_expression
    :   primary_expression
		(     '[' expression ']'
			| '(' ')'
			| '(' argument_expression_list ')'
			| '.' IDENTIFIER
			| '*' IDENTIFIER
			| '->' IDENTIFIER
			| '++'
			| '--'
        )*
    ;

unary_operator
    : '&'
    | '*'
    | '+'
    | '-'
    | '~'
    | '!'
    ;

primary_expression
    : IDENTIFIER
    | constant
    | '(' expression ')'
    ;

constant
    :   HEX_LITERAL
    |   OCTAL_LITERAL
    |   DECIMAL_LITERAL
    |	CHARACTER_LITERAL
    |	STRING_LITERAL
    |   FLOATING_POINT_LITERAL
    ;

/////

expression
    : assignment_expression (',' assignment_expression)*
    ;

constant_expression
    : conditional_expression
    ;

assignment_expression
    : lvalue assignment_operator assignment_expression
    | conditional_expression
    ;
	
lvalue
    :	unary_expression
    ;

assignment_operator
    : '='
    | '*='
    | '/='
    | '%='
    | '+='
    | '-='
    | '<<='
    | '>>='
    | '&='
    | '^='
    | '|='
    ;

conditional_expression
    : logical_or_expression ('?' expression ':' conditional_expression)?
    ;

logical_or_expression
    : logical_and_expression ('||' logical_and_expression)*
    ;

logical_and_expression
    : inclusive_or_expression ('&&' inclusive_or_expression)*
    ;

inclusive_or_expression
    : exclusive_or_expression ('|' exclusive_or_expression)*
    ;

exclusive_or_expression
    : and_expression ('^' and_expression)*
    ;

and_expression
    : equality_expression ('&' equality_expression)*
    ;

equality_expression
    : relational_expression (('=='|'!=') relational_expression)*
    ;

relational_expression
    : shift_expression (('<'|'>'|'<='|'>=') shift_expression)*
    ;

shift_expression
    : additive_expression (('<<'|'>>') additive_expression)*
    ;

// S t a t e m e n t s

statement
    : labeled_statement
    | compound_statement
    | expression_statement
    | selection_statement
    | iteration_statement
    | jump_statement
    ;

labeled_statement
    : IDENTIFIER ':' statement
    | 'case' constant_expression ':' statement
    | 'default' ':' statement
    ;

compound_statement
    scope Symbols; // blocks have a scope of symbols
    @init 
    {
	$Symbols::types = NULL;  // Will be created if we create any symbols
	SCOPE_TOP(Symbols)->free	= freeTable;	// This is called when the scope is popped
    }
    : '{' declaration* statement_list? '}'
    ;

statement_list
    : statement+
    ;

expression_statement
    : ';'
    | expression ';'
    ;

selection_statement
    : 'if' '(' expression ')' statement (('else')=> 'else' statement)?
    | 'switch' '(' expression ')' statement
    ;

iteration_statement
    : 'while' '(' expression ')' statement
    | 'do' statement 'while' '(' expression ')' ';'
    | 'for' '(' expression_statement expression_statement expression? ')' statement
    ;

jump_statement
    : 'goto' IDENTIFIER ';'
    | 'continue' ';'
    | 'break' ';'
    | 'return' ';'
    | 'return' expression ';'
    ;

IDENTIFIER
    :	LETTER (LETTER|'0'..'9')*
    ;
	
fragment
LETTER
    :	'$'
    |	'A'..'Z'
    |	'a'..'z'
    |	'_'
    ;

CHARACTER_LITERAL
    :   '\'' ( EscapeSequence | ~('\''|'\\') ) '\''
    ;

STRING_LITERAL
    :  '"' STRING_GUTS '"'
    ;

fragment
STRING_GUTS :	( EscapeSequence | ~('\\'|'"') )* ;

HEX_LITERAL : '0' ('x'|'X') HexDigit+ IntegerTypeSuffix? ;

DECIMAL_LITERAL : ('0' | '1'..'9' '0'..'9'*) IntegerTypeSuffix? ;

OCTAL_LITERAL : '0' ('0'..'7')+ IntegerTypeSuffix? ;

fragment
HexDigit : ('0'..'9'|'a'..'f'|'A'..'F') ;

fragment
IntegerTypeSuffix
    :	('l'|'L')
    |	('u'|'U')  ('l'|'L')?
    ;

FLOATING_POINT_LITERAL
    :   ('0'..'9')+ '.' ('0'..'9')* Exponent? FloatTypeSuffix?
    |   '.' ('0'..'9')+ Exponent? FloatTypeSuffix?
    |   ('0'..'9')+ ( Exponent FloatTypeSuffix? | FloatTypeSuffix)
    ;

fragment
Exponent : ('e'|'E') ('+'|'-')? ('0'..'9')+ ;

fragment
FloatTypeSuffix : ('f'|'F'|'d'|'D') ;

fragment
EscapeSequence
    :   '\\' ('b'|'t'|'n'|'f'|'r'|'\"'|'\''|'\\')
    |   OctalEscape
    ;

fragment
OctalEscape
    :   '\\' ('0'..'3') ('0'..'7') ('0'..'7')
    |   '\\' ('0'..'7') ('0'..'7')
    |   '\\' ('0'..'7')
    ;

fragment
UnicodeEscape
    :   '\\' 'u' HexDigit HexDigit HexDigit HexDigit
    ;

WS  :  (' '|'\r'|'\t'|'\u000C'|'\n') {$channel=HIDDEN;}
    ;

COMMENT
    :   '/*' ( options {greedy=false;} : . )* '*/' {$channel=HIDDEN;}
    ;

LINE_COMMENT
    : '//' ~('\n'|'\r')* '\r'? '\n' {$channel=HIDDEN;}
    ;

// ignore #line info for now
LINE_COMMAND 
    : '#' (' ' | '\t')*
		(
			'include' (' ' | '\t')+ '"' file = STRING_GUTS '"' (' ' | '\t')* '\r'? '\n'
			{
				pANTLR3_STRING			fName;
				pANTLR3_INPUT_STREAM    in;

				// Create an initial string, then take a substring
				// We can do this by messing with the start and end
				// pointers of tokens and so on. This shows a reasonable way to
				// manipulate strings.
				//
				fName = $file.text;
				printf("Including file '\%s'\n", fName->chars);

				// Create a new input stream and take advantage of built in stream stacking
				// in C target runtime.
				//
				in = antlr3FileStreamNew(fName->chars, ANTLR3_ENC_8BIT);
				PUSHSTREAM(in);

				// Note that the input stream is not closed when it EOFs, I don't bother
				// to do it here (hence this is leaked at the program end), 
				// but it is up to you to track streams created like this
				// and destroy them when the whole parse session is complete. Remember that you
				// don't want to do this until all tokens have been manipulated all the way through 
				// your tree parsers etc as the token does not store the text it just refers
				// back to the input stream and trying to get the text for it will abort if you
				// close the input stream too early.
				//

			}
            | (('0'..'9')=>('0'..'9'))+ ~('\n'|'\r')* '\r'? '\n'
	    )
	 {$channel=HIDDEN;}
    ;
