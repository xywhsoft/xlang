#include "analysis/frontend_internal.h"
#include <stdio.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
/* Real generated native TU and real receipt encoder. These boundary fixtures
 * supplement the independent full original AST/IR pipeline comparison. */
#include "xl6_sem.c"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"analysis_final_environment_test:%d:%s\n",__LINE__,#x);return 0;}}while(0)
static int different(XL6_SemContext* ctx,const xstrbuf* original)
{xstrbuf key={0};int okay=xlsSemFinalEnvironment(ctx,&key) && !xlsSemEqualKey(&key,original);xrtStrBufFree(&key);return okay;}
static int environment(void)
{
    XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(root);
    XL6_Scope scope={0};XL6_SemContext ctx={.root=root,.scope=&scope};xl6TypeRegistryInit(&ctx.types);CHECK(xl6TypeRegistryAddBuiltins(&ctx.types));
    xstrbuf key={0};CHECK(xlsSemFinalEnvironment(&ctx,&key));XL6_TypeInfo* type=&ctx.types.items[0];XL6_TypeInfo saved=*type;
#define CHANGE(field,value) do{type->field=(value);CHECK(different(&ctx,&key));*type=saved;}while(0)
    CHANGE(id,saved.id+1);CHANGE(abiId,saved.abiId^UINT64_C(0x8000000000000001));
    CHANGE(name,"different-name");CHANGE(abiName,"different-abi");CHANGE(kind,XL6_TYPE_ALIAS);
    CHANGE(allowGeneric,!saved.allowGeneric);CHANGE(genericArity,saved.genericArity+1);
    XL6_AstNode* declaration=xl6AstNew(XL6_AST_TYPE_ALIAS,NULL);CHECK(declaration);
    CHANGE(decl,declaration);
    uint32_t next=ctx.types.nextId;CHECK(xl6TypeRegistryReserveId(&ctx.types) && different(&ctx,&key));ctx.types.nextId=next;
    type->decl=declaration;xrtStrBufFree(&key);CHECK(xlsSemFinalEnvironment(&ctx,&key));
    declaration->semanticTypeId=77;CHECK(different(&ctx,&key));declaration->semanticTypeId=0;
    declaration->line=5;declaration->column=8;declaration->range=(XL6_AstRange){.known=1,.startOffset=12,.endOffset=17};
    xstrbuf same={0};CHECK(xlsSemFinalEnvironment(&ctx,&same) && xlsSemEqualKey(&key,&same));xrtStrBufFree(&same);
    ctx.currentClass=declaration;CHECK(!xlsSemFinalEnvironment(&ctx,&same));ctx.currentClass=NULL;
    ctx.completionQuery=(XL6_CompletionQuery*)(uintptr_t)1;CHECK(!xlsSemFinalEnvironment(&ctx,&same));ctx.completionQuery=NULL;
    *type=saved;xl6AstFree(declaration);xrtStrBufFree(&key);xl6TypeRegistryFree(&ctx.types);xl6AstFree(root);return 1;
}
static int planState(void)
{
    xls_sem_memo memo={.finalPrepared=1,.finalContext=(xls_sem_final_context*)(uintptr_t)1};
    CHECK(xrtArrayInit(&memo.entries,sizeof(xls_sem_entry)) && xrtArrayResize(&memo.entries,1));
    xls_sem_entry* entry=xrtArrayGet(&memo.entries,0);entry->finalReplayed=1;entry->finalPredictedStart=7;entry->finalPredictedEnd=11;
    xls_sem_build build={.previous=xlsActiveSemBuild,.memo=&memo};xlsActiveSemBuild=&build;uint32_t next=7;
    XL6_SemPlanWalkState base={.currentRegion=1,.functionTargetRegion=1,.exceptionTargetRegion=1,.nextRegion=&next};
    CHECK(xlsSemFinalPlanBefore(NULL,0,&base) && next==11);xlsSemFinalPlanAfter(NULL,0,&base);
    CHECK(entry->finalPlanValid && entry->finalRegionStart==7 && entry->finalRegionEnd==11);
    for(unsigned i=0;i<8;++i){XL6_SemPlanWalkState changed=base;next=7;
        switch(i){case 0:changed.scopeDepth=1;break;case 1:changed.functionTargetScopeDepth=1;break;case 2:changed.exceptionTargetScopeDepth=1;break;
            case 3:changed.currentRegion=2;break;case 4:changed.functionTargetRegion=2;break;case 5:changed.exceptionTargetRegion=2;break;
            case 6:changed.loop=(const XL6_SemPlanLoopFrame*)(uintptr_t)1;break;default:changed.nextRegion=NULL;break;}
        CHECK(!xlsSemFinalPlanBefore(NULL,0,&changed) && next==7);
    }
    next=8;CHECK(!xlsSemFinalPlanBefore(NULL,0,&base) && next==8);
    next=7;entry->finalReplayed=0;CHECK(!xlsSemFinalPlanBefore(NULL,0,&base) && next==7);
    xlsActiveSemBuild=build.previous;xrtArrayUnit(&memo.entries);return 1;
}
static xls_sem_final_reader reader(const xls_sem_ir_receipt* receipt,int write)
{return (xls_sem_final_reader){.receipt=receipt,.oldStart=2,.oldEnd=3,.start=11,.end=12,.write=write};}
static int receipts(void)
{
    XL6_AstNode original={.kind=XL6_AST_NAME,.flags=UINT32_C(0x80000000),.semanticTypeId=17,.semanticTypeAbiId=UINT64_C(0xfedcba9876543210),
        .semanticOwnership=(XL6_SemOwnershipAction)1,.semanticExitKind=XL6_SEM_EXIT_RETURN,.semanticScopeDepth=3,.semanticTargetScopeDepth=2,.semanticRegionId=2,.semanticTargetRegionId=2};
    uint64_t before[XLS_SEM_IR_FIELDS]={0};xls_sem_ir_writer writer={.limit=8192};CHECK(xlsSemIrRecord(&writer,&original,before,NULL));
    xls_sem_ir_receipt receipt={.data=writer.data,.nodes=1};XL6_AstNode node={.kind=XL6_AST_NAME};xls_sem_final_reader read=reader(&receipt,0);
    CHECK(xlsSemFinalVisit(&read,&node,0,0) && read.at==receipt.data.Size && read.nodes==1 && !node.semanticTypeId);
    read=reader(&receipt,1);CHECK(xlsSemFinalVisit(&read,&node,0,0));
    CHECK(node.flags==original.flags && node.semanticTypeId==17 && node.semanticTypeAbiId==original.semanticTypeAbiId &&
        node.semanticOwnership==original.semanticOwnership && node.semanticExitKind==original.semanticExitKind &&
        node.semanticScopeDepth==3 && node.semanticTargetScopeDepth==2 && node.semanticRegionId==11 && node.semanticTargetRegionId==11);
    size_t bytes=receipt.data.Size;receipt.data.Size=3;read=reader(&receipt,0);CHECK(!xlsSemFinalVisit(&read,&node,0,0));receipt.data.Size=bytes;
    uint32_t mask=0;memcpy(&mask,receipt.data.Data,4);uint32_t invalid=mask|(UINT32_C(1)<<3);memcpy(receipt.data.Data,&invalid,4);
    read=reader(&receipt,0);CHECK(!xlsSemFinalVisit(&read,&node,0,0));memcpy(receipt.data.Data,&mask,4);
    read=reader(&receipt,0);read.oldEnd=2;CHECK(!xlsSemFinalVisit(&read,&node,0,0));
    read=reader(&receipt,0);read.end=11;CHECK(!xlsSemFinalVisit(&read,&node,0,0));
    receipt.nodes=0;read=reader(&receipt,0);CHECK(!xlsSemFinalVisit(&read,&node,0,0));receipt.nodes=1;
    xrtStrBufFree(&writer.data);return 1;
}
static int geometrySnapshot(xls_sem_memo* memo,const XL6_AstNode* root,const xls_sem_memo* previous)
{
    xstrbuf original={0};CHECK(xlsSemKeyNode(&original,root,0,1));
    CHECK(xlsSemKeyGeometry(memo,root,previous,1) && xlsSemEqualKey(&original,&memo->context));
    xrtStrBufFree(&original);return 1;
}
static void geometryUnit(xls_sem_memo* memo)
{xlsSemGeometryRelease(memo->geometry);xrtStrBufFree(&memo->context);*memo=(xls_sem_memo){0};}
static int geometryProducers(xls_sem_memo* memo,const XL6_AstNode* header)
{
    XL6_Scope scope;xl6ScopeInit(&scope,NULL);XL6_SemContext ctx={.scope=&scope};
    XL6_AstNode* target=xl6AstNewText(XL6_AST_NAME,"_",1,NULL);CHECK(target);
    target->line=header->line;target->column=header->column;
    CHECK(xl6SemDeclareForTargetType(&ctx,target,xl6SemCloneTypeNode(header)));
    XL6_AstNode* direct=xl6SemCloneTypeNode(header);CHECK(direct);
    const XL6_AstNode* values[]={target->inferredType,direct};
    xls_ast_intern* owner=xlsAstInternCapture(values,2,65536,65536,NULL);
    CHECK(owner && values[0]==values[1]);
    /* A real for-target producer can share the exact immutable TYPE with a
     * declared call result. Provenance belongs to each edge,not the pointer. */
    XL6_AstNode callee={.kind=XL6_AST_NAME,.semanticSymbolId=998,.semanticTargetSymbolId=73};XL6_AstNode* children[]={&callee};
    XL6_AstNode call={.kind=XL6_AST_CALL,.children=children,.childCount=1,
        .semanticCallKind=XL6_SEM_CALL_DIRECT,.semanticTargetSymbolId=73};
    xls_sem_unit unit={.symbols={.floor=1000,.start=1001,.end=1002,.local=1}};
    xarray raw={0};CHECK(xrtArrayInit(&unit.facts,sizeof(xls_sem_fact)) && xrtArrayInit(&raw,sizeof(xls_sem_raw)));
    for(size_t i=0;i<3;++i){xls_sem_fact fact={.inferred=values[i ? 1 : 0]};xls_sem_raw at={.node=i==2 ? &callee : i ? &call : target};
        CHECK(xrtArrayPush(&unit.facts,&fact) && xrtArrayPush(&raw,&at));}
    xls_sem_build build={.previous=xlsActiveSemBuild,.memo=memo,.proofRaw=&raw};xlsActiveSemBuild=&build;
    CHECK(xlsSemProofOrigin(&unit,1)==73 && xlsSemProofOrigin(&unit,2)==73);
    call.semanticCallKind=0;CHECK(!xlsSemProofOrigin(&unit,1) && !xlsSemProofOrigin(&unit,2));call.semanticCallKind=XL6_SEM_CALL_DIRECT;
    callee.semanticSymbolId=1001;CHECK(!xlsSemProofOrigin(&unit,2));callee.semanticSymbolId=998;
    callee.semanticTargetSymbolId=74;CHECK(!xlsSemProofOrigin(&unit,2));callee.semanticTargetSymbolId=73;
    unit.symbols.floor=73;CHECK(!xlsSemProofOrigin(&unit,1));unit.symbols.floor=1000;
    xls_sem_proof* proof=xlsSemProofCreate(&unit,4096);xlsActiveSemBuild=build.previous;
    CHECK(proof && proof->located.Count==3);
    const xls_sem_location *loop=xrtArrayConstGet(&proof->located,0),*result=xrtArrayConstGet(&proof->located,1);
    CHECK(loop->node==result->node && !loop->header && result->header==73);
    CHECK(((const xls_sem_location*)xrtArrayConstGet(&proof->located,2))->header==73);
    /* The native loop producer mixes a declaration's ranges/source with the
     * binding's line/column. Coordinates alone cannot establish its origin. */
    target->line++;CHECK(xl6SemDeclareForTargetType(&ctx,target,xl6SemCloneTypeNode(header)));
    CHECK(target->inferredType->line==header->line+1 &&
        xlsSemSameRange(target->inferredType->range,header->range) &&
        !strcmp(xl6AstSourcePath(target->inferredType),xl6AstSourcePath(header)));
    xlsSemProofDestroy(proof);xrtArrayUnit(&raw);xrtArrayUnit(&unit.facts);xlsAstInternDestroy(owner);
    xl6AstFree(direct);xl6AstFree(target);xl6SemFreeOwnedTypeNodes(&ctx);xl6ScopeFree(&scope);return 1;
}
static int geometry(void)
{
    const char* text="func callee(int value=1) as int\r\n return value+0\r\nend\r\nfunc main(...) as int\r\n return callee()\r\nend\r\n";
    const char* moved="// 中文😀 moved\r\nfunc callee(int value=1) as int\r\n return value+1\r\nend\r\nfunc main(...) as int\r\n return callee()\r\nend\r\n";
    char* path=xrtPathAbs("geometry-origin-中文😀.xl");CHECK(path);
    XL6_ParseResult first,current;xl6ParseResultInit(&first);xl6ParseResultInit(&current);
    CHECK(xl6ParseSource(text,path,&first) && xl6ParseSource(moved,path,&current));
    first.root->children[0]->semanticSymbolId=current.root->children[0]->semanticSymbolId=73;
    first.root->children[1]->semanticSymbolId=current.root->children[1]->semanticSymbolId=74;
    XL6_AstNode* oldType=first.root->children[0]->children[2]->children[0];
    XL6_AstNode* newType=current.root->children[0]->children[2]->children[0];CHECK(oldType->kind==XL6_AST_TYPE && newType->kind==XL6_AST_TYPE);
    xls_sem_memo before={0},same={0},after={0};
    CHECK(geometrySnapshot(&before,first.root,NULL) && before.geometry && before.geometryWork.captured==1);
    CHECK(geometrySnapshot(&same,first.root,&before) && same.geometry==before.geometry && same.geometryWork.reused==1);
    CHECK(geometrySnapshot(&after,current.root,&before) && after.geometry && after.geometry!=before.geometry && xlsSemEqualKey(&before.context,&after.context));
    xls_sem_position expected={newType->range,newType->tokenRange,newType->line,newType->column},mapped={0};
    CHECK(xlsSemGeometryPosition(before.geometry,after.geometry,oldType,73,&mapped) && xlsSemGeometryPositionEqual(mapped,expected));
    CHECK(geometryProducers(&before,oldType));
    size_t known=before.geometry->bytes;
    for(unsigned mode=0;mode<3;++mode){
        xls_sem_geometry_control outer,inner;size_t limit=mode==0 ? 0 : mode==1 ? known-1 : known;
        xlsFrontendSemGeometryBegin(&outer,limit);xlsFrontendSemGeometryBegin(&inner,SIZE_MAX);CHECK(xlsSemGeometryBudget()==limit);
        xls_sem_memo bounded={0};CHECK(geometrySnapshot(&bounded,first.root,NULL));
        CHECK(mode==2 ? bounded.geometry && bounded.geometry->bytes==known : !bounded.geometry);
        geometryUnit(&bounded);xlsFrontendSemGeometryEnd(&inner);xlsFrontendSemGeometryEnd(&outer);
    }
    /* The complete semantic context may differ even for equal AST header
     * values. A coordinate-only receipt must not bypass that decision. */
    xls_sem_moves moves={.changed=1};CHECK(xrtMapInit(&moves.paths,sizeof(xls_sem_move)));
    xls_text *oldText=xlsTextCreate(text,strlen(text),4096,NULL),*newText=xlsTextCreate(moved,strlen(moved),4096,NULL);CHECK(oldText && newText);
    xls_sem_move move={.oldText=oldText,.newText=newText,.path=path,.start=0,.end=strlen(text),.delta=(int64_t)strlen(moved)-(int64_t)strlen(text),.changed=1};
    CHECK(xrtMapSet(&moves.paths,(xbytesview){(const unsigned char*)path,strlen(path)},&move));
    xarray empty={0},positions={0};CHECK(xrtArrayInit(&positions,sizeof(xls_sem_position)));
    xls_sem_apply apply={.moves=&moves,.previous=&empty,.positions=&positions};XL6_AstNode projected=*oldType;
    xls_sem_build build={.previous=xlsActiveSemBuild,.memo=&after,.baseline=&before};after.trackInputs=1;xlsActiveSemBuild=&build;
    CHECK(!xlsSemApplyPosition(oldType,&projected,&apply) && !after.geometryWork.mapped);
    build.compatible=1;CHECK(!xlsSemApplyPosition(oldType,&projected,&apply) && !after.geometryWork.mapped);
    apply.geometryOrigin=73;build.compatible=0;CHECK(!xlsSemApplyPosition(oldType,&projected,&apply) && !after.geometryWork.mapped);
    build.compatible=1;CHECK(xlsSemApplyPosition(oldType,&projected,&apply) && after.geometryWork.mapped==1);
    CHECK(xlsSemSameRange(projected.range,newType->range) && xlsSemSameRange(projected.tokenRange,newType->tokenRange));
    xlsActiveSemBuild=build.previous;xrtArrayUnit(&positions);xrtMapUnit(&moves.paths);xlsTextRelease(oldText);xlsTextRelease(newText);
    XL6_AstSource source={.path=path,.references=-12345};XL6_AstNode query={.kind=oldType->kind,.range=oldType->range,.tokenRange=oldType->tokenRange,.line=oldType->line,.column=oldType->column,.source=&source};
    XL6_AstNode wrong=query;wrong.tokenRange.endColumn++;mapped.line=-7;
    CHECK(!xlsSemGeometryPosition(before.geometry,after.geometry,&wrong,73,&mapped) && mapped.line==-7);
    wrong=query;wrong.kind=XL6_AST_NAME;CHECK(!xlsSemGeometryPosition(before.geometry,after.geometry,&wrong,73,&mapped));
    CHECK(!xlsSemGeometryPosition(before.geometry,after.geometry,&query,74,&mapped));
    source.path="D:/not-the-origin.xl";CHECK(!xlsSemGeometryPosition(before.geometry,after.geometry,&query,73,&mapped));source.path=path;
    /* Two old nodes with the same complete tuple must agree about the current
     * tuple. The production parser need not manufacture such corrupt input. */
    XL6_AstNode* parameter=first.root->children[0]->children[1]->children[0]->children[0];CHECK(parameter->kind==XL6_AST_TYPE);
    oldType->range=parameter->range;oldType->tokenRange=parameter->tokenRange;oldType->line=parameter->line;oldType->column=parameter->column;
    xls_sem_memo ambiguous={0};CHECK(geometrySnapshot(&ambiguous,first.root,NULL));
    CHECK(!xlsSemGeometryPosition(ambiguous.geometry,after.geometry,parameter,73,&mapped));geometryUnit(&ambiguous);
    xl6ParseResultFree(&first);xl6ParseResultFree(&current);geometryUnit(&before);geometryUnit(&after);
    /* Only the independent reused coordinate asset remains. No AST, source
     * object, report or complete document is needed for this read. */
    CHECK(xlsSemGeometryPosition(same.geometry,same.geometry,&query,73,&mapped) && xlsSemSameRange(mapped.range,query.range) && source.references==-12345);
    geometryUnit(&same);xrtFree(path);return 1;
}
int main(void)
{
    if(!environment() || !planState() || !receipts() || !geometry())return 1;
    printf("analysis_final_environment_test passed:%u checks;all native TypeInfo fields,registry sequence,exact declarations,complete root plan frame,and real final receipt boundaries\n",checks);return 0;
}
#else
int main(void){puts("analysis_final_environment_test skipped:old native representation");return 0;}
#endif
