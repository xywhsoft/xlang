#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "platform/workspace_edit_aliases.h"
#include "platform/workspace_edit_physical_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
enum {ALIAS_MODEL=1,ALIAS_TARGET=2,ALIAS_EXCLUDED=4};
typedef struct alias_row {
 wchar_t* path;HANDLE handle;BY_HANDLE_FILE_INFORMATION info;
 uint64_t model;int roles,directory,missing,follow;
} alias_row;
struct xide_edit_aliases {xarray rows,ancestors;size_t pathBytes;};
static int fail(xide_model_error* e,int code,const char* text)
{if(e){e->code=code;snprintf(e->message,sizeof(e->message),"%s",text);}return 0;}
static int stopped(int(*fn)(void*),void* user,xide_model_error* e)
{return fn&&fn(user)?fail(e,XIDE_MODEL_CONFLICT,"Physical alias policy cancelled."),1:0;}
static int keyOrder(const void* a,const void* b)
{return xidePathNativeKeyCompare(((const alias_row*)a)->path,((const alias_row*)b)->path);}
static int idOrder(const void* a,const void* b)
{uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;return(x>y)-(x<y);}
static int physicalOrder(const void* a,const void* b)
{
 const alias_row *x=a,*y=b;if(x->missing!=y->missing)return x->missing-y->missing;
 if(x->missing)return keyOrder(a,b);
 if(x->info.dwVolumeSerialNumber!=y->info.dwVolumeSerialNumber)return(x->info.dwVolumeSerialNumber>y->info.dwVolumeSerialNumber)-(x->info.dwVolumeSerialNumber<y->info.dwVolumeSerialNumber);
 uint64_t u=((uint64_t)x->info.nFileIndexHigh<<32)|x->info.nFileIndexLow,v=((uint64_t)y->info.nFileIndexHigh<<32)|y->info.nFileIndexLow;return(u>v)-(u<v);
}
size_t xideEditAliasesStorage(const xide_edit_aliases* p)
{return p?sizeof(*p)+p->pathBytes+(p->rows.Capacity+p->ancestors.Capacity)*sizeof(alias_row):0;}
static int room(xide_edit_aliases* p,xide_model_error* e)
{return(p->pathBytes<=8u*1024u*1024u&&xideEditAliasesStorage(p)<=16u*1024u*1024u)||fail(e,XIDE_MODEL_MEMORY,"Physical alias metadata admission exceeded.");}
static void clearRows(xarray* rows)
{for(size_t i=0;i<rows->Count;++i){alias_row* row=xrtArrayGet(rows,i);if(row->handle!=INVALID_HANDLE_VALUE)CloseHandle(row->handle);free(row->path);}xrtArrayUnit(rows);}
void xideEditAliasesDestroy(xide_edit_aliases* p)
{if(p){clearRows(&p->rows);clearRows(&p->ancestors);xrtFree(p);}}
static int append(xide_edit_aliases* p,xarray* rows,wchar_t* path,int roles,int directory,uint64_t model,xide_model_error* e)
{
 if(!path)return fail(e,XIDE_MODEL_MEMORY,"Cannot prepare physical alias path.");
 alias_row row={.path=path,.handle=INVALID_HANDLE_VALUE,.roles=roles,.directory=directory,.model=model,.follow=roles==ALIAS_MODEL};
 if(!xrtArrayPush(rows,&row)){free(path);return fail(e,XIDE_MODEL_MEMORY,"Cannot retain physical alias metadata.");}
 p->pathBytes+=(wcslen(path)+1)*sizeof(wchar_t);return room(p,e);
}
static int beneath(const wchar_t* file,const wchar_t* root,int directory)
{
 size_t n=wcslen(root),length=wcslen(file);if(length<n||CompareStringOrdinal(file,(int)n,root,(int)n,TRUE)!=CSTR_EQUAL)return 0;
 return length==n||(directory&&(root[n-1]==L'\\'||file[n]==L'\\'));
}
static HANDLE openRow(const alias_row* row)
{return CreateFileW(row->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|(row->follow?0:FILE_FLAG_OPEN_REPARSE_POINT),NULL);}
static int absent(DWORD error){return error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND;}
static int pin(alias_row* row,xide_model_error* e)
{
 row->handle=openRow(row);
 if(row->handle==INVALID_HANDLE_VALUE){if(absent(GetLastError())&&!(row->roles&ALIAS_TARGET&&!(row->roles&ALIAS_MODEL))){row->missing=1;return 1;}return fail(e,XIDE_MODEL_CONFLICT,"Cannot inspect/pin physical policy path.");}
 if(!xideEditPhysicalOrdinary(row->handle,row->directory,&row->info)||((row->roles&ALIAS_TARGET)&&row->info.nNumberOfLinks!=1))return fail(e,XIDE_MODEL_CONFLICT,"Physical policy requires an ordinary object and single-link targets.");
 return 1;
}
static int validateRow(alias_row* row,xide_model_error* e)
{
 HANDLE named=openRow(row);DWORD error=named==INVALID_HANDLE_VALUE?GetLastError():ERROR_SUCCESS;BY_HANDLE_FILE_INFORMATION current={0},held={0};int okay;
 if(row->missing)okay=named==INVALID_HANDLE_VALUE&&absent(error);
 else okay=xideEditPhysicalOrdinary(row->handle,row->directory,&held)&&xideEditPhysicalSame(&held,&row->info)&&xideEditPhysicalOrdinary(named,row->directory,&current)&&xideEditPhysicalSame(&held,&current)&&(!(row->roles&ALIAS_TARGET)||current.nNumberOfLinks==1);
 if(named!=INVALID_HANDLE_VALUE)CloseHandle(named);
 return okay||fail(e,XIDE_MODEL_CONFLICT,"Physical policy name, identity, absence or target links changed.");
}
int xideEditAliasesValidate(xide_edit_aliases* p,int(*cancel)(void*),void* user,xide_model_error* e)
{
 if(e){*e=(xide_model_error){0};}if(!p)return fail(e,XIDE_MODEL_INVALID,"Missing physical alias policy.");
 for(size_t i=0;i<p->ancestors.Count;++i)if(stopped(cancel,user,e)||!validateRow(xrtArrayGet(&p->ancestors,i),e))return 0;
 for(size_t i=0;i<p->rows.Count;++i)if(stopped(cancel,user,e)||!validateRow(xrtArrayGet(&p->rows,i),e))return 0;
 return !stopped(cancel,user,e);
}
xide_edit_aliases* xideEditAliasesPin(const xide_workspace_edit* plan,const xide_edit_alias_model* models,size_t count,const xide_edit_alias_exclusion* excluded,size_t exclusions,int(*cancel)(void*),void* user,xide_model_error* e)
{
 if(e){*e=(xide_model_error){0};}xide_workspace_edit_info info={0};
 if(!xideWorkspaceEditInfo(plan,&info)||info.documents>512||count>4096||exclusions>256||(count&&!models)||(exclusions&&!excluded)){fail(e,XIDE_MODEL_INVALID,"Invalid complete alias roster or exclusions.");return NULL;}
 xide_edit_aliases* p=xrtCalloc(1,sizeof(*p));if(!p){fail(e,XIDE_MODEL_MEMORY,"Cannot allocate physical alias policy.");return NULL;}
 int okay=xrtArrayInit(&p->rows,sizeof(alias_row))&&xrtArrayInit(&p->ancestors,sizeof(alias_row));uint64_t ids[4096];
 for(size_t i=0;okay&&i<count;++i){
  if(stopped(cancel,user,e)){okay=0;break;}
  if(!models[i].path||!*models[i].path||strlen(models[i].path)>65535||!models[i].identity){okay=fail(e,XIDE_MODEL_INVALID,"Named models require bounded paths and nonzero identities.");break;}
  ids[i]=models[i].identity;okay=append(p,&p->rows,xidePathNativeWide(models[i].path),ALIAS_MODEL,0,models[i].identity,e);
 }
 if(okay&&count){qsort(ids,count,sizeof(uint64_t),idOrder);qsort(p->rows.Data,count,sizeof(alias_row),keyOrder);for(size_t i=1;i<count;++i)if(ids[i]==ids[i-1]||!keyOrder(xrtArrayGet(&p->rows,i-1),xrtArrayGet(&p->rows,i))){okay=fail(e,XIDE_MODEL_CONFLICT,"Duplicate model identities or lexical names.");break;}}
 for(size_t i=0;okay&&i<info.documents;++i){
  if(stopped(cancel,user,e)){okay=0;break;}xide_workspace_edit_document d={0};if(!xideWorkspaceEditDocumentAt(plan,i,&d)){okay=0;break;}wchar_t* key=xidePathNativeWide(d.path);
  if(!key||!xideEditPhysicalLocalPath(key)){free(key);okay=fail(e,XIDE_MODEL_INVALID,"Edit target must have an ordinary local fixed-drive spelling.");break;}
  size_t low=0,high=count;while(low<high){size_t mid=low+(high-low)/2;if(xidePathNativeKeyCompare(((alias_row*)p->rows.Data)[mid].path,key)<0)low=mid+1;else high=mid;}
  alias_row* model=low<count&&!xidePathNativeKeyCompare(((alias_row*)p->rows.Data)[low].path,key)?xrtArrayGet(&p->rows,low):NULL;
  if(d.hasVersion){if(!model||!d.identity||!d.revision||d.identity!=model->model||(model->roles&ALIAS_TARGET)){free(key);okay=fail(e,XIDE_MODEL_CONFLICT,"Overlay target does not identify its captured named model.");break;}model->roles|=ALIAS_TARGET;model->follow=0;}
  else if(model){free(key);okay=fail(e,XIDE_MODEL_CONFLICT,"Closed target is already a named open model.");break;}
  else if(!append(p,&p->rows,key,ALIAS_TARGET,0,0,e)){okay=0;break;}else key=NULL;
  const wchar_t* target=key?key:((alias_row*)p->rows.Data)[p->rows.Count-1].path;size_t n=wcslen(target);
  for(size_t j=6;okay&&j<n;++j)if(target[j]==L'\\'){
   if(stopped(cancel,user,e)){okay=0;break;}size_t length=j==6?7:j;
   if(p->ancestors.Count>=32768){okay=fail(e,XIDE_MODEL_MEMORY,"Physical policy ancestor candidate limit exceeded.");break;}
   wchar_t* ancestor=calloc(length+1,sizeof(wchar_t));if(ancestor)memcpy(ancestor,target,length*sizeof(wchar_t));okay=append(p,&p->ancestors,ancestor,0,1,0,e);
  }
  free(key);
 }
 for(size_t i=0;okay&&i<exclusions;++i){
  if(stopped(cancel,user,e)){okay=0;break;}
  if(!excluded[i].path||!*excluded[i].path||strlen(excluded[i].path)>65535||(excluded[i].directory!=0&&excluded[i].directory!=1)){okay=fail(e,XIDE_MODEL_INVALID,"Invalid protected path metadata.");break;}
  okay=append(p,&p->rows,xidePathNativeWide(excluded[i].path),ALIAS_EXCLUDED,excluded[i].directory,0,e);
 }
 if(okay&&p->ancestors.Count){
  qsort(p->ancestors.Data,p->ancestors.Count,sizeof(alias_row),keyOrder);size_t unique=0;
  for(size_t i=0;i<p->ancestors.Count;++i){alias_row* row=xrtArrayGet(&p->ancestors,i);if(unique&&!keyOrder(xrtArrayGet(&p->ancestors,unique-1),row)){p->pathBytes-=(wcslen(row->path)+1)*sizeof(wchar_t);free(row->path);continue;}if(unique!=i)*(alias_row*)xrtArrayGet(&p->ancestors,unique)=*row;++unique;}
  p->ancestors.Count=unique;if(unique>4096)okay=fail(e,XIDE_MODEL_MEMORY,"Physical policy ancestor handle limit exceeded.");
 }
 for(size_t i=0;okay&&i<p->rows.Count;++i){alias_row* row=xrtArrayGet(&p->rows,i);if(stopped(cancel,user,e)||!pin(row,e)){okay=0;break;}}
 for(size_t i=0;okay&&i<p->ancestors.Count;++i){alias_row* row=xrtArrayGet(&p->ancestors,i);if(stopped(cancel,user,e)||!pin(row,e)){okay=0;break;}}
 /* Check exclusions before the physical sort; no body read/hash is needed. */
 for(size_t i=0;okay&&i<p->rows.Count;++i){const alias_row* root=xrtArrayConstGet(&p->rows,i);if(!(root->roles&ALIAS_EXCLUDED))continue;
  for(size_t j=0;okay&&j<p->rows.Count;++j){const alias_row* target=xrtArrayConstGet(&p->rows,j);if((target->roles&ALIAS_TARGET)&&beneath(target->path,root->path,root->directory))okay=fail(e,XIDE_MODEL_CONFLICT,"Edit target is a protected path or descendant.");}
  if(root->directory&&!root->missing)for(size_t j=0;okay&&j<p->ancestors.Count;++j){const alias_row* ancestor=xrtArrayConstGet(&p->ancestors,j);if(!ancestor->missing&&xideEditPhysicalSame(&ancestor->info,&root->info))okay=fail(e,XIDE_MODEL_CONFLICT,"Target ancestor physically identifies a protected root.");}
  if(stopped(cancel,user,e))okay=0;
 }
 if(okay&&p->rows.Count){qsort(p->rows.Data,p->rows.Count,sizeof(alias_row),physicalOrder);for(size_t at=0;okay&&at<p->rows.Count;){alias_row* row=xrtArrayGet(&p->rows,at);size_t end=at+1;int target=row->roles&ALIAS_TARGET;while(end<p->rows.Count&&!row->missing&&!physicalOrder(row,xrtArrayGet(&p->rows,end))){target|=((alias_row*)p->rows.Data)[end].roles&ALIAS_TARGET;++end;}if(target&&end-at>1)okay=fail(e,XIDE_MODEL_CONFLICT,"An edit target physically aliases another target, model or protected file.");at=end;if(stopped(cancel,user,e))okay=0;}}
 if(okay)okay=xideEditAliasesValidate(p,cancel,user,e);
 if(!okay){if(e&&!e->code)fail(e,XIDE_MODEL_MEMORY,"Cannot construct complete physical alias policy.");xideEditAliasesDestroy(p);return NULL;}return p;
}
