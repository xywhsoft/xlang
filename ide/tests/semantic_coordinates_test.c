#include "services/semantic_definition.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
typedef struct point {int line,character;} point;
typedef struct endpoint {int source,offset,line,column;point* output;} endpoint;
static int failAt,operations,live,bad,tracking;static size_t allocated;
static bool denseInit(xarray* a,size_t n){if(tracking && ++operations==failAt)return false;bool ok=xrtArrayInit(a,n);if(ok)++live;return ok;}
static bool denseResize(xarray* a,size_t n){if(tracking && ++operations==failAt)return false;bool okay=xrtArrayResize(a,n);if(okay)allocated=a->Capacity*a->ItemSize;return okay;}
static void denseUnit(xarray* a){if(--live<0)bad=1;xrtArrayUnit(a);}
#define xrtArrayInit denseInit
#define xrtArrayResize denseResize
#define xrtArrayUnit denseUnit
#include "services/semantic_coordinate_dense.inc"
#undef xrtArrayInit
#undef xrtArrayResize
#undef xrtArrayUnit

static int compareEndpoint(const void* a,const void* b) { const endpoint *x=a,*y=b; return x->source!=y->source ? (x->source>y->source)-(x->source<y->source) : (x->offset>y->offset)-(x->offset<y->offset); }
#include "services/semantic_coordinate_sorted.inc"

static unsigned checks,seed=17;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_coordinates_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static unsigned randomValue(void){seed=seed*1664525u+1013904223u;return seed;}
static int compare(endpoint* input,size_t count,const xide_semantic_source* sources,size_t sourceCount,int wanted,size_t budget,int failure)
{
 endpoint* a=calloc(count+1,sizeof(*a));endpoint* b=calloc(count+1,sizeof(*b));point* x=calloc(count+1,sizeof(*x));point* y=calloc(count+1,sizeof(*y));CHECK(a && b && x && y);
 for(size_t i=0;i<count;++i){a[i]=b[i]=input[i];a[i].output=&x[i];b[i].output=&y[i];}
 int original=convert(a,count,sources);CHECK(wanted<0 || original==wanted);
 tracking=1;failAt=failure;operations=0;allocated=0;int result=convertDense(b,count,sources,sourceCount,budget);tracking=0;
 CHECK(!live && !bad && allocated<=budget);if(failure)CHECK(result<0 && operations==failure);
 if(result<0)result=convert(b,count,sources);
 CHECK(result==original);if(result)for(size_t i=0;i<count;++i)CHECK(x[i].line==y[i].line && x[i].character==y[i].character);
 free(a);free(b);free(x);free(y);return 1;
}
static int randomized(void)
{
 xstrbuf texts[4]={0};xide_semantic_source sources[4]={0};endpoint valid[4096];size_t total=0;
 const char* tokens[]={"a","中","😀","\r\n","\n","\r","β","_"};
 for(unsigned s=0;s<4;++s){for(unsigned i=0;i<256;++i)CHECK(xrtStrBufAppend(&texts[s],xrtStrView(tokens[randomValue()%8])));
  sources[s].text=texts[s].Data;sources[s].utf16Prefix=s%2;sources[s].kind=1;
  size_t at=0,size=texts[s].Size;int line=1,column=1;
  for(;;){CHECK(total<4096);valid[total++]=(endpoint){(int)s+1,(int)at,line,column,NULL};if(at==size)break;
   unsigned char c=(unsigned char)texts[s].Data[at];size_t n=c<128 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
   if(c=='\r' && at+1<size && texts[s].Data[at+1]=='\n')n=2;
   if(c=='\r' || c=='\n'){++line;column=1;}else ++column;at+=n;
  }
 }
 endpoint rows[2048];
 for(unsigned test=0;test<240;++test){size_t n=1+randomValue()%2048;for(size_t i=0;i<n;++i)rows[i]=valid[randomValue()%total];
  CHECK(compare(rows,n,sources,4,1,8u*1024u*1024u,0));
  if(test%6==0){CHECK(compare(rows,n,sources,4,1,0,0));CHECK(compare(rows,n,sources,4,1,8u*1024u*1024u,1));CHECK(compare(rows,n,sources,4,1,8u*1024u*1024u,2));}
  endpoint save=rows[0];++rows[0].column;CHECK(compare(rows,n,sources,4,0,8u*1024u*1024u,0));rows[0]=save;
  ++rows[0].line;CHECK(compare(rows,n,sources,4,0,8u*1024u*1024u,0));rows[0]=save;
  ++rows[0].offset;CHECK(compare(rows,n,sources,4,-1,8u*1024u*1024u,0));
 }
 for(unsigned budget=0;budget<=512;++budget){endpoint row={1,0,1,1,NULL};CHECK(compare(&row,1,sources,4,1,budget,0));}
 for(unsigned s=0;s<4;++s)xrtStrBufFree(&texts[s]);
 return 1;
}
static int boundaries(void)
{
 xide_semantic_source sources[512]={0};endpoint rows[4096];
 for(unsigned s=0;s<512;++s){sources[s].text=s%2 ? "😀\r\n中" : "";sources[s].utf16Prefix=s%2;sources[s].kind=1;rows[s]=(endpoint){(int)s+1,0,1,1,NULL};}
 CHECK(compare(rows,512,sources,512,1,8u*1024u*1024u,0));
 sources[0].text="😀\r\n中";for(unsigned offset=0;offset<=10;++offset){rows[0]=(endpoint){1,(int)offset,1,1,NULL};CHECK(compare(rows,1,sources,512,-1,8u*1024u*1024u,0));}
 size_t bytes=1024u*1024u;char* text=malloc(bytes+1);CHECK(text);memset(text,'x',bytes);text[bytes]=0;sources[0].text=text;sources[0].utf16Prefix=1;
 for(size_t i=0;i<4096;++i){size_t offset=i*bytes/4095;rows[i]=(endpoint){1,(int)offset,1,(int)offset+1,NULL};}
 CHECK(compare(rows,4096,sources,1,1,8u*1024u*1024u,0));CHECK(compare(rows,4096,sources,1,1,64u*1024u*1024u,0));free(text);
 rows[0]=(endpoint){1,0,1,1,NULL};sources[0].text=NULL;CHECK(convertDense(rows,1,sources,1,8u*1024u*1024u)==0);
 CHECK(convertDense(NULL,0,NULL,0,0)==1);return 1;
}
int main(void){if(!randomized() || !boundaries())return 1;printf("semantic_coordinates_test passed: %u checks;original sorted oracle,UTF8/CRLF/CR/BOMprefix/duplicates/EOF/unsorted/512sources/longline,budget andallocationfallback,all expected scalar columns verified\n",checks);return 0;}
