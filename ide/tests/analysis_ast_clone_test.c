#include "analysis/ast_clone.h"
#include "src/parser/xl6_parser_api.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_ast_clone_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
static int same(const char* a,const char* b) { return a && b ? !strcmp(a,b) : a==b; }
static int equal(const XL6_AstNode* a,const XL6_AstNode* b,int sources)
{
    if(!a || !b) return a==b;
    XL6_AstNode left=*a,right=*b;
    left.text=right.text=left.doc=right.doc=NULL; left.literalData=right.literalData=NULL;
    left.children=right.children=NULL; left.inferredType=right.inferredType=left.semanticDeclaredType=right.semanticDeclaredType=NULL;
    left.xlsLeafReferences=right.xlsLeafReferences=0; left.source=right.source=NULL; left.childCapacity=right.childCapacity=0;
    if(memcmp(&left,&right,sizeof(left)) || !same(a->text,b->text) || !same(a->doc,b->doc) || !!a->literalData!=!!b->literalData ||
       (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize))) return 0;
    if(sources && a->source) {
        if(!b->source || b->source==a->source || !same(a->source->path,b->source->path) ||
           a->source->completionQuery!=b->source->completionQuery || a->source->completionStart!=b->source->completionStart ||
           a->source->completionEnd!=b->source->completionEnd) return 0;
    } else if(b->source) return 0;
    if(!equal(a->inferredType,b->inferredType,sources) || !equal(a->semanticDeclaredType,b->semanticDeclaredType,sources)) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!equal(a->children[i],b->children[i],sources)) return 0;
    return 1;
}
static int reject(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{ (void)in; (void)out; unsigned* count=user; return ++*count<5; }
static XL6_AstSource* destination(const XL6_AstSource* in,void* user)
{ XL6_AstNode* node=user; return in->path && !strcmp(in->path,xl6AstSourcePath(node)) ? node->source : NULL; }
typedef struct cursor_policy {int admit;unsigned calls;} cursor_policy;
static int cursorPolicy(const XL6_AstSource* in,const XL6_AstSource* target,void* user)
{cursor_policy* policy=user;++policy->calls;return policy->admit && in->completionStart==10 && in->completionEnd==13 && target->completionStart==20 && target->completionEnd==23;}
static XL6_AstSource* alwaysDestination(const XL6_AstSource* in,void* user)
{(void)in;return ((XL6_AstNode*)user)->source;}
static int cursorSources(void)
{
 XL6_AstNode *input=xl6AstNew(XL6_AST_MODULE,NULL),*owner=xl6AstNew(XL6_AST_MODULE,NULL),*other=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(input && owner && other);
 CHECK(xl6AstSetSourceTree(input,"/cursor.xl") && xl6AstSetSourceTree(owner,"/cursor.xl") && xl6AstSetSourceTree(other,"/other.xl"));
 input->source->completionQuery=owner->source->completionQuery=other->source->completionQuery=1;
 input->source->completionStart=10;input->source->completionEnd=13;owner->source->completionStart=other->source->completionStart=20;owner->source->completionEnd=other->source->completionEnd=23;
 xls_ast_clone_options options={.user=owner,.targetSource=alwaysDestination};cursor_policy policy={0};
 CHECK(!xlsAstClone(input,&options,NULL));CHECK(!xlsAstCloneWithCursorSource(input,&options,NULL,NULL,NULL));
 CHECK(!xlsAstCloneWithCursorSource(input,&options,cursorPolicy,&policy,NULL) && policy.calls==1);
 CHECK(input->source->references==1 && owner->source->references==1);
 policy.admit=1;XL6_AstNode* copy=xlsAstCloneWithCursorSource(input,&options,cursorPolicy,&policy,NULL);
 CHECK(copy && copy->source==owner->source && copy->source->completionStart==20 && input->source->completionStart==10 && input->source->references==1);xl6AstFree(copy);CHECK(owner->source->references==1);
 options.user=other;policy.calls=0;CHECK(!xlsAstCloneWithCursorSource(input,&options,cursorPolicy,&policy,NULL) && !policy.calls);
 options.user=input;CHECK(!xlsAstCloneWithCursorSource(input,&options,cursorPolicy,&policy,NULL));
 options.user=owner;owner->source->completionQuery=0;CHECK(!xlsAstCloneWithCursorSource(input,&options,cursorPolicy,&policy,NULL));
 input->source->completionQuery=0;CHECK(!xlsAstCloneWithCursorSource(input,&options,cursorPolicy,&policy,NULL));
 input->source->completionQuery=owner->source->completionQuery=1;owner->source->references=0;CHECK(!xlsAstCloneWithCursorSource(input,&options,cursorPolicy,&policy,NULL));owner->source->references=1;
 owner->source->completionStart=10;owner->source->completionEnd=13;copy=xlsAstClone(input,&options,NULL);CHECK(copy && copy->source==owner->source);xl6AstFree(copy);
 CHECK(input->source->references==1 && owner->source->references==1 && other->source->references==1);
 xl6AstFree(input);xl6AstFree(owner);xl6AstFree(other);return 1;
}
typedef struct reader { const XL6_AstNode* root; unsigned failures; } reader;
static int32 readCopies(void* user)
{
    reader* r=user;
    for(unsigned i=0;i<40;++i) {
        XL6_AstNode* copy=xlsAstClone(r->root,NULL,NULL);
        if(!copy || !equal(r->root,copy,1)) ++r->failures;
        xl6AstFree(copy);
    }
    return 0;
}
static int run(void)
{
    CHECK(cursorSources());
    const char* source="#doc\ndesc:\n 文档😀\n#end\nfunc helper<T>(T value) as T\n return value\nend\nclass Box\n string value\nend\nfunc main(...) as int\n string 中文 = \"a\\0😀\"\n auto box = Box()\n int result = helper::<int>(7)\n return result\nend\n";
    XL6_ParseResult parsed; xl6ParseResultInit(&parsed);
    CHECK(xl6ParseSource(source,"/memory/入口😀.xl",&parsed) && parsed.root->childCount>=3);
    CHECK(xl6AstSetSourceTree(parsed.root->children[0],"/memory/另一个来源.xl"));
    CHECK(xl6StdlibExpandModule(parsed.root,&parsed.diags) && xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"clone",1));
    size_t documented=SIZE_MAX;
    for(size_t i=0;i<parsed.root->childCount;++i) if(parsed.root->children[i]->doc && strstr(parsed.root->children[i]->doc,"文档")) documented=i;
    CHECK(documented!=SIZE_MAX);
    XL6_AstSource *first=parsed.root->source,*other=parsed.root->children[0]->source;
    size_t before=first->references,otherBefore=other->references,bytes=0;
    XL6_AstNode* copy=xlsAstClone(parsed.root,NULL,&bytes);
    CHECK(copy && bytes>sizeof(*copy) && equal(parsed.root,copy,1));
    CHECK(first->references==before && other->references==otherBefore);
    XL6_AstNode* again=xlsAstClone(parsed.root,&(xls_ast_clone_options){.bytes=bytes},NULL);
    CHECK(again && equal(parsed.root,again,1)); xl6AstFree(again);
    CHECK(!xlsAstClone(parsed.root,&(xls_ast_clone_options){.bytes=bytes-1},NULL));
    CHECK(!xlsAstClone(parsed.root,&(xls_ast_clone_options){.bytes=1},NULL));
    unsigned visited=0; CHECK(!xlsAstClone(parsed.root,&(xls_ast_clone_options){reject,&visited,0,0,NULL},NULL) && visited==5);
    again=xlsAstClone(parsed.root,&(xls_ast_clone_options){.dropSources=1},NULL);
    CHECK(again && equal(parsed.root,again,0)); xl6AstFree(again);
    XL6_AstNode* owner=xl6AstNew(XL6_AST_MODULE,NULL);
    CHECK(owner && xl6AstSetSourceTree(owner,xl6AstSourcePath(parsed.root)));
    again=xlsAstClone(parsed.root,&(xls_ast_clone_options){.user=owner,.targetSource=destination},NULL);
    CHECK(again && equal(parsed.root,again,1) && again->source==owner->source && owner->source->references>1);
    xl6AstFree(again); CHECK(owner->source->references==1 && first->references==before && other->references==otherBefore); xl6AstFree(owner);
    reader readers[4]; xthread* threads[4];
    for(unsigned i=0;i<4;++i) { readers[i]=(reader){parsed.root,0}; threads[i]=xrtThreadCreate(readCopies,&readers[i],0); CHECK(threads[i]); }
    for(unsigned i=0;i<4;++i) { CHECK(xrtThreadWait(threads[i])==XWAIT_OK && !readers[i].failures); xrtThreadDestroy(threads[i]); }
    CHECK(first->references==before && other->references==otherBefore);
    xl6ParseResultFree(&parsed);
    /* No pointers or non-atomic source reference increments borrowed from the
     * old tree: the new semantic facts survive its complete destruction. */
    CHECK(copy->childCount>documented && copy->children[documented]->doc && strstr(copy->children[documented]->doc,"文档"));
    again=xlsAstClone(copy,NULL,NULL); CHECK(again && equal(copy,again,1));
    xl6AstFree(copy); xl6AstFree(again); return 1;
}
int main(void)
{ if(!run()) return 1; printf("analysis_ast_clone_test passed: %u checks; complete semantic fields, literal NUL, mixed independent sources, exact budget/rejection, concurrent copying and old-tree destruction.\n",checks); return 0; }
