#include "protocol/workspace_symbol_wire.h"
#include "protocol/lsp_wire.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define WIRE_LIMIT (XIDE_LSP_BODY_BYTES-1024u*1024u)
static xstrview sv(const char* p){return (xstrview){p,strlen(p)};}
static int put(xvalue* o,const char* key,xvalue* v){return v && xrtValueObjectSetNew(o,sv(key),v);}
static xvalue* position(xide_lsp_position p)
{xvalue* v=xrtValueObject();if(v && put(v,"line",xrtValueInt(p.line)) && put(v,"character",xrtValueInt(p.character)))return v;xrtValueRelease(v);return NULL;}
static int supported(uint32_t mask,int kind){return kind>=1 && kind<=26 && (mask&(UINT32_C(1)<<(kind-1)));}
static int mappedKind(uint32_t mask,int kind)
{
 if(supported(mask,kind))return kind;
 int fallback=kind==6 || kind==9 || kind==12 ? 12 : kind==5 || kind==10 || kind==11 || kind==22 ? 5 : 13;
 if(supported(mask,fallback))return fallback;
 for(int i=1;i<=26;++i)if(supported(mask,i))return i;
 return 1;
}
typedef struct wire_count {size_t bytes;int (*cancelled)(void*);void* user;} wire_count;
static bool countBytes(xbytesview part,void* p)
{wire_count* count=p;if((count->cancelled && count->cancelled(count->user)) || part.Size>WIRE_LIMIT-count->bytes)return false;count->bytes+=part.Size;return true;}
xvalue* xideWorkspaceSymbolWire(const xide_workspace_symbol_result* result,uint32_t kindMask,int (*cancelled)(void*),void* user,xide_model_error* error)
{
 if(error)*error=(xide_model_error){0};
 xvalue* list=result?xrtValueArray():NULL;if(!list)goto failed;
 kindMask&=UINT32_C(0x3ffffff);if(!kindMask)kindMask=UINT32_C(0x3ffff);
 wire_count count={.bytes=2,.cancelled=cancelled,.user=user};xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);config.MaxOutputBytes=WIRE_LIMIT;
 for(size_t i=0;i<xideWorkspaceSymbolResultCount(result);++i){
  if(cancelled && cancelled(user))goto failed;
  xide_workspace_symbol_view symbol;if(!xideWorkspaceSymbolResultAt(result,i,&symbol))goto failed;
  const xide_semantic_symbol_view* declaration=&symbol.declaration;char* uri=xideLspPathToUri(declaration->path,NULL);xvalue *item=xrtValueObject(),*location=xrtValueObject(),*range=xrtValueObject();
  int okay=uri && item && location && range && put(item,"name",xrtValueString(sv(declaration->name))) && put(item,"kind",xrtValueInt(mappedKind(kindMask,declaration->kind))) && put(location,"uri",xrtValueString(sv(uri))) && put(range,"start",position(declaration->selectionStart)) && put(range,"end",position(declaration->selectionEnd));
  if(okay){okay=put(location,"range",range);range=NULL;}if(okay){okay=put(item,"location",location);location=NULL;}
  size_t qualified=strlen(symbol.qualified),leaf=strlen(declaration->name);
  if(okay && qualified>leaf+1)okay=put(item,"containerName",xrtValueString((xstrview){symbol.qualified,qualified-leaf-1}));
  if(okay && i){if(count.bytes==WIRE_LIMIT)okay=0;else ++count.bytes;}
  if(okay)okay=xrtJsonWrite(item,&config,countBytes,&count);
  if(okay){okay=xrtValueArrayAppendNew(list,item);item=NULL;}xrtValueRelease(item);xrtValueRelease(location);xrtValueRelease(range);free(uri);if(!okay)goto failed;
 }
 if(cancelled && cancelled(user))goto failed;
 return list;
failed:
 xrtValueRelease(list);if(error){error->code=cancelled && cancelled(user)?XIDE_MODEL_CONFLICT:XIDE_MODEL_MEMORY;snprintf(error->message,sizeof(error->message),"Workspace symbol response invalid,cancelled or exceeds its7MiB allocation/wire budget.");}return NULL;
}
