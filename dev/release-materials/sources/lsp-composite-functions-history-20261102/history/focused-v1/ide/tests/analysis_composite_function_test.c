/* Reuse the original allocation/teardown probes and native field oracle. */
#define main originalOwnershipMain
#include "analysis_typed_function_test.c"
#undef main

static xls_function_origin* emptyOrigin(void)
{
    XL6_SourceReport report={0};report.complete=1;
    return xlsFunctionOriginCapture(&report,NULL,NULL,NULL,NULL,NULL);
}
static void detach(XL6_AstNode* root,xls_typed_function* const* owners)
{for(size_t i=0;i<root->childCount;++i)if(owners[i])root->children[i]=NULL;}
static XL6_AstNode* presentation(xls_typed_function* const* owners,size_t count)
{
    XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);if(!root)return NULL;
    for(size_t i=0;i<count;++i){
        XL6_AstNode* child=owners[i] ? (XL6_AstNode*)xlsTypedFunctionRoot(owners[i]) : xl6AstNew(XL6_AST_FUNC,NULL);
        if(!child)goto failed;
        xl6AstAddChild(root,child);
        if(root->childCount!=i+1){if(!owners[i])xl6AstFree(child);goto failed;}
    }
    return root;
failed:detach(root,owners);xl6AstFree(root);return NULL;
}
static int compositeFaults(void)
{
    XL6_AstNode* original=fixture();CHECK(original);
    xls_typed_module* source=xlsTypedModuleTake(original);CHECK(source);
    xls_typed_function* f=xlsTypedModuleFunction(source,0);CHECK(f);
    xls_typed_function* owners[]={f,NULL};
    XL6_AstNode* invalid=presentation(owners,2);CHECK(invalid);
    CHECK(!xlsTypedModuleTakeBorrowed(NULL,owners,2));
    CHECK(!xlsTypedModuleTakeBorrowed(invalid,NULL,2));
    CHECK(!xlsTypedModuleTakeBorrowed(invalid,owners,1));
    xls_typed_function* wrong[]={NULL,f};
    CHECK(!xlsTypedModuleTakeBorrowed(invalid,wrong,2));
    CHECK(invalid->children[0]==xlsTypedFunctionRoot(f));detach(invalid,owners);xl6AstFree(invalid);
    size_t sites=0;
    for(size_t i=0;i<=sites;++i){
        XL6_AstNode* root=presentation(owners,2);CHECK(root);
        XL6_AstNode* fresh=root->children[1];watch(0,root);watch(1,fresh);
        faultCalls=faultDenied=0;faultAt=i;faultActive=1;
        xls_typed_module* module=xlsTypedModuleTakeBorrowed(root,owners,2);faultActive=0;
        if(!i){sites=faultCalls;CHECK(module && sites==4);}
        else CHECK(faultDenied==1);
        CHECK(root->children[0]==xlsTypedFunctionRoot(f) && root->children[1]==fresh);
        if(module){
            xls_typed_function* borrowed=xlsTypedModuleFunction(module,0);CHECK(borrowed==f);xlsTypedFunctionRelease(borrowed);
            xls_typed_function* created=xlsTypedModuleFunction(module,1);
            CHECK((created!=NULL)==(i!=4));xlsTypedFunctionRelease(created);xlsTypedModuleRelease(module);
        }else{CHECK(i==1 || i==2);detach(root,owners);xl6AstFree(root);}
        CHECK(!tracked && xrtAtomic32Load(&drops[0],XMEMORY_RELAXED)==1 && xrtAtomic32Load(&drops[1],XMEMORY_RELAXED)==1);
        CHECK(sameText(xlsTypedFunctionRoot(f)->text,"first"));
    }
    XL6_AstNode* root=presentation(owners,2);CHECK(root);failMutex=1;
    xls_typed_module* noForest=xlsTypedModuleTakeBorrowed(root,owners,2);failMutex=0;CHECK(noForest);
    CHECK(!xlsTypedModuleFunction(noForest,1));xlsTypedModuleRelease(noForest);
    xlsTypedModuleRelease(source);CHECK(sameText(xlsTypedFunctionRoot(f)->text,"first"));xlsTypedFunctionRelease(f);
    CHECK(!xlsTypedFunctionMetadataOwner(NULL) && !xlsFunctionIndexAssetMetadataOwner(NULL));
    printf("composite construction:%zu actual allocation refusals;mandatory failure unchanged;optional identity/forest/mutex fallback\n",sites);return 1;
}
static int compositeSets(void)
{
    xls_typed_module* sources[3];xls_typed_function* owners[6]={0};
    for(size_t i=0;i<3;++i){
        XL6_AstNode* original=fixture();CHECK(original);xls_ast_pool_work work={0};
        XL6_AstNode* pooled=xlsAstClonePooled(original,NULL,NULL,NULL,8u*1024u*1024u,&work,NULL);xl6AstFree(original);CHECK(pooled);
        sources[i]=xlsTypedModuleTake(pooled);CHECK(sources[i]);
        owners[i*2]=xlsTypedModuleFunction(sources[i],0);CHECK(owners[i*2]);
    }
    owners[1]=xlsTypedModuleFunction(sources[0],1);CHECK(owners[1]);
    XL6_AstNode* root=presentation(owners,6);CHECK(root);
    xls_typed_module* module=xlsTypedModuleTakeBorrowed(root,owners,6);CHECK(module);
    const XL6_AstNode* borrowed=xlsTypedFunctionRoot(owners[0]);
    for(size_t i=0;i<3;++i)xlsTypedModuleRelease(sources[i]);
    for(size_t i=0;i<6;++i)xlsTypedFunctionRelease(owners[i]);
    xls_function_origin* origin=emptyOrigin();CHECK(origin);
    xls_function_unit* units[6];size_t sum=0,duplicates=0;
    const void* identities[6]={0};
    for(size_t i=0;i<6;++i){
        xls_typed_function* f=xlsTypedModuleFunction(module,i);CHECK(f);
        identities[i]=xlsTypedFunctionMetadataOwner(f);CHECK(identities[i]);
        for(size_t j=0;j<i;++j)if(identities[j]==identities[i]){duplicates+=xlsTypedFunctionMetadataBytes(f);break;}
        units[i]=xlsFrontendFunctionUnitCaptureOwned(f,NULL,origin,64u*1024u*1024u,NULL,NULL);CHECK(units[i]);
        CHECK(xlsFrontendFunctionUnitRoot(units[i])==root->children[i]);sum+=xlsFunctionUnitInfo(units[i]).ownedBytes;
        xlsTypedFunctionRelease(f);
    }
    CHECK(identities[0]==identities[1] && identities[0]!=identities[2] && identities[2]!=identities[4] && identities[3]==identities[5]);
    XL6_AstNode* empty=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(empty);
    xls_function_set* emptySet=xlsFrontendFunctionSetCapture(empty,NULL,origin,64u*1024u*1024u,NULL,NULL);CHECK(emptySet);
    size_t overhead=xlsFunctionSetInfo(emptySet).ownedBytes;xlsFunctionSetRelease(emptySet);xl6AstFree(empty);
    xarray slots={0};CHECK(xrtArrayInit(&slots,sizeof(void*)));
    size_t initial=slots.Capacity;for(size_t i=0;i<6;++i)CHECK(xrtArrayPush(&slots,&root));
    size_t expected=overhead+sum-duplicates+(slots.Capacity-initial)*sizeof(void*);xrtArrayUnit(&slots);
    countClones=1;xrtAtomic32Store(&cloneCalls,0,XMEMORY_RELAXED);
    xls_function_set* set=xlsFrontendFunctionSetCaptureOwned(module,root,NULL,origin,64u*1024u*1024u,NULL,NULL);countClones=0;
    CHECK(set && !xrtAtomic32Load(&cloneCalls,XMEMORY_RELAXED));
    CHECK(xlsFunctionSetInfo(set).ownedBytes==expected && xlsFunctionSetInfo(set).functions==6);
    xls_function_set* exact=xlsFrontendFunctionSetCaptureOwned(module,root,NULL,origin,expected,NULL,NULL);CHECK(exact);xlsFunctionSetRelease(exact);
    CHECK(!xlsFrontendFunctionSetCaptureOwned(module,root,NULL,origin,expected-1,NULL,NULL));
    size_t sites=0;
    for(size_t i=0;i<=sites;++i){
        faultCalls=faultDenied=0;faultAt=i;faultActive=1;
        xls_function_set* attempt=xlsFrontendFunctionSetCaptureOwned(module,root,NULL,origin,64u*1024u*1024u,NULL,NULL);faultActive=0;
        if(!i){sites=faultCalls;CHECK(attempt && sites>20 && sites<1024);}
        else CHECK(faultDenied==1);
        xlsFunctionSetRelease(attempt);CHECK(!tracked);
    }
    xlsTypedModuleRelease(module);xlsFunctionOriginRelease(origin);
    for(size_t i=0;i<6;++i){
        xls_function_unit* held=xlsFunctionSetUnit(set,i);CHECK(held && sameAst(xlsFrontendFunctionUnitRoot(held),xlsFrontendFunctionUnitRoot(units[i])));
        xlsFunctionUnitRelease(held);xlsFunctionUnitRelease(units[i]);
    }
    CHECK(xlsFrontendFunctionUnitRoot(xlsFunctionSetUnit(NULL,0))==NULL);
    xls_function_unit* first=xlsFunctionSetUnit(set,0);CHECK(first && xlsFrontendFunctionUnitRoot(first)==borrowed);
    xlsFunctionSetRelease(set);CHECK(sameText(xlsFrontendFunctionUnitRoot(first)->text,"first"));xlsFunctionUnitRelease(first);
    printf("composite set:4 distinct native forests,6 functions,no clones,exact quota=%zu,%zu actual refusal sites,producer-independent lifetime\n",expected,sites);return 1;
}
int main(void)
{
    if(originalOwnershipMain())return 1;
    if(!compositeFaults() || !compositeSets())return 1;
    printf("analysis_composite_function_test passed:%u checks\n",checks);return 0;
}
