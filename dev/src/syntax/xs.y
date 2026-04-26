%include {
	#include <stdlib.h>
	#include "xs.h"
}

%token_type { int }
%left PLUS MINUS.
%left  DIVIDE TIMES.

%syntax_error {
	printf("Syntax error!\n");
	exit(1);
}

program ::= expr(A).					{ printf("Result : %d\n", A); }

expr(A) ::= expr(B) MINUS expr(C).		{ A = B - C; }
expr(A) ::= expr(B) PLUS expr(C).		{ A = B + C; }
expr(A) ::= expr(B) TIMES expr(C).		{ A = B * C; }
expr(A) ::= expr(B) DIVIDE expr(C).		{ A = B / C; }

expr(A) ::= INTEGER(B).					{ A = B; }



%code {
	int main()
	{
		void* objParser = ParseAlloc(malloc);
		Parse(objParser, INTEGER, 1);
		Parse(objParser, PLUS, 0);
		Parse(objParser, INTEGER, 2);
		Parse(objParser, 0, 0);
		ParseFree(objParser, free);
		return 0;
	}
}