/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_XS_TAB_H_INCLUDED
# define YY_YY_XS_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    XLTK_INTEGER = 258,            /* XLTK_INTEGER  */
    XLTK_HEX = 259,                /* XLTK_HEX  */
    XLTK_NUMBER = 260,             /* XLTK_NUMBER  */
    XLTK_STRING = 261,             /* XLTK_STRING  */
    XLTK_CHAR = 262,               /* XLTK_CHAR  */
    XLTK_VAR = 263,                /* XLTK_VAR  */
    XLTK_NULL = 264,               /* XLTK_NULL  */
    XLTK_TRUE = 265,               /* XLTK_TRUE  */
    XLTK_FALSE = 266,              /* XLTK_FALSE  */
    XLTK_IF = 267,                 /* XLTK_IF  */
    XLTK_THEN = 268,               /* XLTK_THEN  */
    XLTK_ELSE = 269,               /* XLTK_ELSE  */
    XLTK_ELSEIF = 270,             /* XLTK_ELSEIF  */
    XLTK_SELECT = 271,             /* XLTK_SELECT  */
    XLTK_CASE = 272,               /* XLTK_CASE  */
    XLTK_DO = 273,                 /* XLTK_DO  */
    XLTK_WHILE = 274,              /* XLTK_WHILE  */
    XLTK_FOR = 275,                /* XLTK_FOR  */
    XLTK_IN = 276,                 /* XLTK_IN  */
    XLTK_END = 277,                /* XLTK_END  */
    XLTK_BREAK = 278,              /* XLTK_BREAK  */
    XLTK_CONTINUE = 279,           /* XLTK_CONTINUE  */
    XLTK_FUNCTION = 280,           /* XLTK_FUNCTION  */
    XLTK_RETURN = 281,             /* XLTK_RETURN  */
    XLTK_PARANTHESES_L = 282,      /* XLTK_PARANTHESES_L  */
    XLTK_PARANTHESES_R = 283,      /* XLTK_PARANTHESES_R  */
    XLTK_BRACES_L = 284,           /* XLTK_BRACES_L  */
    XLTK_BRACES_R = 285,           /* XLTK_BRACES_R  */
    XLTK_BRACKETS_L = 286,         /* XLTK_BRACKETS_L  */
    XLTK_BRACKETS_R = 287,         /* XLTK_BRACKETS_R  */
    XLTK_OP_MORE = 288,            /* XLTK_OP_MORE  */
    XLTK_OP_LESS = 289,            /* XLTK_OP_LESS  */
    XLTK_OP_MORE_EQUAL = 290,      /* XLTK_OP_MORE_EQUAL  */
    XLTK_OP_LESS_EQUAL = 291,      /* XLTK_OP_LESS_EQUAL  */
    XLTK_OP_UNEQUAL = 292,         /* XLTK_OP_UNEQUAL  */
    XLTK_OP_EQUAL = 293,           /* XLTK_OP_EQUAL  */
    XLTK_OP_ADD = 294,             /* XLTK_OP_ADD  */
    XLTK_OP_SUB = 295,             /* XLTK_OP_SUB  */
    XLTK_OP_MUL = 296,             /* XLTK_OP_MUL  */
    XLTK_OP_DIV = 297,             /* XLTK_OP_DIV  */
    XLTK_OP_MOD = 298,             /* XLTK_OP_MOD  */
    XLTK_OP_POW = 299,             /* XLTK_OP_POW  */
    XLTK_OP_BINAND = 300,          /* XLTK_OP_BINAND  */
    XLTK_OP_BINOR = 301,           /* XLTK_OP_BINOR  */
    XLTK_OP_BINNOT = 302,          /* XLTK_OP_BINNOT  */
    XLTK_OP_BINXOR = 303,          /* XLTK_OP_BINXOR  */
    XLTK_OP_BINRSH = 304,          /* XLTK_OP_BINRSH  */
    XLTK_OP_BINLSH = 305,          /* XLTK_OP_BINLSH  */
    XLTK_OP_NOT = 306,             /* XLTK_OP_NOT  */
    XLTK_OP_AND = 307,             /* XLTK_OP_AND  */
    XLTK_OP_OR = 308,              /* XLTK_OP_OR  */
    XLTK_OP_SET = 309,             /* XLTK_OP_SET  */
    XLTK_OP_SET_ADD = 310,         /* XLTK_OP_SET_ADD  */
    XLTK_OP_SET_SUB = 311,         /* XLTK_OP_SET_SUB  */
    XLTK_OP_SET_MUL = 312,         /* XLTK_OP_SET_MUL  */
    XLTK_OP_SET_DIV = 313,         /* XLTK_OP_SET_DIV  */
    XLTK_OP_ADDONE = 314,          /* XLTK_OP_ADDONE  */
    XLTK_OP_SUBONE = 315,          /* XLTK_OP_SUBONE  */
    XLTK_OP_IS = 316               /* XLTK_OP_IS  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_XS_TAB_H_INCLUDED  */
