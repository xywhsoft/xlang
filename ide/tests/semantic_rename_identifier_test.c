#define main fieldBindingOriginalMain
#include "semantic_field_binding_test.c"
#undef main
#include "analysis/text.h"
#include <limits.h>
#include "rename_identifier_fixture.h"
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_rename_identifier_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int exactReferences(owner* o,xide_reference_key* key,const char* name,int units,size_t count,int qualified)
{
 xstrview captured={0};int width=0;CHECK(xideSemanticReferenceKeyIdentifier(key,&captured,&width) && captured.Size==strlen(name) && !memcmp(captured.Data,name,captured.Size) && width==units);
 xide_reference_identifier_view view;CHECK(xideSemanticReferenceShardIdentifiers(o->shard,key,1,&view)==XIDE_REFERENCE_MATCH && view.references.count==count);
 xide_reference_identifier_view uses;CHECK(xideSemanticReferenceShardIdentifiers(o->shard,key,0,&uses)==XIDE_REFERENCE_MATCH && uses.references.count+1==count);
 size_t declarations=0,refined=0;
 for(size_t i=0;i<count;++i){xide_semantic_reference at,raw;CHECK(xideSemanticReferenceShardIdentifierAt(&view,i,&at) && xideSemanticReferenceShardAt(&view.references,i,&raw));
  xide_semantic_source source={0};for(size_t j=0;j<xideSemanticReferenceShardSources(o->shard);++j){xide_semantic_source item;CHECK(xideSemanticReferenceShardSource(o->shard,j,&item));if(!strcmp(item.path,at.path))source=item;}CHECK(source.text);
  CHECK(at.end.offset-at.start.offset==(int)strlen(name) && !memcmp(source.text+at.start.offset,name,strlen(name)) && at.start.offset>=raw.start.offset && at.end.offset==raw.end.offset);
  xls_text* text=xlsTextCreate(source.text,strlen(source.text),8u*1024u*1024u,NULL);xls_position a,b;CHECK(text && xlsTextPosition(text,(size_t)at.start.offset,&a) && xlsTextPosition(text,(size_t)at.end.offset,&b));
  CHECK(at.start.line==(int)a.line && at.end.line==(int)b.line && at.start.character==(int)a.character+(a.line==0 ? source.utf16Prefix : 0) && at.end.character==(int)b.character+(b.line==0 ? source.utf16Prefix : 0));xlsTextRelease(text);
  declarations+=at.declaration;refined+=at.start.offset>raw.start.offset;
 }
 CHECK(declarations==1 && (!qualified || refined));
 for(size_t i=0;i<uses.references.count;++i){xide_semantic_reference at;CHECK(xideSemanticReferenceShardIdentifierAt(&uses,i,&at) && !at.declaration);}
 xide_semantic_reference cleared={.path="sentinel"};CHECK(!xideSemanticReferenceShardIdentifierAt(&view,count,&cleared) && !cleared.path);
 xide_reference_identifier_view invalid=view;invalid.references.first=SIZE_MAX;CHECK(!xideSemanticReferenceShardIdentifierAt(&invalid,0,&cleared) && !cleared.path);
 invalid=view;++invalid.targetStart;CHECK(!xideSemanticReferenceShardIdentifierAt(&invalid,0,&cleared));invalid=view;invalid.utf16Units=INT_MAX;CHECK(!xideSemanticReferenceShardIdentifierAt(&invalid,0,&cleared));
 invalid=view;++invalid.name.Size;CHECK(!xideSemanticReferenceShardIdentifierAt(&invalid,0,&cleared));invalid=view;invalid.name.Data="xxxxxxxxxxxxxxxx";CHECK(!xideSemanticReferenceShardIdentifierAt(&invalid,0,&cleared));
 return 1;
}
static int identifierRun(void)
{
 char* directory=xrtPathAbs("build/rename-identifiers");CHECK(directory && xrtDirCreateAll(directory));char *a=xrtPathJoin(directory,"入口😀.xl"),*b=xrtPathJoin(directory,"other.xl"),*dep=xrtPathJoin(directory,"dep.xl");CHECK(a && b && dep);
 xls_semantic_input inputs[]={{a,identifierEntry,strlen(identifierEntry)},{b,identifierOther,strlen(identifierOther)},{dep,identifierLibrary,strlen(identifierLibrary)}};owner first={0},second={0};CHECK(capture(&first,a,inputs,3) && capture(&second,b,inputs,3));
 xide_rename_target* t=target(&first,a,identifierEntry,"型号?",0);CHECK(t);xide_reference_key* key=xideRenameTargetRetainKey(t);CHECK(key && exactReferences(&first,key,"型号",2,3,1) && exactReferences(&second,key,"型号",2,2,0));xideRenameTargetRelease(t);
 xide_reference_identifier_view held;CHECK(xideSemanticReferenceShardIdentifiers(first.shard,key,1,&held)==XIDE_REFERENCE_MATCH);xideSemanticReferenceKeyRelease(key);xlsSemanticCheckDestroy(first.check);first.check=NULL;xide_semantic_reference at;CHECK(xideSemanticReferenceShardIdentifierAt(&held,0,&at));
 t=target(&second,b,identifierOther,"😀字段",4);CHECK(t);key=xideRenameTargetRetainKey(t);CHECK(exactReferences(&second,key,"😀字段",4,2,0));xideRenameTargetRelease(t);xideSemanticReferenceKeyRelease(key);
 t=target(&second,dep,identifierLibrary,"参数)",0);CHECK(t);key=xideRenameTargetRetainKey(t);CHECK(exactReferences(&second,key,"参数",2,2,0));xideRenameTargetRelease(t);
 xstrview name={"sentinel",8};int units=99;CHECK(!xideSemanticReferenceKeyIdentifier(NULL,&name,&units) && !name.Data && !units);CHECK(!xideSemanticReferenceKeyIdentifier(key,NULL,&units) && !units);
 xide_reference_identifier_view invalid={0};CHECK(xideSemanticReferenceShardIdentifiers(second.shard,key,2,&invalid)==XIDE_REFERENCE_INVALID && !invalid.references.shard);CHECK(!xideSemanticReferenceShardIdentifierAt(NULL,0,&at) && !at.path);
 xideSemanticReferenceKeyRelease(key);
 /* A real native variadic parameter has a reference identity but its "..."
    selection is not an IDENT. Ordinary references remain available. */
 const char* dots=strstr(identifierOther,"...");int line,column;CHECK(dots && xideLspOffsetToPosition(identifierOther,(int)(dots-identifierOther),&line,&column));
 key=xideSemanticReferenceKeyCreate(NULL,second.check,second.sources,second.count,b,line,column,XIDE_REFERENCE_BYTES,NULL);CHECK(key);
 name=(xstrview){"sentinel",8};units=99;CHECK(!xideSemanticReferenceKeyIdentifier(key,&name,&units) && !name.Data && !units);
 xide_reference_shard_view ordinary;CHECK(xideSemanticReferenceShardQuery(second.shard,key,1,&ordinary)==XIDE_REFERENCE_MATCH && ordinary.count==1);
 invalid=(xide_reference_identifier_view){0};CHECK(xideSemanticReferenceShardIdentifiers(second.shard,key,1,&invalid)==XIDE_REFERENCE_IDENTIFIER_UNSUPPORTED && !invalid.references.shard && !invalid.name.Data);
 enum xide_rename_target_error why;CHECK(!xideRenameTargetCreate(NULL,second.check,second.sources,second.count,b,line,column,0,NULL,NULL,&why) && why==XIDE_RENAME_TARGET_ABSENT);
 xideSemanticReferenceKeyRelease(key);release(&first);release(&second);xrtFree(a);xrtFree(b);xrtFree(dep);return 1;
}
int main(void){if(!identifierRun())return 1;printf("semantic_rename_identifier_test passed: %u checks; native bound identifier suffixes, qualified types, Unicode/emoji UTF16, BOM/CRLF, cross-owner identity, declaration filtering, invalid views and shard-only lifetime\n",checks);return 0;}
