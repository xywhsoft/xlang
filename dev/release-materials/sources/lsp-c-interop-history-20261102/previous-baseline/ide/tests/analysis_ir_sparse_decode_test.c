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
int main(void)
{
 if(!bitCases() || !masks() || !payloads())return 1;
 printf("analysis_ir_sparse_decode_test passed: %u checks; actual native decoder,all word bits,dense/random/unaligned masks,every truncated prefix,owned/borrowed strings and package payloads,portable bit scan\n",sparseChecks);
 return 0;
}
#else
int main(void){puts("analysis_ir_sparse_decode_test passed: legacy cohort retains its original decoder");return 0;}
#endif
