#include "workspace_edit_fixture.h"
static int edgeCases(void)
{
 xls_text* text=xlsTextCreate("abc",3,1024,NULL);CHECK(text);xide_workspace_edit_input input={"file:///D:/edit/edge.xl",text,1,1,1,0};const char* replacements[]={"","X","😀"};
 for(int a=0;a<=3;++a)for(int b=a;b<=3;++b)for(size_t k=0;k<3;++k){
  xvalue *value=xrtValueObject(),*docs=xrtValueArray(),*edits=xrtValueArray();CHECK(value && docs && edits && xrtValueArrayAppendNew(edits,textEdit(0,a,b,replacements[k])) && xrtValueArrayAppendNew(docs,documentEdit(input.uri,1,0,edits)) && putEdit(value,"documentChanges",docs));
  char expected[32];size_t size=strlen(replacements[k]);memcpy(expected,"abc",(size_t)a);memcpy(expected+a,replacements[k],size);memcpy(expected+a+size,"abc"+b,(size_t)(3-b));expected[a+size+(3-b)]=0;
  enum xide_workspace_edit_error why;xide_workspace_edit* p=xideWorkspaceEditPrepare(value,&input,1,0,0,NULL,NULL,&why);CHECK(p && !why);xide_workspace_edit_document d;CHECK(xideWorkspaceEditDocumentAt(p,0,&d) && !strcmp(xlsTextData(d.after),expected) && !strcmp(xlsTextData(text),"abc"));xideWorkspaceEditRelease(p);xrtValueRelease(value);
 }
 xvalue* value=oneEdit(input.uri,1,0,"x");CHECK(value);xvalue* docs=xrtValueObjectGet(value,xrtStrView("documentChanges"));xvalue* row=xrtValueArrayGet(docs,0);xvalue* edits=xrtValueObjectGet(row,xrtStrView("edits"));xvalue* change=xrtValueArrayGet(edits,0);const char nul[]={'x',0,'y'};CHECK(putEdit(change,"newText",xrtValueString((xstrview){nul,3})));enum xide_workspace_edit_error why;CHECK(!xideWorkspaceEditPrepare(value,&input,1,0,0,NULL,NULL,&why) && why==XIDE_WORKSPACE_EDIT_SHAPE);xrtValueRelease(value);
 value=parseEdit("{\"documentChanges\":[{\"textDocument\":{\"uri\":\"file:///D:/edit/edge.xl\",\"version\":0},\"edits\":[]}],\"changeAnnotations\":{\"empty-label\":{\"label\":\"\",\"description\":\"\"}}}");CHECK(value);xide_workspace_edit* p=xideWorkspaceEditPrepare(value,&input,1,0,0,NULL,NULL,&why);CHECK(p && !why);xide_workspace_edit_annotation note;CHECK(xideWorkspaceEditAnnotationAt(p,0,&note) && note.label.Size==0 && note.description.Size==0);xideWorkspaceEditRelease(p);xrtValueRelease(value);xlsTextRelease(text);
 printf("workspace_edit_edge_test passed: %u checks; every ASCII endpoint pair with empty/delete/insert/Unicode replacements, empty labels and embedded-NUL rejection\n",checks);return 1;
}
int main(void){return edgeCases()?0:1;}
