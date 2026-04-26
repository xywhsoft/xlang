#include <stdio.h>
#include <stdlib.h>

#include "xs_lex.h"



char* sTokenType[] = {
	NULL,
	"NULL",
	"TRUE",
	"FALSE",
	
	"IF",
	"THEN",
	"ELSE",
	"ELSEIF",
	
	"DO",
	"WHILE",
	"FOR",
	"IN",
	"TO",
	
	"END",
	"BREAK",
	"CONTINUE",
	
	"FUNCTION",
	"RETURN",
	
	"STRUCT",
	"UNION",
	
	"OBJECT",
	"GET",
	"SET",
	"META",
	
	"NEW",
	"DELETE",
	
	"PRE_ASTR",
	"PRE_WSTR",
	"PRE_USTR",
	"PRE_TSTR",
	"PRE_DEFINE",
	"PRE_UNDEF",
	"PRE_IF",
	"PRE_IFDEF",
	"PRE_IFNDEF",
	"PRE_ELSE",
	"PRE_ENDIF",
	"PRE_INCLUDE",
	
	"PARANTHESES_L",
	"PARANTHESES_R",
	"BRACES_L",
	"BRACES_R",
	"BRACKETS_L",
	"BRACKETS_R",
	
	"OP_MORE",
	"OP_LESS",
	"OP_MORE_EQUAL",
	"OP_LESS_EQUAL",
	"OP_EQUAL",
	"OP_UNEQUAL",
	"OP_APPREQUAL",
	
	"OP_ADD",
	"OP_SUB",
	"OP_MUL",
	"OP_DIV",
	"OP_MOD",
	"OP_POW",
	
	"OP_BINAND",
	"OP_BINOR",
	"OP_BINNOT",
	"OP_BINXOR",
	"OP_BINRSH",
	"OP_BINLSH",
	
	"OP_NOT",
	"OP_AND",
	"OP_OR",
	
	"OP_SET",
	"OP_SET_ADD",
	"OP_SET_SUB",
	"OP_SET_MUL",
	"OP_SET_DIV",
	"OP_ADDONE",
	"OP_SUBONE",
	
	"OP_QUEST",
	"OP_COLON",
	
	"OP_QUESTDET",
	"OP_QUESTSET",
	
	"OP_DOT",
	"OP_QUESTDOT",
	
	"OP_IS",
	
	"BRANCH",
	
	"MOREPARAM",
	
	"COMMA",
	
	"IDENT",
	
	"INTEGER",
	"HEX",
	"NUMBER",
	"STRING",
	"ESCAPESTRING",
	"TEMPLETSTRING"
};



int main(int argc, char** argv)
{
    printf("\n\nTest 0\n");
    
    
    
    yyscan_t scanner;
    xs_lex_init(&scanner);
    
    xs_set_in(fopen("xs_in.txt", "r"), scanner);
    xs_set_out(fopen("xs_out.txt", "w+"), scanner);
    
    int tok;
    while ( tok = xs_lex(scanner) ) {
		printf("Toekn : (%d) %s\t\t\t%s\n", tok, sTokenType[tok], xs_get_text(scanner));
    }
    
    xs_lex_destroy(scanner);
    
    
    
    printf("\n\nTest End\n");
    return 0;
}


