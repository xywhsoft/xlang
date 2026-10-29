/* The qualification target privately namespaces the real index TU before
 * including it here. Exercise its actual declaration-version serializer. */
#include "xls_index_package_probe.h"
#include <stdio.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_entity_package_key_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int same(const xstrbuf* a,const xstrbuf* b){return a->Size==b->Size && !memcmp(a->Data,b->Data,a->Size);}
static int key(const XL6_AstNode* node,xstrbuf* out){return xlsEntityVersionHeader(out,NULL,node,0);}
static int run(void)
{
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    XL6_AstNode* root=xl6AstNewText(XL6_AST_TYPE,"Owner",5,NULL);CHECK(root);
    xl6AstAddChild(root,xl6AstNewText(XL6_AST_TYPE,"Child",5,NULL));CHECK(root->childCount==1 && root->children[0]);
    root->inferredType=xl6AstNewText(XL6_AST_TYPE,"Inferred",8,NULL);
    root->semanticDeclaredType=xl6AstNewText(XL6_AST_TYPE,"Declared",8,NULL);CHECK(root->inferredType && root->semanticDeclaredType);
    XL6_AstNode* nodes[]={root,root->children[0],root->inferredType,root->semanticDeclaredType};
    xstrbuf baseline={0};CHECK(key(root,&baseline));
    for(size_t i=0;i<sizeof(nodes)/sizeof(*nodes);++i){
        xstrbuf changed={0},restored={0};
#define FIELD(field) do{nodes[i]->field=23;CHECK(key(root,&changed) && !same(&baseline,&changed));xrtStrBufFree(&changed);nodes[i]->field=0;CHECK(key(root,&restored) && same(&baseline,&restored));xrtStrBufFree(&restored);}while(0)
        FIELD(semanticReceiverSymbolId);FIELD(compiledPackageError);FIELD(compiledPackageRoot);
#undef FIELD
        nodes[i]->compiledPackageLink=xrtStrDup("");CHECK(nodes[i]->compiledPackageLink);
        CHECK(key(root,&changed) && !same(&baseline,&changed));xstrbuf empty=changed;changed=(xstrbuf){0};
        xrtFree(nodes[i]->compiledPackageLink);nodes[i]->compiledPackageLink=xrtStrDup("abi-v2-包");CHECK(nodes[i]->compiledPackageLink);
        CHECK(key(root,&changed) && !same(&empty,&changed));xrtStrBufFree(&empty);xrtStrBufFree(&changed);
        xrtFree(nodes[i]->compiledPackageLink);nodes[i]->compiledPackageLink=NULL;
        CHECK(key(root,&restored) && same(&baseline,&restored));xrtStrBufFree(&restored);
    }
    xls_entity_binding left={.native=23,.stable=9001},right={.native=47,.stable=9001};
    xls_entity_view a={0},b={0};
    CHECK(xrtArrayInit(&a.symbols,sizeof(left)) && xrtArrayPush(&a.symbols,&left));
    CHECK(xrtArrayInit(&b.symbols,sizeof(right)) && xrtArrayPush(&b.symbols,&right));
    xstrbuf boundA={0},boundB={0},different={0};root->semanticReceiverSymbolId=23;
    CHECK(xlsEntityVersionHeader(&boundA,&a,root,0));root->semanticReceiverSymbolId=47;
    CHECK(xlsEntityVersionHeader(&boundB,&b,root,0) && same(&boundA,&boundB));
    ((xls_entity_binding*)xrtArrayGet(&b.symbols,0))->stable=9002;
    CHECK(xlsEntityVersionHeader(&different,&b,root,0) && !same(&boundA,&different));
    xrtStrBufFree(&boundA);xrtStrBufFree(&boundB);xrtStrBufFree(&different);root->semanticReceiverSymbolId=0;
    xrtArrayUnit(&a.symbols);xrtArrayUnit(&b.symbols);
    root->compiledPackageLink=xrtMalloc(XLS_ENTITY_KEY_BYTES+1);CHECK(root->compiledPackageLink);
    memset(root->compiledPackageLink,'x',XLS_ENTITY_KEY_BYTES);root->compiledPackageLink[XLS_ENTITY_KEY_BYTES]=0;
    xstrbuf oversized={0};CHECK(!key(root,&oversized));xrtStrBufFree(&oversized);
    xrtFree(root->compiledPackageLink);root->compiledPackageLink=NULL;CHECK(key(root,&oversized) && same(&baseline,&oversized));xrtStrBufFree(&oversized);
    xrtStrBufFree(&baseline);xl6AstFree(root);
#endif
    /* Function bodies are deliberately separate production assets. Preserve
     * their existing exclusion from a declaration's environment version. */
    XL6_AstNode* function=xl6AstNew(XL6_AST_FUNC,NULL);CHECK(function);
    xl6AstAddChild(function,xl6AstNewText(XL6_AST_NAME,"f",1,NULL));CHECK(function->childCount==1 && function->children[0]);
    XL6_AstNode* body=xl6AstNew(XL6_AST_BLOCK,NULL);CHECK(body);xl6AstAddChild(function,body);CHECK(function->childCount==2);
    xstrbuf before={0},after={0};CHECK(key(function,&before));body->text=xrtStrDup("body edit");CHECK(body->text);
    CHECK(key(function,&after) && same(&before,&after));xrtStrBufFree(&before);xrtStrBufFree(&after);xl6AstFree(function);
    return 1;
}
int main(void)
{if(!run())return 1;printf("analysis_entity_package_key_test passed: %u checks; receiver/package metadata across all header forests and original body exclusion\n",checks);return 0;}
