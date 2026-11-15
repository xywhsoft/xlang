#include "services/analysis_configuration.h"
#include "platform/path.h"
#include <stdio.h>
#include <string.h>
static xstrview sv(const char* s) {return (xstrview){s,strlen(s)};}
static int put(xvalue* root,const char* key,xvalue* owned) {return owned && xrtValueObjectSetNew(root,sv(key),owned);}
xvalue* xideAnalysisConfigurationPacket(const xide_workspace* workspace,const char* selected,const char* configuration,
 const char* package,int64 epoch,xide_model_error* error)
{
 if(error) memset(error,0,sizeof(*error));
 if(!workspace || epoch<=0 || epoch==INT64_MAX || (workspace->projectCount && !workspace->projects) ||
    (workspace->files.count && !workspace->files.items)) return NULL;
 xvalue *packet=xrtValueObject(),*catalog=xrtValueObject(),*projects=xrtValueArray(),*roots=xrtValueArray();int okay=packet && catalog && projects && roots,complete=1;
 const char* selectedManifest="";size_t count=0,bytes=0,bindings=0,labels=0;
 for(size_t i=0;okay && i<workspace->projectCount;++i) {
  const xide_project* project=&workspace->projects[i];
  if(!project->available || !project->entry || !xidePathHasExtension(project->entry,".xl")) {
   if(!project->available || !project->entry)complete=0;
   if(selected && project->id && !strcmp(selected,project->id) && !project->available) {if(error) {error->code=XIDE_MODEL_INVALID;snprintf(error->message,sizeof(error->message),"Selected project is unavailable for analysis.");}okay=0;}
   continue;
  }
  xls_context* context=xideAnalysisProjectContext(project,configuration,XIDE_ANALYSIS_PROFILE,package,error);if(!context) {okay=0;break;}
  if(count==64 || project->files.count>=65536-bindings || xlsContextBytes(context)>16u*1024u*1024u-bytes) {xlsContextRelease(context);okay=0;break;}
  ++count;bindings+=1+project->files.count;bytes+=xlsContextBytes(context);
  if(selected && project->id && !strcmp(selected,project->id)) selectedManifest=project->manifest;
  xvalue* wire=xideAnalysisContextWrite(context);xlsContextRelease(context);okay=wire && xrtValueArrayAppend(projects,wire);xrtValueRelease(wire);
 }
 /* The owner already holds decoded file references. Send metadata; the server
    unions these with its prepared project index without another GUI-side index. */
 for(size_t i=0;okay && i<workspace->files.count;++i){
  const char* path=workspace->files.items[i].path;if(!path || !xidePathHasExtension(path,".xl"))continue;
  size_t size=strlen(path)+1;
  if(!xrtPathIsAbs(path) || size>32768 || size>16u*1024u*1024u-labels || xrtValueCount(roots)==65536){okay=0;break;}
  labels+=size;xvalue* item=xrtValueString(sv(path));okay=item && xrtValueArrayAppend(roots,item);xrtValueRelease(item);
 }
 int extended=!complete || xrtValueCount(roots)>0;
 if(okay) okay=put(catalog,"schema",xrtValueInt(extended ? 2 : 1)) && put(catalog,"selectedProject",xrtValueString(sv(selectedManifest))) &&
  put(catalog,"packagePath",package ? xrtValueString(sv(package)) : xrtValueNull());
 if(okay && extended){okay=put(catalog,"complete",xrtValueBool(complete)) && put(catalog,"standaloneRoots",roots);roots=NULL;}
 if(okay) {okay=put(catalog,"projects",projects);projects=NULL;}
 if(okay) okay=put(packet,"epoch",xrtValueInt(epoch));
 if(okay) {okay=put(packet,"catalog",catalog);catalog=NULL;}
 xrtValueRelease(roots);xrtValueRelease(projects);xrtValueRelease(catalog);
 if(!okay) {xrtValueRelease(packet);packet=NULL;if(error && !error->code) {error->code=XIDE_MODEL_INVALID;snprintf(error->message,sizeof(error->message),"Analysis configuration capacity or allocation failed.");}}
 return packet;
}
