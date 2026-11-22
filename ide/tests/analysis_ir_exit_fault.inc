/* Private fault-injection driver; never linked into the ordinary service. */
_Thread_local int xlsExitTestInKey;
static _Thread_local size_t failClone,cloneCalls,failCapture,captureCalls,failReserve,reserveCalls;
void* xlsExitFaultMalloc(size_t bytes){++cloneCalls;if(failClone==cloneCalls)return NULL;return xrtMalloc(bytes);}
void* xlsExitFaultCalloc(size_t n,size_t width){++captureCalls;if(failCapture==captureCalls)return NULL;return xrtCalloc(n,width);}
extern bool __real_xrtStrBufReserve(xstrbuf*,size_t);
bool __wrap_xrtStrBufReserve(xstrbuf* b,size_t n){if(xlsExitTestInKey){++reserveCalls;if(failReserve==reserveCalls)return false;}return __real_xrtStrBufReserve(b,n);}
static void clearFaults(void){failClone=cloneCalls=failCapture=captureCalls=failReserve=reserveCalls=0;}
static int faultCases(void)
{
 const char* text="func values(int count) as generator<string>\n string suffix=\"owned\"\n for i in range(count)\n string item=suffix+string(i)\n yield item\n end\nend\n";char* path=xrtPathAbs("exit-faults.xl");CHECK(path);xls_semantic_check* source=checked(path,text);CHECK(source&&xlsSemanticCheckInfo(source).accepted);char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);XL6_DiagList diags;xl6DiagInit(&diags);xlsExitTestLimit(8u*1024u*1024u);clearFaults();xls_ir_result* baseline=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,NULL,32u*1024u*1024u);CHECK(baseline&&!diags.count&&xlsExitResultWork(baseline).captured==1);
 for(size_t i=1;i<=3;++i){clearFaults();failClone=i;xls_ir_result* result=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,baseline,32u*1024u*1024u);CHECK(result&&!diags.count&&cloneCalls==i&&sameResult(result,expected));xls_exit_result_work work=xlsExitResultWork(result);CHECK(!work.reused&&work.originals==1&&work.declined==1&&!work.knownBytes);xlsFrontendIrResultDestroy(result);}
 clearFaults();failCapture=1;xls_ir_result* cold=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,NULL,32u*1024u*1024u);CHECK(cold&&!diags.count&&captureCalls==1&&sameResult(cold,expected));xls_exit_result_work work=xlsExitResultWork(cold);CHECK(!work.knownBytes&&work.originals==1&&work.declined==1);xlsFrontendIrResultDestroy(cold);
 clearFaults();failReserve=1;cold=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,NULL,32u*1024u*1024u);CHECK(cold&&!diags.count&&reserveCalls==1&&sameResult(cold,expected));work=xlsExitResultWork(cold);CHECK(!work.knownBytes&&work.originals==1&&work.declined==1);xlsFrontendIrResultDestroy(cold);
 clearFaults();cold=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,baseline,32u*1024u*1024u);CHECK(cold&&!diags.count&&xlsExitResultWork(cold).reused==1&&!reserveCalls&&sameResult(cold,expected));xlsFrontendIrResultDestroy(baseline);xlsSemanticCheckDestroy(source);CHECK(sameResult(cold,expected));xlsFrontendIrResultDestroy(cold);xl6DiagFree(&diags);xrtFree(expected);xrtFree(path);return 1;
}
static int cancelExit(void* user){unsigned* polls=user;return xlsExitTestInKey&&++*polls>=3;}
static int cancelCases(void)
{
 char* path=xrtPathAbs("exit-cancel.xl"),*text=body(0);CHECK(path&&text);xls_semantic_check* source=checked(path,text);CHECK(source&&xlsSemanticCheckInfo(source).accepted);char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);XL6_DiagList diags;xl6DiagInit(&diags);xlsExitTestLimit(8u*1024u*1024u);xls_ir_result* baseline=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,NULL,32u*1024u*1024u);CHECK(baseline&&!diags.count&&xlsExitResultWork(baseline).captured==1);
 for(unsigned mode=0;mode<2;++mode){unsigned polls=0;xls_frontend_cancel_scope scope;xlsFrontendCancelBegin(&scope,cancelExit,&polls);xls_ir_result* result=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,mode?baseline:NULL,32u*1024u*1024u);CHECK(!result&&polls>=3&&xlsFrontendCancelObserved());xlsFrontendCancelEnd(&scope,NULL);xl6DiagFree(&diags);xl6DiagInit(&diags);result=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,baseline,32u*1024u*1024u);CHECK(result&&!diags.count&&xlsExitResultWork(result).reused==1&&sameResult(result,expected));xlsFrontendIrResultDestroy(result);}
 xlsFrontendIrResultDestroy(baseline);xlsSemanticCheckDestroy(source);xl6DiagFree(&diags);xrtFree(expected);xrtFree(text);xrtFree(path);return 1;
}
int main(void){if(qaMain()||!faultCases()||!cancelCases())return 1;printf("exit allocation/cancellation QA passed: %d total assertions\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;}
