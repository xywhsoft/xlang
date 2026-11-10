#include "services/semantic_rename.h"
#include "analysis/frontend_internal.h"
#include "analysis/text.h"
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_rename_policy_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct owner {xls_semantic_check* check;xls_projection_store* cache;xide_semantic_source sources[8];size_t count;} owner;
static int capture(owner* o,const char* path,const char* text)
{
 xls_semantic_input input={path,text,strlen(text)};
 o->check=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);CHECK(o->check);
 xls_check_info info=xlsSemanticCheckInfo(o->check);
 if(!info.accepted)for(size_t i=0;i<info.diagnostics;++i){xls_check_diagnostic d;xlsSemanticCheckDiagnostic(o->check,i,&d);fprintf(stderr,"%s:%d %s\n",d.path,d.line,d.message);}
 CHECK(info.accepted);xls_check_inputs inputs;CHECK(xlsSemanticCheckInputs(o->check,&inputs) && inputs.sources<=8);o->count=inputs.sources;
 for(size_t i=0;i<o->count;++i){xls_input_source s;CHECK(xlsSemanticCheckSource(o->check,i,&s));o->sources[i]=(xide_semantic_source){s.path,s.text,s.kind,0};}
 o->cache=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(o->cache);return 1;
}
static void clear(owner* o){xlsProjectionStoreDestroy(o->cache);xlsSemanticCheckDestroy(o->check);*o=(owner){0};}
static xide_reference_key* keyAt(owner* o,const char* path,const char* text,const char* needle)
{
 const char* at=strstr(text,needle);if(!at)return NULL;
 xls_text* raw=xlsTextCreate(text,strlen(text),1024*1024,NULL);xls_position position;
 if(!raw || !xlsTextPosition(raw,(size_t)(at-text),&position)){xlsTextRelease(raw);return NULL;}
 xlsTextRelease(raw);return xideSemanticReferenceKeyCreate(o->cache,o->check,o->sources,o->count,path,(int)position.line,(int)position.character,XIDE_REFERENCE_BYTES,NULL);
}
static int familyPolicy(owner* o,xide_reference_graph* graph,const char* path,const char* text,const char* needle,unsigned expected)
{
 xide_reference_key* key=keyAt(o,path,text,needle);CHECK(key);
 enum xide_reference_family_error why;
 xide_reference_family* direct=xideSemanticReferenceFamilyCreate(o->cache,o->check,o->sources,o->count,key,0,NULL,NULL,&why);
 CHECK(direct && why==XIDE_REFERENCE_FAMILY_OK && xideSemanticReferenceFamilyRenamePolicy(direct)==expected);
 xide_reference_family* indexed=xideSemanticReferenceGraphFamily(graph,key,0,NULL,NULL,&why);
 CHECK(indexed && why==XIDE_REFERENCE_FAMILY_OK && xideSemanticReferenceFamilyRenamePolicy(indexed)==expected);
 xide_reference_graph* graphs[]={graph,graph};
 xide_reference_family* merged=xideSemanticReferenceGraphsFamily(graphs,2,key,0,NULL,NULL,&why);
 CHECK(merged && why==XIDE_REFERENCE_FAMILY_OK && xideSemanticReferenceFamilyRenamePolicy(merged)==expected);
 xide_reference_family_info a,b,c;CHECK(xideSemanticReferenceFamilyInfo(direct,&a) && xideSemanticReferenceFamilyInfo(indexed,&b) && xideSemanticReferenceFamilyInfo(merged,&c));CHECK(a.members==b.members && a.members==c.members && a.members>0);
 xide_reference_shard* shard=xideSemanticReferenceGraphRetainShard(graph);xide_reference_shard_view view;
 CHECK(shard && xideSemanticReferenceShardQuery(shard,key,1,&view)==XIDE_REFERENCE_MATCH && view.count>0);
 xideSemanticReferenceShardRelease(shard);xideSemanticReferenceKeyRelease(key);xideSemanticReferenceFamilyRelease(direct);xideSemanticReferenceFamilyRelease(indexed);xideSemanticReferenceFamilyRelease(merged);return 1;
}
static int sourcePolicy(void)
{
 const char* text="extern \"native_call\" func nativeCall(c_int number) as c_int\n"
 "extern \"C\" func sameName(c_int number) as c_int\n"
 "export \"native_export\" func nativeExport(int number) as int\n return number\nend\n"
 "extern \"NativeRecord\" struct Row @header(\"record.h\")\n c_int field\n c_int logical @c_name(\"native_field\")\nend\n"
 "export func visible(int number) as int\n int local=number\n return local\nend\n"
 "class Normal\n int field\nend\n"
 "func main(...) as int\n Row row=Row()\n row.field=c_int(1)\n return int(nativeCall(c_int(visible(1))))\nend\n";
 char* path=xrtPathAbs("rename-policy.xl");CHECK(path);owner o={0};CHECK(capture(&o,path,text));enum xide_reference_family_error why;
 xide_reference_graph* graph=xideSemanticReferenceGraphCreate(o.cache,o.check,o.sources,o.count,0,NULL,NULL,&why);CHECK(graph && why==XIDE_REFERENCE_FAMILY_OK);
 const char* foreign[]={"nativeCall(c_int","sameName(c_int","nativeExport(int","Row @header","field\n c_int logical","logical @c_name","number) as c_int"};
 for(size_t i=0;i<sizeof(foreign)/sizeof(*foreign);++i)CHECK(familyPolicy(&o,graph,path,text,foreign[i],XLS_RENAME_POLICY_FOREIGN));
 const char* ordinary[]={"visible(int","local=","Normal\n","field\nend"};
 for(size_t i=0;i<sizeof(ordinary)/sizeof(*ordinary);++i)CHECK(familyPolicy(&o,graph,path,text,ordinary[i],0));
 unsigned policy=99;const xls_semantic_index* index=xlsSemanticCheckTypedIndex(o.check);
 CHECK(!xlsSemanticIndexRenamePolicy(NULL,0,&policy) && policy==XLS_RENAME_POLICY_UNSUPPORTED);
 CHECK(!xlsSemanticIndexRenamePolicy(index,xlsSemanticIndexCount(index),&policy) && policy==XLS_RENAME_POLICY_UNSUPPORTED);
 CHECK(!xlsSemanticIndexRenamePolicy(index,0,NULL));CHECK(xideSemanticReferenceFamilyRenamePolicy(NULL)==XLS_RENAME_POLICY_UNSUPPORTED);
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
 /* Controlled native metadata injection, not a claim of compiled-loader coverage.
  * Preserve the source/IR and ordinary graph: only the second provider marks the
  * existing declaration as package-owned, testing loss through dedup/composition. */
 XL6_AstNode* root=(XL6_AstNode*)xlsSemanticCheckAst(o.check);CHECK(root);int before=root->compiledPackageRoot;root->compiledPackageRoot=1;
 xide_reference_graph* packaged=xideSemanticReferenceGraphCreate(o.cache,o.check,o.sources,o.count,0,NULL,NULL,&why);CHECK(packaged && why==XIDE_REFERENCE_FAMILY_OK);
 CHECK(familyPolicy(&o,packaged,path,text,"visible(int",XLS_RENAME_POLICY_COMPILED));
 xide_reference_key* key=keyAt(&o,path,text,"visible(int");CHECK(key);xide_reference_graph* graphs[]={graph,packaged};
 xide_reference_family* combined=xideSemanticReferenceGraphsFamily(graphs,2,key,0,NULL,NULL,&why);CHECK(combined && why==XIDE_REFERENCE_FAMILY_OK && xideSemanticReferenceFamilyRenamePolicy(combined)==XLS_RENAME_POLICY_COMPILED);
 root->compiledPackageRoot=before;xideSemanticReferenceKeyRelease(key);xideSemanticReferenceGraphRelease(packaged);
 clear(&o);xideSemanticReferenceGraphRelease(graph);graph=NULL;
 for(size_t i=0;i<1000;++i)CHECK(xideSemanticReferenceFamilyRenamePolicy(combined)==XLS_RENAME_POLICY_COMPILED);
 xideSemanticReferenceFamilyRelease(combined);
#endif
 xideSemanticReferenceGraphRelease(graph);clear(&o);xrtFree(path);return 1;
}
int main(void)
{
 if(!sourcePolicy())return 1;
 printf("semantic_rename_policy_test passed: %u checks; foreign/ordinary native ancestry, original references, direct/indexed/merged families, conservative compiled metadata, duplicate provider policy and detached lifetime\n",checks);return 0;
}
