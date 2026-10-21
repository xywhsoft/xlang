#include "model/resources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"resource_text_test:%d: %s; %s\n",__LINE__,#v,error.message); goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* s) { return xrtValueObjectGet(v,sv(s)); }
static const char* typed="{\n  \"format\":\"xlang-windows-resource-1\",\n  \"language\":1033, \"codepage\":\"1200\",\n  \"version\":{\"fileVersion\":\"1.2.3.4\",\"FileVersion\":\"5.6.7.8\",\"keep\":set[char(\"你\"),char(\"😀\")]},\n  \"icon\":{\"path\":\"图标😀.ico\",\"base64\":\"ignored\",\"keep\":time(\"2000-01-02T03:04:05Z\")},\n  \"manifest\":{\"text\":\"<assembly/>\",\"keep\":float(\"nan\")},\n  \"extension\":intmap{-1:[float(\"inf\"),float(\"-inf\"),bytes(\"AQID\"),{\"value\":\"escaped \\\" [,}:)\"}]}\n}";
int main(void)
{
 xide_model_error error={0}; xide_resource_config c={0},round={0}; xvalue *draft=NULL,*previous=NULL; char *body=NULL,*large=NULL; int code=1;
 CHECK(xideResourceParse(&c,typed,&error) && c.xson && c.text.original && !strcmp(c.text.original,typed));
 CHECK(!get(c.root,"extension") && !get(get(c.root,"version"),"keep"));
 CHECK(!strcmp(xideResourceVersion(c.root,3),"1.2.3.4") && xideResourceNumber(c.root,"language")==1033 && xideResourceNumber(c.root,"codepage")==1200);
 draft=xrtValueDeepClone(c.root); CHECK(draft);
 body=xideResourceSerialize(&c,draft,&error); CHECK(body && !strcmp(body,typed)); xrtFree(body); body=NULL;
 const char* fields[9]; for(int i=0;i<9;++i) fields[i]=xideResourceVersion(c.root,i); fields[3]="9.8.7.6"; fields[7]="修改 产品😀";
 CHECK(xideResourceSetVersion(draft,1,fields,&error) && xideResourceSetNumber(draft,"language","2052",&error) && xideResourceSetNumber(draft,"codepage","65001",&error));
 body=xideResourceSerialize(&c,draft,&error); CHECK(body && xideResourceParse(&round,body,&error) && xrtValueEqual(round.root,draft));
 CHECK(strstr(body,"set[char(\"你\"),char(\"😀\")]"));
 CHECK(strstr(body,"time(\"2000-01-02T03:04:05Z\")")); CHECK(strstr(body,"float(\"nan\")"));
 CHECK(strstr(body,"intmap{-1:[float(\"inf\"),float(\"-inf\"),bytes(\"AQID\"),{\"value\":\"escaped \\\" [,}:)\"}]}"));
 CHECK(strstr(body,"\"FileVersion\":\"5.6.7.8\"") && strstr(body,"\"format\":\"xlang-windows-resource-1\""));
 CHECK(strstr(body,"\"codepage\":\"65001\"") && !strcmp(xideResourceVersion(round.root,7),"修改 产品😀"));
 xrtFree(body); body=NULL;
 CHECK(xideResourceSetSource(draft,0,XIDE_RESOURCE_BASE64,"AQID",&error) && xideResourceSetSource(draft,1,XIDE_RESOURCE_PATH,"new.manifest",&error));
 body=xideResourceSerialize(&c,draft,&error); CHECK(body && !strstr(body,"ignored") && !strstr(body,"图标😀.ico") && !strstr(body,"<assembly/>"));
 CHECK(strstr(body,"time(\"2000-01-02T03:04:05Z\")") && strstr(body,"float(\"nan\")") && xideResourceParse(&round,body,&error) && xrtValueEqual(round.root,draft)); xrtFree(body); body=NULL;
 CHECK(xideResourceSetVersion(draft,0,NULL,&error) && xideResourceSetSource(draft,0,XIDE_RESOURCE_NONE,"",&error) && xideResourceSetSource(draft,1,XIDE_RESOURCE_NONE,"",&error));
 body=xideResourceSerialize(&c,draft,&error); CHECK(body && !strstr(body,"set[") && !strstr(body,"time(") && !strstr(body,"\"manifest\"") && strstr(body,"intmap{") && xideResourceParse(&round,body,&error) && xrtValueEqual(round.text.projection,draft)); xrtFree(body); body=NULL;
 /* New projected fields; null segments; escaped property names and atomic failure. */
 CHECK(xideResourceParse(&c,"{\"version\":null,\"icon\":null,\"keep\":char(\"😀\")}",&error)); xrtValueRelease(draft); draft=xrtValueDeepClone(c.root); CHECK(draft);
 fields[3]="1.0.0.0"; fields[7]=""; CHECK(xideResourceSetVersion(draft,1,fields,&error) && xideResourceSetSource(draft,0,XIDE_RESOURCE_PATH,"图标.ico",&error) && xideResourceSetSource(draft,1,XIDE_RESOURCE_TEXT,"<xml/>" ,&error) && xideResourceSetNumber(draft,"language","2052",&error));
 body=xideResourceSerialize(&c,draft,&error); CHECK(body && strstr(body,"char(\"😀\")") && xideResourceParse(&round,body,&error) && xrtValueEqual(round.root,draft)); xrtFree(body); body=NULL;
 CHECK(xideResourceParse(&c,"{\"v\\u0065rsion\":{\"file\\u0056ersion\":\"1.0\",\"extension\":char(\"你\")},\"future\\u0000key\":time(\"2000-01-02T03:04:05Z\")}",&error));
 xrtValueRelease(draft); draft=xrtValueDeepClone(c.root); CHECK(draft); for(int i=0;i<9;++i) fields[i]=xideResourceVersion(c.root,i); fields[3]="2.0"; CHECK(xideResourceSetVersion(draft,1,fields,&error));
 body=xideResourceSerialize(&c,draft,&error); CHECK(body && strstr(body,"\"file\\u0056ersion\":\"2.0\"") && strstr(body,"\"future\\u0000key\":time(")); xrtFree(body); body=NULL;
 previous=xrtValueDeepClone(c.root); CHECK(previous);
 const char* invalid[]={"{", "{\"version\":{\"fileVersion\":char(\"a\")},\"x\":char(\"a\")}","{\"icon\":{\"path\":time(\"x\")}}", "{\"version\":{\"fileVersion\":\"1.0\",\"fileVersion\":\"2.0\"},\"x\":char(\"a\")}","{\"language\":1,\"language\":2,\"x\":char(\"a\")}","{\"x\":intmap{1:[char(\"a\")}}}","{\"x\":char(\"a\"),}","{\"x\":char(\"a\")} trailing","{\"version\":{\"productName\":\"x\\u0000y\"},\"x\":char(\"a\")}"};
 for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) CHECK(!xideResourceParse(&c,invalid[i],&error) && xrtValueEqual(c.root,previous) && strstr(c.text.original,"future\\u0000key"));
 CHECK(xrtValueObjectSetNew(draft,sv("unprojected"),xrtValueInt(1)) && !xideResourceSerialize(&c,draft,&error)); CHECK(xrtValueObjectRemove(draft,sv("unprojected")));
 CHECK(xrtValueObjectSetNew(get(draft,"version"),sv("unprojected"),xrtValueInt(1)) && !xideResourceSerialize(&c,draft,&error));
 /* Comments/trivia and BOM are retained outside modified scalar spans. */
 CHECK(xideResourceParse(&c,"\xef\xbb\xbf/* lead */ {\n \"version\": {\"fileVersion\": /* field */ \"1.0\" /* tail */,\"keep\":char(\"你\")}, // next\n \"keep\":set[char(\"😀\")]\n} /* end */",&error));
 xrtValueRelease(draft); draft=xrtValueDeepClone(c.root); CHECK(draft); for(int i=0;i<9;++i) fields[i]=xideResourceVersion(c.root,i); fields[3]="2.0"; CHECK(xideResourceSetVersion(draft,1,fields,&error));
 body=xideResourceSerialize(&c,draft,&error); CHECK(body && !memcmp(body,"\xef\xbb\xbf",3) && strstr(body,"/* field */ \"2.0\" /* tail */") && strstr(body,"// next\n") && strstr(body,"} /* end */") && xideResourceParse(&round,body,&error) && xrtValueEqual(round.root,draft)); xrtFree(body); body=NULL;
 /* Input/output budgets and opaque nesting cannot bypass limits. */
 large=malloc(16u*1024u*1024u+2); CHECK(large); memset(large,' ',16u*1024u*1024u+1); large[16u*1024u*1024u+1]=0;
 CHECK(!xideResourceParse(&round,large,&error)); free(large); large=malloc(1200); CHECK(large); strcpy(large,"{\"x\":"); size_t n=strlen(large); for(int i=0;i<513;++i) large[n++]='['; large[n++]='0'; for(int i=0;i<513;++i) large[n++]=']'; large[n++]='}'; large[n]=0;
 CHECK(!xideResourceParse(&round,large,&error)); free(large); large=NULL;
 CHECK(xideResourceParse(&c,"{\"version\":{\"fileVersion\":\"1.0\"},\"x\":char(\"你\")}",&error));
 xrtValueRelease(draft); draft=xrtValueDeepClone(c.root); CHECK(draft);
 large=malloc(16u*1024u*1024u+1); CHECK(large); memset(large,'a',16u*1024u*1024u); large[16u*1024u*1024u]=0;
 CHECK(xideResourceSetSource(draft,1,XIDE_RESOURCE_TEXT,large,&error) && !xideResourceSerialize(&c,draft,&error));
 free(large); large=NULL;
 /* Stable field reconstruction with only/first/last removals and asset promotion. */
 const char* cases[]={"{\"version\":{},\"keep\":char(\"你\")}","{\"keep\":char(\"你\"),\"version\":{}}","{\"version\":{}}","{\"icon\":\"old.ico\",\"version\":{},\"keep\":char(\"你\")}"};
 for(size_t k=0;k<sizeof(cases)/sizeof(*cases);++k) {
  CHECK(xideResourceParse(&c,cases[k],&error)); xrtValueRelease(draft); draft=xrtValueDeepClone(c.root); CHECK(draft && xideResourceSetVersion(draft,0,NULL,&error));
  if(k==3) CHECK(xideResourceSetSource(draft,0,XIDE_RESOURCE_BASE64,"AQID",&error));
  body=xideResourceSerialize(&c,draft,&error); CHECK(body && xideResourceParse(&round,body,&error) && xrtValueEqual(round.text.projection,draft));
  if(k!=2) CHECK(strstr(body,"char(\"你\")"));
  xrtFree(body); body=NULL;
 }
 /* Compatible JSON extension DOM still rejects edits outside the form, while
    unchanged whitespace and exact extension values survive known-field edits. */
 CHECK(xideResourceParse(&c," { \"version\":{\"fileVersion\":\"1.0\",\"keep\":{\"x\":[1,2]}}, \"keep\":{\"a\":true} } ",&error) && !c.xson && get(c.root,"keep"));
 xrtValueRelease(draft); draft=xrtValueDeepClone(c.root); CHECK(draft); for(int i=0;i<9;++i) fields[i]=xideResourceVersion(c.root,i); fields[3]="2.0"; CHECK(xideResourceSetVersion(draft,1,fields,&error));
 body=xideResourceSerialize(&c,draft,&error); CHECK(body && strstr(body,"\"keep\":{\"x\":[1,2]}") && strstr(body,"\"keep\":{\"a\":true}") && xideResourceParse(&round,body,&error) && xrtValueEqual(round.root,draft)); xrtFree(body); body=NULL;
 CHECK(xrtValueObjectSetNew(get(draft,"keep"),sv("a"),xrtValueBool(false)) && !xideResourceSerialize(&c,draft,&error));
 CHECK(xrtValueObjectSetNew(get(draft,"keep"),sv("a"),xrtValueBool(true)) && xrtValueObjectSetNew(get(get(draft,"version"),"keep"),sv("x"),xrtValueNull()) && !xideResourceSerialize(&c,draft,&error));
 printf("resource_text_test passed: %d checks; modern XLON opaque spans, known fields, aliases, insertion/removal/source modes, exact no-op, comments/BOM, atomic errors and limits.\n",checks); code=0;
cleanup:
 xideResourceClear(&c); xideResourceClear(&round); xrtValueRelease(draft); xrtValueRelease(previous); xrtFree(body); free(large); return code;
}
