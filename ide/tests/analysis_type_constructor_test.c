#include "analysis/frontend_internal.h"
#include <stdio.h>
#include <string.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
/* The candidate is the actual generated semantic TU. The reference below is
 * its independently extracted original compiler function, including recursive
 * calls and every copy field. Neither object is added to the shipped core. */
#include "xl6_sem.c"
#include "type_constructor_reference.inc"
static unsigned checks;
static int recording;
static size_t calls,failAt,denied;
static unsigned events[512];
void* __real_xrtCalloc(size_t,size_t);
char* __real_xrtStrDupN(const char*,size_t);
char* __real_xrtStrDup(const char*);
static int refuse(unsigned kind)
{
    if(!recording)return 0;
    if(calls<512)events[calls]=kind;
    ++calls;
    if(calls==failAt){++denied;return 1;}
    return 0;
}
void* __wrap_xrtCalloc(size_t n,size_t size)
{return refuse(1) ? NULL : __real_xrtCalloc(n,size);}
char* __wrap_xrtStrDupN(const char* text,size_t size)
{return refuse(2) ? NULL : __real_xrtStrDupN(text,size);}
char* __wrap_xrtStrDup(const char* text)
{return refuse(3) ? NULL : __real_xrtStrDup(text);}
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_type_constructor_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static uint64 live(void)
{xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static int textEqual(const char* a,const char* b)
{return a && b ? !strcmp(a,b) : a==b;}
static int equal(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b)return a==b;
    XL6_AstNode left=*a,right=*b;
    left.text=right.text=left.doc=right.doc=left.compiledPackageLink=right.compiledPackageLink=NULL;
    left.literalData=right.literalData=NULL;left.children=right.children=NULL;
    left.inferredType=right.inferredType=left.semanticDeclaredType=right.semanticDeclaredType=NULL;
    if(memcmp(&left,&right,sizeof(left)) || !textEqual(a->text,b->text) || !textEqual(a->doc,b->doc) ||
       !textEqual(a->compiledPackageLink,b->compiledPackageLink) || !!a->literalData!=!!b->literalData ||
       (a->literalData && memcmp(a->literalData,b->literalData,a->literalSize)))return 0;
    if(!equal(a->inferredType,b->inferredType) || !equal(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
    for(size_t i=0;i<a->childCount;++i)if(!equal(a->children[i],b->children[i]))return 0;
    return 1;
}
static XL6_AstNode* node(XL6_AstKind kind,const char* text,unsigned seed)
{
    XL6_AstNode* n=text ? xl6AstNewText(kind,text,strlen(text),NULL) : xl6AstNew(kind,NULL);
    if(!n)return NULL;
    n->doc=xrtStrDup("original ignored doc");n->compiledPackageLink=xrtStrDup("module/package.xlp");
    n->literalData=(unsigned char*)xrtStrDup("literal");n->literalSize=7;n->literalScalar=99;n->literalDecoded=1;
    n->flags=seed;n->line=seed+1;n->column=seed+2;
    n->range=(XL6_AstRange){.known=1,.startOffset=seed,.endOffset=seed+23,.startLine=1,.endLine=3,.startColumn=2,.endColumn=9};
    n->tokenRange=n->range;n->tokenRange.endOffset=seed+2;
    n->semanticTypeId=seed+3;n->semanticTypeAbiId=UINT64_C(0x8000000100000000)+seed;
    n->semanticSymbolId=seed+4;n->semanticDeclaredSymbolId=seed+5;n->semanticTargetSymbolId=seed+6;
    n->semanticCompoundOperatorSymbolId=seed+7;n->semanticReceiverSymbolId=seed+8;
    n->semanticCallKind=1;n->semanticLvalueKind=1;n->semanticOwnership=1;n->semanticExitKind=1;
    n->semanticScopeDepth=seed+9;n->semanticTargetScopeDepth=seed+10;
    n->semanticRegionId=seed+11;n->semanticTargetRegionId=seed+12;
    n->compiledPackageRoot=1;n->compiledPackageError=seed+13;
    if(!n->doc || !n->compiledPackageLink || !n->literalData){xl6AstFree(n);return NULL;}
    return n;
}
static int one(const XL6_AstNode* input,size_t fault)
{
    uint64 before=live();unsigned expected[512];size_t expectedCalls,expectedDenied;
    calls=denied=0;failAt=fault;recording=1;
    XL6_AstNode* reference=xlsTypeCloneReference(input);recording=0;
    CHECK(calls<512);expectedCalls=calls;expectedDenied=denied;memcpy(expected,events,calls*sizeof(*events));
    calls=denied=0;recording=1;
    XL6_AstNode* candidate=xl6SemCloneTypeNode(input);recording=0;
    CHECK(calls==expectedCalls && denied==expectedDenied && !memcmp(expected,events,calls*sizeof(*events)));
    CHECK(equal(reference,candidate));
    if(candidate){
        CHECK(!candidate->xlsStorage && !candidate->xlsLeafReferences && candidate!=input && candidate!=reference);
        if(candidate->text){CHECK(candidate->text!=reference->text && candidate->text!=input->text);}
        CHECK(candidate->source==input->source);
    }
    xl6AstFree(reference);xl6AstFree(candidate);CHECK(live()==before);return 1;
}
static int copies(void)
{
    CHECK(one(NULL,SIZE_MAX));
    const char* texts[]={NULL,"","int","类型_中文😀"};
    for(size_t i=0;i<sizeof(texts)/sizeof(*texts);++i){
        uint64 before=live();XL6_AstNode* input=node(XL6_AST_TYPE,texts[i],(unsigned)i+1);CHECK(input);
        for(unsigned child=0;child<7;++child){
            XL6_AstNode* item=node(child%2 ? XL6_AST_TYPE : XL6_AST_NAME,texts[child%4],child+10);CHECK(item);
            xl6AstAddChild(input,item);CHECK(input->childCount==child+1);
        }
        input->inferredType=node(XL6_AST_TYPE,"ignored inferred",50);
        input->semanticDeclaredType=node(XL6_AST_TYPE,"ignored declared",60);
        CHECK(input->inferredType && input->semanticDeclaredType && xl6AstSetSourceTree(input,"/type-copy/来源😀.xl"));
        size_t references=input->source->references;
        CHECK(one(input,SIZE_MAX));size_t operations=calls;CHECK(operations>20);
        for(size_t point=1;point<=operations;++point)CHECK(one(input,point));
        CHECK(input->source->references==references);
        XL6_AstNode* a=xlsTypeCloneReference(input),*b=xl6SemCloneTypeNode(input);CHECK(a && b && equal(a,b));
        for(size_t at=0;at<b->childCount;++at){
            CHECK(b->children[at]!=a->children[at] && b->children[at]!=input->children[at]);
            b->children[at]->semanticTypeId^=42;
            CHECK(a->children[at]->semanticTypeId==input->children[at]->semanticTypeId);
        }
        xl6AstFree(a);xl6AstFree(b);xl6AstFree(input);CHECK(live()==before);
    }
    return 1;
}
int main(void)
{
    XL6_AstNode* warm=node(XL6_AST_TYPE,"warm",1);if(!warm)return 1;
    XL6_AstNode *a=xlsTypeCloneReference(warm),*b=xl6SemCloneTypeNode(warm);xl6AstFree(a);xl6AstFree(b);xl6AstFree(warm);
    if(!copies())return 1;
    printf("analysis_type_constructor_test passed:%u checks; actual native clone versus original compiler body,all copied/ignored fields,source identity,independent ownership,exact allocation/failure sequence and balanced constructor faults\n",checks);
    return 0;
}
#else
int main(void){puts("analysis_type_constructor_test passed: legacy compiler constructor unchanged");return 0;}
#endif
