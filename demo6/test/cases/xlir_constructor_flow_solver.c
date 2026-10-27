#include "src/common/xl6_common.h"
#include "src/type/xl6_constructor_flow.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    XL6_AstKind kind;
    const char* text;
    unsigned facts;
    struct Node* children[4];
    size_t count;
} Node;
static Node nodes[256];
static size_t used, checks, errors;
static Node* node(XL6_AstKind kind,const char* text,unsigned facts,size_t count,...)
{
    assert(used<XL6_ARRAY_COUNT(nodes)&&count<=4);
    Node* n=&nodes[used++];*n=(Node){kind,text,facts,{NULL},count};
    va_list ap;va_start(ap,count);
    for(size_t i=0;i<count;++i)n->children[i]=va_arg(ap,Node*);
    va_end(ap);return n;
}
static XL6_AstKind kind(void* c,XL6_ConstructorNode n){(void)c;return ((Node*)n)->kind;}
static size_t count(void* c,XL6_ConstructorNode n){(void)c;return ((Node*)n)->count;}
static XL6_ConstructorNode child(void* c,XL6_ConstructorNode n,size_t i){(void)c;return (XL6_ConstructorNode)((Node*)n)->children[i];}
static const char* text(void* c,XL6_ConstructorNode n){(void)c;return ((Node*)n)->text;}
static unsigned facts(void* c,XL6_ConstructorNode n){(void)c;return ((Node*)n)->facts;}
static void error(void* c,XL6_ConstructorNode n,const char* message){(void)c;(void)n;assert(message&&*message);++errors;}
static void check(const char* name,int expected,Node* root)
{
    XL6_ConstructorView view={NULL,kind,count,child,text,facts,error,100000};
    errors=0;int actual=xl6ConstructorFlowCheck(&view,(XL6_ConstructorNode)root,0,1);
    if(actual!=expected){fprintf(stderr,"constructor flow: %s expected %d got %d\n",name,expected,actual);abort();}
    assert(errors==(expected?0u:1u));++checks;used=0;
}
#define N(k,...) node(XL6_AST_##k,__VA_ARGS__)
#define EMPTY() N(BLOCK,NULL,0,0)
#define FIELD() N(MEMBER,"resource",XL6_CONSTRUCTOR_FIELD|XL6_CONSTRUCTOR_DIRECT_FIELD,0)
#define SELF() N(NAME,"self",XL6_CONSTRUCTOR_SELF,0)
#define WRITE() N(ASSIGN,"=",XL6_CONSTRUCTOR_MAY_RAISE,2,FIELD(),N(INT,"1",0,0))
#define READ() FIELD()
#define BOOL(v) N(BOOL,v,0,0)
#define UNKNOWN() N(NAME,"condition",0,0)
#define THROW() N(THROW,NULL,0,0)
#define RETURN() N(RETURN,NULL,0,0)
#define BLOCK2(a,b) N(BLOCK,NULL,0,2,a,b)
#define IF(c,a,b) N(IF,NULL,0,4,c,a,EMPTY(),b)
#define CATCH(a) N(LIST,NULL,0,1,N(CATCH,NULL,0,2,N(NAME,"e",0,0),a))
#define TRY(a,b,c) N(TRY,NULL,0,3,a,b,c)
int main(void)
{
    check("required on fallthrough",0,EMPTY());
    check("write commits",1,WRITE());
    check("read before commit",0,BLOCK2(READ(),WRITE()));
    check("RHS before write",0,N(ASSIGN,"=",8,2,FIELD(),READ()));
    check("read after commit",1,BLOCK2(WRITE(),READ()));
    check("replacement after commit",1,BLOCK2(WRITE(),WRITE()));
    check("compound reads old slot",0,N(ASSIGN,"+=",8,2,FIELD(),N(INT,"1",0,0)));
    check("self alias before commit",0,BLOCK2(SELF(),WRITE()));
    check("self after commit",1,BLOCK2(WRITE(),SELF()));
    check("other field receiver is borrow",1,BLOCK2(N(MEMBER,"ready",2,0),WRITE()));
    check("closure captures partial self",0,BLOCK2(N(ANON_FUNC,NULL,8,1,READ()),WRITE()));
    check("closure after commit",1,BLOCK2(WRITE(),N(ANON_FUNC,NULL,8,1,READ())));
    check("export declaration does not execute its body",0,N(EXPORT_FUNC,NULL,0,1,WRITE()));
    check("both branches",1,IF(UNKNOWN(),WRITE(),WRITE()));
    check("missing else",0,IF(UNKNOWN(),WRITE(),EMPTY()));
    check("constant true",1,IF(BOOL("true"),WRITE(),EMPTY()));
    check("throwing branch",1,IF(UNKNOWN(),THROW(),WRITE()));
    check("returning branch",0,IF(UNKNOWN(),RETURN(),WRITE()));
    check("unreachable read",1,BLOCK2(THROW(),READ()));
    check("while may execute zero times",0,N(WHILE,NULL,0,2,UNKNOWN(),WRITE()));
    check("do executes once",1,N(DO_WHILE,NULL,0,2,WRITE(),BOOL("false")));
    check("true loop break commits",1,N(WHILE,NULL,0,2,BOOL("true"),BLOCK2(WRITE(),N(BREAK,NULL,0,0))));
    check("continue bypasses assignment",0,N(DO_WHILE,NULL,0,2,BLOCK2(N(CONTINUE,NULL,0,0),WRITE()),BOOL("false")));
    check("for may execute zero times",0,N(FOR,NULL,0,3,EMPTY(),N(NAME,"items",0,0),WRITE()));
    check("catch sees failed field write",0,TRY(WRITE(),CATCH(READ()),EMPTY()));
    check("catch rethrows failed field write",1,TRY(WRITE(),CATCH(THROW()),EMPTY()));
    check("catch initializes",1,TRY(WRITE(),CATCH(WRITE()),EMPTY()));
    check("finally initializes early return",1,TRY(RETURN(),EMPTY(),WRITE()));
    check("finally reads early pending return",0,TRY(RETURN(),EMPTY(),READ()));
    check("finally sees catch thrown error",0,TRY(THROW(),CATCH(THROW()),READ()));
    check("finally initializes every completion",1,TRY(IF(UNKNOWN(),RETURN(),THROW()),CATCH(EMPTY()),WRITE()));
    check("short circuit cannot prove write",0,N(BINARY,"and",0,2,UNKNOWN(),WRITE()));
    check("coalesce cannot prove write",0,N(BINARY,"??",0,2,UNKNOWN(),WRITE()));
    check("conditional both arms",1,N(CONDITIONAL,NULL,0,3,UNKNOWN(),WRITE(),WRITE()));
    check("base initializes inherited slot",1,N(CALL,NULL,8|16|32,2,N(NAME,"base",4,0),EMPTY()));
    check("exposing base before derived slot",0,BLOCK2(N(CALL,NULL,8|16|64,2,N(NAME,"base",4,0),EMPTY()),WRITE()));
    check("exposing base after derived slot",1,BLOCK2(WRITE(),N(CALL,NULL,8|16|64,2,N(NAME,"base",4,0),EMPTY())));
    check("non-exposing base construction borrow",1,BLOCK2(N(CALL,NULL,8|16,2,N(NAME,"base",4,0),EMPTY()),WRITE()));
    check("assert false never returns",1,N(ASSERT,NULL,0,1,BOOL("false")));
    check("match with else",1,N(MATCH,NULL,0,2,UNKNOWN(),N(LIST,NULL,0,2,
        N(MATCH_CASE,NULL,0,2,N(PATTERN_LITERAL,NULL,0,0),WRITE()),
        N(MATCH_CASE,NULL,0,2,N(PATTERN_ELSE,NULL,0,0),WRITE()))));
    check("exhaustive match",1,N(MATCH,NULL,0,2,UNKNOWN(),N(LIST,NULL,0,2,
        N(MATCH_CASE,NULL,0,2,N(PATTERN_LITERAL,NULL,0,1,BOOL("true")),WRITE()),
        N(MATCH_CASE,NULL,0,2,N(PATTERN_LITERAL,NULL,0,1,BOOL("false")),WRITE()))));
    check("product assignment",1,N(MULTI_ASSIGN,"=",8,2,N(LIST,NULL,0,2,
        N(MEMBER,"other",2,0),FIELD()),N(CALL,"pair",8,0)));
    printf("constructor flow: %lu control-flow/exception/escape checks passed\n",(unsigned long)checks);
    return 0;
}
