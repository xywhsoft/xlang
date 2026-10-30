#define main aliasReadFrontBaseMain
#include "analysis_semantic_read_front_test.c"
#undef main
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
#undef FRONT_CHECK
#define FRONT_CHECK(v) do{++frontChecks;if(!(v)){fprintf(stderr,"analysis_semantic_alias_reads_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static size_t aliasFaultAt,aliasFaultCalls,aliasFaultDenied;
static int aliasFaultEnabled;
static int aliasRefuse(void)
{if(aliasFaultEnabled && ++aliasFaultCalls==aliasFaultAt){++aliasFaultDenied;return 1;}return 0;}
char* __real_xrtStrDup(const char*);bool __real_xrtStrBufReserve(xstrbuf*,size_t);
bool __real_xrtArrayPush(xarray*,const void*);bool __real_xrtArrayResize(xarray*,size_t);
char* __wrap_xrtStrDup(const char* s){return aliasRefuse() ? NULL : __real_xrtStrDup(s);}
bool __wrap_xrtStrBufReserve(xstrbuf* b,size_t n){return !aliasRefuse() && __real_xrtStrBufReserve(b,n);}
bool __wrap_xrtArrayPush(xarray* a,const void* v){return !aliasRefuse() && __real_xrtArrayPush(a,v);}
bool __wrap_xrtArrayResize(xarray* a,size_t n){return !aliasRefuse() && __real_xrtArrayResize(a,n);}
static XL6_AstNode* aliasNode(const char* name)
{return xl6AstNewText(XL6_AST_TYPE,name,strlen(name),NULL);}
static const XL6_AstNode* aliasReference(XL6_SemContext* ctx,const XL6_AstNode* node)
{
 if(!ctx || !node || node->kind!=XL6_AST_TYPE || !node->text || node->childCount)return NULL;
 const XL6_TypeInfo* info=xl6TypeRegistryFind(&ctx->types,node->text);
 return info && info->kind==XL6_TYPE_ALIAS && info->decl && info->decl->childCount ? info->decl->children[0] : NULL;
}
static int aliasQuery(front_fixture* f,XL6_AstNode* node)
{const XL6_AstNode* expected=aliasReference(&f->ctx,node);FRONT_CHECK(xl6SemAliasTargetType(&f->ctx,node)==expected);return 1;}
static int aliasFinish(front_fixture* f,int complete)
{xls_sem_reads* reads=frontStop(f);FRONT_CHECK(reads && reads->complete==complete);xlsSemReadsDestroy(reads);return 1;}
static int scalarReads(void)
{
 uint64_t live=frontLive();front_fixture f;FRONT_CHECK(frontInit(&f));XL6_AstNode* node=aliasNode("int");FRONT_CHECK(node && aliasQuery(&f,node));
 FRONT_CHECK(f.capture.reads->items.Count==1);xls_sem_read* read=xrtArrayGet(&f.capture.reads->items,0);
 FRONT_CHECK(read->type==2 && read->key.Size==16 && read->count==1);
 FRONT_CHECK(aliasQuery(&f,node) && f.capture.aliasComparisons==1 && f.capture.typedComparisons==1);
 XL6_TypeInfo* info=(XL6_TypeInfo*)xl6TypeRegistryFind(&f.ctx.types,"int");FRONT_CHECK(info);XL6_TypeInfo saved=*info;
 info->id+=999;info->abiId^=UINT64_C(0xdeadbeef);info->abiName=(char*)"unused.alias.abi";info->allowGeneric^=1;info->genericArity+=3;
 /* These fields are not consumed by the native alias predicate. */
 FRONT_CHECK(aliasQuery(&f,node) && f.capture.reads->complete);
 *info=saved;FRONT_CHECK(aliasFinish(&f,1) && f.memo.readWork.typedComparisons==2);xl6AstFree(node);FRONT_CHECK(frontLive()==live);
 return 1;
}
static int upgrades(void)
{
 uint64_t live=frontLive();
 for(unsigned mode=0;mode<6;++mode){
  front_fixture f;FRONT_CHECK(frontInit(&f));XL6_AstNode* node=aliasNode("int");FRONT_CHECK(node);
  XL6_TypeInfo* info=(XL6_TypeInfo*)xl6TypeRegistryFind(&f.ctx.types,"int");FRONT_CHECK(info);XL6_TypeInfo saved=*info;
  if(mode<3){
   FRONT_CHECK(aliasQuery(&f,node));info->abiId^=UINT64_C(0x12345678);
   if(mode==2)info->kind=XL6_TYPE_ALIAS;
   FRONT_CHECK(xlsSemReadTypeFind(&f.ctx.types,"int")==info);
   if(mode==2)FRONT_CHECK(!f.capture.reads->complete);
   else{
    FRONT_CHECK(f.capture.reads->complete && f.capture.aliasUpgrades==1 && f.capture.reads->items.Count==1);
    FRONT_CHECK(((xls_sem_read*)f.capture.reads->items.Data)[0].type==1);
    FRONT_CHECK(aliasQuery(&f,node));
    FRONT_CHECK(f.capture.aliasFrontComparisons);
    if(mode==1){info->abiId^=1;FRONT_CHECK(xlsSemReadTypeFind(&f.ctx.types,"int")==info && !f.capture.reads->complete);}
   }
  }else{
   FRONT_CHECK(xlsSemReadTypeFind(&f.ctx.types,"int")==info);
   info->abiId^=UINT64_C(0xabcdef);if(mode==5)info->kind=XL6_TYPE_ALIAS;
   FRONT_CHECK(aliasQuery(&f,node));
   FRONT_CHECK(f.capture.aliasFrontComparisons);
   FRONT_CHECK(f.capture.reads->complete==(mode!=5));
   if(mode==3)*info=saved; /* Unread transient ABI changes do not poison reads. */
  }
  int expected=mode==0 || mode==3;
  FRONT_CHECK(aliasFinish(&f,expected));
  xl6AstFree(node);FRONT_CHECK(frontLive()==live);
 }
 return 1;
}
static int declarations(void)
{
 uint64_t live=frontLive();
 for(unsigned mode=0;mode<5;++mode){
  front_fixture f;FRONT_CHECK(frontInit(&f));XL6_AstNode* decl=xl6AstNewText(XL6_AST_TYPE_ALIAS,"sample_alias",12,NULL);
  XL6_AstNode *target=aliasNode("int"),*node=aliasNode("sample_alias");FRONT_CHECK(decl && target && node);
  xl6AstAddChild(decl,target);FRONT_CHECK(decl->childCount==1);
  FRONT_CHECK(xl6TypeRegistryAdd(&f.ctx.types,"sample_alias","alias.sample",XL6_TYPE_ALIAS,0,0,decl));
  FRONT_CHECK(aliasQuery(&f,node));xls_sem_read* r=xrtArrayGet(&f.capture.reads->items,0);FRONT_CHECK(r && r->type==2 && r->key.Size>16);
  XL6_TypeInfo* info=(XL6_TypeInfo*)xl6TypeRegistryFind(&f.ctx.types,"sample_alias");FRONT_CHECK(info);
  if(mode==0)info->abiId^=1;
  if(mode==1)target->flags^=XL6_AST_FLAG_BYREF;
  if(mode==2)info->kind=XL6_TYPE_BUILTIN;
  if(mode==3)decl->childCount=0;
  if(mode==4){FRONT_CHECK(xlsSemReadTypeFind(&f.ctx.types,"sample_alias")==info);FRONT_CHECK(f.capture.aliasUpgrades==1 && f.capture.reads->items.Count==1);target->flags^=XL6_AST_FLAG_BYVAL;}
  if(mode<4)FRONT_CHECK(aliasQuery(&f,node));
  FRONT_CHECK(aliasFinish(&f,mode==0));decl->childCount=1;xl6AstFree(decl);xl6AstFree(node);FRONT_CHECK(frontLive()==live);
 }
 return 1;
}
static int negativesAndBounds(void)
{
 uint64_t live=frontLive();
 for(unsigned mode=0;mode<4;++mode){
  front_fixture f;FRONT_CHECK(frontInit(&f));XL6_AstNode* n=aliasNode("unregistered_type");FRONT_CHECK(n && aliasQuery(&f,n));
  if(mode<2){FRONT_CHECK(xl6TypeRegistryAdd(&f.ctx.types,"unregistered_type","new.type",XL6_TYPE_BUILTIN,0,0,NULL));if(!mode)FRONT_CHECK(aliasQuery(&f,n));}
  else if(mode==2){FRONT_CHECK(xlsSemReadTypeFind(&f.ctx.types,"unregistered_type")==NULL);FRONT_CHECK(f.capture.aliasUpgrades==1 && f.capture.reads->items.Count==1);}
  else{char name[40];for(unsigned i=0;i<255;++i){snprintf(name,sizeof(name),"absent_%u",i);xlsSemReadAliasFind(&f.ctx.types,name);}FRONT_CHECK(f.capture.reads->items.Count==256 && f.capture.reads->complete);xlsSemReadAliasFind(&f.ctx.types,"unregistered_type");}
  FRONT_CHECK(aliasFinish(&f,mode==2));xl6AstFree(n);FRONT_CHECK(frontLive()==live);
 }
 front_fixture outer,inner;FRONT_CHECK(frontInit(&outer));FRONT_CHECK(xlsSemReadAliasFind(&outer.ctx.types,"int"));
 FRONT_CHECK(frontInit(&inner));FRONT_CHECK(xlsSemReadAliasFind(&inner.ctx.types,"int"));
 FRONT_CHECK(xlsSemReadAliasFind(&outer.ctx.types,"int"));FRONT_CHECK(aliasFinish(&inner,0));FRONT_CHECK(xlsActiveSemReads==&outer.capture);
 FRONT_CHECK(xlsSemReadAliasFind(&outer.ctx.types,"int"));FRONT_CHECK(aliasFinish(&outer,1));FRONT_CHECK(frontLive()==live);
 return 1;
}
static int faults(void)
{
 uint64_t live=frontLive();size_t sites=0;
 for(size_t site=0;site<=sites;++site){
  front_fixture f;FRONT_CHECK(frontInit(&f));FRONT_CHECK(xlsSemReadAliasFind(&f.ctx.types,"int"));
  aliasFaultCalls=aliasFaultDenied=0;aliasFaultAt=site;aliasFaultEnabled=1;
  const XL6_TypeInfo* info=xlsSemReadTypeFind(&f.ctx.types,"int");aliasFaultEnabled=0;FRONT_CHECK(info);
  if(!site){sites=aliasFaultCalls;FRONT_CHECK(sites>=3 && sites<20);}else FRONT_CHECK(aliasFaultDenied==1);
  xls_sem_reads* reads=frontStop(&f);FRONT_CHECK(reads);xlsSemReadsDestroy(reads);FRONT_CHECK(frontLive()==live);
 }
 return 1;
}
static int namespaceAndCollisions(void)
{
 uint64_t live=frontLive();front_fixture f;FRONT_CHECK(frontInit(&f));
 xlsSemReadsName(&f.ctx,"int");FRONT_CHECK(xlsSemReadAliasFind(&f.ctx.types,"int"));
 FRONT_CHECK(f.capture.reads->items.Count==2 && ((xls_sem_read*)f.capture.reads->items.Data)[0].type==0);
 FRONT_CHECK(xlsSemReadTypeFind(&f.ctx.types,"int"));
 FRONT_CHECK(f.capture.reads->items.Count==2 && ((xls_sem_read*)f.capture.reads->items.Data)[1].type==1);
 const XL6_TypeInfo* integer=xl6TypeRegistryFind(&f.ctx.types,"int");FRONT_CHECK(integer);
 unsigned bucket=xlsSemReadFrontBucket(integer->id);
 while(xlsSemReadFrontBucket(f.ctx.types.nextId)!=bucket)FRONT_CHECK(xl6TypeRegistryReserveId(&f.ctx.types));
 XL6_AstNode* decl=xl6AstNewText(XL6_AST_TYPE_ALIAS,"collision_alias",15,NULL);
 XL6_AstNode *target=aliasNode("int"),*n=aliasNode("collision_alias");FRONT_CHECK(decl && target && n);xl6AstAddChild(decl,target);
 FRONT_CHECK(xl6TypeRegistryAdd(&f.ctx.types,"collision_alias","alias.collision",XL6_TYPE_ALIAS,0,0,decl));
 integer=xl6TypeRegistryFind(&f.ctx.types,"int");FRONT_CHECK(integer);
 const XL6_TypeInfo* alias=xl6TypeRegistryFind(&f.ctx.types,"collision_alias");FRONT_CHECK(alias && xlsSemReadFrontBucket(alias->id)==bucket);
 FRONT_CHECK(aliasQuery(&f,n));FRONT_CHECK(xlsSemReadTypeFind(&f.ctx.types,"collision_alias")==alias);
 FRONT_CHECK(xlsSemReadAliasFind(&f.ctx.types,"int")==integer && f.capture.reads->complete);
 FRONT_CHECK(f.capture.reads->items.Count==3 && f.capture.aliasUpgrades==2);
 target->flags^=XL6_AST_FLAG_BYREF;FRONT_CHECK(aliasQuery(&f,n) && !f.capture.reads->complete);
 FRONT_CHECK(aliasFinish(&f,0));xl6AstFree(n);xl6AstFree(decl);FRONT_CHECK(frontLive()==live);return 1;
}
static int guardsAndFallback(void)
{
 uint64_t live=frontLive();
 for(unsigned mode=0;mode<3;++mode){
  front_fixture f;FRONT_CHECK(frontInit(&f));XL6_AstNode* n=aliasNode("int");FRONT_CHECK(n);
  if(!mode){n->kind=XL6_AST_NAME;FRONT_CHECK(aliasQuery(&f,n) && !f.capture.reads->items.Count);}
  if(mode==1){char* saved=n->text;n->text=NULL;FRONT_CHECK(aliasQuery(&f,n) && !f.capture.reads->items.Count);n->text=saved;}
  if(mode==2){xl6AstAddChild(n,aliasNode("int"));FRONT_CHECK(n->childCount && aliasQuery(&f,n) && !f.capture.reads->items.Count);}
  FRONT_CHECK(!xl6SemAliasTargetType(NULL,n) && !f.capture.reads->items.Count);
  FRONT_CHECK(aliasFinish(&f,1));xl6AstFree(n);FRONT_CHECK(frontLive()==live);
 }
 front_fixture f;xls_sem_value_control off;xlsFrontendSemValuesBegin(&off,0);FRONT_CHECK(frontInit(&f));
 FRONT_CHECK(xlsSemReadAliasFind(&f.ctx.types,"int"));FRONT_CHECK(xlsSemReadTypeFind(&f.ctx.types,"int"));
 FRONT_CHECK(xlsSemReadAliasFind(&f.ctx.types,"int") && f.capture.reads->items.Count==1 && f.capture.reads->complete &&
  !f.capture.typedComparisons && !f.capture.aliasComparisons);
 xlsFrontendSemValuesEnd(&off);FRONT_CHECK(aliasFinish(&f,1));FRONT_CHECK(frontLive()==live);
 return 1;
}
static int classProjection(void)
{
 uint64_t live=frontLive();
 for(unsigned mode=0;mode<3;++mode){
  front_fixture f;FRONT_CHECK(frontInit(&f));XL6_AstNode* decl=xl6AstNewText(XL6_AST_CLASS,"sample_class",12,NULL);
  XL6_AstNode *target=aliasNode("int"),*n=aliasNode("sample_class");FRONT_CHECK(decl && target && n);xl6AstAddChild(decl,target);
  FRONT_CHECK(xl6TypeRegistryAdd(&f.ctx.types,"sample_class","class.sample",XL6_TYPE_CLASS,0,0,decl));
  XL6_TypeInfo* info=(XL6_TypeInfo*)xlsSemReadTypeFind(&f.ctx.types,"sample_class");FRONT_CHECK(info);
  FRONT_CHECK(f.capture.reads->items.Count==1);xls_sem_read* r=xrtArrayGet(&f.capture.reads->items,0);
  FRONT_CHECK(r->type==1 && r->aliasReady && r->aliasKind==XL6_TYPE_CLASS && r->aliasHasDecl);
  decl->flags^=XL6_AST_FLAG_BYREF;
  if(mode==2)info->kind=XL6_TYPE_ALIAS;
  FRONT_CHECK(aliasQuery(&f,n));FRONT_CHECK(f.capture.reads->complete==(mode!=2));
  if(!mode)decl->flags^=XL6_AST_FLAG_BYREF;
  if(mode==2)info->kind=XL6_TYPE_CLASS;
  FRONT_CHECK(aliasFinish(&f,mode==0));xl6AstFree(n);xl6AstFree(decl);FRONT_CHECK(frontLive()==live);
 }
 return 1;
}
int main(void)
{
 front_fixture warm;if(!frontInit(&warm))return 1;xlsSemReadsDestroy(frontStop(&warm));
 if(!scalarReads() || !upgrades() || !declarations() || !negativesAndBounds() || !faults() || !namespaceAndCollisions() || !guardsAndFallback() || !classProjection())return 1;
 printf("analysis_semantic_alias_reads_test passed: %u checks;actual native alias projection,full-read upgrades andsubsumption,livekind/declaration mutations,negative transitions,recordbounds,nestedcontexts,actual optional allocation failures andbalanced retirement\n",frontChecks);return 0;
}
#else
int main(void){puts("analysis_semantic_alias_reads_test passed: legacy retains full native type observations");return 0;}
#endif
