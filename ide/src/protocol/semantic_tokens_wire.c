#include "protocol/semantic_tokens_wire.h"
#include <string.h>
#include <limits.h>
#define TOKEN_WORDS ((7u*1024u*1024u-4096u)/11u)
struct xide_token_data {xatomic32 references;size_t count,capacity;uint32* words;char id[64];};
static const char* tokenNames[XLS_TOKEN_KIND_COUNT]={NULL,"type","class","interface","enum","struct","typeParameter","parameter","variable","property","enumMember","function","method"};
static const char* modifierNames[3]={"declaration","readonly","async"};
static xstrview sv(const char* s){return (xstrview){s,strlen(s)};}
static const xvalue* get(const xvalue* o,const char* key){return o && xrtValueType(o)==XVALUE_OBJECT?xrtValueObjectGet(o,sv(key)):NULL;}
static int put(xvalue* o,const char* key,xvalue* value){return value && xrtValueObjectSetNew(o,sv(key),value);}
static int list(const xvalue* array)
{
 if(!array || xrtValueType(array)!=XVALUE_ARRAY || xrtValueCount(array)>1024)return 0;
 for(size_t i=0;i<xrtValueCount(array);++i){xstrview text={0};if(!xrtValueGetString(xrtValueArrayGet(array,i),&text) || !text.Size || text.Size>65535 || memchr(text.Data,0,text.Size) || !xrtUtf8Valid(text,NULL))return 0;}
 return 1;
}
static int contains(const xvalue* array,const char* value)
{
 for(size_t i=0;i<xrtValueCount(array);++i){xstrview text={0};if(xrtValueGetString(xrtValueArrayGet(array,i),&text) && text.Size==strlen(value) && !memcmp(text.Data,value,text.Size))return 1;}return 0;
}
static int requestOption(const xvalue* value,int* enabled,int* delta)
{
 *enabled=0;if(!value)return 1;bool yes=false;
 if(xrtValueGetBool(value,&yes)){*enabled=yes;return 1;}
 if(xrtValueType(value)!=XVALUE_OBJECT)return 0;
 *enabled=1;
 if(delta){const xvalue* d=get(value,"delta");if(d && !xrtValueGetBool(d,&yes))return 0;*delta=d && yes;}
 return 1;
}
int xideTokenLegendParse(const xvalue* capabilities,xide_token_legend* out)
{
 if(!out)return 0;
 xide_token_legend legend={0};for(unsigned i=0;i<XLS_TOKEN_KIND_COUNT;++i)legend.types[i]=-1;
 const xvalue* value=get(get(capabilities,"textDocument"),"semanticTokens");
 if(!value){*out=legend;return 1;}
 if(xrtValueType(value)!=XVALUE_OBJECT)return 0;
 const xvalue* booleans[]={get(value,"dynamicRegistration"),get(value,"overlappingTokenSupport"),get(value,"multilineTokenSupport"),get(value,"serverCancelSupport"),get(value,"augmentsSyntaxTokens")};
 for(size_t i=0;i<5;++i){bool flag=false;if(booleans[i] && !xrtValueGetBool(booleans[i],&flag))return 0;}
 const xvalue* requests=get(value,"requests"),*types=get(value,"tokenTypes"),*modifiers=get(value,"tokenModifiers"),*formats=get(value,"formats");
 if(!requests || xrtValueType(requests)!=XVALUE_OBJECT || !list(types) || !list(modifiers) || !list(formats) || !requestOption(get(requests,"full"),&legend.full,&legend.delta) || !requestOption(get(requests,"range"),&legend.range,NULL))return 0;
 for(unsigned kind=1;kind<XLS_TOKEN_KIND_COUNT;++kind)if(contains(types,tokenNames[kind])){legend.types[kind]=legend.typeCount;legend.typeKinds[legend.typeCount++]=kind;}
 for(unsigned kind=1;kind<XLS_TOKEN_KIND_COUNT;++kind)if(legend.types[kind]<0){
  unsigned fallback=kind>=XLS_TOKEN_CLASS && kind<=XLS_TOKEN_TYPE_PARAMETER?XLS_TOKEN_TYPE:
   kind==XLS_TOKEN_PARAMETER || kind==XLS_TOKEN_PROPERTY || kind==XLS_TOKEN_ENUM_MEMBER?XLS_TOKEN_VARIABLE:
   kind==XLS_TOKEN_METHOD?XLS_TOKEN_FUNCTION:0;
  if(fallback)legend.types[kind]=legend.types[fallback];
 }
 for(unsigned i=0;i<3;++i)if(contains(modifiers,modifierNames[i])){legend.modifierKinds[legend.modifierCount]=i;legend.modifierBits[i]=1u<<legend.modifierCount++;}
 bool augment=true;if(booleans[4])xrtValueGetBool(booleans[4],&augment);
 legend.enabled=legend.typeCount && (legend.full || legend.range) && contains(formats,"relative") && augment;
 *out=legend;return 1;
}
static int append(xvalue* array,xvalue* value)
{int okay=value && xrtValueArrayAppend(array,value);xrtValueRelease(value);return okay;}
xvalue* xideTokenLegendProvider(const xide_token_legend* l)
{
 if(!l || !l->enabled)return NULL;
 xvalue *out=xrtValueObject(),*legend=xrtValueObject(),*types=xrtValueArray(),*modifiers=xrtValueArray(),*full=NULL;
 int okay=out && legend && types && modifiers;
 for(int i=0;okay && i<l->typeCount;++i)okay=append(types,xrtValueString(sv(tokenNames[l->typeKinds[i]])));
 for(int i=0;okay && i<l->modifierCount;++i)okay=append(modifiers,xrtValueString(sv(modifierNames[l->modifierKinds[i]])));
 if(okay){okay=put(legend,"tokenTypes",types);types=NULL;}
 if(okay){okay=put(legend,"tokenModifiers",modifiers);modifiers=NULL;}
 if(okay){okay=put(out,"legend",legend);legend=NULL;}
 if(okay && l->range)okay=put(out,"range",xrtValueBool(true));
 if(okay && l->full){full=l->delta?xrtValueObject():xrtValueBool(true);okay=full!=NULL;if(okay && l->delta)okay=put(full,"delta",xrtValueBool(true));if(okay){okay=put(out,"full",full);full=NULL;}}
 xrtValueRelease(types);xrtValueRelease(modifiers);xrtValueRelease(legend);xrtValueRelease(full);if(!okay){xrtValueRelease(out);out=NULL;}return out;
}
static int before(int line,int column,int otherLine,int otherColumn){return line<otherLine || (line==otherLine && column<otherColumn);}
static int selected(const xide_token_position* row,const xide_token_legend* l,const xide_token_range* range)
{
 if(!row->kind || row->kind>=XLS_TOKEN_KIND_COUNT || l->types[row->kind]<0)return 0;
 if(!range || !range->enabled)return 1;
 return before(range->startLine,range->startCharacter,range->endLine,range->endCharacter) && before(row->line,row->character,range->endLine,range->endCharacter) && before(range->startLine,range->startCharacter,row->line,row->character+row->length);
}
xide_token_data* xideTokenDataEncode(const xide_token_positions* positions,const xide_token_legend* legend,
 const xide_token_range* range,const char* id,int (*cancelled)(void*),void* user)
{
 if(!positions || !legend || !legend->enabled || (id && strlen(id)>=64))return NULL;
 const xide_token_position* rows=xideTokenPositionsData(positions);size_t count=xideTokenPositionsCount(positions),selectedCount=0;
 for(size_t i=0;i<count;++i){if(!(i%256) && cancelled && cancelled(user))return NULL;if(selected(rows+i,legend,range))++selectedCount;}
 if(selectedCount>TOKEN_WORDS/5)return NULL;
 xide_token_data* data=xrtCalloc(1,sizeof(*data));if(!data)return NULL;
 xrtAtomic32Init(&data->references,1);data->capacity=selectedCount*5;if(id)memcpy(data->id,id,strlen(id)+1);
 if(data->capacity && !(data->words=xrtCalloc(data->capacity,sizeof(*data->words)))){xideTokenDataRelease(data);return NULL;}
 int line=0,column=0;
 for(size_t i=0;i<count;++i){
  if(!(i%256) && cancelled && cancelled(user)){xideTokenDataRelease(data);return NULL;}
  const xide_token_position* row=rows+i;if(!selected(row,legend,range))continue;
  if(row->line<line || row->character<0 || row->length<=0 || row->character>INT_MAX-row->length || (row->line==line && row->character<column)){xideTokenDataRelease(data);return NULL;}
  unsigned mask=0;for(unsigned m=0;m<3;++m)if(row->modifiers&(1u<<m))mask|=legend->modifierBits[m];
  uint32* words=data->words+data->count;words[0]=(uint32)(row->line-line);words[1]=(uint32)(row->character-(row->line==line?column:0));words[2]=(uint32)row->length;words[3]=(uint32)legend->types[row->kind];words[4]=mask;data->count+=5;line=row->line;column=row->character;
 }
 if(cancelled && cancelled(user)){xideTokenDataRelease(data);return NULL;}return data;
}
xide_token_data* xideTokenDataRetain(xide_token_data* d){if(d)xrtAtomic32FetchAdd(&d->references,1,XMEMORY_RELAXED);return d;}
void xideTokenDataRelease(xide_token_data* d){if(d && xrtAtomic32FetchSub(&d->references,1,XMEMORY_ACQ_REL)==1){xrtFree(d->words);xrtFree(d);}}
size_t xideTokenDataCount(const xide_token_data* d){return d?d->count:0;}
const uint32* xideTokenDataWords(const xide_token_data* d){return d?d->words:NULL;}
const char* xideTokenDataId(const xide_token_data* d){return d?d->id:NULL;}
size_t xideTokenDataBytes(const xide_token_data* d){return d?sizeof(*d)+d->capacity*sizeof(uint32):0;}
static xvalue* numbers(const uint32* words,size_t count,int (*cancelled)(void*),void* user)
{
 xvalue* out=xrtValueArray();if(!out)return NULL;
 for(size_t i=0;i<count;++i)if((!(i%256) && cancelled && cancelled(user)) || !append(out,xrtValueInt(words[i]))){xrtValueRelease(out);return NULL;}
 return out;
}
xvalue* xideTokenWireResult(const xide_token_data* data,const xide_token_data* previous,int delta,int (*cancelled)(void*),void* user)
{
 if(!data || (cancelled && cancelled(user)))return NULL;
 size_t prefix=0,suffix=0;
 if(delta && previous){
  size_t common=data->count<previous->count?data->count:previous->count;
  while(prefix<common && data->words[prefix]==previous->words[prefix]){if(!(prefix%1024) && cancelled && cancelled(user))return NULL;++prefix;}
  while(suffix<common-prefix && data->words[data->count-1-suffix]==previous->words[previous->count-1-suffix]){if(!(suffix%1024) && cancelled && cancelled(user))return NULL;++suffix;}
 }else delta=0;
 size_t inserted=data->count-prefix-suffix,deleted=previous?previous->count-prefix-suffix:0;
 if(delta && (inserted || deleted) && inserted+16>=data->count)delta=0;
 xvalue *out=xrtValueObject(),*values=NULL,*edit=NULL;int okay=out!=NULL;
 if(okay && *data->id)okay=put(out,"resultId",xrtValueString(sv(data->id)));
 if(okay && delta){
  values=xrtValueArray();okay=values!=NULL;
  if(okay && (inserted || deleted)){
   edit=xrtValueObject();okay=edit && put(edit,"start",xrtValueInt((int64)prefix)) && put(edit,"deleteCount",xrtValueInt((int64)deleted));
   if(okay && inserted)okay=put(edit,"data",numbers(data->words+prefix,inserted,cancelled,user));
   if(okay)okay=xrtValueArrayAppend(values,edit);
  }
  if(okay){okay=put(out,"edits",values);values=NULL;}
 }else if(okay)okay=put(out,"data",numbers(data->words,data->count,cancelled,user));
 xrtValueRelease(values);xrtValueRelease(edit);
 if(!okay || (cancelled && cancelled(user))){xrtValueRelease(out);out=NULL;}return out;
}
