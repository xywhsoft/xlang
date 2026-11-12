#include "editor/documents.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"revision:%d: %s\n",__LINE__,#e);failed=1;goto cleanup;}}while(0)
int main(int argc,char** argv)
{
 int failed=0;xui_context context=NULL;xide_document_store store={0};xide_model_error error={0};xide_text_file file={0};char *path=NULL,*other=NULL;
 CHECK(argc==2 && xrtDirCreateAll(argv[1]));path=xidePathResolveLexical(argv[1],"revision.xl");other=xidePathResolveLexical(argv[1],"revision.c");CHECK(path && other);
 CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)"// base\n",8}) && xrtFileWriteAtomic(other,(xbytesview){(const unsigned char*)"int n;\n",7}));
 CHECK(xuiCreate(&context)==XUI_OK);xideDocumentsInit(&store,context,NULL,NULL,NULL);
 xide_document* d=xideDocumentOpen(&store,path,&error);xide_document* c=xideDocumentOpen(&store,other,&error);CHECK(d && c && d->revision==1 && c->revision==1 && d->id!=c->id);
 xui_code_document core=xuiCodeEditGetDocument(d->editor),ccore=xuiCodeEditGetDocument(c->editor);
 CHECK(xuiCodeDocumentInsert(core,0,"// 中文😀\n")==XUI_OK && d->revision==2 && d->dirty);
 CHECK(xuiCodeDocumentUndo(core)==XUI_OK && d->revision==3 && !d->dirty);
 CHECK(xuiCodeDocumentRedo(core)==XUI_OK && d->revision==4 && d->dirty);
 CHECK(xuiCodeDocumentInsert(ccore,0,"/* c */")==XUI_OK && c->revision==2);
 uint64_t revision=d->revision;CHECK(xideDocumentSave(d,NULL,0,&error) && d->revision==revision && !d->dirty);
 CHECK(xideDocumentSetLanguage(d,"c",&error) && xideDocumentSetLanguage(d,"xlang",&error) && d->revision==revision);
 const char* reload="// external\n";CHECK(xideTextFileDecode((const unsigned char*)reload,strlen(reload),&file,&error));
 CHECK(xideDocumentAcceptExternal(d,&file,0,xideDocumentsSourceEpoch((d)->store),&error) && d->revision==revision+1 && !d->dirty);revision=d->revision;
 CHECK(xideTextFileDecode((const unsigned char*)reload,strlen(reload),&file,&error));
 CHECK(xideDocumentAcceptExternal(d,&file,0,xideDocumentsSourceEpoch((d)->store),&error) && d->revision==revision);
 d->suppressChange=1;int status=xuiCodeDocumentInsert(core,0,"// suppressed\n");d->suppressChange=0;
 CHECK(status==XUI_OK && d->revision==revision+1 && !d->dirty);
 d->revision=UINT32_MAX;CHECK(xuiCodeDocumentInsert(core,0,"// high\n")==XUI_OK && d->revision==UINT64_C(4294967296));
 CHECK(xuiCodeDocumentUndo(core)==XUI_OK && d->revision==UINT64_C(4294967297));
 d->revision=UINT64_MAX;CHECK(xuiCodeDocumentInsert(core,0,"// exhausted\n")==XUI_OK && !d->revision);
 CHECK(xuiCodeDocumentUndo(core)==XUI_OK && !d->revision && xuiCodeDocumentRedo(core)==XUI_OK && !d->revision);
 CHECK(xideDocumentSave(d,NULL,XIDE_DOCUMENT_OVERWRITE_EXTERNAL,&error) && !d->revision);
 CHECK(xideTextFileDecode((const unsigned char*)reload,strlen(reload),&file,&error) && xideDocumentAcceptExternal(d,&file,0,xideDocumentsSourceEpoch((d)->store),&error) && !d->revision);
 uint64_t id=d->id;CHECK(xideDocumentClose(d,1,&error) && !d->revision);
 CHECK(xideDocumentOpen(&store,path,&error)==d && d->id!=id && d->revision==1);
 CHECK(xideDocumentClose(c,1,&error));size_t count=store.count;uint64_t clock=store.nextId;store.nextId=UINT64_MAX;
 CHECK(!xideDocumentNew(&store,"text",&error) && error.code==XIDE_MODEL_CONFLICT && store.count==count && store.nextId==UINT64_MAX);
 CHECK(xideDocumentOpen(&store,path,&error)==d && d->revision==1 && d->open);store.nextId=clock;
 CHECK(xideDocumentNew(&store,"text",&error)==c && c->revision==1 && c->id>d->id);
 printf("document_revision_test passed: %d checks; real C/xlang edits/undo, suppressed reload,64bit clock, exhaustion and slot reuse\n",checks);
cleanup:
 xideTextFileClear(&file);xideDocumentsClear(&store);if(context)xuiDestroy(context);free(path);free(other);return failed;
}
