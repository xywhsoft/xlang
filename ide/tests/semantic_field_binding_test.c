#include "services/semantic_rename.h"
#include "analysis/frontend_internal.h"
#include "analysis/text.h"
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_field_binding_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct owner {xls_semantic_check* check;xide_semantic_source sources[8];size_t count;xide_reference_shard* shard;} owner;
static int capture(owner* o,const char* path,const xls_semantic_input* inputs,size_t count)
{
 o->check=xlsSemanticCheckCreate(path,inputs,count,1,NULL,NULL,NULL,0);CHECK(o->check);xls_check_info info=xlsSemanticCheckInfo(o->check);
 if(!info.accepted)for(size_t i=0;i<info.diagnostics;++i){xls_check_diagnostic d;CHECK(xlsSemanticCheckDiagnostic(o->check,i,&d));fprintf(stderr,"%s:%d:%d %s\n",d.path,d.line,d.column,d.message);}
 CHECK(info.accepted);xls_inputs captured;CHECK(xlsSemanticCheckInputs(o->check,&captured) && captured.sources<=8);o->count=captured.sources;
 for(size_t i=0;i<o->count;++i){xls_input_source row;CHECK(xlsSemanticCheckSource(o->check,i,&row));int prefix=0;for(size_t j=0;j<count;++j)if(!strcmp(row.path,inputs[j].path))prefix=inputs[j].size>=3 && !memcmp(inputs[j].text,"\xef\xbb\xbf",3);o->sources[i]=(xide_semantic_source){row.path,row.text,row.kind,prefix};}
 o->shard=xideSemanticReferenceShardCreate(NULL,o->check,o->sources,o->count,XIDE_REFERENCE_BYTES,NULL);CHECK(o->shard);return 1;
}
static void release(owner* o){xlsSemanticCheckDestroy(o->check);xideSemanticReferenceShardRelease(o->shard);}
static xide_rename_target* target(owner* o,const char* path,const char* raw,const char* needle,size_t inside)
{
 const char* found=strstr(raw,needle);if(!found)return NULL;xls_text* text=xlsTextCreate(raw,strlen(raw),1024*1024,NULL);xls_position position;if(!text || !xlsTextPosition(text,(size_t)(found-raw)+inside,&position)){xlsTextRelease(text);return NULL;}xlsTextRelease(text);return xideRenameTargetCreate(NULL,o->check,o->sources,o->count,path,(int)position.line,(int)position.character,0,NULL,NULL,NULL);
}
static int bound(owner* o,const char* path,const char* raw,const char* needle,size_t inside,
 const char* declarationPath,size_t declarationOffset,const char* spelling,size_t references)
{
 xide_rename_target* t=target(o,path,raw,needle,inside);CHECK(t);xide_rename_target_info info;CHECK(xideRenameTargetInfo(t,&info));CHECK(!strcmp(info.declaration.path,declarationPath) && info.declarationStart==declarationOffset && info.name.Size==strlen(spelling) && !memcmp(info.name.Data,spelling,info.name.Size));xide_reference_key* key=xideRenameTargetRetainKey(t);xide_reference_shard_view view;CHECK(key && xideSemanticReferenceShardQuery(o->shard,key,1,&view)==XIDE_REFERENCE_MATCH && view.count==references);
 for(size_t i=0;i<view.count;++i){xide_semantic_reference r;CHECK(xideSemanticReferenceShardAt(&view,i,&r));CHECK(r.end.offset>r.start.offset);}
 xideSemanticReferenceKeyRelease(key);xideRenameTargetRelease(t);return 1;
}
static int one(const char* path,const char* raw,const char* declaration,const char* occurrence,size_t inside,size_t references,int functionField,int method)
{
 owner o={0};xls_semantic_input input={path,raw,strlen(raw)};CHECK(capture(&o,path,&input,1));const char* first=strstr(raw,declaration);CHECK(first);size_t bom=!memcmp(raw,"\xef\xbb\xbf",3)?3:0,offset=(size_t)(first-raw)-bom;
 CHECK(bound(&o,path,raw,declaration,0,path,offset,"value",references));CHECK(bound(&o,path,raw,occurrence,inside,path,offset,"value",references));
 const xls_semantic_index* index=xlsSemanticCheckTypedIndex(o.check);unsigned identity=0;int field=0;
 for(size_t i=0;i<xlsSemanticIndexCount(index);++i){xls_index_point_fact fact;CHECK(xlsSemanticIndexPointFact(index,i,&fact));if(fact.fact.source && fact.kind==XLS_INDEX_POINT_FIELD && fact.fact.selectionRange.startOffset==offset){CHECK(fact.fact.declaredSymbol && fact.fact.declaredSymbol==fact.fact.symbol && fact.fact.selectionSource==fact.fact.source);identity=fact.fact.symbol;++field;}}
 CHECK(field==1 && identity);size_t uses=0;
 for(size_t i=0;i<xlsSemanticIndexCount(index);++i){xls_index_fact fact;CHECK(xlsSemanticIndexFact(index,i,&fact));if(fact.source && fact.targetSymbol==identity)++uses;}CHECK(uses+1==references);
 const XL6_IrModule* ir=xlsSemanticCheckIr(o.check);CHECK(ir);unsigned fieldValues=0,methodValues=0;
 for(size_t i=0;i<ir->nodeCount;++i){const XL6_IrNode* n=ir->nodes+i;if(!n->text || n->sourceKind!=XL6_AST_MEMBER)continue;
  if(functionField && !strcmp(n->text,"value")){CHECK(n->kind==XL6_IR_NODE_MEMBER);++fieldValues;}
  if(method && !strcmp(n->text,"read")){CHECK(n->kind==XL6_IR_NODE_MEMBER && !n->symbol && n->targetSymbol);++methodValues;}}
 CHECK((!functionField || fieldValues) && (!method || methodValues));release(&o);return 1;
}
static int run(void)
{
 CHECK(XLS_INDEX_POINT_FIELD>XLS_INDEX_POINT_C_RECORD && xlsIndexPointKindParse("field")==XLS_INDEX_POINT_FIELD);
 char* directory=xrtPathAbs("build/field-binding");CHECK(directory && xrtDirCreateAll(directory));char *path=xrtPathJoin(directory,"入口.xl"),*other=xrtPathJoin(directory,"other.xl"),*dep=xrtPathJoin(directory,"dep.xl");CHECK(path && other && dep);
 CHECK(one(path,"\xef\xbb\xbf" "class Box\r\n int value\r\nend\r\nfunc main(...) as int\r\n auto box=Box()\r\n box.value=3\r\n box.value+=1\r\n return box.value\r\nend\r\n","value\r","value=3",2,4,0,0));
 CHECK(one(path,"class Box<T>\n T value\nend\nfunc main(...) as int\n auto box=Box::<int>()\n return box.value\nend\n","value\n","box.value",5,2,0,0));
 CHECK(one(path,"class Box\n int value\n func read() as int\n return value\n end\nend\nfunc main(...) as int\n auto box=Box()\n func() as int callback=box.read\n return callback()\nend\n","value\n","return value",8,2,0,1));
 CHECK(one(path,"class Box\n func() as int value=func() as int\n return 3\n end\nend\nfunc main(...) as int\n auto box=Box()\n return box.value()\nend\n","value=","value()",1,2,1,0));
 CHECK(one(path,"class Box\n static int value=3\nend\nfunc main(...) as int\n return Box.value\nend\n","value=","Box.value",5,2,0,0));
 CHECK(one(path,"class Base\n int value\nend\nclass Derived extends Base\nend\nfunc main(...) as int\n auto box=Derived()\n return box.value\nend\n","value\n","box.value",5,2,0,0));
 CHECK(one(path,"class Box\n int value\nend\nfunc read(Box? box) as int?\n return box?.value\nend\nfunc main(...) as int\n return 0\nend\n","value\n","box?.value",6,2,0,0));
 CHECK(one(path,"union Box\n int value\nend\nfunc main(...) as int\n Box box\n box.value=3\n return box.value\nend\n","value\n","value=3",1,3,0,0));
 const char* lib="\xef\xbb\xbf" "export class Box\r\n int value\r\nend\r\nexport class Other\r\n int value\r\nend\r\n";
 const char* a="import dep\nfunc main(...) as int\n auto box=dep.Box()\n auto other=dep.Other()\n return box.value+other.value\nend\n";
 char* extra=xrtPathJoin(directory,"extra.xl");CHECK(extra);
 const char* padding="export class Extra\n int padding\nend\n";
 const char* b="import extra\nimport dep\nfunc main(...) as int\n auto box=dep.Box()\n return box.value\nend\n";
 xls_semantic_input inputs[]={{path,a,strlen(a)},{other,b,strlen(b)},{dep,lib,strlen(lib)},{extra,padding,strlen(padding)}};owner first={0},second={0};CHECK(capture(&first,path,inputs,4) && capture(&second,other,inputs,4));size_t declaration=(size_t)(strstr(lib,"value\r")-lib)-3;
 CHECK(bound(&first,path,a,"value+",1,dep,declaration,"value",2));CHECK(bound(&second,other,b,"value\nend",1,dep,declaration,"value",2));
 unsigned ids[2]={0};owner* owners[]={&first,&second};
 for(size_t i=0;i<2;++i){const xls_semantic_index* index=xlsSemanticCheckTypedIndex(owners[i]->check);for(size_t j=0;j<xlsSemanticIndexCount(index);++j){xls_index_fact f;CHECK(xlsSemanticIndexFact(index,j,&f));if(f.selectionSource && !strcmp(owners[i]->sources[f.selectionSource-1].path,dep) && f.selectionRange.startOffset==declaration && f.declaredSymbol)ids[i]=f.declaredSymbol;}}
 CHECK(ids[0] && ids[1] && ids[0]!=ids[1]);
 xide_rename_target* t=target(&first,path,a,"value+",1);CHECK(t);xide_reference_key* key=xideRenameTargetRetainKey(t);CHECK(key);xideRenameTargetRelease(t);xide_reference_shard_view v;CHECK(xideSemanticReferenceShardQuery(second.shard,key,1,&v)==XIDE_REFERENCE_MATCH && v.count==2);xlsSemanticCheckDestroy(first.check);first.check=NULL;xlsSemanticCheckDestroy(second.check);second.check=NULL;CHECK(xideSemanticReferenceShardQuery(second.shard,key,1,&v)==XIDE_REFERENCE_MATCH && v.count==2);xideSemanticReferenceKeyRelease(key);release(&first);release(&second);
 const char* unicode="\xef\xbb\xbf" "class Box\r\n int 😀数值\r\nend\r\nfunc main(...) as int\r\n auto box=Box()\r\n box.😀数值=3\r\n return box.😀数值\r\nend\r\n";
 owner u={0};xls_semantic_input input={path,unicode,strlen(unicode)};CHECK(capture(&u,path,&input,1));size_t offset=(size_t)(strstr(unicode,"😀数值")-unicode)-3;CHECK(bound(&u,path,unicode,"😀数值=3",4,path,offset,"😀数值",3));CHECK(bound(&u,path,unicode,"return box.😀数值",11,path,offset,"😀数值",3));release(&u);
 xrtFree(extra);xrtFree(path);xrtFree(other);xrtFree(dep);xrtFree(directory);return 1;
}
int main(void){if(!run())return 1;printf("semantic_field_binding_test passed: %u checks; native declared and selected field identity, cross-owner references, generic/static/inherited/safe/implicit/union/function fields and original IR value categories\n",checks);return 0;}
