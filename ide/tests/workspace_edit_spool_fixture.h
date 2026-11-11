#ifndef XIDE_WORKSPACE_EDIT_SPOOL_FIXTURE_H
#define XIDE_WORKSPACE_EDIT_SPOOL_FIXTURE_H
#define main diskFixtureMain
#include "workspace_edit_files_test.c"
#undef main
#include "platform/workspace_edit_spool.h"
#include "platform/file_io.h"
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"edit-spool:%d: %s\n",__LINE__,#v);exit(1);}}while(0)
static xvalue* field(const xvalue* v,const char* key){return xrtValueObjectGet(v,xrtStrView(key));}
static const char* asText(const xvalue* v){xstrview s={0};CHECK(xrtValueGetString(v,&s)&&!memchr(s.Data,0,s.Size));return s.Data;}
static int asIndex(const xvalue* v){int64 n=0;CHECK(xrtValueGetInt(v,&n)&&n>=0&&n<2048);return(int)n;}
static int diskProof(xvalue* row,const char* path,const char* raw,const char* normal)
{
 char a[65],b[65];hashBytes(raw,strlen(raw),a);hashBytes(normal,strlen(normal),b);xvalue *extension=xrtValueObject(),*document=xrtValueObject(),*input=xrtValueObject();
 int okay=extension&&document&&input&&putEdit(document,"path",xrtValueString(xrtStrView(path)))&&putEdit(document,"origin",xrtValueString(xrtStrView("disk")))&&putEdit(document,"normalizedSize",xrtValueInt((int64)strlen(normal)))&&putEdit(document,"normalizedSha256",xrtValueString(xrtStrView(b)))&&putEdit(input,"path",xrtValueString(xrtStrView(path)))&&putEdit(input,"origin",xrtValueString(xrtStrView("disk")))&&putEdit(input,"rawSize",xrtValueInt((int64)strlen(raw)))&&putEdit(input,"rawSha256",xrtValueString(xrtStrView(a)));
 if(okay){okay=putEdit(document,"input",input);input=NULL;}if(okay){okay=putEdit(extension,"document",document);document=NULL;}if(okay)okay=putEdit(extension,"schema",xrtValueInt(1));if(okay){okay=putEdit(row,"xlang",extension);extension=NULL;}xrtValueRelease(extension);xrtValueRelease(document);xrtValueRelease(input);return okay;
}
typedef struct data {char *paths[3],*uris[3];xide_workspace_edit* plan;xide_text_file baseline;xide_edit_spool_baseline view;} data;
static const char *savedRaw[3]={"\xef\xbb\xbf" "a\r\nb\r\n","int n=0;\r\n","same\r\nlast\n"},*before[3]={"a\nb\n","int n=1;\n","same\nlast\n"},*after[3]={"new\nb\n","int n=2;\n","same\nlast\n"},*encoded[3]={"\xef\xbb\xbf" "new\r\nb\r\n","int n=2;\r\n","same\r\nlast\n"};
static void init(data* d,const char* directory)
{
 memset(d,0,sizeof(*d));CHECK(xrtDirCreateAll(directory));xvalue *root=xrtValueObject(),*docs=xrtValueArray(),*annotations=xrtValueObject(),*annotation=xrtValueObject();CHECK(root&&docs&&annotations&&annotation);xide_workspace_edit_input inputs[3]={0};
 for(unsigned i=0;i<3;++i){char name[64];snprintf(name,sizeof(name),"spool-source-%u.%s",i,i==1?"c":"xl");d->paths[i]=xidePathResolveLexical(directory,name);d->uris[i]=xideLspPathToUri(d->paths[i],NULL);CHECK(d->paths[i]&&d->uris[i]&&writeSource(d->paths[i],savedRaw[i]));inputs[i]=(xide_workspace_edit_input){d->uris[i],xlsTextCreate(before[i],strlen(before[i]),1024,NULL),i==1?17:0,23,i==1,9};CHECK(inputs[i].text);xvalue* edits=xrtValueArray();CHECK(edits);if(i<2){xvalue* edit=textEdit(0,i?6:0,i?7:1,i?"2":"new");CHECK(edit&&putEdit(edit,"annotationId",xrtValueString(xrtStrView("rename")))&&xrtValueArrayAppendNew(edits,edit));}xvalue* row=documentEdit(d->uris[i],i==1,9,edits);CHECK(row&&(i==1||diskProof(row,d->paths[i],savedRaw[i],before[i]))&&xrtValueArrayAppendNew(docs,row));}
 CHECK(putEdit(annotation,"label",xrtValueString(xrtStrView("Symbol change")))&&putEdit(annotation,"description",xrtValueString(xrtStrView("Review the complete group")))&&putEdit(annotation,"needsConfirmation",xrtValueBool(true))&&putEdit(annotations,"rename",annotation)&&putEdit(root,"changeAnnotations",annotations)&&putEdit(root,"documentChanges",docs));enum xide_workspace_edit_error error;d->plan=xideWorkspaceEditPrepare(root,inputs,3,0,0,NULL,NULL,&error);CHECK(d->plan&&!error);for(unsigned i=0;i<3;++i)xlsTextRelease(inputs[i].text);xrtValueRelease(root);xide_model_error model={0};CHECK(xideTextFileDecode((const unsigned char*)savedRaw[1],strlen(savedRaw[1]),&d->baseline,&model));d->view=(xide_edit_spool_baseline){1,&d->baseline};
}
static void freeData(data* d){xideWorkspaceEditRelease(d->plan);xideTextFileClear(&d->baseline);for(unsigned i=0;i<3;++i){free(d->paths[i]);free(d->uris[i]);}}
static void bodyEquals(xide_edit_spool* p,const xvalue* row,const char* key,const char* expected)
{size_t size=0;xide_model_error e={0};unsigned char* body=xideEditSpoolReadBody(p,(size_t)asIndex(field(row,key)),&size,NULL,NULL,&e);CHECK(body&&!e.code&&size==strlen(expected)&&!memcmp(body,expected,size)&&body[size]==0);xrtFree(body);}
static void inspect(xide_edit_spool* p,int fresh)
{
 const xvalue* root=xideEditSpoolManifest(p);CHECK(root&&!strcmp(asText(field(root,"format")),"xlang-ide-edit-spool-1")&&!strcmp(asText(field(root,"state")),"prepared"));bool approved=true;CHECK(xrtValueGetBool(field(root,"approved"),&approved)&&!approved);const xvalue *docs=field(root,"documents"),*bodies=field(root,"bodies"),*annotations=field(root,"annotations");CHECK(xrtValueCount(docs)==3&&xrtValueCount(bodies)==10&&xrtValueCount(annotations)==1);CHECK(xideEditSpoolSources(p)?fresh&&xideWorkspaceEditFilesCount(xideEditSpoolSources(p))==2:!fresh);
 for(unsigned i=0;i<3;++i){const xvalue* row=xrtValueArrayGet(docs,i);bodyEquals(p,row,"baseline",savedRaw[i]);bodyEquals(p,row,"before",before[i]);bodyEquals(p,row,"after",after[i]);bodyEquals(p,row,"encoded",encoded[i]);if(i==2)CHECK(asIndex(field(row,"baseline"))==asIndex(field(row,"encoded"))&&asIndex(field(row,"before"))==asIndex(field(row,"after"))&&asIndex(field(row,"before"))!=asIndex(field(row,"baseline")));}
 const xvalue* a=xrtValueArrayGet(annotations,0);CHECK(xrtValueGetBool(field(a,"needsConfirmation"),&approved)&&approved&&!strcmp(asText(field(a,"id")),"rename"));
}
static void diskEquals(const char* path,const char* bytes)
{size_t size=0;unsigned char* read=xideIoReadAllLimit(path,1024,&size);CHECK(read&&size==strlen(bytes)&&!memcmp(read,bytes,size));xrtFree(read);}
static void warmServices(const char* directory)
{
 /* Native XRT retains its dynamically loaded CNG provider. A separate control
  * using only GUID/atomic-write/read, without Spool, observed startup handles
  * once and stable counts for four subsequent rounds. Initialize those APIs
  * before comparing this component's transaction handles. */
 char* id=xideCreateId("warm");char* path=xidePathResolveLexical(directory,"spool-io-warm.tmp");CHECK(id&&path);free(id);unsigned char random[8];CHECK(xrtSecureRandom(random,sizeof(random)));CHECK(xideIoWriteAtomic(path,(xbytesview){(const unsigned char*)"warm",4}));diskEquals(path,"warm");CHECK(removeSource(path));free(path);
}

#endif
