/* Included after the real current raw-memo IR implementation. */
#include "analysis/frontend_internal.h"
#include "src/sem/xl6_sem.h"
#include "analysis/ast_clone.h"
static unsigned irChecks;
static unsigned irDupFault,irDupCalls;
char* __real_xrtStrDupN(const char*,size_t);
char* __wrap_xrtStrDupN(const char* text,size_t size)
{if(irDupFault && ++irDupCalls==irDupFault)return NULL;return __real_xrtStrDupN(text,size);}
xls_entity_view* xlsProbeEntityBinding(uint32_t,uint64_t);
void xlsProbeEntityBindingDrop(xls_entity_view*);
int xlsReferenceIrBuildModuleFromAst(XL6_IrModule*,const XL6_AstNode*,XL6_DiagList*);
int xlsReferenceIrVerifyModule(const XL6_IrModule*,XL6_DiagList*);
void xlsReferenceIrModuleFree(XL6_IrModule*);
void xlsReferenceIrDumpModuleJson(XL6_Output*,const XL6_IrModule*);
#define IR_CHECK(v) do{++irChecks;if(!(v)){fprintf(stderr,"analysis_ir_current_probe:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static uint64_t irLive(void)
{xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
static int irTypeKeys(void)
{
    uint64_t live=irLive();XL6_IrTypeId params[4]={1,2,3,4},returns[4]={5,6,7,8};XL6_IrParameterMode modes[4]={0};XL6_IrParameterAbi abis[4]={0};
    XL6_IrType type={.params=params,.paramCount=2,.parameterModes=modes,.parameterAbis=abis,.returns=returns,.returnCount=2,.hash=79};XL6_IrTypePool pool={.items=&type,.count=1,.capacity=1};
    xstrbuf before={0};IR_CHECK(xlsIrMemoTypes(&pool,&before,65536));
#define IR_TYPE_FIELD(field) do{uint64_t saved=type.field;type.field=saved+1;xstrbuf changed={0};IR_CHECK(xlsIrMemoTypes(&pool,&changed,65536) && !xlsIrMemoEqual(&before,&changed));xrtStrBufFree(&changed);type.field=saved;}while(0)
    IR_TYPE_FIELD(kind);IR_TYPE_FIELD(cAbi);IR_TYPE_FIELD(collectionAbi);IR_TYPE_FIELD(collectionCarrierAbi);IR_TYPE_FIELD(runtimeResourceAbi);IR_TYPE_FIELD(ownershipAbi);IR_TYPE_FIELD(nullAbi);
    IR_TYPE_FIELD(persistenceValueAbi);IR_TYPE_FIELD(persistenceRestoreScalarAbi);IR_TYPE_FIELD(persistenceRestoreCarrierAbi);IR_TYPE_FIELD(persistencePayloadType);IR_TYPE_FIELD(persistenceWireFormat);
    IR_TYPE_FIELD(integerTarget);IR_TYPE_FIELD(nativeProductAbi);IR_TYPE_FIELD(nativeSlotAbi);IR_TYPE_FIELD(xvalueAdmissionAbi);IR_TYPE_FIELD(xvalueAdmissionNullable);IR_TYPE_FIELD(xvalueAdmissionTypeValueId);
    IR_TYPE_FIELD(sourceTypeId);IR_TYPE_FIELD(abiId);IR_TYPE_FIELD(typeValueId);IR_TYPE_FIELD(nominalSymbol);IR_TYPE_FIELD(paramCount);IR_TYPE_FIELD(returnCount);IR_TYPE_FIELD(callableResultType);IR_TYPE_FIELD(callableErasureAbi);IR_TYPE_FIELD(hash);
    IR_TYPE_FIELD(sequenceLayout.abi);IR_TYPE_FIELD(sequenceLayout.role);IR_TYPE_FIELD(sequenceLayout.elementType);IR_TYPE_FIELD(sequenceLayout.baseElementType);IR_TYPE_FIELD(sequenceLayout.slotAbi);IR_TYPE_FIELD(sequenceLayout.codecAbi);IR_TYPE_FIELD(sequenceLayout.transferAbi);IR_TYPE_FIELD(sequenceLayout.admissionAbi);IR_TYPE_FIELD(sequenceLayout.nullable);IR_TYPE_FIELD(sequenceLayout.defaultAbi);IR_TYPE_FIELD(sequenceLayout.childRole);IR_TYPE_FIELD(sequenceLayout.childElementType);IR_TYPE_FIELD(sequenceLayout.childSlotAbi);IR_TYPE_FIELD(sequenceLayout.childTransferAbi);
    IR_TYPE_FIELD(nativeValueAbi);IR_TYPE_FIELD(messageSchemaType);IR_TYPE_FIELD(treeKeySchemaType);IR_TYPE_FIELD(treeValueSchemaType);IR_TYPE_FIELD(collectionValueSchemaType);IR_TYPE_FIELD(setKeyAbi);IR_TYPE_FIELD(dictKeyAbi);
#undef IR_TYPE_FIELD
    for(unsigned n=0;n<4;++n){xstrbuf changed={0};if(n==0)++params[0];if(n==1)++returns[0];if(n==2)++modes[0];if(n==3)++abis[0];IR_CHECK(xlsIrMemoTypes(&pool,&changed,65536) && !xlsIrMemoEqual(&before,&changed));xrtStrBufFree(&changed);if(n==0)--params[0];if(n==1)--returns[0];if(n==2)--modes[0];if(n==3)--abis[0];}
    for(unsigned n=0;n<4;++n){xstrbuf changed={0};if(n==0)type.name="";if(n==1)type.displayName="";if(n==2)type.cName[63]='x';if(n==3)type.errorResultCName[47]='x';IR_CHECK(xlsIrMemoTypes(&pool,&changed,65536) && !xlsIrMemoEqual(&before,&changed));xrtStrBufFree(&changed);type.name=type.displayName=NULL;type.cName[63]=type.errorResultCName[47]=0;}
    xstrbuf small={0};IR_CHECK(!xlsIrMemoTypes(&pool,&small,1));xrtStrBufFree(&small);xrtStrBufFree(&before);IR_CHECK(irLive()==live);return 1;
}
static int irPayloads(void)
{
    uint64_t live=irLive();const unsigned char bytes[]={0,1,254,0};
    for(unsigned variant=0;variant<3;++variant){
        XL6_IrNode source={.kind=XL6_IR_NODE_NAME,.sourceKind=XL6_AST_NAME,.symbol=101,.constructorReceiverSymbol=102,.compiledPackageError=3,.compiledPackageRoot=1,.cOwnedCompareOp=1,.literalSize=3,.literalDecoded=1,.unionField=SIZE_MAX};
        source.text=xrtStrDup("value");source.compiledPackageLink=variant ? xrtStrDup(variant==1 ? "" : "package:包") : NULL;source.literalData=xrtMalloc(sizeof(bytes));IR_CHECK(source.text && source.literalData && (!variant || source.compiledPackageLink));memcpy(source.literalData,bytes,sizeof(bytes));
        xls_ir_unit unit={0};xls_ir_capture capture={.unit=&unit};xstrbuf encoded={0},small={0};IR_CHECK(!xlsIrMemoEncodeNode(&small,&source,1,&capture));xrtStrBufFree(&small);IR_CHECK(xlsIrMemoEncodeNode(&encoded,&source,65536,&capture));
        xrtFree(source.text);xrtFree(source.compiledPackageLink);xrtFree(source.literalData);
        for(int borrow=0;borrow<=1;++borrow){
            xls_ir_reader reader={(const unsigned char*)encoded.Data,0,encoded.Size};XL6_IrNode decoded={0};IR_CHECK(xlsIrMemoDecodeNode(&reader,&decoded,borrow,NULL,&unit,NULL) && reader.at==reader.size);
            IR_CHECK(decoded.constructorReceiverSymbol==102 && decoded.compiledPackageError==3 && decoded.compiledPackageRoot==1 && decoded.cOwnedCompareOp==1 && !strcmp(decoded.text,"value") && !memcmp(decoded.literalData,bytes,sizeof(bytes)));
            IR_CHECK(variant ? decoded.compiledPackageLink && !strcmp(decoded.compiledPackageLink,variant==1 ? "" : "package:包") : !decoded.compiledPackageLink);
            if(!borrow){xrtFree(decoded.text);xrtFree(decoded.literalData);xrtFree(decoded.compiledPackageLink);}
            for(size_t size=0;size<encoded.Size;++size){
                uint64_t before=irLive();reader=(xls_ir_reader){(const unsigned char*)encoded.Data,0,size};decoded=(XL6_IrNode){0};IR_CHECK(!xlsIrMemoDecodeNode(&reader,&decoded,borrow,NULL,&unit,NULL));
                if(!borrow){xrtFree(decoded.text);xrtFree(decoded.literalData);xrtFree(decoded.compiledPackageLink);}IR_CHECK(irLive()==before);
            }
        }
        xrtStrBufFree(&encoded);
    }
    IR_CHECK(irLive()==live);return 1;
}
static void irRename(XL6_AstNode* node,XL6_SymbolId from,XL6_SymbolId to)
{
    XL6_SymbolId* fields[]={&node->semanticSymbolId,&node->semanticDeclaredSymbolId,&node->semanticTargetSymbolId,&node->semanticCompoundOperatorSymbolId,&node->semanticReceiverSymbolId};
    for(size_t i=0;i<5;++i)if(*fields[i]==from)*fields[i]=to;
    for(size_t i=0;i<node->childCount;++i)irRename(node->children[i],from,to);
}
static int irReceiverDomains(void)
{
    char* path=xrtPathAbs("ir-receiver-domain.xl");IR_CHECK(path);XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
    IR_CHECK(xl6ParseSource("func receiver(int value) as int\n return value\nend\n",path,&parsed) && xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"receiver",1));
    XL6_AstNode* first=parsed.root->children[0];XL6_AstNode* second=xlsAstClone(first,NULL,NULL);IR_CHECK(second);
    xls_lexical_symbols a={0},b={0};IR_CHECK(xlsLexicalSymbolsBuild(&a,first,XLS_LEXICAL_SYMBOL_BYTES) && a.order.Count);
    XL6_SymbolId symbol=((const xls_lexical_symbol*)xrtArrayConstGet(&a.order,0))->symbol;IR_CHECK(symbol>1 && symbol<1000);irRename(second,symbol,symbol+100);IR_CHECK(xlsLexicalSymbolsBuild(&b,second,XLS_LEXICAL_SYMBOL_BYTES));
    xls_ir_unit unit={0};xls_ir_capture capture={.unit=&unit,.symbols=&a};XL6_IrNode node={.unionField=SIZE_MAX,.constructorReceiverSymbol=symbol};xstrbuf local={0},native={0};
    IR_CHECK(xlsIrMemoEncodeNode(&local,&node,65536,&capture));xls_ir_reader reader={(const unsigned char*)local.Data,0,local.Size};XL6_IrNode result={0};IR_CHECK(xlsIrMemoDecodeNode(&reader,&result,0,&b,&unit,NULL) && result.constructorReceiverSymbol==symbol+100);
    node.constructorReceiverSymbol=1;IR_CHECK(xlsIrMemoEncodeNode(&native,&node,65536,&capture) && !xlsIrMemoEqual(&local,&native));reader=(xls_ir_reader){(const unsigned char*)native.Data,0,native.Size};IR_CHECK(xlsIrMemoDecodeNode(&reader,&result,0,&b,&unit,NULL) && result.constructorReceiverSymbol==1);
    xrtStrBufFree(&local);xrtStrBufFree(&native);xlsLexicalSymbolsClear(&a);xlsLexicalSymbolsClear(&b);xl6AstFree(second);xl6ParseResultFree(&parsed);xrtFree(path);return 1;
}
static int irPersistentPayloads(void)
{
    const unsigned char binary[]={0,1,0};
    for(int borrow=0;borrow<=1;++borrow){
        uint64_t live=irLive();XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);IR_CHECK(root);
        XL6_IrNode source={.kind=XL6_IR_NODE_MODULE,.sourceKind=XL6_AST_MODULE,.unionField=SIZE_MAX,.literalSize=2,.literalDecoded=1};
        source.text=xrtStrDup("owned");source.compiledPackageLink=xrtStrDup("");source.literalData=xrtMalloc(sizeof(binary));IR_CHECK(source.text && source.compiledPackageLink && source.literalData);memcpy(source.literalData,binary,sizeof(binary));
        xls_ir_result* result=xrtCalloc(1,sizeof(*result));IR_CHECK(result);xrtAtomic32Init(&result->persistentState,0);result->storageLimit=128u*1024u;
        IR_CHECK(xrtArrayInit(&result->fragments,sizeof(xls_ir_unit*)) && xrtArrayInit(&result->borrowed,1));
        if(borrow){
            xls_ir_unit* unit=xrtCalloc(1,sizeof(*unit));IR_CHECK(unit);xrtAtomic32Init(&unit->references,1);unit->nodes=1;
            xls_ir_capture capture={.unit=unit};IR_CHECK(xlsIrMemoEncodeNode(&unit->data,&source,65536,&capture));
            xls_ir_result* previous=xlsActiveIrOwner;xlsActiveIrOwner=result;
            root->kind=XL6_AST_NAME;IR_CHECK(!xlsIrMemoRestore(unit,&result->module,root,NULL,NULL));
            IR_CHECK(!result->module.nodeCount && !result->module.childCount && !result->fragments.Count && !strcmp(source.text,"owned"));root->kind=XL6_AST_MODULE;
            IR_CHECK(xlsIrMemoRestore(unit,&result->module,root,NULL,NULL)==1);xlsActiveIrOwner=previous;
            IR_CHECK(result->fragments.Count==1);xlsIrUnitRelease(unit);xrtFree(source.text);xrtFree(source.literalData);xrtFree(source.compiledPackageLink);
        }else{
            result->module.nodes=xrtMalloc(sizeof(source));IR_CHECK(result->module.nodes);*result->module.nodes=source;result->module.nodeCount=result->module.nodeCapacity=1;
        }
        result->module.ready=1;xls_ir_persistent_keys key={0};IR_CHECK(xlsIrPersistentKeysBuild(&key,root,1024u*1024u));
        IR_CHECK(!xlsIrPersistentCapture(result,root,&key,1,0) && !result->persistent && key.size && !strcmp(result->module.nodes[0].text,"owned"));
        IR_CHECK(xlsIrPersistentCapture(result,root,&key,1024u*1024u,0));
        IR_CHECK(result->persistent && result->persistent->texts.Count==(borrow ? 0 : 3) && result->persistent->fragments.Count==(borrow ? 1 : 0));
        IR_CHECK(result->persistent->textNodes==1 && result->persistent->textBytes==sizeof(binary)+strlen("owned")+1+1);
        xl6AstFree(root);xlsFrontendIrResultDestroy(result);IR_CHECK(irLive()==live);
    }
    return 1;
}
static int irEntityReceiver(void)
{
    uint64_t live=irLive();xls_entity_view *before=xlsProbeEntityBinding(77,UINT64_C(2001)),*after=xlsProbeEntityBinding(177,UINT64_C(2001)),*foreign=xlsProbeEntityBinding(177,UINT64_C(2002));IR_CHECK(before && after && foreign);
    xls_ir_unit unit={0};xls_ir_capture capture={.unit=&unit,.entities=before};IR_CHECK(xrtArrayInit(&unit.globals,sizeof(xls_ir_global)) && xrtMapInit(&capture.globals,sizeof(uint32_t)));
    XL6_IrNode node={.constructorReceiverSymbol=77,.unionField=SIZE_MAX},decoded={0};xstrbuf data={0};IR_CHECK(xlsIrMemoEncodeNode(&data,&node,65536,&capture) && unit.globals.Count==1);
    xls_ir_reader r={(const unsigned char*)data.Data,0,data.Size};IR_CHECK(xlsIrMemoDecodeNode(&r,&decoded,0,NULL,&unit,after) && decoded.constructorReceiverSymbol==177);
    r=(xls_ir_reader){(const unsigned char*)data.Data,0,data.Size};IR_CHECK(!xlsIrMemoDecodeNode(&r,&decoded,0,NULL,&unit,foreign));
    r=(xls_ir_reader){(const unsigned char*)data.Data,0,data.Size};IR_CHECK(!xlsIrMemoDecodeNode(&r,&decoded,0,NULL,&unit,NULL));
    xrtStrBufFree(&data);xrtArrayUnit(&unit.globals);xrtMapUnit(&capture.globals);xlsProbeEntityBindingDrop(before);xlsProbeEntityBindingDrop(after);xlsProbeEntityBindingDrop(foreign);IR_CHECK(irLive()==live);return 1;
}
static int irRestoreFailures(void)
{
    uint64_t live=irLive();XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);IR_CHECK(root);
    XL6_IrNode source={.kind=XL6_IR_NODE_MODULE,.sourceKind=XL6_AST_MODULE,.unionField=SIZE_MAX,.text="payload",.literalData=(unsigned char*)"bin",.literalSize=3,.compiledPackageLink="package:包"};
    xls_ir_unit unit={.nodes=1};xls_ir_capture capture={.unit=&unit};IR_CHECK(xlsIrMemoEncodeNode(&unit.data,&source,65536,&capture));
    for(unsigned failure=1;failure<=3;++failure){
        XL6_IrModule module={0};module.nodes=xrtCalloc(1,sizeof(*module.nodes));IR_CHECK(module.nodes);module.nodeCount=module.nodeCapacity=1;module.nodes[0].text=xrtStrDup("retained");IR_CHECK(module.nodes[0].text);
        char* retained=module.nodes[0].text;
        if(failure<=2){irDupCalls=0;irDupFault=failure;}else root->kind=XL6_AST_NAME;
        XL6_IrNodeId restored=xlsIrMemoRestore(&unit,&module,root,NULL,NULL);irDupFault=0;
        IR_CHECK(!restored && (failure==3 || irDupCalls==failure));
        IR_CHECK(module.nodeCount==1 && !module.childCount && module.nodes[0].text==retained && !strcmp(retained,"retained"));
        IR_CHECK(!module.nodes[1].text && !module.nodes[1].literalData && !module.nodes[1].compiledPackageLink);
        xl6IrModuleFree(&module);root->kind=XL6_AST_MODULE;
    }
    xrtStrBufFree(&unit.data);xl6AstFree(root);IR_CHECK(irLive()==live);return 1;
}
int xlsTestCurrentIrCodec(unsigned* count)
{if(!irTypeKeys() || !irPayloads() || !irReceiverDomains() || !irPersistentPayloads() || !irEntityReceiver() || !irRestoreFailures())return 0;*count=irChecks;return 1;}
