/* Actual private UI controllers and public Document producer. The deliberate
 * version-field equality models a wrapping toolkit clock without billions of
 * edits; body changes and model notices still execute through the real DLL. */
#ifdef XIDE_REVISION_LANGUAGE
#include "ui/language_view.c"
typedef xide_language_view tested_view;
#define CAPTURE(v) captureInputs((v)->app)
#define CURRENT(v) snapshotCurrent(v)
#define FREE_INPUTS(v) freeInputs(v)
#define SUBMIT(v) submitSyncInputs(v)
#else
#include "ui/completion_view.c"
typedef xide_completion_view tested_view;
#define CAPTURE(v) cvcapture(v)
#define CURRENT(v) cvinputsCurrent(v)
#define FREE_INPUTS(v) cvfreeInputs(v)
#define SUBMIT(v) cvsubmitSyncInputs(v)
#endif
static unsigned checks;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"revision-view:%d: %s\n",__LINE__,#e);failed=1;goto cleanup;}}while(0)
static xide_sync_result* takeResult(xide_sync_planner* planner)
{uint64_t until=GetTickCount64()+10000;xide_sync_result* result;do{result=xideSyncPlannerTake(planner);if(result)return result;Sleep(1);}while(GetTickCount64()<until);return NULL;}
int main(int argc,char** argv)
{
 int failed=0;xide_app app={0};tested_view* v=calloc(1,sizeof(*v));xide_model_error error={0};char* path=NULL;xvalue* first=NULL;xide_sync_result* result=NULL;
 CHECK(v && argc==2 && xrtDirCreateAll(argv[1]));path=xidePathResolveLexical(argv[1],"revision-view.xl");CHECK(path && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)"// base\n",8}));
 CHECK(xuiCreate(&app.context)==XUI_OK);xideDocumentsInit(&app.documents,app.context,NULL,NULL,NULL);xide_document* d=xideDocumentOpen(&app.documents,path,&error);CHECK(d);
 v->app=&app;v->document=d;
#ifdef XIDE_REVISION_LANGUAGE
 app.languageView=v;
#else
 app.completionView=v;
#endif
 d->revision=UINT64_C(4294967303);CHECK(CAPTURE(v) && v->inputCount==1 && CURRENT(v));CHECK(v->inputs[0].revision==d->revision);
 first=xrtValueRetain(v->inputs[0].textValue);CHECK(CAPTURE(v) && v->inputs[0].textValue==first);
 v->sync=xlsSyncCreateMonotonic(2);v->syncPlanner=xideSyncPlannerCreate(NULL,NULL);v->syncToken=1;CHECK(v->sync && v->syncPlanner && SUBMIT(v)==1);
 result=takeResult(v->syncPlanner);CHECK(result && result->valid && result->workerThreadId!=xrtThreadCurrentId());xls_sync_binding binding={0};
 CHECK(xlsSyncBindingAt(result->sync,0,&binding) && binding.revision==d->revision && binding.identity==d->id);
 {xls_sync* old=v->sync;v->sync=result->sync;result->sync=old;}xideSyncResultRelease(result);result=NULL;v->syncToken=0;
 xui_code_document core=xuiCodeEditGetDocument(d->editor);CHECK(xuiCodeDocumentInsert(core,0,"// changed\n")==XUI_OK);
 v->inputs[0].version=xuiCodeDocumentGetVersion(core);CHECK(!CURRENT(v));
 CHECK(CAPTURE(v) && CURRENT(v) && v->inputs[0].textValue!=first && strstr(v->inputs[0].text,"changed") && v->inputs[0].revision==d->revision);
 v->syncToken=2;CHECK(SUBMIT(v)==1);result=takeResult(v->syncPlanner);CHECK(result && result->valid && xlsSyncBindingAt(result->sync,0,&binding) && binding.revision==d->revision && binding.version==2);
 xideSyncResultRelease(result);result=NULL;v->syncToken=0;
#ifdef XIDE_REVISION_LANGUAGE
 CHECK(xideDocumentSave(d,NULL,0,&error));
 v->documentId=d->id;v->documentRevision=d->revision;v->version=xuiCodeDocumentGetVersion(core);v->path=copy(d->path);v->snapshotMode=0;
 CHECK(v->path && current(v));--v->documentRevision;CHECK(!current(v));++v->documentRevision;CHECK(current(v));
 free(v->path);v->path=NULL;
 v->references=calloc(1,sizeof(*v->references));v->workspaceSearch=calloc(1,sizeof(*v->workspaceSearch));CHECK(v->references && v->workspaceSearch);
 v->references->source=d;v->references->sourceId=d->id;v->references->sourceVersion=xuiCodeDocumentGetVersion(core);
 CHECK(referenceInputsCapture(&app) && referenceInputsCurrent(&app));CHECK(workspaceInputsCapture(&app) && workspaceSearchInputsCurrent(&app));
 --d->revision;CHECK(!referenceInputsCurrent(&app) && !workspaceSearchInputsCurrent(&app));++d->revision;
 v->annotations[0]=(annotation_binding){d,d->id,d->revision-1,xuiCodeDocumentGetVersion(core),d->externalState};v->annotationCount=1;CHECK(!CURRENT(v));v->annotationCount=0;
 xide_automatic_diagnostics automatic={0};v->autoView=&automatic;v->autoEnabled=1;CHECK(automaticInputsCurrent(v));--v->inputs[0].revision;CHECK(!automaticInputsCurrent(v));++v->inputs[0].revision;v->autoEnabled=0;v->autoView=NULL;
#endif
 d->revision=0;CHECK(!CURRENT(v));CHECK(!CAPTURE(v) && !v->inputCount);
 printf("document_revision_view_test passed: %u checks; actual %s controller capture/cache/fences and worker sync projection above32bits\n",checks,
#ifdef XIDE_REVISION_LANGUAGE
 "language"
#else
 "completion"
#endif
 );
cleanup:
 if(v){xideSyncResultRelease(result);xideSyncPlannerDestroy(v->syncPlanner);xlsSyncDestroy(v->sync);FREE_INPUTS(v);
#ifdef XIDE_REVISION_LANGUAGE
 free(v->path);if(v->references){referenceInputsFree(v->references);free(v->references);}if(v->workspaceSearch){workspaceInputsFree(v->workspaceSearch);free(v->workspaceSearch);}
#endif
 }xrtValueRelease(first);xideDocumentsClear(&app.documents);if(app.context)xuiDestroy(app.context);free(v);free(path);return failed;
}
