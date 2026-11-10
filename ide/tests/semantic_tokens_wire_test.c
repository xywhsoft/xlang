#include "services/language_check.h"
#include "services/semantic_token_job.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_tokens_wire_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static const char* caps="{\"textDocument\":{\"semanticTokens\":{\"requests\":{\"full\":{\"delta\":true},\"range\":true},\"tokenTypes\":[\"method\",\"function\",\"enumMember\",\"property\",\"variable\",\"parameter\",\"typeParameter\",\"struct\",\"enum\",\"interface\",\"class\",\"type\"],\"tokenModifiers\":[\"async\",\"readonly\",\"declaration\"],\"formats\":[\"relative\"],\"augmentsSyntaxTokens\":true}}}";
static xstrview sv(const char* s){return (xstrview){s,strlen(s)};}
static const xvalue* get(const xvalue* v,const char* key){return v?xrtValueObjectGet(v,sv(key)):NULL;}
static xvalue* parse(const char* s){return xrtJsonParse(sv(s));}
static int legend(xide_token_legend* l){xvalue* c=parse(caps);int okay=c && xideTokenLegendParse(c,l);xrtValueRelease(c);return okay;}
static int capabilityTests(void)
{
 xide_token_legend l;CHECK(legend(&l) && l.enabled && l.full && l.range && l.delta && l.typeCount==12 && l.modifierCount==3 && l.types[XLS_TOKEN_METHOD]==11 && l.modifierBits[0]==1);
 xvalue* provider=xideTokenLegendProvider(&l);CHECK(provider && xrtValueCount(get(get(provider,"legend"),"tokenTypes"))==12);xrtValueRelease(provider);
 xvalue* c=parse("{}");CHECK(xideTokenLegendParse(c,&l) && !l.enabled);xrtValueRelease(c);
 const char* cases[]={"{\"textDocument\":{\"semanticTokens\":null}}","{\"textDocument\":{\"semanticTokens\":{}}}","{\"textDocument\":{\"semanticTokens\":[]}}"};
 for(size_t i=0;i<3;++i){c=parse(cases[i]);CHECK(!xideTokenLegendParse(c,&l));xrtValueRelease(c);}
 c=parse("{\"textDocument\":{\"semanticTokens\":{\"requests\":{\"full\":true},\"tokenTypes\":[\"type\",\"variable\",\"function\"],\"tokenModifiers\":[],\"formats\":[\"relative\"]}}}");CHECK(xideTokenLegendParse(c,&l) && l.enabled && !l.range && !l.delta && l.typeCount==3 && l.types[XLS_TOKEN_CLASS]==0 && l.types[XLS_TOKEN_PARAMETER]==1 && l.types[XLS_TOKEN_METHOD]==2);xrtValueRelease(c);
 CHECK(legend(&l));return 1;
}
static xls_semantic_check* checked(const char* path,const char* text)
{
 xls_semantic_input input={path,text,strlen(text)};return xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);
}
static int verifyWords(const xvalue* values,const xide_token_data* expected)
{
 CHECK(values && xrtValueType(values)==XVALUE_ARRAY && xrtValueCount(values)==xideTokenDataCount(expected));const uint32* words=xideTokenDataWords(expected);
 for(size_t i=0;i<xrtValueCount(values);++i){int64 n=-1;CHECK(xrtValueGetInt(xrtValueArrayGet(values,i),&n) && n==words[i]);}return 1;
}
static int applyDelta(const xide_token_data* old,const xvalue* result,const xide_token_data* expected)
{
 const xvalue* values=get(result,"data");if(values)return verifyWords(values,expected);
 const xvalue* edits=get(result,"edits");CHECK(edits && xrtValueType(edits)==XVALUE_ARRAY && xrtValueCount(edits)<=1);
 const uint32 *a=xideTokenDataWords(old),*z=xideTokenDataWords(expected);size_t an=xideTokenDataCount(old),zn=xideTokenDataCount(expected);
 if(!xrtValueCount(edits)){CHECK(an==zn);for(size_t i=0;i<an;++i)CHECK(a[i]==z[i]);return 1;}
 const xvalue* edit=xrtValueArrayGet(edits,0);int64 start=-1,remove=-1;CHECK(xrtValueGetInt(get(edit,"start"),&start) && xrtValueGetInt(get(edit,"deleteCount"),&remove) && start>=0 && remove>=0 && (uint64)start<=an && (uint64)remove<=an-(size_t)start);
 values=get(edit,"data");size_t inserted=values?xrtValueCount(values):0;CHECK(an-(size_t)remove+inserted==zn);
 for(size_t i=0;i<(size_t)start;++i)CHECK(a[i]==z[i]);
 for(size_t i=0;i<inserted;++i){int64 n=-1;CHECK(xrtValueGetInt(xrtValueArrayGet(values,i),&n) && n==z[(size_t)start+i]);}
 for(size_t i=(size_t)start+(size_t)remove;i<an;++i)CHECK(a[i]==z[i-(size_t)remove+inserted]);
 return 1;
}
static int positionsAndDelta(void)
{
 const char* text="\xef\xbb\xbf" "func helper(int 输入) as int\r\n return 输入\r\nend\r\nfunc main(...) as int\r\n string 文本=\"😀\"; int 值=helper(7)\r\n return 值\r\nend\r\n";
 char* path=xrtPathAbs("token-wire.xl");CHECK(path);xls_semantic_check* c=checked(path,text);CHECK(c && xlsSemanticCheckInfo(c).accepted);
 enum xls_semantic_tokens_error error;xide_token_positions* positions=xideTokenPositionsCreate(c,0,1,0,NULL,NULL,&error);CHECK(positions && !error);
 xls_input_source input;CHECK(xlsSemanticCheckSource(c,0,&input));xls_text* original=xlsTextCreate(input.text,input.size,8u*1024u*1024u,NULL);CHECK(original);
 xls_semantic_tokens* native=xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error);CHECK(native && xlsSemanticTokensCount(native)==xideTokenPositionsCount(positions));
 const xls_semantic_token* raw=xlsSemanticTokensData(native);const xide_token_position* rows=xideTokenPositionsData(positions);
 for(size_t i=0;i<xideTokenPositionsCount(positions);++i){xls_position a,z;CHECK(xlsTextPosition(original,raw[i].range.startOffset,&a) && xlsTextPosition(original,raw[i].range.endOffset,&z));CHECK(rows[i].line==(int)a.line && rows[i].character==(int)a.character+(a.line?0:1) && rows[i].length==(int)(z.character-a.character) && rows[i].kind==raw[i].kind && rows[i].modifiers==raw[i].modifiers);}
 CHECK(xideTokenPositionsWork(positions).scannedBytes<=input.size && xideTokenPositionsWork(positions).peakBytes<=16u*1024u*1024u);
 xide_token_legend l;CHECK(legend(&l));xide_token_data* before=xideTokenDataEncode(positions,&l,NULL,"before",NULL,NULL);CHECK(before && xideTokenDataCount(before)==xideTokenPositionsCount(positions)*5);
 xvalue* result=xideTokenWireResult(before,NULL,0,NULL,NULL);CHECK(result && verifyWords(get(result,"data"),before));xrtValueRelease(result);
 xide_token_range range={1,0,7,0,8};xide_token_data* partial=xideTokenDataEncode(positions,&l,&range,NULL,NULL,NULL);CHECK(partial && xideTokenDataCount(partial)==5 && xideTokenDataWords(partial)[0]==0 && xideTokenDataWords(partial)[1]==6 && xideTokenDataWords(partial)[2]==6);xideTokenDataRelease(partial);
 range=(xide_token_range){1,0,7,0,7};partial=xideTokenDataEncode(positions,&l,&range,NULL,NULL,NULL);CHECK(partial && !xideTokenDataCount(partial));xideTokenDataRelease(partial);
 xstrbuf changed={0};CHECK(xrtStrBufAppend(&changed,xrtStrView("\n")) && xrtStrBufAppend(&changed,xrtStrView(text+3)));xls_semantic_check* next=checked(path,changed.Data);CHECK(next && xlsSemanticCheckInfo(next).accepted);xide_token_positions* moved=xideTokenPositionsCreate(next,0,0,0,NULL,NULL,&error);CHECK(moved);xide_token_data* after=xideTokenDataEncode(moved,&l,NULL,"after",NULL,NULL);CHECK(after);
 result=xideTokenWireResult(after,before,1,NULL,NULL);CHECK(result && get(result,"edits") && applyDelta(before,result,after));xrtValueRelease(result);
 result=xideTokenWireResult(after,after,1,NULL,NULL);CHECK(result && get(result,"edits") && !xrtValueCount(get(result,"edits")));xrtValueRelease(result);
 result=xideTokenWireResult(after,NULL,1,NULL,NULL);CHECK(result && verifyWords(get(result,"data"),after));xrtValueRelease(result);
 xlsSemanticCheckDestroy(c);xlsSemanticCheckDestroy(next);CHECK(xideTokenPositionsCount(positions) && xideTokenPositionsRetain(positions)==positions);xideTokenPositionsRelease(positions);
 xideTokenPositionsRelease(positions);xideTokenPositionsRelease(moved);xideTokenDataRelease(before);xideTokenDataRelease(after);xlsTextRelease(original);xlsSemanticTokensRelease(native);xrtStrBufFree(&changed);xrtFree(path);return 1;
}
typedef struct stop_at {size_t calls,at;} stop_at;
static int stop(void* user){stop_at* s=user;return ++s->calls>=s->at && s->at;}
static int cancellationAndJobs(void)
{
 char* path=xrtPathAbs("token-worker.xl");const char* text="func main(...) as int\n int value=7\n return value\nend\n";CHECK(path);xls_semantic_check* c=checked(path,text);CHECK(c && xlsSemanticCheckInfo(c).accepted);
 enum xls_semantic_tokens_error error;stop_at state={0};xide_token_positions* p=xideTokenPositionsCreate(c,0,0,0,stop,&state,&error);CHECK(p && !error);size_t points=state.calls;xideTokenPositionsRelease(p);CHECK(points>10);
 for(size_t i=1;i<=points;++i){state=(stop_at){0,i};p=xideTokenPositionsCreate(c,0,0,0,stop,&state,&error);CHECK(!p && error==XLS_TOKENS_CANCELLED);}
 xide_token_legend l;CHECK(legend(&l));xide_token_request request={.mode=XIDE_TOKENS_FULL,.resultId="job"};xide_semantic_token_job* job=xideSemanticTokenJobStart(c,path,0,&l,&request,NULL,NULL,NULL,NULL,0);CHECK(job);xlsSemanticCheckDestroy(c);
 uint64_t began=GetTickCount64();while(!xideSemanticTokenJobFinished(job) && GetTickCount64()-began<10000)Sleep(1);CHECK(xideSemanticTokenJobFinished(job) && xideSemanticTokenJobState(job)==XIDE_TOKEN_READY);xvalue* result=xideSemanticTokenJobResult(job);xide_token_data* data=xideSemanticTokenJobData(job);CHECK(result && data && verifyWords(get(result,"data"),data));xideSemanticTokenJobDestroy(job);CHECK(xideTokenDataCount(data));xrtValueRelease(result);xideTokenDataRelease(data);
 xide_toolchains* cache=xideToolchainsCreate();char* compiler=xrtPathAbs("absent-xl.exe"),*directory=xrtPathAbs(".");CHECK(cache && compiler && directory);
 char* unrelated=xrtPathAbs("token-unrelated.xl");CHECK(unrelated);
 xide_language_snapshot snapshots[]={{path,text,9},{unrelated,text,9}};xide_language_analysis_options options={.nativeBackend=1};xide_model_error model={0};xide_language_check* host=xideLanguageCheckStartSnapshotsEx(cache,compiler,directory,path,snapshots,2,&options,&model);CHECK(host);
 began=GetTickCount64();while(xideLanguageCheckState(host,NULL,0)==XIDE_CHECK_RUNNING && GetTickCount64()-began<10000)Sleep(1);CHECK(xideLanguageCheckState(host,NULL,0)==XIDE_CHECK_READY);
 int current=99;CHECK(!xideLanguageCheckStartTokens(host,unrelated,9,&l,&request,NULL,&current,&model) && !current && !model.code);
 job=xideLanguageCheckStartTokens(host,path,9,&l,&request,NULL,&current,&model);CHECK(job && current && !model.code && xideLanguageCheckMatchesTokenOwner(host,job));xideLanguageCheckDestroy(host);
 began=GetTickCount64();while(!xideSemanticTokenJobFinished(job) && GetTickCount64()-began<10000)Sleep(1);CHECK(xideSemanticTokenJobFinished(job) && xideSemanticTokenJobState(job)==XIDE_TOKEN_READY);xideSemanticTokenJobDestroy(job);
 snapshots[0].text="func main(...) as int\n return missing\nend\n";
 host=xideLanguageCheckStartSnapshotsEx(cache,compiler,directory,path,snapshots,2,&options,&model);CHECK(host);
 began=GetTickCount64();while(xideLanguageCheckState(host,NULL,0)==XIDE_CHECK_RUNNING && GetTickCount64()-began<10000)Sleep(1);CHECK(xideLanguageCheckState(host,NULL,0)==XIDE_CHECK_READY);
 current=0;CHECK(!xideLanguageCheckStartTokens(host,path,9,&l,&request,NULL,&current,&model) && current && !model.code);
 current=99;CHECK(!xideLanguageCheckStartTokens(host,unrelated,9,&l,&request,NULL,&current,&model) && !current && !model.code);xideLanguageCheckDestroy(host);
 xideToolchainsDestroy(cache);xrtFree(compiler);xrtFree(directory);xrtFree(unrelated);xrtFree(path);return 1;
}
int main(void){if(!capabilityTests() || !positionsAndDelta() || !cancellationAndJobs())return 1;printf("semantic_tokens_wire_test passed: %u checks; negotiated roles,original/BOM/UTF16 proof,range overlap,numeric delta,cancellation and detached guarded ownership\n",checks);return 0;}
