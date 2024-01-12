


#include <stdio.h>



extern FILE *yyin, *yyout;
extern int yylex (void);



void yyerror(char* s)
{
    printf("Error : %s\n", s);
}

int main(int argc, char** argv)
{
	/*
    printf("计算器\n支持的运算符：+ - * / & | ~ ! ^ @ \n");
    printf("+: 加法 2+3 \n");
    printf("-: 减法 2-3 \n");
    printf("*: 乘法 2*3 \n");
    printf("/: 除法 2/3 \n");
    printf("！: 阶乘 2！\n");
    printf("^: 乘方  2^3 \n");
    printf("^: 开方  2@3 \n");
    printf("&: 与 2&3 结果为二进制转化的十进制数\n");
    printf("|: 或 2|3 结果为二进制转化的十进制数\n");
    printf("~: 非 2|3 结果为二进制转化的十进制数\n");
    printf("sin: 正弦值  sin(30) 单位为rad \n");
    printf("sin: 余弦值  cos(30) 单位为rad \n");
    printf("tan: 正切值  tan(30) 单位为rad \n");
    yyparse();
    */
    if ( argc > 1 ) {
		yyin = fopen(argv[1], "r");
		if ( yyin == NULL ) {
			printf("fopen error : %s\n", argv[1]);
			return 1;
		}
		yylex();
		fclose(yyin);
    } else {
		do {
			printf("请输入语句：\n");
		} while ( yylex() != 0 );
    }
    return 0;
}


