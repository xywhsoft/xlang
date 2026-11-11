#include "workspace_edit_fixture.h"
static size_t textConstructions;static int countConstructions;
xls_text* __real_xlsTextCreate(const char*,size_t,size_t,enum xls_text_error*);
xls_text* __wrap_xlsTextCreate(const char* text,size_t n,size_t limit,enum xls_text_error* error)
{if(countConstructions)++textConstructions;return __real_xlsTextCreate(text,n,limit,error);}
#ifdef XLS_COMPILER_XRT
#include "services/rename_plan.h"
#include "services/language_check.h"
static int encoderRoundTrip(void)
{
 const char* path="D:/edit/native.xl";const char* source="foo(1)\n";xide_semantic_source s={path,source,1,0};
 xvalue* proof=xideLanguageReferenceDocument(&s,41,NULL);CHECK(proof);xide_rename_plan_input input={s,proof};xide_rename_plan_change change={0,{0,0,0},{0,3,3}};enum xide_rename_plan_error error;
 xide_rename_plan* original=xideRenamePlanCreate(&input,1,&change,1,xrtStrView("foo"),xrtStrView("bar"),0,NULL,NULL,&error);CHECK(original && !error);xvalue* wire=xideRenamePlanWorkspaceEdit(original,1,NULL,NULL);CHECK(wire);xideRenamePlanRelease(original);xrtValueRelease(proof);
 char* uri=xideLspPathToUri(path,NULL);xls_text* before=xlsTextCreate(source,strlen(source),1024,NULL);CHECK(uri && before);xide_workspace_edit_input captured={uri,before,1,9,1,41};enum xide_workspace_edit_error why;
 xide_workspace_edit* p=xideWorkspaceEditPrepare(wire,&captured,1,1,0,NULL,NULL,&why);CHECK(p && !why);xide_workspace_edit_document d;CHECK(xideWorkspaceEditDocumentAt(p,0,&d) && d.proof.present && d.proof.overlay && d.proof.version==41 && !strcmp(xlsTextData(d.after),"bar(1)\n"));
 xvalue* row=xrtValueArrayGet(xrtValueObjectGet(wire,xrtStrView("documentChanges")),0);xvalue* extension=xrtValueObjectGet(row,xrtStrView("xlang"));xvalue* document=xrtValueObjectGet(extension,xrtStrView("document"));CHECK(putEdit(document,"normalizedSha256",xrtValueString(xrtStrView("0000000000000000000000000000000000000000000000000000000000000000"))));CHECK(!xideWorkspaceEditPrepare(wire,&captured,1,1,0,NULL,NULL,&why) && why==XIDE_WORKSPACE_EDIT_PROOF);
 xrtValueRelease(wire);xlsTextRelease(before);free(uri);CHECK(!strcmp(xlsTextData(d.after),"bar(1)\n"));xideWorkspaceEditRelease(p);return 1;
}
static int closedEncoder(void)
{
 const char* path="D:/edit/closed.xl",*raw="foo(1)\r\n";xide_semantic_source source={path,raw,1,0};xsha256 state;xrtSha256Init(&state);unsigned char digest[32];CHECK(xrtSha256Update(&state,"\xef\xbb\xbf",3) && xrtSha256Update(&state,raw,strlen(raw)) && xrtSha256Final(&state,digest));char hex[65];for(size_t i=0;i<32;++i)snprintf(hex+2*i,3,"%02x",digest[i]);
 xvalue* disk=xrtValueObject();CHECK(disk && putEdit(disk,"path",xrtValueString(xrtStrView(path))) && putEdit(disk,"origin",xrtValueString(xrtStrView("disk"))) && putEdit(disk,"rawSize",xrtValueInt((int64)strlen(raw)+3)) && putEdit(disk,"rawSha256",xrtValueString(xrtStrView(hex))));xvalue* proof=xideLanguageReferenceDocument(&source,0,disk);xrtValueRelease(disk);CHECK(proof);
 xide_rename_plan_input input={source,proof};xide_rename_plan_change change={0,{0,0,0},{0,3,3}};enum xide_rename_plan_error error;xide_rename_plan* native=xideRenamePlanCreate(&input,1,&change,1,xrtStrView("foo"),xrtStrView("bar"),0,NULL,NULL,&error);CHECK(native && !error);xvalue* wire=xideRenamePlanWorkspaceEdit(native,1,NULL,NULL);CHECK(wire);xideRenamePlanRelease(native);xrtValueRelease(proof);
 char* uri=xideLspPathToUri(path,NULL);xls_text* before=xlsTextCreate("foo(1)\n",7,1024,NULL);CHECK(uri && before);xide_workspace_edit_input captured={uri,before,0,1,0,0};enum xide_workspace_edit_error why;xide_workspace_edit* p=xideWorkspaceEditPrepare(wire,&captured,1,1,0,NULL,NULL,&why);CHECK(p && !why);xide_workspace_edit_document d;CHECK(xideWorkspaceEditDocumentAt(p,0,&d) && d.proof.present && !d.proof.overlay && d.proof.rawSize==strlen(raw)+3 && !memcmp(d.proof.rawSha256,digest,32) && !strcmp(xlsTextData(d.after),"bar(1)\n"));
 xrtValueRelease(wire);xlsTextRelease(before);free(uri);xideWorkspaceEditRelease(p);return 1;
}
#endif
static int basic(void)
{
 xls_text* before=xlsTextCreate("a😀bc\n中文foo\n",strlen("a😀bc\n中文foo\n"),1024,NULL);CHECK(before);xide_workspace_edit_input in={"file:///D:/edit/a.xl",before,7,9,1,41};
 xvalue* value=parseEdit("{\"documentChanges\":[{\"textDocument\":{\"uri\":\"file:///d:/edit/./a.xl\",\"version\":41},\"edits\":[{\"range\":{\"start\":{\"line\":1,\"character\":2},\"end\":{\"line\":1,\"character\":5}},\"newText\":\"名字\"},{\"range\":{\"start\":{\"line\":0,\"character\":3},\"end\":{\"line\":0,\"character\":3}},\"newText\":\"1\"},{\"range\":{\"start\":{\"line\":0,\"character\":3},\"end\":{\"line\":0,\"character\":3}},\"newText\":\"2\"},{\"range\":{\"start\":{\"line\":0,\"character\":3},\"end\":{\"line\":0,\"character\":4}},\"newText\":\"B\"},{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":1}},\"newText\":\"A\"}]}]}");CHECK(value);
 enum xide_workspace_edit_error why;xide_workspace_edit* plan=xideWorkspaceEditPrepare(value,&in,1,0,0,NULL,NULL,&why);CHECK(plan && why==XIDE_WORKSPACE_EDIT_OK);xide_workspace_edit_info info;CHECK(xideWorkspaceEditInfo(plan,&info) && info.documents==1 && info.edits==5 && info.changedDocuments==1 && info.bytes);
 xide_workspace_edit_document d;CHECK(xideWorkspaceEditDocumentAt(plan,0,&d) && d.before==before && !strcmp(xlsTextData(d.after),"A😀12Bc\n中文名字\n") && !strcmp(d.uri,in.uri) && d.input==0 && d.count==5);
 xide_workspace_text_edit e;CHECK(xideWorkspaceEditAt(plan,1,&e) && e.ordinal==1 && e.afterEnd-e.afterStart==1);CHECK(xideWorkspaceEditStillCurrent(plan,0,&in));
 xide_workspace_edit_input stale=in;stale.revision++;CHECK(!xideWorkspaceEditStillCurrent(plan,0,&stale));stale=in;stale.identity++;CHECK(!xideWorkspaceEditStillCurrent(plan,0,&stale));stale=in;stale.version++;CHECK(!xideWorkspaceEditStillCurrent(plan,0,&stale));
 xls_text* equal=xlsTextCreate(xlsTextData(before),xlsTextSize(before),1024,NULL);stale=in;stale.text=equal;CHECK(equal && !xideWorkspaceEditStillCurrent(plan,0,&stale));xlsTextRelease(equal);
 xrtValueRelease(value);xlsTextRelease(before);CHECK(!strcmp(xlsTextData(d.before),"a😀bc\n中文foo\n") && !strcmp(xlsTextData(d.after),"A😀12Bc\n中文名字\n"));CHECK(xideWorkspaceEditRetain(plan)==plan);xideWorkspaceEditRelease(plan);xideWorkspaceEditRelease(plan);return 1;
}
static int failures(void)
{
 xls_text* before=xlsTextCreate("abc\n",4,1024,NULL);CHECK(before);xide_workspace_edit_input in={"file:///D:/edit/a.xl",before,7,9,1,41};
 const char* bad[]={
 "{\"documentChanges\":null}","{\"documentChanges\":[{\"kind\":\"create\",\"uri\":\"file:///D:/edit/a.xl\"}]}",
 "{\"documentChanges\":[{\"textDocument\":{\"uri\":\"file:///D:/edit/a.xl\",\"version\":42},\"edits\":[]}]}",
 "{\"documentChanges\":[{\"textDocument\":{\"uri\":\"file:///D:/edit/a.xl\"},\"edits\":[]}]}",
 "{\"changes\":{\"file:///D:/edit/unknown.xl\":[]}}",
 "{\"changes\":{\"file:///D:/edit/a.xl\":[{\"range\":{\"start\":{\"line\":0,\"character\":4},\"end\":{\"line\":0,\"character\":4}},\"newText\":\"x\"}]}}",
 "{\"changes\":{\"file:///D:/edit/a.xl\":[{\"range\":{\"start\":{\"line\":0,\"character\":2},\"end\":{\"line\":0,\"character\":1}},\"newText\":\"x\"}]}}",
 "{\"changes\":{\"file:///D:/edit/a.xl\":[{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":2}},\"newText\":\"x\"},{\"range\":{\"start\":{\"line\":0,\"character\":1},\"end\":{\"line\":0,\"character\":3}},\"newText\":\"y\"}]}}",
 "{\"documentChanges\":[{\"textDocument\":{\"uri\":\"file:///D:/edit/a.xl\",\"version\":41},\"edits\":[]},{\"textDocument\":{\"uri\":\"file:///d:/edit/a.xl\",\"version\":41},\"edits\":[]}]}",
 "{\"changes\":{\"file:///D:/edit/a.xl\":[{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":0}},\"newText\":\"x\",\"annotationId\":\"missing\"}]}}"};
 for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i){xvalue* v=parseEdit(bad[i]);enum xide_workspace_edit_error why;CHECK(v && !xideWorkspaceEditPrepare(v,&in,1,0,0,NULL,NULL,&why) && why!=XIDE_WORKSPACE_EDIT_OK);CHECK(!strcmp(xlsTextData(before),"abc\n"));xrtValueRelease(v);}
 xvalue* value=oneEdit(in.uri,1,41,"A");CHECK(value);enum xide_workspace_edit_error why;CHECK(!xideWorkspaceEditPrepare(value,&in,1,1,0,NULL,NULL,&why) && why==XIDE_WORKSPACE_EDIT_PROOF);CHECK(!xideWorkspaceEditPrepare(value,&in,1,0,1,NULL,NULL,&why) && why==XIDE_WORKSPACE_EDIT_LIMIT);
 xide_workspace_edit_input duplicates[]={in,in};CHECK(!xideWorkspaceEditPrepare(value,duplicates,2,0,0,NULL,NULL,&why) && why==XIDE_WORKSPACE_EDIT_SOURCE);xrtValueRelease(value);xlsTextRelease(before);return 1;
}
static int variants(void)
{
 xls_text* a=xlsTextCreate("abc",3,1024,NULL),*b=xlsTextCreate("xyz",3,1024,NULL);CHECK(a && b);xide_workspace_edit_input inputs[]={{"file:///D:/edit/a.xl",a,1,4,1,INT_MIN},{"file:///D:/edit/b.xl",b,0,8,0,0}};
 xvalue* value=parseEdit("{\"documentChanges\":[{\"textDocument\":{\"uri\":\"file:///D:/edit/b.xl\",\"version\":null},\"edits\":[{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":1}},\"newText\":\"X\\r\\nY\\r\",\"annotationId\":\"confirm\"}]},{\"textDocument\":{\"uri\":\"file:///D:/edit/a.xl\",\"version\":-2147483648},\"edits\":[]}],\"changeAnnotations\":{\"confirm\":{\"label\":\"跨文件改名\",\"needsConfirmation\":true,\"description\":\"保留预览\"}},\"changes\":{\"ignored\":null}}");CHECK(value);
 enum xide_workspace_edit_error why;xide_workspace_edit* p=xideWorkspaceEditPrepare(value,inputs,2,0,0,NULL,NULL,&why);CHECK(p && !why);xide_workspace_edit_info info;CHECK(xideWorkspaceEditInfo(p,&info) && info.documents==2 && info.edits==1 && info.annotations==1 && info.needsConfirmation && info.changedDocuments==1);
 xide_workspace_edit_document d;CHECK(xideWorkspaceEditDocumentAt(p,0,&d) && d.input==1 && !d.hasVersion && !strcmp(xlsTextData(d.after),"X\nY\nyz"));CHECK(xideWorkspaceEditDocumentAt(p,1,&d) && d.input==0 && d.before==d.after && !d.changed);
 xide_workspace_edit_annotation note;CHECK(xideWorkspaceEditAnnotationAt(p,0,&note) && note.needsConfirmation && !strcmp(note.label.Data,"跨文件改名"));xrtValueRelease(value);CHECK(!strcmp(note.description.Data,"保留预览"));xideWorkspaceEditRelease(p);
 value=parseEdit("{\"changes\":{\"file:///D:/edit/a.xl\":[{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":1}},\"newText\":\"a\"}]}}");p=xideWorkspaceEditPrepare(value,inputs,2,0,0,NULL,NULL,&why);CHECK(p && !why);CHECK(xideWorkspaceEditDocumentAt(p,0,&d) && !d.changed && d.before==d.after);xideWorkspaceEditRelease(p);xrtValueRelease(value);
 value=parseEdit("{}");p=xideWorkspaceEditPrepare(value,NULL,0,0,0,NULL,NULL,&why);CHECK(p && !why && xideWorkspaceEditInfo(p,&info) && !info.documents);xideWorkspaceEditRelease(p);xrtValueRelease(value);xlsTextRelease(a);xlsTextRelease(b);return 1;
}
static int lateFailure(void)
{
 xls_text *a=xlsTextCreate("abc",3,1024,NULL),*b=xlsTextCreate("a😀b",6,1024,NULL);CHECK(a && b);xide_workspace_edit_input in[]={{"file:///D:/edit/a.xl",a,1,1,1,1},{"file:///D:/edit/b.xl",b,2,1,1,1}};
 xvalue *value=xrtValueObject(),*docs=xrtValueArray(),*first=xrtValueArray(),*last=xrtValueArray();CHECK(value && docs && first && last && xrtValueArrayAppendNew(first,textEdit(0,0,1,"A")) && xrtValueArrayAppendNew(last,textEdit(0,2,3,"invalid surrogate")));
 CHECK(xrtValueArrayAppendNew(docs,documentEdit(in[0].uri,1,1,first)) && xrtValueArrayAppendNew(docs,documentEdit(in[1].uri,1,1,last)) && putEdit(value,"documentChanges",docs));enum xide_workspace_edit_error why;uint64_t blocks=editBlocks();CHECK(!xideWorkspaceEditPrepare(value,in,2,0,0,NULL,NULL,&why) && why==XIDE_WORKSPACE_EDIT_RANGE && editBlocks()==blocks);CHECK(!strcmp(xlsTextData(a),"abc") && !strcmp(xlsTextData(b),"a😀b"));
 xrtValueRelease(value);xlsTextRelease(a);xlsTextRelease(b);return 1;
}
static int large(void)
{
 const size_t n=12000;char* body=xrtMalloc(n+1);CHECK(body);memset(body,'a',n);body[n]=0;xls_text* before=xlsTextCreate(body,n,n,NULL);CHECK(before);xrtFree(body);xide_workspace_edit_input input={"file:///D:/edit/wide.xl",before,1,1,1,INT_MAX};xvalue *value=xrtValueObject(),*docs=xrtValueArray(),*edits=xrtValueArray();CHECK(value && docs && edits);
 for(size_t i=n;i>0;--i)CHECK(xrtValueArrayAppendNew(edits,textEdit(0,(int)i-1,(int)i,"b")));
 CHECK(xrtValueArrayAppendNew(docs,documentEdit(input.uri,1,input.version,edits)) && putEdit(value,"documentChanges",docs));enum xide_workspace_edit_error why;textConstructions=0;countConstructions=1;xide_workspace_edit* p=xideWorkspaceEditPrepare(value,&input,1,0,0,NULL,NULL,&why);countConstructions=0;CHECK(p && !why && textConstructions==1);xide_workspace_edit_document d;CHECK(xideWorkspaceEditDocumentAt(p,0,&d) && xlsTextSize(d.after)==n);
 for(size_t i=0;i<n;++i){CHECK(xlsTextData(d.after)[i]=='b' && xlsTextData(before)[i]=='a');}xideWorkspaceEditRelease(p);xrtValueRelease(value);xlsTextRelease(before);return 1;
}
int main(void){
#ifdef XLS_COMPILER_XRT
 xrtMemStatsEnable(true);
#endif
 if(!basic() || !failures() || !variants() || !lateFailure() || !large())return 1;
#ifdef XLS_COMPILER_XRT
 if(!encoderRoundTrip() || !closedEncoder())return 1;
#endif
 printf("workspace_edit_test passed: %u checks; real simultaneous original-snapshot edits, stable same-position order, UTF16/Unicode, LF normalization, annotations, versions, alias rejection, immutable leases and12000 reverse-ordered edits\n",checks);return 0;}
