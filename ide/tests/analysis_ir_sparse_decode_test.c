#include "analysis/frontend_internal.h"
#include <stdio.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
/* Resolve the exact generated IR TU from this build. The fixture's object
 * precedes the core archive and never enters the core or server. */
#include "xl6_ir.c"
static unsigned sparseChecks;
#define CHECK(v) do{++sparseChecks;if(!(v)){fprintf(stderr,"analysis_ir_sparse_decode_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static uint64_t sparseRandom=UINT64_C(0x9e3779b97f4a7c15);
static uint64_t nextWord(void)
{sparseRandom^=sparseRandom<<13;sparseRandom^=sparseRandom>>7;return sparseRandom^=sparseRandom<<17;}
static int bitCases(void)
{
 for(unsigned bit=0;bit<64;++bit){
  uint64_t mask=UINT64_C(1)<<bit;
  CHECK(xlsIrMemoWordBit(mask)==bit);CHECK(xlsIrMemoWordBitPortable(mask)==bit);
  CHECK(xlsIrMemoWordBit(UINT64_MAX<<bit)==bit);
  CHECK(xlsIrMemoWordBitPortable(UINT64_MAX<<bit)==bit);
 }
 for(unsigned i=0;i<8192;++i){
  uint64_t mask=nextWord();if(!mask)mask=1;unsigned bit=0;
  while(!(mask&(UINT64_C(1)<<bit)))++bit;
  CHECK(xlsIrMemoWordBit(mask)==bit);CHECK(xlsIrMemoWordBitPortable(mask)==bit);
 }
 return 1;
}
static int maskCase(uint64_t mask,size_t offset,int truncate)
{
 unsigned char data[32+(1+XLS_IR_NODE_WORDS)*sizeof(uint64_t)];
 memset(data,0xcc,sizeof(data));uint64_t expectedWords[XLS_IR_NODE_WORDS]={0};size_t end=offset;
 memcpy(data+end,&mask,sizeof(mask));end+=sizeof(mask);
 for(size_t bit=0;bit<XLS_IR_NODE_WORDS;++bit)if(mask&(UINT64_C(1)<<bit)){
  uint64_t value=nextWord();expectedWords[bit]=value;memcpy(data+end,&value,sizeof(value));end+=sizeof(value);
 }
 size_t packedEnd=end;
 /* Exact native symbol IDs; absent text/literal payloads. */
 memset(data+end,0,4);end+=4;
 XL6_IrNode expected;memcpy(&expected,expectedWords,sizeof(expected));
 expected.text=NULL;expected.literalData=NULL;expected.compiledPackageLink=NULL;expected.unionField^=SIZE_MAX;
 for(int borrow=0;borrow<2;++borrow){
  xls_ir_reader reader={data,offset,end};XL6_IrNode actual={0};
  CHECK(xlsIrMemoDecodeNode(&reader,&actual,borrow,NULL,NULL,NULL));
  CHECK(reader.at==end);CHECK(!memcmp(&expected,&actual,sizeof(actual)));
  if(truncate)for(size_t size=offset;size<end;++size){
   reader=(xls_ir_reader){data,offset,size};actual=(XL6_IrNode){0};
   CHECK(!xlsIrMemoDecodeNode(&reader,&actual,borrow,NULL,NULL,NULL));
   CHECK(reader.at>=offset && reader.at<=size);
   if(size<packedEnd){
    size_t consumed=size-offset<8 ? 0 : 8+((size-offset-8)/8)*8;
    CHECK(reader.at==offset+consumed);
    CHECK(!memcmp(&actual,&(XL6_IrNode){0},sizeof(actual)));
   }
   CHECK(!actual.text && !actual.literalData && !actual.compiledPackageLink);
  }
 }
 return 1;
}
static int masks(void)
{
 CHECK(XLS_IR_NODE_WORDS<=64 && sizeof(XL6_IrNode)<=sizeof(uint64_t)*XLS_IR_NODE_WORDS);
 uint64_t all=UINT64_MAX>>(64-XLS_IR_NODE_WORDS);
 CHECK(maskCase(0,0,1));CHECK(maskCase(all,7,1));
 for(size_t bit=0;bit<XLS_IR_NODE_WORDS;++bit){
  CHECK(maskCase(UINT64_C(1)<<bit,1,1));
  CHECK(maskCase(all^(UINT64_C(1)<<bit),3,0));
 }
 for(unsigned i=0;i<4096;++i)CHECK(maskCase(nextWord()&all,i%8,i<32));
 if(XLS_IR_NODE_WORDS<64){
  uint64_t mask=UINT64_C(1)<<XLS_IR_NODE_WORDS;unsigned char data[8];memcpy(data,&mask,sizeof(mask));
  xls_ir_reader reader={data,0,sizeof(data)};XL6_IrNode actual={0};
  CHECK(!xlsIrMemoDecodeNode(&reader,&actual,1,NULL,NULL,NULL));
  CHECK(reader.at==8);CHECK(!memcmp(&actual,&(XL6_IrNode){0},sizeof(actual)));
 }
 return 1;
}
static int payloads(void)
{
 unsigned char literal[]={0xff,0,'x',0x80,0};char text[]="raw \xe4\xb8\xad\xe6\x96\x87";
 XL6_IrNode input={.kind=XL6_IR_NODE_VALUE_DECL,.text=text,.literalData=literal,.literalSize=4,
  .span={73,19},.symbol=UINT32_MAX,.targetSymbol=72,.compiledPackageLink="module/library.xlp",
  .constructorReceiverSymbol=81,.unionField=SIZE_MAX};
 xstrbuf bytes={0};xls_ir_unit unit={0};xls_ir_capture capture={.unit=&unit};
 CHECK(xlsIrMemoEncodeNode(&bytes,&input,1024u*1024u,&capture));
 for(int borrow=0;borrow<2;++borrow){
  xls_ir_reader reader={(const unsigned char*)bytes.Data,0,bytes.Size};XL6_IrNode actual={0};
  CHECK(xlsIrMemoDecodeNode(&reader,&actual,borrow,NULL,&unit,NULL));CHECK(reader.at==bytes.Size);
  CHECK(actual.text && !strcmp(actual.text,text));CHECK(actual.literalData && !memcmp(actual.literalData,literal,sizeof(literal)));
  CHECK(actual.compiledPackageLink && !strcmp(actual.compiledPackageLink,input.compiledPackageLink));
  XL6_IrNode expected=input;expected.text=actual.text;expected.literalData=actual.literalData;
  expected.compiledPackageLink=actual.compiledPackageLink;expected.span=(XL6_IrSpan){0};
  CHECK(!memcmp(&actual,&expected,sizeof(actual)));
  if(borrow){
   CHECK(actual.text>=bytes.Data && actual.text<bytes.Data+bytes.Size);
   CHECK(actual.literalData>=(unsigned char*)bytes.Data && actual.literalData<(unsigned char*)bytes.Data+bytes.Size);
   CHECK(actual.compiledPackageLink>=bytes.Data && actual.compiledPackageLink<bytes.Data+bytes.Size);
  }else{xrtFree(actual.text);xrtFree(actual.literalData);xrtFree(actual.compiledPackageLink);}
 }
 xrtStrBufFree(&bytes);return 1;
}
#ifdef XLS_NATIVE_IR_FOREIGN_FIELDS
#include "analysis/module_store.h"
#include "analysis/check_session.h"
static int foreignTypeKeys(void)
{
 /* Encoder coverage, not semantic admission of the mutated synthetic types. */
 char firstHeader[]="native.h",secondHeader[]="native.h";
 XL6_IrType type={.kind=XL6_IR_TYPE_CLASS,.name="Handle",.displayName="Handle",.foreignHeader=firstHeader};
 XL6_IrTypePool pool={.items=&type,.count=1,.capacity=1};
 xstrbuf first={0},other={0};const size_t limit=1024u*1024u;
 CHECK(xlsIrMemoTypes(&pool,&first,limit));
 type.foreignHeader=secondHeader;CHECK(xlsIrMemoTypes(&pool,&other,limit));
 CHECK(first.Size==other.Size && !memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 type.foreignHeader=NULL;CHECK(xlsIrMemoTypes(&pool,&other,limit));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 type.foreignHeader=secondHeader;secondHeader[0]='N';CHECK(xlsIrMemoTypes(&pool,&other,limit));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);secondHeader[0]='n';
 type.foreignBase=7;CHECK(xlsIrMemoTypes(&pool,&other,limit));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);type.foreignBase=0;
 type.nativeArrayExtent=UINT64_C(0x8000000000000001);CHECK(xlsIrMemoTypes(&pool,&other,limit));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);xrtStrBufFree(&first);return 1;
}
#ifdef XLS_NATIVE_FINAL_INTEROP
static int foreignPayloads(void)
{
 XL6_IrNode input={0};input.kind=XL6_IR_NODE_VALUE_DECL;input.unionField=SIZE_MAX;
 input.foreignCall.constAggregateAddresses=1;
 input.foreignCall.staticCallbackAddresses=2;
 input.foreignCall.textViews=3;
 input.foreignCall.bytesViews=4;
 input.foreignCall.terminatedStrings=5;
 input.foreignCall.physicalPairs=6;
 input.foreignCall.retainedCallbacks=7;
 input.foreignCall.controlViews=8;
 input.foreignCall.borrowedViewResult=9;
 input.foreignCall.valueOutput=10;
 input.foreignCall.receiverDereference=11;
 input.foreignCall.inplaceOutput=12;
 input.foreignCall.inputMappingFacts=13;
 input.foreignCall.resultOwnerIndexPlusOne=14;
 input.foreignCall.outputIndexPlusOne=15;
 input.foreignCall.successCode=16;
 input.foreignCall.rollbackSymbol=17;

 xstrbuf first={0},other={0};xls_ir_unit unit={0};xls_ir_capture capture={.unit=&unit};
 CHECK(xlsIrMemoEncodeNode(&first,&input,1024u*1024u,&capture));
 XL6_IrNode changed=input;
 size_t begin=offsetof(XL6_ForeignCallFacts,inputMappingFacts)+sizeof(input.foreignCall.inputMappingFacts);
 size_t end=offsetof(XL6_ForeignCallFacts,resultOwnerIndexPlusOne);
 CHECK(end-begin==2 && sizeof(XL6_ForeignCallFacts)==88);
 for(size_t i=begin;i<end;++i)((unsigned char*)&changed.foreignCall)[i]=0xa5;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size==other.Size && !memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.constAggregateAddresses;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.staticCallbackAddresses;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.textViews;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.bytesViews;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.terminatedStrings;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.physicalPairs;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.retainedCallbacks;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.controlViews;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.borrowedViewResult;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.valueOutput;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.receiverDereference;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.inplaceOutput;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.inputMappingFacts;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.resultOwnerIndexPlusOne;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.outputIndexPlusOne;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.successCode;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 changed=input;++changed.foreignCall.rollbackSymbol;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);

 for(int borrow=0;borrow<2;++borrow){
  xls_ir_reader reader={(const unsigned char*)first.Data,0,first.Size};XL6_IrNode actual={0};
  CHECK(xlsIrMemoDecodeNode(&reader,&actual,borrow,NULL,&unit,NULL) && reader.at==first.Size);
  CHECK(actual.foreignCall.constAggregateAddresses==input.foreignCall.constAggregateAddresses);
  CHECK(actual.foreignCall.staticCallbackAddresses==input.foreignCall.staticCallbackAddresses);
  CHECK(actual.foreignCall.textViews==input.foreignCall.textViews);
  CHECK(actual.foreignCall.bytesViews==input.foreignCall.bytesViews);
  CHECK(actual.foreignCall.terminatedStrings==input.foreignCall.terminatedStrings);
  CHECK(actual.foreignCall.physicalPairs==input.foreignCall.physicalPairs);
  CHECK(actual.foreignCall.retainedCallbacks==input.foreignCall.retainedCallbacks);
  CHECK(actual.foreignCall.controlViews==input.foreignCall.controlViews);
  CHECK(actual.foreignCall.borrowedViewResult==input.foreignCall.borrowedViewResult);
  CHECK(actual.foreignCall.valueOutput==input.foreignCall.valueOutput);
  CHECK(actual.foreignCall.receiverDereference==input.foreignCall.receiverDereference);
  CHECK(actual.foreignCall.inplaceOutput==input.foreignCall.inplaceOutput);
  CHECK(actual.foreignCall.inputMappingFacts==input.foreignCall.inputMappingFacts);
  CHECK(actual.foreignCall.resultOwnerIndexPlusOne==input.foreignCall.resultOwnerIndexPlusOne);
  CHECK(actual.foreignCall.outputIndexPlusOne==input.foreignCall.outputIndexPlusOne);
  CHECK(actual.foreignCall.successCode==input.foreignCall.successCode);
  CHECK(actual.foreignCall.rollbackSymbol==input.foreignCall.rollbackSymbol);

 }
 xrtStrBufFree(&first);return 1;
}
static int finalSchema(void)
{
 XL6_IrModule module={0};CHECK(xlsFinalIrTablesEmpty(&module));
 module.foreignParameterCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignParameterCount=0;
 module.foreignOutputCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignOutputCount=0;
 module.foreignFieldLengthCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignFieldLengthCount=0;
 module.foreignStorageCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignStorageCount=0;
 module.foreignRetainCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignRetainCount=0;
 module.foreignValueCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignValueCount=0;
 module.foreignCallbackSpanCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignCallbackSpanCount=0;
 module.foreignPointerQualifierCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignPointerQualifierCount=0;
 module.foreignFieldCallCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignFieldCallCount=0;
 module.foreignFunctionHeaderCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignFunctionHeaderCount=0;
 module.foreignOutputSizeCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignOutputSizeCount=0;
 module.foreignNativeSequenceCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignNativeSequenceCount=0;
 module.foreignOutputViewCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignOutputViewCount=0;
 module.foreignOutputRecordCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignOutputRecordCount=0;
 module.foreignOutputFieldCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignOutputFieldCount=0;
 module.foreignRegionCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignRegionCount=0;
 module.foreignRegionObserverCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignRegionObserverCount=0;
 module.foreignPackCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignPackCount=0;
 module.foreignPackFieldCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignPackFieldCount=0;
 module.foreignFixedInputCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignFixedInputCount=0;
 module.foreignArrayGroupCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignArrayGroupCount=0;
 module.foreignArrayInputCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignArrayInputCount=0;
 module.foreignArrayFieldCount=1;CHECK(!xlsFinalIrTablesEmpty(&module));module.foreignArrayFieldCount=0;

 XL6_AstNode attr={.kind=XL6_AST_FIELD_ATTR,.text="c_code_owner"};CHECK(xlsFinalIrForeignAst(&attr,0));
 attr.kind=XL6_AST_NAME;CHECK(!xlsFinalIrForeignAst(&attr,0));
 XL6_IrType type={0};XL6_IrTypePool pool={.items=&type,.count=1,.capacity=1};xstrbuf first={0},other={0};
 CHECK(xlsIrMemoTypes(&pool,&first,1024u*1024u));
 type.foreignLifetime=1;
 CHECK(xlsIrMemoTypes(&pool,&other,1024u*1024u));CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);type.foreignLifetime=0;
 type.foreignOwner=1;
 CHECK(xlsIrMemoTypes(&pool,&other,1024u*1024u));CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);type.foreignOwner=0;
 type.foreignPolicy="retained text";
 CHECK(xlsIrMemoTypes(&pool,&other,1024u*1024u));CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);type.foreignPolicy=0;
 type.foreignScope="retained text";
 CHECK(xlsIrMemoTypes(&pool,&other,1024u*1024u));CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);type.foreignScope=0;
 type.foreignWatch="retained text";
 CHECK(xlsIrMemoTypes(&pool,&other,1024u*1024u));CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);type.foreignWatch=0;
 type.foreignDomain="retained text";
 CHECK(xlsIrMemoTypes(&pool,&other,1024u*1024u));CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);type.foreignDomain=0;

 xrtStrBufFree(&first);return 1;
}
#else
static int foreignPayloads(void)
{
 XL6_IrNode input={0};input.kind=XL6_IR_NODE_VALUE_DECL;input.unionField=SIZE_MAX;
 input.foreignCall=(XL6_ForeignCallFacts){UINT64_C(1)<<63,UINT64_C(1)<<62,17,-2147483647,91};
 xstrbuf first={0},other={0};xls_ir_unit unit={0};xls_ir_capture capture={.unit=&unit};
 CHECK(xlsIrMemoEncodeNode(&first,&input,1024u*1024u,&capture));
 XL6_IrNode changed=input;size_t end=offsetof(XL6_ForeignCallFacts,rollbackSymbol)+sizeof(input.foreignCall.rollbackSymbol);
 CHECK(end<=sizeof(input.foreignCall));
 for(size_t i=end;i<sizeof(input.foreignCall);++i)((unsigned char*)&changed.foreignCall)[i]=0xa5;
 CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
 CHECK(first.Size==other.Size && !memcmp(first.Data,other.Data,first.Size));
 for(size_t i=end;i<sizeof(input.foreignCall);++i)CHECK(((unsigned char*)&changed.foreignCall)[i]==0xa5);
 xrtStrBufFree(&other);
 for(unsigned field=0;field<5;++field){
  changed=input;
  switch(field){
   case 0:changed.foreignCall.constRecordAddresses^=1;break;
   case 1:changed.foreignCall.staticCallbackAddresses^=1;break;
   case 2:++changed.foreignCall.outputIndexPlusOne;break;
   case 3:++changed.foreignCall.successCode;break;
   default:++changed.foreignCall.rollbackSymbol;break;
  }
  CHECK(xlsIrMemoEncodeNode(&other,&changed,1024u*1024u,&capture));
  CHECK(first.Size!=other.Size || memcmp(first.Data,other.Data,first.Size));xrtStrBufFree(&other);
 }
 for(int borrow=0;borrow<2;++borrow){
  xls_ir_reader reader={(const unsigned char*)first.Data,0,first.Size};XL6_IrNode actual={0};
  CHECK(xlsIrMemoDecodeNode(&reader,&actual,borrow,NULL,&unit,NULL) && reader.at==first.Size);
  CHECK(actual.foreignCall.constRecordAddresses==input.foreignCall.constRecordAddresses);
  CHECK(actual.foreignCall.staticCallbackAddresses==input.foreignCall.staticCallbackAddresses);
  CHECK(actual.foreignCall.outputIndexPlusOne==input.foreignCall.outputIndexPlusOne);
  CHECK(actual.foreignCall.successCode==input.foreignCall.successCode);
  CHECK(actual.foreignCall.rollbackSymbol==input.foreignCall.rollbackSymbol);
 }
 CHECK(XLS_IR_SYMBOL_FIELDS==5 && !(XLS_IR_PACKAGE_MASK&((1u<<(XLS_IR_SYMBOL_FIELDS*2))-1u)));
 xrtStrBufFree(&first);return 1;
}
#endif
static uint32_t rollbackDeclaration(const xls_semantic_check* check)
{
 const XL6_AstNode* root=xlsSemanticCheckAst(check);
 for(size_t i=0;root && i<root->childCount;++i){const XL6_AstNode* n=root->children[i];
  if(n && n->kind==XL6_AST_FUNC && n->childCount && n->children[0]->text &&
     !strcmp(n->children[0]->text,"dispose"))return n->semanticDeclaredSymbolId;}
 return 0;
}
static int foreignBindings(void)
{
 char *directory=xrtPathAbs("."),*path=directory ? xrtPathJoin(directory,"foreign-codec.xl") : NULL;
 char* project=directory ? xrtPathJoin(directory,"foreign-codec.xlp") : NULL;CHECK(directory && path && project);
 const char *members[]={path},*source="func dispose() as int\n return 0\nend\nfunc main(...) as int\n return dispose()\nend\n";
 xstrbuf shifted={0};CHECK(xrtStrBufAppend(&shifted,xrtStrView("func inserted() as int\n return 1\nend\n")) && xrtStrBufAppend(&shifted,xrtStrView(source)));
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=path,.directory=directory,.analysisProfile="foreign-codec-fixture",.targetKind="exe"};
 spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,1};xls_context* context=xlsContextCreate(&spec,1024u*1024u,NULL);CHECK(context);
 xls_module_store* store=xlsModuleStoreCreate(NULL);CHECK(store);xls_module_scope scope;xlsModuleScopeBegin(&scope,store);
 char error[256];xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);
 xls_text *a=xlsTextCreate(source,strlen(source),1024u*1024u,NULL),*b=xlsTextCreate(shifted.Data,shifted.Size,1024u*1024u,NULL);CHECK(a && b);
 xls_analysis_document document={"entry",path,a,1,1};
 xls_semantic_check* before=xlsCheckSessionCheckInContext(session,context,&document,1,"entry",1,NULL,NULL,error,sizeof(error));
 document.text=b;document.revision=2;
 xls_semantic_check* after=xlsCheckSessionCheckInContext(session,context,&document,1,"entry",1,NULL,NULL,error,sizeof(error));
 CHECK(before && after && xlsSemanticCheckInfo(before).accepted && xlsSemanticCheckInfo(after).accepted);
 uint32_t old=rollbackDeclaration(before),current=rollbackDeclaration(after);CHECK(old && current && old!=current);
 const xls_entity_view *oldView=xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(before)),*currentView=xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(after));
 CHECK(oldView && currentView && xlsEntityViewSymbol(oldView,old) && xlsEntityViewSymbol(oldView,old)==xlsEntityViewSymbol(currentView,current));
 xls_ir_unit unit={0};xls_ir_capture capture={.unit=&unit,.entities=oldView};
 CHECK(xrtArrayInit(&unit.globals,sizeof(xls_ir_global)) && xrtMapInit(&capture.globals,sizeof(uint32_t)));
 XL6_IrNode input={.kind=XL6_IR_NODE_VALUE_DECL,.unionField=SIZE_MAX,.constructorReceiverSymbol=old};input.foreignCall.rollbackSymbol=old;
 xstrbuf bytes={0};CHECK(xlsIrMemoEncodeNode(&bytes,&input,1024u*1024u,&capture));CHECK(unit.globals.Count==1);
 for(int borrow=0;borrow<2;++borrow){
  xls_ir_reader reader={(const unsigned char*)bytes.Data,0,bytes.Size};XL6_IrNode result={0};
  CHECK(xlsIrMemoDecodeNode(&reader,&result,borrow,NULL,&unit,currentView) && reader.at==bytes.Size);
  CHECK(result.foreignCall.rollbackSymbol==current && result.constructorReceiverSymbol==current && !result.compiledPackageLink);
 }
 xrtStrBufFree(&bytes);xrtMapUnit(&capture.globals);xrtArrayUnit(&unit.globals);
 xlsSemanticCheckDestroy(before);xlsSemanticCheckDestroy(after);xlsCheckSessionDestroy(session);xlsTextRelease(a);xlsTextRelease(b);
 xlsModuleScopeEnd(&scope);xlsModuleStoreRelease(store);xlsContextRelease(context);
 xrtStrBufFree(&shifted);xrtFree(project);xrtFree(path);xrtFree(directory);return 1;
}
#endif
int main(void)
{
 if(!bitCases() || !masks() || !payloads())return 1;
#ifdef XLS_NATIVE_IR_FOREIGN_FIELDS
 if(!foreignTypeKeys() || !foreignPayloads() || !foreignBindings())return 1;
#ifdef XLS_NATIVE_FINAL_INTEROP
 if(!finalSchema())return 1;
#endif
#endif
 printf("analysis_ir_sparse_decode_test passed: %u checks; actual native decoder,all word bits,dense/random/unaligned masks,every truncated prefix,owned/borrowed strings and package payloads,portable bit scan\n",sparseChecks);
 return 0;
}
#else
int main(void){puts("analysis_ir_sparse_decode_test passed: legacy cohort retains its original decoder");return 0;}
#endif
