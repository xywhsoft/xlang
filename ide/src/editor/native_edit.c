#include "editor/native_edit.h"
#include "editor/workspace_edit_transaction_internal.h"
#include "services/edit_candidate_internal.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define POLICY_RESERVE (16u*1024u*1024u)
typedef struct dependency_pin {xfile file;const xvalue* report;} dependency_pin;
typedef struct edit_target {wchar_t* key;size_t document;int closed;} edit_target;
struct xide_native_edit {xide_edit_candidate* candidate;xide_edit_transaction* transaction;dependency_pin* pins;const xvalue** reports;size_t count,pinned,targets,bytes,candidateBytes,limit;edit_target target[512];xatomic32 cancel;int bound,busy,attached,failed;};
struct xide_native_edit_ticket {xide_native_edit* work;xide_edit_transaction_ticket* inner;xide_document_store* store;xui_context context;uint64_t thread;int busy;int (*guard)(void*);void* user;};
static const xvalue* field(const xvalue* v,const char* key){return xrtValueIs(v,XVALUE_OBJECT)?xrtValueObjectGet(v,xrtStrView(key)):NULL;}
static const char* text(const xvalue* v){xstrview s={0};return xrtValueGetString(v,&s)&&s.Data&&s.Size&&!memchr(s.Data,0,s.Size)?s.Data:NULL;}
static int fail(xide_model_error* e,const char* message){if(e){e->code=XIDE_MODEL_CONFLICT;snprintf(e->message,sizeof(e->message),"%s",message);}return 0;}
static int charged(xide_native_edit* w,size_t bytes){if(bytes>POLICY_RESERVE-w->bytes)return 0;w->bytes+=bytes;return 1;}
static int targetOrder(const void* a,const void* b){return xidePathNativeKeyCompare(((const edit_target*)a)->key,((const edit_target*)b)->key);}
static const edit_target* findTarget(const xide_native_edit* w,const wchar_t* key)
{size_t lo=0,hi=w->targets;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(xidePathNativeKeyCompare(w->target[mid].key,key)<0)lo=mid+1;else hi=mid;}return lo<w->targets&&!xidePathNativeKeyCompare(w->target[lo].key,key)?w->target+lo:NULL;}
typedef struct native_stop {xide_native_edit* work;int(*stop)(void*);void* user;} native_stop;
static int cancelled(void* user){native_stop* c=user;return xrtAtomic32Load(&c->work->cancel,XMEMORY_ACQUIRE)||!xideEditCandidateCurrent(c->work->candidate)||(c->stop&&c->stop(c->user));}
void xideNativeEditCancel(xide_native_edit* w){if(w){xrtAtomic32Store(&w->cancel,1,XMEMORY_RELEASE);xideEditTransactionCancel(w->transaction);}}
int xideNativeEditDestroy(xide_native_edit* w)
{if(!w)return XUI_OK;if(w->busy||w->attached)return XUI_ERROR_BUSY;int status=xideEditTransactionDestroy(w->transaction);if(status!=XUI_OK)return status;for(size_t i=0;i<w->pinned;++i)if(w->pins[i].file)xrtClose(w->pins[i].file);for(size_t i=0;i<w->targets;++i)free(w->target[i].key);xrtFree(w->pins);xrtFree(w->reports);xideEditCandidateRelease(w->candidate);xrtFree(w);return XUI_OK;}
static int dependencies(xide_native_edit* w,int (*stop)(void*),void* user,xide_model_error* e)
{
 for(size_t i=0;i<w->pinned;++i){if(stop&&stop(user))return fail(e,"Native source policy cancelled.");xfileinfo held={0},named={0};const xvalue* report=w->pins[i].report;const char* path=text(field(report,"path"));
  if(!path||!xideIoReceiptFileStat(w->pins[i].file,&held)||!xideLanguageInputFileMatchesInfo(report,&held)||!xideIoReceiptStat(path,&named)||!xideLanguageInputFileMatchesInfo(report,&named))return fail(e,"Pinned native dependency metadata or name changed.");
 }return 1;
}
xide_native_edit* xideNativeEditPrepare(xide_edit_candidate* candidate,const char* directory,const xide_edit_alias_exclusion* protected,size_t exclusions,size_t budget,int(*stop)(void*),void* user,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 size_t limit=budget?budget:256u*1024u*1024u;xide_workspace_edit_info info;
 if(!candidate||!xideEditCandidateCurrent(candidate)||!xideWorkspaceEditInfo(xideEditCandidatePlan(candidate),&info)||info.documents>512||exclusions>255||(exclusions&&!protected)||limit>512u*1024u*1024u)return(fail(e,"Invalid native transaction admission."),NULL);
 xide_native_edit* w=xrtCalloc(1,sizeof(*w));if(!w)return NULL;xrtAtomic32Init(&w->cancel,0);w->candidate=xideEditCandidateRetain(candidate);w->limit=limit;w->bytes=sizeof(*w);w->candidateBytes=xideEditCandidateStorage(candidate);native_stop s={w,stop,user};int okay=w->candidate&&w->candidateBytes<limit&&POLICY_RESERVE<limit-w->candidateBytes;
 for(size_t i=0;okay&&i<info.documents;++i){xide_workspace_edit_document d;okay=!cancelled(&s)&&xideWorkspaceEditDocumentAt(xideEditCandidatePlan(candidate),i,&d);if(!okay)break;edit_target row={xidePathNativeWide(d.path),i,!d.hasVersion};if(!row.key||!charged(w,(wcslen(row.key)+1)*sizeof(wchar_t))){free(row.key);okay=0;break;}w->target[w->targets++]=row;}
 if(okay)qsort(w->target,w->targets,sizeof(*w->target),targetOrder);
 size_t sources=xideEditCandidateSourceCount(candidate);if(okay)okay=sources<=65536&&charged(w,sources*(sizeof(*w->pins)+sizeof(*w->reports)));
 if(okay){w->pins=xrtCalloc(sources?sources:1,sizeof(*w->pins));w->reports=xrtCalloc(sources?sources:1,sizeof(*w->reports));okay=w->pins&&w->reports;}
 for(size_t i=0;okay&&i<sources;++i){const xvalue* report=xideEditCandidateSourceAt(candidate,i);const char* origin=text(field(report,"origin"));if(!origin){okay=0;break;}if(strcmp(origin,"disk"))continue;if(cancelled(&s)){okay=0;break;}const char* path=text(field(report,"path"));wchar_t* key=path?xidePathNativeWide(path):NULL;if(!key){okay=0;break;}const edit_target* target=findTarget(w,key);free(key);w->reports[w->count++]=report;if(target&&target->closed)continue;
  char* native=xidePathNativeUtf8(path);xfileoptions options={.Flags=XFILE_READ,.Share=XFILE_SHARE_READ};xfile file=native?xrtFileOpen(native,&options):NULL;free(native);if(!file){okay=0;break;}w->pins[w->pinned++]=(dependency_pin){file,report};
 }
 if(okay)okay=dependencies(w,cancelled,&s,e)&&xideEditCandidateValidate(candidate,cancelled,&s,e)&&dependencies(w,cancelled,&s,e);
 if(okay){w->transaction=xideEditTransactionPrepare((xide_workspace_edit*)xideEditCandidatePlan(candidate),directory,limit-POLICY_RESERVE-w->candidateBytes,cancelled,&s,e);okay=w->transaction&&xideEditTransactionAllNamed(w->transaction)&&xideEditTransactionProtect(w->transaction,protected,exclusions,e);}
 if(!okay){if(e&&!e->code)fail(e,"Cannot pin the original native sources or admit the transaction.");xideNativeEditDestroy(w);w=NULL;}return w;
}
static int modelCurrent(xide_native_edit_ticket* t)
{
 if(!t->context||t->store->context!=t->context||!xideEditCandidateCurrent(t->work->candidate)||xrtAtomic32Load(&t->work->cancel,XMEMORY_ACQUIRE))return 0;
 const xide_edit_request* request=xideEditCallRequest(xideEditCandidateCall(t->work->candidate));if(t->store->count!=xideEditRequestModelCount(request))return 0;
 for(size_t i=0;i<t->store->count;++i){const xide_document* d=t->store->items[i];if(!d)return 0;xide_edit_request_model m={d->id,d->revision,d->open,d->dirty,d->externalState,d->path,d->language};if(!xideEditRequestModelMatches(request,i,&m))return 0;}return 1;
}
int xideNativeEditCapture(xide_document_store* store,xide_native_edit* w,xide_native_edit_ticket** out)
{
 if(out)*out=NULL;
 if(!out||!store||!store->context||!w||w->busy||w->attached||w->failed)return XUI_ERROR_INVALID_ARGUMENT;
 xide_native_edit_ticket* t=xrtCalloc(1,sizeof(*t));if(!t)return XUI_ERROR_OUT_OF_MEMORY;t->store=store;t->context=store->context;t->thread=xrtThreadCurrentId();t->work=w;int status=modelCurrent(t)?xideEditTransactionCapture(store,w->transaction,&t->inner):XUI_ERROR_INVALID_STATE;if(status==XUI_OK){w->attached=1;*out=t;}else xrtFree(t);return status;
}
static int hex64(const xvalue* v,uint64_t* out)
{const char* p=text(v);if(!p||strlen(p)!=16)return 0;uint64_t n=0;for(size_t i=0;i<16;++i){unsigned x=p[i]>='0'&&p[i]<='9'?(unsigned)(p[i]-'0'):p[i]>='a'&&p[i]<='f'?(unsigned)(p[i]-'a'+10):16;if(x>15)return 0;n=(n<<4)|x;}*out=n;return 1;}
static int identityOrder(const void* a,const void* b)
{const xide_edit_alias_target *x=a,*y=b;if(x->volume!=y->volume)return x->volume>y->volume?1:-1;return(x->index>y->index)-(x->index<y->index);}
static int bind(xide_native_edit* w,native_stop* stop,xide_model_error* e)
{
 if(!xideEditTransactionVerifyPinned(w->transaction,1,cancelled,stop,e))return 0;
 const xide_edit_aliases* aliases=xideEditTransactionAliases(w->transaction);xide_edit_alias_target targets[512];size_t count=0;
 for(size_t i=0;i<xideEditAliasesRows(aliases);++i){xide_edit_alias_target t;int found=xideEditAliasesTargetAt(aliases,i,&t);if(found<0)return 0;if(found&&!t.missing){if(count==512)return 0;targets[count++]=t;}}
 qsort(targets,count,sizeof(*targets),identityOrder);
 for(size_t i=0;i<w->count;++i){const xvalue* report=w->reports[i];uint64_t device=0,identity=0;const xvalue* metadata=field(report,"metadata");if(cancelled(stop)||!hex64(field(metadata,"device"),&device)||device>UINT32_MAX||!hex64(field(metadata,"identity"),&identity))return 0;xide_edit_alias_target key={.volume=(uint32_t)device,.index=identity};const xide_edit_alias_target* t=bsearch(&key,targets,count,sizeof(*targets),identityOrder);if(t){wchar_t* name=xidePathNativeWide(text(field(report,"path")));int same=name&&!xidePathNativeKeyCompare(name,t->path);free(name);if(!same)return fail(e,"A native source physically aliases an edited target through another name.");}}
 for(size_t i=0;i<xideEditTransactionClosed(w->transaction);++i){xide_edit_file_status file;xide_workspace_edit_document d;xfileinfo now={0};if(cancelled(stop)||!xideEditTransactionFileAt(w->transaction,i,&file)||!xideWorkspaceEditDocumentAt(xideEditCandidatePlan(w->candidate),file.document,&d))return 0;const xvalue* report=xideEditCandidateSource(w->candidate,d.path);
  if(!report||!xideIoReceiptStat(d.path,&now)||!xideLanguageInputFileMatchesInfo(report,&now)||now.Device!=file.before.volume||now.Identity!=file.before.index||file.before.length!=d.proof.rawSize||memcmp(file.before.sha256,d.proof.rawSha256,32))return fail(e,"Pinned original target does not match its native source identity and bytes.");
 }return 1;
}
static int groupCheck(xide_native_edit* w,native_stop* stop,xide_model_error* e)
{return w->bound&&!w->failed&&!cancelled(stop)&&dependencies(w,cancelled,stop,e)&&xideEditTransactionVerifyPinned(w->transaction,0,cancelled,stop,e)&&xideEditCandidateValidateOwned(w->candidate,w->reports,w->count,cancelled,stop,e)&&dependencies(w,cancelled,stop,e);}
int xideNativeEditStage(xide_native_edit* w,int(*stop)(void*),void* user,xide_model_error* e)
{if(!w||w->busy||!w->attached||w->bound||w->failed)return fail(e,"Native edit is not captured for staging.");w->busy=1;native_stop s={w,stop,user};int okay=!cancelled(&s)&&dependencies(w,cancelled,&s,e)&&xideEditTransactionStage(w->transaction,cancelled,&s,e)&&bind(w,&s,e);if(okay){w->bound=1;okay=groupCheck(w,&s,e);}if(!okay)w->failed=1;w->busy=0;return okay;}
static int uiGuard(void* user)
{xide_native_edit_ticket* t=user;if(!t->work->bound||t->work->failed||!modelCurrent(t))return XUI_ERROR_INVALID_STATE;int status=t->guard(t->user);return status==XUI_OK&&!modelCurrent(t)?XUI_ERROR_INVALID_STATE:status;}
int xideNativeEditArm(xide_native_edit_ticket* t,int(*guard)(void*),void* user)
{if(!t||!guard||t->thread!=xrtThreadCurrentId()||t->busy||t->work->busy)return XUI_ERROR_INVALID_ARGUMENT;t->busy=1;t->guard=guard;t->user=user;int status=xideEditTransactionArm(t->inner,uiGuard,t);t->guard=NULL;t->user=NULL;t->busy=0;return status;}
typedef struct native_fence {xide_native_edit* work;int(*guard)(void*,size_t,int);void* user;native_stop* stop;xide_model_error* error;} native_fence;
static int fence(void* user,size_t document,int action)
{
 native_fence* f=user;if(action==XIDE_EDIT_FILE_RESTORE)return f->guard(f->user,document,action);
 if(action!=XIDE_EDIT_FILE_APPLY||cancelled(f->stop)||!f->work->bound||f->work->failed)return 0;
 if(document==SIZE_MAX&&!groupCheck(f->work,f->stop,f->error))return 0;
 if(!f->guard(f->user,document,action)||cancelled(f->stop))return 0;
 return document!=SIZE_MAX||groupCheck(f->work,f->stop,f->error);
}
int xideNativeEditApply(xide_native_edit* w,int(*guard)(void*,size_t,int),void* user,int(*stop)(void*),void* cancelUser,xide_model_error* e)
{if(!w||!guard||w->busy||w->failed||!w->bound)return fail(e,"Native edit requires staged source ownership and detached host policy.");w->busy=1;native_stop s={w,stop,cancelUser};native_fence f={w,guard,user,&s,e};int okay=xideEditTransactionApply(w->transaction,fence,&f,cancelled,&s,e);w->busy=0;return okay;}
int xideNativeEditPublish(xide_native_edit_ticket* t,int(*guard)(void*),void* user,xide_open_edit_outcome* out)
{if(out)*out=(xide_open_edit_outcome){0};if(!t||!guard||!out||t->thread!=xrtThreadCurrentId()||t->busy||t->work->busy)return XUI_ERROR_INVALID_ARGUMENT;t->busy=1;t->guard=guard;t->user=user;int status=xideEditTransactionPublish(t->inner,uiGuard,t,out);t->guard=NULL;t->user=NULL;t->busy=0;return status;}
int xideNativeEditFinish(xide_native_edit* w,int(*guard)(void*,size_t,int),void* user,xide_model_error* e)
{if(!w||!guard||w->busy)return fail(e,"Native edit finish requires a definite owner publication outcome.");w->busy=1;native_stop s={w,NULL,NULL};native_fence f={w,guard,user,&s,e};int okay=xideEditTransactionFinish(w->transaction,fence,&f,e);w->busy=0;return okay;}
int xideNativeEditStatus(const xide_native_edit* w,xide_edit_transaction_status* out)
{if(out)*out=(xide_edit_transaction_status){0};if(!w||!out||w->busy||!xideEditTransactionStatus(w->transaction,out))return 0;if(w->failed)out->phase=XIDE_MIX_FAILED;out->knownBytes+=w->bytes+w->candidateBytes;return 1;}
int xideNativeEditTicketDestroy(xide_native_edit_ticket* t)
{if(!t)return XUI_OK;if(t->thread!=xrtThreadCurrentId()||t->busy||t->work->busy)return XUI_ERROR_BUSY;int status=xideEditTransactionTicketDestroy(t->inner);if(status==XUI_OK){t->work->attached=0;xrtFree(t);}return status;}
