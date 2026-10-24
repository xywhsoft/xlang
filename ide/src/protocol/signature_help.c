#include "protocol/signature_help.h"
#include <string.h>
#include <limits.h>
static xstrview view(const char* s){return (xstrview){s,strlen(s)};}
static const xvalue* get(const xvalue* v,const char* key){return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,view(key)) : NULL;}
static int put(xvalue* v,const char* key,xvalue* item){return item && xrtValueObjectSetNew(v,view(key),item);}
static int text(const xvalue* v,xstrview* s,size_t limit)
{return v && xrtValueGetString(v,s) && s->Size<=limit && !memchr(s->Data,0,s->Size) && xrtUtf8Valid(*s,NULL);}
static int indexValue(const xvalue* v,size_t limit)
{int64 n=0;return xrtValueGetInt(v,&n) && n>=0 && (uint64)n<=(uint64)limit;}
static int utf16(const char* s,size_t size,size_t* units)
{
 size_t n=0;while(size){uint32 scalar=0;size_t read=0;if(xrtUtf8Decode((xstrview){s,size},&scalar,&read)!=XUTF_OK || !read)return 0;n+=scalar>0xffff ? 2 : 1;s+=read;size-=read;}*units=n;return 1;
}
static int documentation(const xvalue* v)
{
 if(!v)return 1;
 xstrview s={0},kind={0};
 if(text(v,&s,65536))return 1;
 return text(get(v,"kind"),&kind,16) && ((!strcmp(kind.Data,"plaintext")) || !strcmp(kind.Data,"markdown")) && text(get(v,"value"),&s,65536);
}
int xlsSignatureHelpCapabilities(const xvalue* capabilities,unsigned* flags)
{
 if(!flags)return 0;
 *flags=0;const xvalue* help=get(get(capabilities,"textDocument"),"signatureHelp");
 if(!help)return 1;
 if(xrtValueType(help)!=XVALUE_OBJECT)return 0;
 const xvalue* info=get(help,"signatureInformation");if(info && xrtValueType(info)!=XVALUE_OBJECT)return 0;
 const xvalue* parameter=get(info,"parameterInformation");if(parameter && xrtValueType(parameter)!=XVALUE_OBJECT)return 0;
 const xvalue* values[]={get(parameter,"labelOffsetSupport"),get(info,"activeParameterSupport"),get(help,"contextSupport"),get(help,"dynamicRegistration")};
 for(size_t i=0;i<4;++i)if(values[i]){bool yes=false;if(!xrtValueGetBool(values[i],&yes))return 0;if(yes && i<2)*flags|=1u<<i;}
 const xvalue* formats=get(info,"documentationFormat");
 if(formats){if(xrtValueType(formats)!=XVALUE_ARRAY || xrtValueCount(formats)>16)return 0;for(size_t i=0;i<xrtValueCount(formats);++i){xstrview s={0};if(!text(xrtValueArrayGet(formats,i),&s,64))return 0;}}
 return 1;
}
static int activeHelp(const xvalue* help)
{
 if(!help || xrtValueType(help)!=XVALUE_OBJECT)return 0;
 const xvalue* signatures=get(help,"signatures");if(!signatures || xrtValueType(signatures)!=XVALUE_ARRAY || xrtValueCount(signatures)>256)return 0;
 const xvalue* active=get(help,"activeSignature");if(active && !indexValue(active,UINT32_MAX))return 0;
 active=get(help,"activeParameter");if(active && !indexValue(active,UINT32_MAX))return 0;
 for(size_t i=0;i<xrtValueCount(signatures);++i){
  const xvalue* sig=xrtValueArrayGet(signatures,i);xstrview label={0};size_t units=0;
  if(!text(get(sig,"label"),&label,32768) || !utf16(label.Data,label.Size,&units) || !documentation(get(sig,"documentation")))return 0;
  active=get(sig,"activeParameter");if(active && !indexValue(active,UINT32_MAX))return 0;
  const xvalue* params=get(sig,"parameters");if(!params)continue;
  if(xrtValueType(params)!=XVALUE_ARRAY || xrtValueCount(params)>128)return 0;
  for(size_t j=0;j<xrtValueCount(params);++j){const xvalue* param=xrtValueArrayGet(params,j);const xvalue* range=get(param,"label");xstrview s={0};
   if(!documentation(get(param,"documentation")))return 0;
   if(text(range,&s,32768))continue;
   if(!range || xrtValueType(range)!=XVALUE_ARRAY || xrtValueCount(range)!=2)return 0;
   int64 start=0,end=0;if(!xrtValueGetInt(xrtValueArrayGet(range,0),&start) || !xrtValueGetInt(xrtValueArrayGet(range,1),&end) || start<0 || end<start || (uint64)end>units)return 0;
  }
 }
 return 1;
}
int xlsSignatureHelpContextValid(const xvalue* context)
{
 if(!context)return 1;
 int64 trigger=0;bool retrigger=false;
 if(xrtValueType(context)!=XVALUE_OBJECT || !xrtValueGetInt(get(context,"triggerKind"),&trigger) || trigger<1 || trigger>3 || !xrtValueGetBool(get(context,"isRetrigger"),&retrigger))return 0;
 const xvalue* raw=get(context,"triggerCharacter");xstrview character={0};
 if(raw && (!text(raw,&character,4) || !character.Size || xrtUtf8Count(character)!=1))return 0;
 if(trigger==2 && !raw)return 0;
 const xvalue* help=get(context,"activeSignatureHelp");return !help || activeHelp(help);
}
xvalue* xlsSignatureHelpValue(const xls_signature_help* result,unsigned flags)
{
 xls_signature_info info=xlsSignatureInfo(result);if(!result)return NULL;if(!info.available)return xrtValueNull();
 if(info.count>256 || info.activeSignature>=info.count || (flags&~3u))return NULL;
 xvalue* out=xrtValueObject();xvalue* list=xrtValueArray();int okay=out && list;
 for(size_t i=0;okay && i<info.count;++i){
  xls_signature_item item;if(!xlsSignatureItem(result,i,&item) || !item.label || item.parameters>128){okay=0;break;}
  size_t labelSize=strlen(item.label);xvalue* sig=xrtValueObject();xvalue* params=xrtValueArray();okay=sig && params && labelSize<=32768 && put(sig,"label",xrtValueString(view(item.label)));
  if(okay && item.documentation && *item.documentation)okay=put(sig,"documentation",xrtValueString(view(item.documentation)));
  size_t mappedBytes=0,mappedUnits=0;
  for(size_t j=0;okay && j<item.parameters;++j){
   xls_signature_parameter p;okay=xlsSignatureParameter(result,i,j,&p) && p.labelStart<=p.labelEnd && p.labelEnd<=labelSize;
   xvalue* param=okay ? xrtValueObject() : NULL; xvalue* label=NULL;
   if(okay && (flags&XLS_SIGNATURE_LABEL_OFFSETS)){
    /* The native formatter emits ordered, nonoverlapping spans. Convert the
       label once in total, rather than rescanning its prefix per parameter. */
    size_t gap=0,width=0;okay=p.labelStart>=mappedBytes && utf16(item.label+mappedBytes,p.labelStart-mappedBytes,&gap) && utf16(item.label+p.labelStart,p.labelEnd-p.labelStart,&width);
    size_t start=mappedUnits+gap,end=start+width;mappedBytes=p.labelEnd;mappedUnits=end;label=okay ? xrtValueArray() : NULL;
    xvalue *a=okay ? xrtValueInt((int64)start) : NULL,*b=okay ? xrtValueInt((int64)end) : NULL;okay=okay && label && a && b && xrtValueArrayAppend(label,a) && xrtValueArrayAppend(label,b);xrtValueRelease(a);xrtValueRelease(b);
   }else if(okay)label=xrtValueString((xstrview){item.label+p.labelStart,p.labelEnd-p.labelStart});
   if(okay){okay=param && put(param,"label",label);label=NULL;}
   if(okay)okay=xrtValueArrayAppend(params,param);
   xrtValueRelease(label);xrtValueRelease(param);
  }
  if(okay && (flags&XLS_SIGNATURE_ACTIVE_PARAMETER) && item.activeParameter!=SIZE_MAX)okay=put(sig,"activeParameter",xrtValueInt((int64)item.activeParameter));
  if(okay){okay=put(sig,"parameters",params);params=NULL;}
  if(okay)okay=xrtValueArrayAppend(list,sig);
  xrtValueRelease(params);xrtValueRelease(sig);
 }
 if(okay){okay=put(out,"signatures",list);list=NULL;}
 if(okay)okay=put(out,"activeSignature",xrtValueInt((int64)info.activeSignature));
 xls_signature_item selected;if(okay && xlsSignatureItem(result,info.activeSignature,&selected) && selected.activeParameter!=SIZE_MAX)okay=put(out,"activeParameter",xrtValueInt((int64)selected.activeParameter));
 xrtValueRelease(list);if(!okay){xrtValueRelease(out);out=NULL;}return out;
}
