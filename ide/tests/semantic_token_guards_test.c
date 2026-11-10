#include "services/semantic_token_positions.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static int forgedMode,measuring,afterGetter;static size_t unicodeRow,callsAfter,cancelAfter;static xls_semantic_token forged[64];
const xls_semantic_token* __real_xlsSemanticTokensData(const xls_semantic_tokens*);
const xls_semantic_token* __wrap_xlsSemanticTokensData(const xls_semantic_tokens* tokens)
{
 const xls_semantic_token* rows=__real_xlsSemanticTokensData(tokens);
 if(measuring)afterGetter=1;
 if(!forgedMode)return rows;
 size_t count=xlsSemanticTokensCount(tokens);if(count>64)return NULL;
 memcpy(forged,rows,count*sizeof(*rows));
 if(forgedMode==1)++forged[0].range.startColumn;
 else if(forgedMode==2)++forged[0].range.startOffset;
 else ++forged[unicodeRow].range.startOffset;
 return forged;
}
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_token_guards_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xls_semantic_check* create(const char* path,const char* text)
{xls_semantic_input input={path,text,strlen(text)};return xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);}
static int coordinateGuards(void)
{
 const char* text="func main(...) as int\n int 变量=7\n return 变量\nend\n";char* path=xrtPathAbs("token-coordinate-guard.xl");CHECK(path);xls_semantic_check* c=create(path,text);CHECK(c && xlsSemanticCheckInfo(c).accepted);
 enum xls_semantic_tokens_error error;xls_semantic_tokens* native=xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error);CHECK(native);const xls_semantic_token* rows=xlsSemanticTokensData(native);unicodeRow=SIZE_MAX;
 for(size_t i=0;i<xlsSemanticTokensCount(native);++i)if(rows[i].range.endOffset-rows[i].range.startOffset==strlen("变量") && !memcmp(text+rows[i].range.startOffset,"变量",strlen("变量"))){unicodeRow=i;break;}
 CHECK(unicodeRow!=SIZE_MAX);xlsSemanticTokensRelease(native);
 for(int mode=1;mode<=3;++mode){forgedMode=mode;xide_token_positions* p=xideTokenPositionsCreate(c,0,0,0,NULL,NULL,&error);forgedMode=0;CHECK(!p && error==XLS_TOKENS_FACTS && xlsSemanticCheckInfo(c).accepted);}
 xide_token_positions* p=xideTokenPositionsCreate(c,0,0,0,NULL,NULL,&error);CHECK(p && !error);xideTokenPositionsRelease(p);
 CHECK(!xideTokenPositionsCreate(c,0,0,1,NULL,NULL,&error) && error==XLS_TOKENS_ARGUMENT);
 CHECK(!xideTokenPositionsCreate(c,0,0,128,NULL,NULL,&error) && error==XLS_TOKENS_LIMIT);
 CHECK(!xideTokenPositionsCreate(c,0,0,17u*1024u*1024u,NULL,NULL,&error) && error==XLS_TOKENS_ARGUMENT);
 xlsSemanticCheckDestroy(c);xrtFree(path);return 1;
}
static int observed(void* user)
{(void)user;if(afterGetter)++callsAfter;return cancelAfter && callsAfter>=cancelAfter;}
static int longUnicodeScan(void)
{
 char* path=xrtPathAbs("token-long-coordinate-guard.xl");xstrbuf text={0};CHECK(path && xrtStrBufAppend(&text,xrtStrView("func main(...) as int\n string text=\"")));
 /* Force the four-byte run off4096-byte alignment. Testing at%4096==0
  * would fail to poll throughout this entire run. */
 while(text.Size%4!=1)CHECK(xrtStrBufAppend(&text,xrtStrView("a")));
 for(unsigned i=0;i<32768;++i)CHECK(xrtStrBufAppend(&text,xrtStrView("😀")));
 CHECK(xrtStrBufAppend(&text,xrtStrView("\"; int 变量=7; return 变量\nend\n")));
 xls_semantic_check* c=create(path,text.Data);CHECK(c && xlsSemanticCheckInfo(c).accepted);enum xls_semantic_tokens_error error;
 measuring=1;afterGetter=0;callsAfter=0;cancelAfter=0;xide_token_positions* p=xideTokenPositionsCreate(c,0,0,0,observed,NULL,&error);CHECK(p && !error);xide_token_position_work work=xideTokenPositionsWork(p);CHECK(work.scannedBytes>128u*1024u && work.scannedBytes<=text.Size && callsAfter>=work.scannedBytes/4096);
 const xide_token_position* data=xideTokenPositionsData(p);int found=0;
 for(size_t i=0;i<xideTokenPositionsCount(p);++i)if(data[i].line==1 && data[i].character>65536 && data[i].kind==XLS_TOKEN_VARIABLE){CHECK(data[i].length==2 && data[i].kind==XLS_TOKEN_VARIABLE);++found;}
 CHECK(found==2);xideTokenPositionsRelease(p);
 afterGetter=0;callsAfter=0;cancelAfter=10;p=xideTokenPositionsCreate(c,0,0,0,observed,NULL,&error);CHECK(!p && error==XLS_TOKENS_CANCELLED && callsAfter==10);measuring=0;cancelAfter=0;
 xlsSemanticCheckDestroy(c);xrtFree(path);xrtStrBufFree(&text);return 1;
}
int main(void){if(!coordinateGuards() || !longUnicodeScan())return 1;printf("semantic_token_guards_test passed: %u checks; actual forged native scalar/byte endpoints,UTF8 interior rejection,budget recovery and unaligned128KiB Unicode cancellation\n",checks);return 0;}
