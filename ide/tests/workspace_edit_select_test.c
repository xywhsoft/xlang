#include "workspace_edit_fixture.h"
#include <windows.h>
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"edit-select:%d: %s\n",__LINE__,#v);exit(1);}}while(0)
static int tracking;static size_t bodyReads,bodyCreates,allocations,failAt,polls,stopAt;
void* __real_xrtCalloc(size_t,size_t);void* __real_malloc(size_t);bool __real_xrtArrayResize(xarray*,size_t);xls_text* __real_xlsTextCreate(const char*,size_t,size_t,enum xls_text_error*);const char* __real_xlsTextData(const xls_text*);
static int refuse(void){return tracking&&++allocations==failAt;}
void* __wrap_xrtCalloc(size_t n,size_t size){return refuse()?NULL:__real_xrtCalloc(n,size);}
void* __wrap_malloc(size_t n){return refuse()?NULL:__real_malloc(n);}
bool __wrap_xrtArrayResize(xarray* a,size_t n){return refuse()?false:__real_xrtArrayResize(a,n);}
xls_text* __wrap_xlsTextCreate(const char* s,size_t n,size_t limit,enum xls_text_error* e){if(tracking)++bodyCreates;return __real_xlsTextCreate(s,n,limit,e);}
const char* __wrap_xlsTextData(const xls_text* t){if(tracking)++bodyReads;return __real_xlsTextData(t);}
static int stop(void* unused){(void)unused;return ++polls==stopAt;}
static xide_workspace_edit* source(size_t large)
{
 xvalue *root=xrtValueObject(),*docs=xrtValueArray(),*notes=xrtValueObject();CHECK(root&&docs&&notes);xide_workspace_edit_input inputs[3]={0};char* body=malloc(large+1);CHECK(body);memset(body,'a',large);body[large]=0;
 const char* uris[]={"file:///C:/select/closed.xl","file:///C:/select/open.c","file:///C:/select/untouched.xl"};const char* before[]={body,"abc\n","unchanged\n"};
 for(size_t i=0;i<3;++i){inputs[i]=(xide_workspace_edit_input){.uri=uris[i],.text=xlsTextCreate(before[i],strlen(before[i]),8u*1024u*1024u,NULL),.identity=i==1?21:0,.revision=11+i,.hasVersion=i==1,.version=17};CHECK(inputs[i].text);xvalue* edits=xrtValueArray();CHECK(edits);if(i<2){xvalue* edit=textEdit(0,0,1,i?"中":"b");CHECK(edit&&putEdit(edit,"annotationId",xrtValueString(xrtStrView(i?"b":"a")))&&xrtValueArrayAppendNew(edits,edit));if(i==1)CHECK(xrtValueArrayAppendNew(edits,textEdit(0,1,1,"😀")));}xvalue* row=documentEdit(uris[i],i==1,17,edits);CHECK(row&&xrtValueArrayAppendNew(docs,row));}
 for(size_t i=0;i<3;++i){xvalue* note=xrtValueObject();CHECK(note&&putEdit(note,"label",xrtValueString(xrtStrView(i?"open":"closed")))&&putEdit(note,"needsConfirmation",xrtValueBool(i!=1))&&putEdit(notes,i==0?"a":i==1?"b":"unused",note));}
 CHECK(putEdit(root,"documentChanges",docs)&&putEdit(root,"changeAnnotations",notes));enum xide_workspace_edit_error e=0;xide_workspace_edit* plan=xideWorkspaceEditPrepare(root,inputs,3,0,256u*1024u*1024u,NULL,NULL,&e);CHECK(plan&&!e);for(size_t i=0;i<3;++i)xlsTextRelease(inputs[i].text);free(body);xrtValueRelease(root);return plan;
}
static xide_workspace_edit* selectPlan(xide_workspace_edit* p,const size_t* rows,size_t n,size_t budget,enum xide_workspace_edit_error* e)
{bodyReads=bodyCreates=allocations=polls=0;tracking=1;xide_workspace_edit* out=xideWorkspaceEditSelect(p,rows,n,budget,stop,NULL,e);tracking=0;CHECK(!bodyReads&&!bodyCreates);return out;}
static void inspectSelection(xide_workspace_edit* parent,xide_workspace_edit* selected,const size_t* rows,size_t n)
{
 xide_workspace_edit_info info;CHECK(xideWorkspaceEditInfo(selected,&info)&&info.documents==n);size_t first=0;
 for(size_t i=0;i<n;++i){xide_workspace_edit_document a,b;CHECK(xideWorkspaceEditDocumentAt(parent,rows[i],&a)&&xideWorkspaceEditDocumentAt(selected,i,&b));CHECK(a.before==b.before&&a.after==b.after&&a.input==b.input&&a.identity==b.identity&&a.revision==b.revision&&a.hasVersion==b.hasVersion&&a.version==b.version&&a.changed==b.changed&&!memcmp(&a.proof,&b.proof,sizeof(a.proof)));CHECK(a.uri!=b.uri&&a.path!=b.path&&!strcmp(a.uri,b.uri)&&!strcmp(a.path,b.path)&&b.first==first&&a.count==b.count);first+=b.count;
  for(size_t j=0;j<b.count;++j){xide_workspace_text_edit x,y;CHECK(xideWorkspaceEditAt(parent,a.first+j,&x)&&xideWorkspaceEditAt(selected,b.first+j,&y));CHECK(y.document==i&&x.ordinal==y.ordinal&&x.afterStart==y.afterStart&&x.afterEnd==y.afterEnd&&!memcmp(&x.start,&y.start,sizeof(x.start))&&!memcmp(&x.end,&y.end,sizeof(x.end)));if(x.annotation==SIZE_MAX)CHECK(y.annotation==SIZE_MAX);else{xide_workspace_edit_annotation note,copy;CHECK(xideWorkspaceEditAnnotationAt(parent,x.annotation,&note)&&xideWorkspaceEditAnnotationAt(selected,y.annotation,&copy));CHECK(note.id.Data!=copy.id.Data&&note.id.Size==copy.id.Size&&!memcmp(note.id.Data,copy.id.Data,note.id.Size)&&note.label.Size==copy.label.Size&&!memcmp(note.label.Data,copy.label.Data,note.label.Size)&&note.needsConfirmation==copy.needsConfirmation);}}
 }CHECK(info.edits==first);
}
int main(int argc,char** argv)
{
#ifdef XLS_COMPILER_XRT
 xrtMemStatsEnable(true);uint64_t initial=editBlocks();void* control=xrtMalloc(64);CHECK(control&&editBlocks()>initial);xrtFree(control);CHECK(editBlocks()==initial);
#else
 uint64_t initial=0;
#endif
 size_t gate=argc==2?(size_t)strtoull(argv[1],NULL,10):0;xide_workspace_edit* p=source(8u*1024u*1024u);uint64_t before=editBlocks();size_t rows[]={1};enum xide_workspace_edit_error e=0;failAt=gate;xide_workspace_edit* q=selectPlan(p,rows,1,0,&e);size_t observed=allocations,observedPolls=polls;
 if(gate){CHECK(allocations>=gate);if(q)CHECK(!e);else CHECK(e);xideWorkspaceEditRelease(q);xrtClearError();CHECK(editBlocks()==before);xideWorkspaceEditRelease(p);printf("workspace_edit_select_test passed: %u checks; allocation gate%zu/%zu, no body reads or creates\n",checks,gate,observed);return 0;}
 CHECK(q&&!e);inspectSelection(p,q,rows,1);xide_workspace_edit_info info;CHECK(xideWorkspaceEditInfo(q,&info)&&info.annotations==1&&!info.needsConfirmation&&info.changedDocuments==1);CHECK(xideWorkspaceEditInfo(p,&info)&&info.annotations==3&&info.needsConfirmation);xideWorkspaceEditRelease(q);
 for(size_t i=1;i<=observedPolls;++i){stopAt=i;CHECK(!selectPlan(p,rows,1,0,&e)&&e==XIDE_WORKSPACE_EDIT_CANCELLED&&polls==i);xrtClearError();CHECK(editBlocks()==before);}stopAt=0;
 size_t mixed[]={0,2};q=selectPlan(p,mixed,2,0,&e);CHECK(q);inspectSelection(p,q,mixed,2);CHECK(xideWorkspaceEditInfo(q,&info)&&info.annotations==1&&info.needsConfirmation&&info.changedDocuments==1);xideWorkspaceEditRelease(q);q=selectPlan(p,NULL,0,0,&e);CHECK(q&&xideWorkspaceEditInfo(q,&info)&&!info.documents&&!info.edits&&!info.annotations&&!info.needsConfirmation);xideWorkspaceEditRelease(q);
 size_t duplicate[]={1,1},reverse[]={2,0},bad[]={3};CHECK(!selectPlan(p,duplicate,2,0,&e)&&e==XIDE_WORKSPACE_EDIT_ARGUMENT);CHECK(!selectPlan(p,reverse,2,0,&e)&&e==XIDE_WORKSPACE_EDIT_ARGUMENT);CHECK(!selectPlan(p,bad,1,0,&e)&&e==XIDE_WORKSPACE_EDIT_ARGUMENT);CHECK(!selectPlan(p,rows,1,1,&e)&&e==XIDE_WORKSPACE_EDIT_LIMIT);
 q=selectPlan(p,rows,1,0,&e);CHECK(q);uint64_t withParent=editBlocks();xideWorkspaceEditRelease(p);
#ifdef XLS_COMPILER_XRT
 CHECK(editBlocks()<withParent);
#else
 (void)withParent;
#endif
 xide_workspace_edit_document d;CHECK(xideWorkspaceEditDocumentAt(q,0,&d)&&!strcmp(xlsTextData(d.before),"abc\n")&&!strcmp(xlsTextData(d.after),"中😀bc\n")&&!strcmp(d.path,"C:/select/open.c"));xideWorkspaceEditRelease(q);xrtClearError();CHECK(editBlocks()==initial);printf("workspace_edit_select_test passed: %u checks; allocation gate0/%zu, %zu cancellation points,8MiB parent body unscanned, independent metadata/text leases and index/annotation remapping\n",checks,observed,observedPolls);return 0;
}
