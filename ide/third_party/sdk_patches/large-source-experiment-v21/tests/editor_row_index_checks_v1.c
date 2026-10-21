static unsigned long long rowIndexAssertions,rowIndexByteCases,rowIndexViewCases,rowIndexStateCases,rowIndexFailureCases;
#define ROW_CHECK(e) do {++rowIndexAssertions;if(!(e)){fprintf(stderr,"row index check %d: %s\n",__LINE__,#e);exit(1);}}while(0)
static void rowDataRead(xui_code_edit_data_t* d,int start,int end,const char** text)
{
 ROW_CHECK(__xuiCodeEditReadRange(d,start,end,text)==XUI_OK && d->bReadRangeValid);
}
static void rowDataCreate(xui_code_edit_data_t* d,const char* text,int n)
{
 const char* read=NULL;memset(d,0,sizeof(*d));d->iTabColumns=4;
 ROW_CHECK(xuiCodeDocumentCreate(&d->pDocument)==XUI_OK);
 ROW_CHECK(xuiCodeDocumentSetTextLength(d->pDocument,text,n)==XUI_OK);
 rowDataRead(d,0,n,&read);
}
static void rowDataDestroy(xui_code_edit_data_t* d)
{
 __xuiCodeEditColumnIndexClear(d);ROW_CHECK(!d->pColumnPoints && !d->iColumnPointCount && !d->iColumnPointCapacity);
 xrtFree(d->sReadBuffer);xuiCodeDocumentDestroy(d->pDocument);memset(d,0,sizeof(*d));
}
static void rowCompareByte(xui_code_edit_data_t* d,int start,int offset)
{
 int actual=__xuiCodeEditLineVisualColumn(d,d->sReadBuffer,start,offset);
 int expected=diagRowOldVisualColumn(d,d->sReadBuffer,start,offset);
 ROW_CHECK(actual==expected);++rowIndexByteCases;
 ROW_CHECK(d->iColumnPointCapacity<=4096 && d->iColumnPointCount<=d->iColumnPointCapacity);
}
static void rowCompareViewport(xui_code_edit_data_t* d,int end,float scroll,float column,float width)
{
 int start=0,finish=0,oldStart=0,oldEnd=0;d->fScrollX=scroll;d->bVisibleRangeCached=0;
 __xuiCodeEditVisibleByteRange(d,d->sReadBuffer,0,end,column,width,&start,&finish);
 int count=d->iColumnPointCount;unsigned long long hits=rowColumnSeeds;
 int againStart=-1,againEnd=-1;
 __xuiCodeEditVisibleByteRange(d,d->sReadBuffer,0,end,column,width,&againStart,&againEnd);
 ROW_CHECK(start==againStart && finish==againEnd && count==d->iColumnPointCount && hits==rowColumnSeeds);
 d->bVisibleRangeCached=0;
 diagRowOldVisibleByteRange(d,d->sReadBuffer,0,end,column,width,&oldStart,&oldEnd);
 ROW_CHECK(start==oldStart && finish==oldEnd);++rowIndexViewCases;
}
static void verifyRowColumnIndex(char** args)
{
 (void)args;
 const unsigned char invalid[]={'A',0xff,0xc0,0xaf,0xe4,0xb8,0xf0,0x9f,'\t',0xed,0xa0,0x80,'Z'};
 const unsigned char embedded[]={'A',0,'\t','B',0,'C',' '};
 const char* patterns[]={"AVATAR 0123456789\tabc ","中文😀 é\t0123456789 ",
  "👩‍💻 🇨🇳🇺🇸 👨‍👩‍👧‍👦\tZ ","각 क्‍ष ؀X\tA ",(const char*)invalid,(const char*)embedded};
 int lengths[]={0,0,0,0,sizeof(invalid),sizeof(embedded)};
 const int n=131072;char* input=malloc((size_t)n+1);ROW_CHECK(input);
 unsigned int random=0x735b42u;
 for(int corpus=0;corpus<6;++corpus) {
  int motif=lengths[corpus] ? lengths[corpus] : (int)strlen(patterns[corpus]);
  int used=(n/motif)*motif;
  for(int i=0;i<used;++i){input[i]=patterns[corpus][i%motif];}
  memset(input+used,' ',(size_t)(n-used));input[n]=0;
  if(corpus==4) {
   char* valid=malloc((size_t)n+1);ROW_CHECK(valid);memset(valid,'A',n);valid[n]=0;
   xui_code_edit_data_t rejected;rowDataCreate(&rejected,valid,n);free(valid);rowCompareByte(&rejected,0,n);
   uint32_t version=xuiCodeDocumentGetVersion(rejected.pDocument);
   ROW_CHECK(xuiCodeDocumentSetTextLength(rejected.pDocument,input,n)==XUI_ERROR_INVALID_ARGUMENT);
   ROW_CHECK(xuiCodeDocumentGetVersion(rejected.pDocument)==version && xuiCodeDocumentGetLength(rejected.pDocument)==n);
   unsigned long long hits=rowColumnSeeds;
   for(int i=0;i<160;++i) {
    random=random*1664525u+1013904223u;int offset=(int)(random%(unsigned int)(n+1));
    ROW_CHECK(__xuiCodeEditLineVisualColumn(&rejected,input,0,offset)==diagRowOldVisualColumn(&rejected,input,0,offset));++rowIndexByteCases;
   }
   for(int i=0;i<96;++i) {
    int a=0,b=0,c=0,e=0;random=random*1664525u+1013904223u;rejected.fScrollX=(float)(random%(unsigned int)(n*2));
    rejected.bVisibleRangeCached=0;__xuiCodeEditVisibleByteRange(&rejected,input,0,n,1,80,&a,&b);
    rejected.bVisibleRangeCached=0;diagRowOldVisibleByteRange(&rejected,input,0,n,1,80,&c,&e);
    ROW_CHECK(a==c && b==e);++rowIndexViewCases;
   }
   ROW_CHECK(hits==rowColumnSeeds);rowCompareByte(&rejected,0,n);++rowIndexFailureCases;++rowIndexStateCases;
   rowDataDestroy(&rejected);continue;
  }
  xui_code_edit_data_t d;rowDataCreate(&d,input,n);
  /* Cold traversal establishes points; later queries exercise arbitrary limits
     inside UTF-8 bytes and text clusters around every established point. */
  rowCompareByte(&d,0,n);ROW_CHECK(d.iColumnPointCount>16);
  int pointCount=d.iColumnPointCount;
  for(int p=0;p<pointCount;++p) {
   int position=d.pColumnPoints[p].iOffset;
   for(int delta=-5;delta<=5;++delta)rowCompareByte(&d,0,position+delta);
  }
  for(int i=0;i<160;++i) {random=random*1664525u+1013904223u;rowCompareByte(&d,0,(int)(random%(unsigned int)(n+1)));}
  for(int i=0;i<96;++i) {
   random=random*1664525u+1013904223u;
   rowCompareViewport(&d,n,(float)(random%(unsigned int)(n*2)),i%2 ? 1.25f : 0.75f,(float)(32+i%97));
  }
  /* Prefix reads must preserve safe points from the same successful document
     range, even though their owned allocation is rewritten. */
  for(int i=0;i<12;++i) {
   const char* read=NULL;int limit=65536+i*997;rowDataRead(&d,0,limit,&read);rowCompareByte(&d,0,limit);
   rowCompareViewport(&d,limit,(float)(limit-100),1,80);
  }
  const char* read=NULL;rowDataRead(&d,0,n,&read);rowCompareByte(&d,0,n);++rowIndexStateCases;
  d.iTabColumns=3;rowCompareByte(&d,0,n);rowCompareViewport(&d,n,99991,1,61);++rowIndexStateCases;
  for(int tabs=2;tabs<=8;tabs+=6){d.iTabColumns=tabs;rowCompareByte(&d,0,n);rowCompareViewport(&d,n,99991,1,61);++rowIndexStateCases;}
  rowDataRead(&d,1,n,&read);rowCompareByte(&d,0,n-1);rowCompareViewport(&d,n-1,99991,1,61);++rowIndexStateCases;
  int firstEnd=xuiInternalTextGraphemeNext(input,n,0);
  ROW_CHECK(xuiCodeDocumentReplace(d.pDocument,0,firstEnd,"界")==XUI_OK);
  int updated=xuiCodeDocumentGetLength(d.pDocument);rowDataRead(&d,0,updated,&read);rowCompareByte(&d,0,updated);++rowIndexStateCases;
  ROW_CHECK(xuiCodeDocumentUndo(d.pDocument)==XUI_OK);rowDataRead(&d,0,n,&read);rowCompareByte(&d,0,n);++rowIndexStateCases;
  char* borrowed=malloc((size_t)n+1);ROW_CHECK(borrowed);memcpy(borrowed,d.sReadBuffer,(size_t)n+1);
  unsigned long long hits=rowColumnSeeds;
  ROW_CHECK(__xuiCodeEditLineVisualColumn(&d,borrowed,0,n)==diagRowOldVisualColumn(&d,borrowed,0,n));
  ROW_CHECK(hits==rowColumnSeeds);free(borrowed);++rowIndexStateCases;
  rowCompareByte(&d,1,n);rowDataDestroy(&d);
 }
 /* A single very long combining cluster cannot produce an unsafe checkpoint. */
 memset(input,'Z',n);input[0]='A';for(int i=1;i+1<n-2;i+=2){input[i]=(char)0xcc;input[i+1]=(char)0x81;}input[n-2]='Z';input[n-1]='Q';input[n]=0;
 xui_code_edit_data_t d;rowDataCreate(&d,input,n);rowCompareByte(&d,0,n);
 for(int i=0;i<20;++i){rowCompareByte(&d,0,n-20+i);}rowCompareViewport(&d,n,50,1,80);rowDataDestroy(&d);
 /* Allocation failure retains the old traversal and retries at most once per
     validity context; a later version can allocate a new index normally. */
 memset(input,'A',n);input[n]=0;rowDataCreate(&d,input,n);
 rowColumnFailAllocations=1;unsigned long long failures=rowColumnFailures;rowCompareByte(&d,0,n);
 ROW_CHECK(rowColumnFailures==failures+1 && !d.pColumnPoints && d.bColumnIndexAllocationFailed);
 rowCompareByte(&d,0,n);ROW_CHECK(rowColumnFailures==failures+1);++rowIndexFailureCases;
 ROW_CHECK(xuiCodeDocumentReplace(d.pDocument,n-1,n,"B")==XUI_OK);const char* read=NULL;rowDataRead(&d,0,n,&read);rowCompareByte(&d,0,n);
 ROW_CHECK(d.iColumnPointCount>16 && !d.bColumnIndexAllocationFailed);++rowIndexStateCases;
 rowDataDestroy(&d);rowDataCreate(&d,input,n);rowCompareByte(&d,0,65536);
 ROW_CHECK(d.iColumnPointCapacity==16);
 /* Fail the next growth while retaining already valid points. */
 rowColumnFailAllocations=1;failures=rowColumnFailures;rowCompareByte(&d,0,n);
 ROW_CHECK(rowColumnFailures==failures+1 && d.iColumnPointCount>0 && d.bColumnIndexAllocationFailed);
 rowCompareByte(&d,0,n);ROW_CHECK(rowColumnFailures==failures+1);++rowIndexFailureCases;
 unsigned long long hits=rowColumnSeeds;ROW_CHECK(__xuiCodeEditReadRange(&d,0,n+1,&read)!=XUI_OK && !d.bReadRangeValid);
 rowCompareByte(&d,0,n);ROW_CHECK(hits==rowColumnSeeds);rowDataRead(&d,0,n,&read);rowCompareByte(&d,0,n);
 ROW_CHECK(rowColumnSeeds>hits);++rowIndexFailureCases;
 xui_code_document old=d.pDocument,newDocument=NULL;ROW_CHECK(xuiCodeDocumentCreate(&newDocument)==XUI_OK);
 ROW_CHECK(xuiCodeDocumentSetTextLength(newDocument,input,n)==XUI_OK);d.pDocument=newDocument;d.bReadRangeValid=0;
 rowDataRead(&d,0,n,&read);rowCompareByte(&d,0,n);ROW_CHECK(d.pColumnIndexDocument==newDocument);xuiCodeDocumentDestroy(old);++rowIndexStateCases;
 rowDataDestroy(&d);free(input);
 /* Exercise the 32 KiB bound with a real document extending past all 4096
     checkpoints; the remaining tail still follows the original traversal. */
 const int large=18*1024*1024;input=malloc((size_t)large+1);ROW_CHECK(input);memset(input,'A',large);input[large]=0;
 rowDataCreate(&d,input,large);rowCompareByte(&d,0,large);
 ROW_CHECK(d.iColumnPointCount==4096 && d.iColumnPointCapacity==4096 && sizeof(*d.pColumnPoints)*4096==32768);
 rowCompareByte(&d,0,large-2);rowCompareViewport(&d,large,(float)(large-80),1,60);rowDataDestroy(&d);free(input);
 ROW_CHECK(rowColumnSeeds>100 && rowColumnPoints>4096 && rowColumnResets>20 && rowColumnFailAllocations==0);
 printf("row index comparison passed: %llu assertions; %llu byte cases; %llu viewport cases; %llu state cases; %llu failure cases; %llu resumed queries; 32768 bytes maximum index\n",
  rowIndexAssertions,rowIndexByteCases,rowIndexViewCases,rowIndexStateCases,rowIndexFailureCases,rowColumnSeeds);fflush(stdout);
}
