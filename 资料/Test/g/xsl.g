grammar xsl;

options 
{
    backtrack	= true;
    memoize	= true;
    k		= 2;
    language	= C;
}



add : DECIMAL_LITERAL '+' DECIMAL_LITERAL;



// integer
fragment HexDigit : ('0'..'9'|'a'..'f'|'A'..'F') ;
HEX_LITERAL : '0' ('x'|'X') HexDigit+ ;
DECIMAL_LITERAL : ('0'..'9'+) ;
BINARY_LITERAL : '0' ('b'|'B') ('0'..'1'+) ;



// float
fragment Exponent : ('e'|'E') ('+'|'-')? ('0'..'9')+ ;
FLOATING_POINT_LITERAL
    :   ('0'..'9')+ '.' ('0'..'9')* Exponent?
    |   ('0'..'9')+ Exponent
    ;



// string
fragment EscapeSequence
    :   '\\' ('b'|'t'|'n'|'f'|'r'|'\"'|'\''|'\\')
    |	'\\' 'u' HexDigit HexDigit HexDigit HexDigit
    ;
STRING_LITERAL
    :  '"' ( EscapeSequence | ~('\\'|'"') )* '"'
    |  '`' ( ~('`') )* '`'
    ;
CHARACTER_LITERAL
    :   '\'' ( EscapeSequence | ~('\''|'\\') ) '\''
    ;



// value
constant
    :   HEX_LITERAL
    |   DECIMAL_LITERAL
    |	CHARACTER_LITERAL
    |	STRING_LITERAL
    |   FLOATING_POINT_LITERAL
    ;



// space
WS  :  (' '|'\r'|'\t'|'\u000C'|'\n') {$channel=HIDDEN;}
    ;



// comment
COMMENT
    :   '/*' ( options {greedy=false;} : . )* '*/' {$channel=HIDDEN;}
    ;
LINE_COMMENT
    : '//' ~('\n'|'\r')* '\r'? '\n' {$channel=HIDDEN;}
    ;



// identifier
fragment LETTER
    :	'$'
    |	'A'..'Z'
    |	'a'..'z'
    |	'_'
    ;
IDENTIFIER
    :	LETTER (LETTER|'0'..'9')*
    ;



// operator
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
additive_expression
    : (multiplicative_expression) ('+' multiplicative_expression | '-' multiplicative_expression)*
    ;
multiplicative_expression
    : (cast_expression) ('*' cast_expression | '/' cast_expression | '%' cast_expression)*
    ;



// expression

cast_expression
    : unary_expression
    ;

expression
    : assignment_expression (',' assignment_expression)*
    ;
lvalue
    :	unary_expression
    ;
assignment_expression
    : lvalue assignment_operator assignment_expression
    | conditional_expression
    ;
argument_expression_list
    :   assignment_expression (',' assignment_expression)*
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
unary_expression
    : postfix_expression
    | '++' unary_expression
    | '--' unary_expression
    | unary_operator cast_expression
    | 'sizeof' unary_expression
    | 'sizeof' '(' type_name ')'
    ;
primary_expression
    : IDENTIFIER
    | constant
    | '(' expression ')'
    ;
unary_operator
    : '&'
    | '*'
    | '+'
    | '-'
    | '~'
    | '!'
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
    | type_id
    ;

specifier_qualifier_list
    : ( type_specifier )+
    ;

parameter_type_list
    : parameter_list (',' '...')?
    ;

parameter_list
    : parameter_declaration (',' parameter_declaration)*
    ;

init_declarator
    : declarator ('=' initializer)?
    ;

initializer
    : assignment_expression
    | '{' initializer_list ','? '}'
    ;

initializer_list
    : initializer (',' initializer)*
    ;
scope Symbols 
{
    // 只有跟踪类型才能使解析器工作。Java示例使用Java.util.Set来跟踪这些�?�ANTLR3运行时有许多有用的�?�对象�?�，我们可以使用它们，它们的行为非常类似于Java哈希表�?�列表和向量。与Java程序员相比，您可以更好地控制它们，但它们有时会更“原始�?�一些�?�这里，对于每个作用域级别，我们都需要一组符号，因此我们可以使用ANTLR3运行时提供的哈希表，然后稍后我们将�?�过使用符号作为哈希表的键并查看该表是否包含该键来查看符号是否存储在任何级别
	
    pANTLR3_HASH_TABLE	    types;


}
init_declarator_list
    : init_declarator (',' init_declarator)*
    ;
declaration_specifiers
    :   ( type_specifier )+
    ;
parameter_declaration
    : declaration_specifiers (declarator | abstract_declarator)*
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

identifier_list
    : IDENTIFIER (',' IDENTIFIER)*
    ;
declarator
    : pointer? direct_declarator
    | pointer
    ;
constant_expression
    : conditional_expression
    ;

conditional_expression
    : logical_or_expression ('?' expression ':' conditional_expression)?
    ;


abstract_declarator_suffix
    :	'[' ']'
    |	'[' constant_expression ']'
    |	'(' ')'
    |	'(' parameter_type_list ')'
    ;
direct_abstract_declarator
    :	( '(' abstract_declarator ')' | abstract_declarator_suffix ) abstract_declarator_suffix*
    ;

pointer
    : '*' pointer
    | '*'
    ;

abstract_declarator
    : pointer direct_abstract_declarator?
    | direct_abstract_declarator
    ;
type_name
    : specifier_qualifier_list abstract_declarator?
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



