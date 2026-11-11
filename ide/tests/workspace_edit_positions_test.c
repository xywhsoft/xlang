#include "workspace_edit_fixture.h"
#include "editor/workspace_edit_positions.h"
#include <windows.h>
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"edit-positions:%d: %s\n",__LINE__,#v);exit(1);}}while(0)
static const char* uri="file:///D:/edit/positions.xl";
static xide_workspace_edit* prepare(const char* before,xvalue* edits)
{
 xls_text* text=xlsTextCreate(before,strlen(before),8u*1024u*1024u,NULL);CHECK(text);
 xide_workspace_edit_input input={uri,text,7,9,1,1};xvalue* root=xrtValueObject();xvalue* docs=xrtValueArray();CHECK(root && docs && xrtValueArrayAppendNew(docs,documentEdit(uri,1,1,edits)) && putEdit(root,"documentChanges",docs));
 enum xide_workspace_edit_error error;xide_workspace_edit* plan=xideWorkspaceEditPrepare(root,&input,1,0,0,NULL,NULL,&error);CHECK(plan && !error);xlsTextRelease(text);xrtValueRelease(root);return plan;
}
static xide_edit_positions* mapped(xide_workspace_edit* plan)
{enum xide_workspace_edit_error error;xide_edit_positions* p=xideEditPositionsPrepare(plan,0,NULL,NULL,&error);CHECK(p && !error);return p;}
static void at(xide_edit_positions* p,size_t offset,size_t left,size_t right)
{size_t value=SIZE_MAX;CHECK(xideEditPositionMap(p,0,offset,XIDE_EDIT_LEFT,&value) && value==left);CHECK(xideEditPositionMap(p,0,offset,XIDE_EDIT_RIGHT,&value) && value==right);}
static void literals(void)
{
 xvalue* edits=xrtValueArray();CHECK(edits && xrtValueArrayAppendNew(edits,textEdit(0,0,9,"abcXYghi")));xide_workspace_edit* plan=prepare("abcDEFghi",edits);xide_edit_positions* p=mapped(plan);xls_text* original=NULL;xide_workspace_edit_document document;CHECK(xideWorkspaceEditDocumentAt(plan,0,&document));original=xlsTextRetain(document.before);xideWorkspaceEditRelease(plan);
 at(p,0,0,0);at(p,2,2,2);at(p,3,3,5);at(p,4,3,5);at(p,6,5,5);at(p,7,6,6);at(p,9,8,8);
 xide_edit_selection selection={99,99};CHECK(xideEditSelectionMap(p,0,(xide_edit_selection){4,4},&selection) && selection.anchor==5 && selection.caret==5);CHECK(xideEditSelectionMap(p,0,(xide_edit_selection){4,3},&selection) && selection.anchor==5 && selection.caret==3);
 selection=(xide_edit_selection){99,98};CHECK(!xideEditSelectionMap(p,0,(xide_edit_selection){0,10},&selection) && selection.anchor==99 && selection.caret==98);size_t value=999;CHECK(!xideEditPositionMap(p,1,0,XIDE_EDIT_LEFT,&value) && value==999);CHECK(!xideEditPositionMap(p,0,0,(enum xide_edit_affinity)8,&value) && value==999);CHECK(!xideEditPositionMap(NULL,0,0,XIDE_EDIT_LEFT,&value));CHECK(!xideEditPositionMap(p,0,0,XIDE_EDIT_LEFT,NULL));xlsTextRelease(original);xideEditPositionsDestroy(p);
 edits=xrtValueArray();CHECK(edits && xrtValueArrayAppendNew(edits,textEdit(0,1,1,"X")) && xrtValueArrayAppendNew(edits,textEdit(0,1,1,"Y")) && xrtValueArrayAppendNew(edits,textEdit(0,1,3,"Q")) && xrtValueArrayAppendNew(edits,textEdit(0,3,3,"Z")));plan=prepare("abcd",edits);p=mapped(plan);at(p,0,0,0);at(p,1,1,4);at(p,2,3,4);at(p,3,4,5);at(p,4,6,6);CHECK(xideEditSelectionMap(p,0,(xide_edit_selection){1,3},&selection) && selection.anchor==1 && selection.caret==5);CHECK(xideEditSelectionMap(p,0,(xide_edit_selection){3,1},&selection) && selection.anchor==5 && selection.caret==1);xideEditPositionsDestroy(p);xideWorkspaceEditRelease(plan);
 edits=xrtValueArray();CHECK(edits && xrtValueArrayAppendNew(edits,textEdit(0,1,2,"")) && xrtValueArrayAppendNew(edits,textEdit(0,2,3,"")));plan=prepare("abcd",edits);p=mapped(plan);at(p,1,1,1);at(p,2,1,1);at(p,3,1,1);at(p,4,2,2);xideEditPositionsDestroy(p);xideWorkspaceEditRelease(plan);
 edits=xrtValueArray();CHECK(edits && xrtValueArrayAppendNew(edits,textEdit(0,0,4,"a😃z")));plan=prepare("a😀z",edits);p=mapped(plan);at(p,0,0,0);at(p,1,1,5);at(p,5,5,5);at(p,6,6,6);for(size_t i=2;i<5;++i){value=999;CHECK(!xideEditPositionMap(p,0,i,XIDE_EDIT_RIGHT,&value) && value==999);}xideEditPositionsDestroy(p);xideWorkspaceEditRelease(plan);
 edits=xrtValueArray();CHECK(edits && xrtValueArrayAppendNew(edits,textEdit(0,0,3,"aĀz")));plan=prepare("aЀz",edits);p=mapped(plan);at(p,1,1,3);at(p,3,3,3);xideEditPositionsDestroy(p);xideWorkspaceEditRelease(plan);
 edits=xrtValueArray();CHECK(edits && xrtValueArrayAppendNew(edits,textEdit(0,0,4,"a😀z")));plan=prepare("a😀z",edits);p=mapped(plan);at(p,0,0,0);at(p,1,1,1);at(p,5,5,5);xideEditPositionsDestroy(p);xideWorkspaceEditRelease(plan);
 edits=xrtValueArray();CHECK(edits && xrtValueArrayAppendNew(edits,textEdit(0,0,0,"\r\n")));plan=prepare("",edits);p=mapped(plan);at(p,0,0,1);xideEditPositionsDestroy(p);xideWorkspaceEditRelease(plan);
 /* A whole-document formatter replacement with only indentation changed. */
 edits=xrtValueArray();xvalue* edit=textEdit(0,0,0,"func main()\n    return 1\nend\n");xvalue* range=xrtValueObjectGet(edit,xrtStrView("range"));CHECK(putEdit(range,"end",editPosition(3,0)) && xrtValueArrayAppendNew(edits,edit));plan=prepare("func main()\n return 1\nend\n",edits);p=mapped(plan);at(p,5,5,5);at(p,13,13,16);at(p,20,23,23);xideEditPositionsDestroy(p);xideWorkspaceEditRelease(plan);
}
static void dense(void)
{
 const unsigned n=12000;xstrbuf before={0};xvalue* edits=xrtValueArray();CHECK(edits);for(unsigned i=0;i<n;++i)CHECK(xrtStrBufAppend(&before,xrtStrView("a ")));
 for(unsigned i=n;i;--i)CHECK(xrtValueArrayAppendNew(edits,textEdit(0,(int)(2*(i-1)),(int)(2*(i-1)+1),"bb")));
 xide_workspace_edit* plan=prepare(before.Data,edits);xide_edit_positions* p=mapped(plan);xideWorkspaceEditRelease(plan);uint64_t blocks=editBlocks();
 for(unsigned i=0;i<n;++i){at(p,2u*i,3u*i,3u*i+2);at(p,2u*i+1,3u*i+2,3u*i+2);}at(p,2u*n,3u*n,3u*n);CHECK(editBlocks()==blocks);xideEditPositionsDestroy(p);xrtStrBufFree(&before);
}
typedef struct read_job{xide_edit_positions* map;int failed;} read_job;
static DWORD WINAPI readMap(void* user)
{read_job* job=user;for(unsigned i=0;i<10000;++i){size_t value;if(!xideEditPositionMap(job->map,0,3,XIDE_EDIT_RIGHT,&value) || value!=5){job->failed=1;break;}}return 0;}
static void lifetime(void)
{
 xvalue* edits=xrtValueArray();CHECK(edits && xrtValueArrayAppendNew(edits,textEdit(0,0,9,"abcXYghi")));xide_workspace_edit* plan=prepare("abcDEFghi",edits);xide_edit_positions* p=mapped(plan);xideWorkspaceEditRelease(plan);HANDLE threads[4];read_job jobs[4];for(unsigned i=0;i<4;++i){jobs[i]=(read_job){p,0};threads[i]=CreateThread(NULL,0,readMap,jobs+i,0,NULL);CHECK(threads[i]);}for(unsigned i=0;i<4;++i){CHECK(WaitForSingleObject(threads[i],30000)==WAIT_OBJECT_0 && !jobs[i].failed);CloseHandle(threads[i]);}xideEditPositionsDestroy(p);
}
int main(void)
{literals();dense();lifetime();printf("workspace_edit_positions_test passed: %u checks; trimmed scalar boundaries, insertion groups, directed/collapsed selections,12000 reverse edits and concurrent retained-plan readers\n",checks);return 0;}
