/* One independently owned, bounded native exit-production result.
 * The build profile proves the complete original producer/read cohort.
 * Hits clone mutable outputs; the original full module verifier still runs. */
typedef struct xls_exit_test_work {uint64_t built,reused,declined,originals,keyBytes,outputBytes;}xls_exit_test_work;
static _Thread_local xls_exit_test_work exitWork;
static _Thread_local size_t exitLimit;
static _Thread_local const XL6_AstNode* exitRoot;
void xlsExitTestReset(void){exitWork=(xls_exit_test_work){0};}
xls_exit_test_work xlsExitTestWork(void){return exitWork;}
void xlsExitTestLimit(size_t bytes){exitLimit=bytes<8u*1024u*1024u?bytes:8u*1024u*1024u;exitRoot=NULL;}
#if XLS_EXPERIMENTAL_IR_EXIT_REUSE && defined(XLS_NATIVE_IR_EXIT_PRODUCTION) && XLS_NATIVE_IR_EXIT_PRODUCTION
static int xlsExitEnabled(void)
{if(exitLimit)return 1;++exitWork.originals;return 0;}
struct xls_exit_asset {
 xatomic32 refs;xstrbuf key;size_t bytes,nodes,plans,cleanups,steps;
 XL6_IrGeneratorClosePointAbi* close;XL6_IrExitPlan* plan;XL6_IrCleanupAction* cleanup;XL6_IrExitStep* step;
};
static void xlsExitRelease(xls_exit_asset* a)
{if(a&&xrtAtomic32FetchSub(&a->refs,1,XMEMORY_ACQ_REL)==1){xrtStrBufFree(&a->key);xrtFree(a);}}
static xls_exit_asset* xlsExitRetain(xls_exit_asset* a)
{if(a)xrtAtomic32FetchAdd(&a->refs,1,XMEMORY_RELAXED);return a;}
static int exitAppend(xls_exit_pending* p,const void* data,size_t count)
{if(p->compare){if(count>p->compareSize-p->at||(count&&memcmp(p->compare+p->at,data,count)))return 0;p->at+=count;return 1;}return p->limit&&xlsIrMemoData(&p->key,data,count,p->limit-1);}
static int exitNumber(xls_exit_pending* p,uint64_t n){unsigned char data[10];size_t bytes=xlsIrMemoNumberBytes(data,n);return exitAppend(p,data,bytes);}
static int exitText(xls_exit_pending* p,const char* s)
{size_t n=strlen(s)+1;return n&&exitNumber(p,n)&&exitAppend(p,s,n);}
static int exitString(xls_exit_pending* p,const char* s)
{return exitNumber(p,s!=NULL)&&(!s||exitText(p,s));}
static int exitVector(xls_exit_pending* p,const void* data,size_t n,size_t extra,size_t width)
{if(n>SIZE_MAX-extra)return 0;n+=extra;return n<=SIZE_MAX/width&&exitNumber(p,n)&&exitAppend(p,data,n*width);}
#include "analysis/frontend_ir_exit_key.inc"
static int exitRegion(size_t* n,size_t count,size_t width,size_t align,size_t* at)
{size_t pad=(align-*n%align)%align;if(pad>SIZE_MAX-*n)return 0;*n+=pad;*at=*n;if(count>(SIZE_MAX-*n)/width)return 0;*n+=count*width;return 1;}
static int exitClone(XL6_IrModule* m,const xls_exit_asset* a)
{
 XL6_IrExitPlan* plans=NULL;XL6_IrCleanupAction* cleanups=NULL;XL6_IrExitStep* steps=NULL;
 if(a->plans){plans=xrtMalloc(a->plans*sizeof(*plans));if(!plans)goto failed;memcpy(plans,a->plan,a->plans*sizeof(*plans));}
 if(a->cleanups){cleanups=xrtMalloc(a->cleanups*sizeof(*cleanups));if(!cleanups)goto failed;memcpy(cleanups,a->cleanup,a->cleanups*sizeof(*cleanups));}
 if(a->steps){steps=xrtMalloc(a->steps*sizeof(*steps));if(!steps)goto failed;memcpy(steps,a->step,a->steps*sizeof(*steps));}
 if(xlsFrontendCancelObserved())goto failed;
 xrtFree(m->exitPlans);xrtFree(m->cleanupActions);xrtFree(m->exitSteps);
 m->exitPlans=plans;m->exitPlanCount=m->exitPlanCapacity=a->plans;m->cleanupActions=cleanups;m->cleanupActionCount=m->cleanupActionCapacity=a->cleanups;m->exitSteps=steps;m->exitStepCount=m->exitStepCapacity=a->steps;
 for(size_t i=0;i<a->nodes;++i)m->nodes[i].generatorClosePointAbi=a->close[i];
 return 1;
failed:xrtFree(plans);xrtFree(cleanups);xrtFree(steps);return 0;
}
static int xlsExitTry(XL6_IrModule* m,xls_exit_pending* p)
{
 xls_ir_memo_build* b=xlsActiveIrMemo;
 if(!m||!b||b->module!=m||!b->memo||b->memo->exits||!exitLimit||(exitRoot&&exitRoot!=b->root)||m->instructionCount||!m->nodes||m->nodeCount>m->nodeCapacity||m->childCount>m->childCapacity||(m->childCount&&!m->children)||xlsFrontendCancelObserved()){++exitWork.originals;return 0;}
 if(b->memo->bytes>b->limit){++exitWork.declined;++exitWork.originals;return 0;}
 b->memo->exitStats.queried=1;
 size_t available=exitLimit<8u*1024u*1024u?exitLimit:8u*1024u*1024u;
 const xls_exit_asset* old=b->baseline?b->baseline->exits:NULL;
 if((old&&old->bytes>available)||available<sizeof(xls_exit_asset)){++b->memo->exitStats.declined;++b->memo->exitStats.originals;++exitWork.declined;++exitWork.originals;return 0;}
 p->limit=available;b->memo->exitStats.limitBytes=available;p->build=b;
 if(old&&old->nodes==m->nodeCount){
  p->compare=(const unsigned char*)old->key.Data;p->compareSize=old->key.Size;p->at=0;
  int equal=exitInputKey(p,m)&&p->at==p->compareSize;
  exitWork.keyBytes+=p->at;b->memo->exitStats.keyBytes=p->at;p->compare=NULL;p->compareSize=p->at=0;
  if(equal){
   if(exitClone(m,old)){b->memo->exits=xlsExitRetain((xls_exit_asset*)old);++b->memo->exitStats.reused;p->build=NULL;++exitWork.reused;return 1;}
   p->build=NULL;++b->memo->exitStats.declined;++exitWork.declined;
  }
 }
 ++b->memo->exitStats.originals;++exitWork.originals;return 0;
}
static void xlsExitFinish(XL6_IrModule* m,xls_exit_pending* p,int okay)
{
 xls_ir_memo_build* b=p->build;
 if(b&&okay&&!xlsFrontendCancelObserved()&&!b->memo->exits){
  if(!exitInputKey(p,m)){++b->memo->exitStats.declined;++exitWork.declined;xrtStrBufFree(&p->key);p->build=NULL;return;}
  b->memo->exitStats.keyBytes=p->key.Size;exitWork.keyBytes+=p->key.Size;
  size_t bytes=sizeof(xls_exit_asset),close,plans,cleanup,steps;
  int fits=exitRegion(&bytes,m->nodeCount,sizeof(XL6_IrGeneratorClosePointAbi),_Alignof(XL6_IrGeneratorClosePointAbi),&close)&&exitRegion(&bytes,m->exitPlanCount,sizeof(XL6_IrExitPlan),_Alignof(XL6_IrExitPlan),&plans)&&exitRegion(&bytes,m->cleanupActionCount,sizeof(XL6_IrCleanupAction),_Alignof(XL6_IrCleanupAction),&cleanup)&&exitRegion(&bytes,m->exitStepCount,sizeof(XL6_IrExitStep),_Alignof(XL6_IrExitStep),&steps);
  if(fits&&bytes<p->limit&&p->key.Capacity<p->limit-bytes){
   xls_exit_asset* a=xrtCalloc(1,bytes);
   if(a){unsigned char* mem=(void*)a;xrtAtomic32Init(&a->refs,1);a->key=p->key;p->key=(xstrbuf){0};a->nodes=m->nodeCount;a->plans=m->exitPlanCount;a->cleanups=m->cleanupActionCount;a->steps=m->exitStepCount;a->bytes=bytes+a->key.Capacity+1;a->close=(void*)(mem+close);a->plan=(void*)(mem+plans);a->cleanup=(void*)(mem+cleanup);a->step=(void*)(mem+steps);
    for(size_t i=0;i<m->nodeCount;++i)a->close[i]=m->nodes[i].generatorClosePointAbi;
    if(a->plans){memcpy(a->plan,m->exitPlans,a->plans*sizeof(*a->plan));}if(a->cleanups){memcpy(a->cleanup,m->cleanupActions,a->cleanups*sizeof(*a->cleanup));}if(a->steps){memcpy(a->step,m->exitSteps,a->steps*sizeof(*a->step));}
    b->memo->exits=a;++b->memo->exitStats.captured;++exitWork.built;exitWork.outputBytes+=bytes;
   }else {++b->memo->exitStats.declined;++exitWork.declined;}
  }else {++b->memo->exitStats.declined;++exitWork.declined;}
 }
 xrtStrBufFree(&p->key);p->build=NULL;
}

int xlsExitTestKey(const XL6_IrModule* m,xstrbuf* out,size_t limit)
{if(!m||!out)return 0;xls_exit_pending p={.limit=limit};int ok=exitInputKey(&p,m);*out=p.key;return ok;}

xls_exit_result_work xlsExitResultWork(const xls_ir_result* r)
{xls_exit_result_work work=r&&r->memo?r->memo->exitStats:(xls_exit_result_work){0};if(r&&r->memo&&r->memo->exits)work.knownBytes=r->memo->exits->bytes;return work;}
static void xlsExitCloneMemo(xls_ir_memo* memo,const xls_ir_memo* previous,const XL6_AstNode* root)
{if(previous->exits&&previous->exits->bytes<=exitLimit&&(!exitRoot||exitRoot==root)){memo->exits=xlsExitRetain(previous->exits);memo->exitStats.limitBytes=exitLimit;}}
int xlsFrontendIrExitAvailable(void){return 1;}
#else
static int xlsExitEnabled(void){return 0;}
static void xlsExitRelease(xls_exit_asset* p){(void)p;}
static int xlsExitTry(XL6_IrModule* m,xls_exit_pending* p){(void)m;(void)p;return 0;}
static void xlsExitFinish(XL6_IrModule* m,xls_exit_pending* p,int okay){(void)m;(void)p;(void)okay;}
static void xlsExitCloneMemo(xls_ir_memo* memo,const xls_ir_memo* previous,const XL6_AstNode* root){(void)memo;(void)previous;(void)root;}
xls_exit_result_work xlsExitResultWork(const xls_ir_result* r){(void)r;return (xls_exit_result_work){0};}
int xlsFrontendIrExitAvailable(void){return 0;}
int xlsExitTestKey(const XL6_IrModule* m,xstrbuf* out,size_t limit){(void)m;(void)out;(void)limit;return 0;}
#endif
