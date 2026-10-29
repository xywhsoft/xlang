#include "analysis/ast_clone.h"
#include "analysis/ast_intern.h"
#include "analysis/ast_materialize.h"
#include "analysis/syntax_node_storage.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#ifndef XLS_NATIVE_AST_PACKAGE_FIELDS
#error This fixture requires the reviewed compiled-package AST layout
#endif
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ast_package_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int textEqual(const char* a,const char* b)
{return a && b ? !strcmp(a,b) : a==b;}
static uint64 live(void)
{xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
static XL6_AstNode* type(const char* link,uint32 receiver,unsigned error,int package)
{
    XL6_AstNode* n=xl6AstNewText(XL6_AST_TYPE,"Owner",5,NULL);if(!n)return NULL;
    n->semanticReceiverSymbolId=receiver;n->compiledPackageError=error;n->compiledPackageRoot=package;
    if(link && !(n->compiledPackageLink=xrtStrDup(link))){xl6AstFree(n);return NULL;}
    return n;
}
static int append(XL6_AstNode* root,XL6_AstNode* child)
{if(!child)return 0;size_t before=root->childCount;xl6AstAddChild(root,child);if(root->childCount!=before+1){xl6AstFree(child);return 0;}return 1;}
static int equal(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b)return a==b;
    XL6_AstNode left=*a,right=*b;
    left.text=left.doc=left.compiledPackageLink=NULL;right.text=right.doc=right.compiledPackageLink=NULL;
    left.literalData=right.literalData=NULL;left.children=right.children=NULL;
    left.inferredType=left.semanticDeclaredType=right.inferredType=right.semanticDeclaredType=NULL;
    left.source=right.source=NULL;left.xlsStorage=right.xlsStorage=NULL;
    left.xlsLeafReferences=right.xlsLeafReferences=0;left.childCapacity=right.childCapacity=0;
    if(memcmp(&left,&right,sizeof(left)) || !textEqual(a->text,b->text) || !textEqual(a->doc,b->doc) ||
       !textEqual(a->compiledPackageLink,b->compiledPackageLink) || !!a->literalData!=!!b->literalData ||
       (a->literalData && memcmp(a->literalData,b->literalData,a->literalSize)) || !!a->source!=!!b->source)return 0;
    if(a->compiledPackageLink && a->compiledPackageLink==b->compiledPackageLink)return 0;
    if(a->source && (a->source==b->source || !textEqual(a->source->path,b->source->path)))return 0;
    if(!equal(a->inferredType,b->inferredType) || !equal(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
    for(size_t i=0;i<a->childCount;++i)if(!equal(a->children[i],b->children[i]))return 0;
    return 1;
}
static XL6_AstNode* make(void)
{
    XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);if(!root)return NULL;
    if(!append(root,type("包/owner😀",71,0x105,1)) || !append(root,type("",0,2,0)) ||
       !append(root,type(NULL,99,0,-1)))goto failed;
    root->children[0]->inferredType=type("inferred",13,1,1);
    if(!root->children[0]->inferredType || !xl6AstSetSourceTree(root,"/memory/包😀.xl"))goto failed;
    return root;
failed:xl6AstFree(root);return NULL;
}
static int measure(const XL6_AstNode* n,size_t* size)
{
    if(!n)return 1;
    *size+=sizeof(*n)+n->childCapacity*sizeof(*n->children);
    if(n->text)*size+=strlen(n->text)+1;
    if(n->doc)*size+=strlen(n->doc)+1;
    if(n->compiledPackageLink)*size+=strlen(n->compiledPackageLink)+1;
    if(n->literalData)*size+=n->literalSize+1;
    if(!measure(n->inferredType,size) || !measure(n->semanticDeclaredType,size))return 0;
    for(size_t i=0;i<n->childCount;++i)if(!measure(n->children[i],size))return 0;
    return 1;
}
static int copies(void)
{
    XL6_AstNode* input=make();CHECK(input);uint64 baseline=live();size_t references=input->source->references;
    for(unsigned pool=0;pool<2;++pool){
        size_t bytes=0;xls_ast_pool_work work;
        XL6_AstNode* out=xlsAstClonePooled(input,NULL,NULL,NULL,pool ? 1024u*1024u : 0,&work,&bytes);
        CHECK(out && equal(input,out) && bytes>strlen(input->children[0]->compiledPackageLink));
        CHECK(pool ? work.nodes>0 && work.pages>0 : !work.nodes);
        CHECK(input->source->references==references);
        xl6AstFree(out);CHECK(live()==baseline);
        xls_ast_clone_options options={.bytes=bytes-1};out=xlsAstClonePooled(input,&options,NULL,NULL,pool ? 1024u*1024u : 0,NULL,NULL);
        CHECK(!out && live()==baseline && input->source->references==references);
    }
    xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,1024u*1024u);
    XL6_AstNode* out=xlsAstMaterialize(input,NULL,NULL);CHECK(out && equal(input,out));
    xls_ast_materialize_work work;xlsAstMaterializeEnd(&scope,&work);CHECK(work.pooledCopies==1 && work.nodes>0);
    xl6AstFree(input);CHECK(!strcmp(out->children[0]->compiledPackageLink,"包/owner😀"));xl6AstFree(out);
    return 1;
}
static int sizing(void)
{
    XL6_AstNode* root=make();CHECK(root);size_t size=0,expected=sizeof(*root->source)+strlen(root->source->path)+1;
    CHECK(measure(root,&expected));CHECK(xlsAstSetSourceTreeSized(root,"/memory/包😀.xl",&size) && size==expected);
    size_t a=0,b=0;XL6_AstNode* n=root->children[1];CHECK(n->compiledPackageLink && !*n->compiledPackageLink);
    CHECK(xlsSyntaxNodeStorage(n,0,&a,SIZE_MAX));char* link=n->compiledPackageLink;n->compiledPackageLink=NULL;
    CHECK(xlsSyntaxNodeStorage(n,0,&b,SIZE_MAX) && a==b+1);n->compiledPackageLink=link;
    a=0;CHECK(!xlsSyntaxNodeStorage(n,0,&a,b));xl6AstFree(root);return 1;
}
static int identities(void)
{
    XL6_AstNode* originals[8];const XL6_AstNode* values[8];
    for(unsigned i=0;i<8;++i){
        originals[i]=type(i==4 ? "" : i==5 || i==6 ? "packageA" : i==7 ? "packageB" : NULL,
            i==1 ? 7 : 0,i==2 ? 1 : 0,i==3 ? 1 : 0);CHECK(originals[i]);values[i]=originals[i];
    }
    xls_ast_intern_work work;xls_ast_intern* owner=xlsAstInternCapture(values,8,1024u*1024u,1024u*1024u,&work);
    CHECK(owner && work.uniqueRoots==7 && work.hits==1 && values[5]==values[6]);
    for(unsigned i=0;i<8;++i)for(unsigned j=0;j<i;++j)CHECK((values[i]==values[j])==(i==6 && j==5));
    for(unsigned i=0;i<8;++i){CHECK(equal(originals[i],values[i]));xl6AstFree(originals[i]);}
    CHECK(!strcmp(values[5]->compiledPackageLink,"packageA"));xlsAstInternDestroy(owner);return 1;
}
static int leaves(void)
{
    XL6_AstNode* input=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(input);
    for(unsigned i=0;i<10;++i)CHECK(append(input,type(i==2 || i==3 ? "" : NULL,i==4 || i==5 ? 11 : 0,
        i==6 || i==7 ? 0x105 : 0,i>=8 ? 1 : 0)));
    uint64 baseline=live();
    for(unsigned pool=0;pool<2;++pool){
        xls_ast_leaf_work leaves;XL6_AstNode* out=xlsAstCloneFactTypes(input,NULL,NULL,NULL,pool ? 1024u*1024u : 0,NULL,&leaves,NULL);
        CHECK(out && equal(input,out) && leaves.entries==1 && leaves.hits==1 && out->children[0]==out->children[1]);
        for(unsigned i=2;i<10;i+=2)CHECK(out->children[i]!=out->children[i+1]);
        xl6AstFree(out);CHECK(live()==baseline);
    }
    xl6AstFree(input);return 1;
}
static int changeReceiver(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{if(in->kind==XL6_AST_TYPE)out->semanticReceiverSymbolId+=100u*(++*(unsigned*)user);return 1;}
static int badTransfer(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{out->compiledPackageLink=in->compiledPackageLink;return user!=NULL;}
static int trees(void)
{
    XL6_AstNode* input=type("shared😀",7,0x105,1);CHECK(input);XL6_AstNode* children[]={input,input};
    XL6_AstNode forest={.kind=XL6_AST_MODULE,.children=children,.childCount=2,.childCapacity=2};uint64 baseline=live();
    for(unsigned pool=0;pool<2;++pool){
        xls_ast_tree_work work;XL6_AstNode* out=xlsAstCloneFactTrees(&forest,NULL,NULL,pool ? 1024u*1024u : 0,NULL,&work,NULL);
        CHECK(out && equal(&forest,out) && out->children[0]==out->children[1] && work.hits==1);xl6AstFree(out);CHECK(live()==baseline);
        unsigned calls=0;xls_ast_clone_options options={.transform=changeReceiver,.user=&calls};
        CHECK(!xlsAstCloneFactTrees(&forest,&options,NULL,pool ? 1024u*1024u : 0,NULL,NULL,NULL) && live()==baseline);
        calls=0;out=xlsAstClone(&forest,&options,NULL);CHECK(out && out->children[0]->semanticReceiverSymbolId==107 && out->children[1]->semanticReceiverSymbolId==207);
        xl6AstFree(out);CHECK(live()==baseline);
    }
    xls_ast_clone_options options={.transform=badTransfer};CHECK(!xlsAstClone(input,&options,NULL) && live()==baseline);
    options.user=input;CHECK(!xlsAstClone(input,&options,NULL) && live()==baseline);
    CHECK(!strcmp(input->compiledPackageLink,"shared😀"));xl6AstFree(input);return 1;
}
int main(void)
{
    xrtMemStatsEnable(true);if(!copies() || !sizing() || !identities() || !leaves() || !trees())return 1;
    printf("analysis_ast_package_test passed: %u checks;owned links,all new scalar identities,heap/pool/materialized lifetimes,sizing,null/empty,intern andfact sharing.\n",checks);return 0;
}
