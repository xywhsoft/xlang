/* Included after the checked declaration binding builder. Reuse its retained
 * module/context registry; no second name resolver or compiler identity space. */
typedef struct xls_entity_version {uint32_t native,type;uint64_t version;} xls_entity_version;
typedef struct xls_entity_version_pending {xstrbuf key;uint64_t module;int failed;} xls_entity_version_pending;
static _Thread_local xls_entity_version_control* xlsActiveEntityVersions;
void xlsEntityVersionsBegin(xls_entity_version_control* c,size_t bytes)
{if(c){size_t limit=xlsActiveEntityVersions ? xlsActiveEntityVersions->bytes : 8u*1024u*1024u;*c=(xls_entity_version_control){xlsActiveEntityVersions,bytes>limit ? limit : bytes};xlsActiveEntityVersions=c;}}
void xlsEntityVersionsEnd(xls_entity_version_control* c)
{if(c && xlsActiveEntityVersions==c)xlsActiveEntityVersions=c->previous;}
static uint64_t xlsEntityVersionFind(const xls_entity_view* view,uint32_t symbol,uint32_t type)
{
 if(!view || !symbol)return 0;
 size_t lo=0,hi=view->versions.Count;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;const xls_entity_version* r=xrtArrayConstGet(&view->versions,mid);if(r->type<type || (r->type==type && r->native<symbol))lo=mid+1;else hi=mid;}
 const xls_entity_version* r=xrtArrayConstGet(&view->versions,lo);return r && r->type==type && r->native==symbol ? r->version : 0;
}
uint64_t xlsEntityViewSymbolVersion(const xls_entity_view* view,uint32_t symbol)
{return xlsEntityVersionFind(view,symbol,0);}
uint64_t xlsEntityViewTypeVersion(const xls_entity_view* view,uint32_t type)
{return xlsEntityVersionFind(view,type,1);}
xls_entity_version_info xlsEntityViewVersions(const xls_entity_view* view)
{
 xls_entity_version_info out={0};if(!view)return out;
 out.bytes=view->versions.Capacity*sizeof(xls_entity_version);out.declined=view->versionDeclined;
 out.symbols=view->versionSymbols;out.types=view->versionTypes;
 return out;
}
static int xlsEntityVersionText(xstrbuf* key,const char* text)
{size_t n=text ? strlen(text) : 0;return xlsEntityNumber(key,text!=NULL) && xlsEntityNumber(key,n) && (!n || xlsEntityBytes(key,text,n));}
static int xlsEntityVersionId(xstrbuf* key,const xls_entity_view* view,uint32_t id,int type)
{
 uint64_t stable=type ? xlsEntityViewType(view,id) : xlsEntityViewSymbol(view,id);
 /* Domains are tagged; unproven/generated/local IDs stay exact. */
 return xlsEntityNumber(key,stable!=0) && xlsEntityNumber(key,stable ? stable : id);
}
static int xlsEntityVersionHeader(xstrbuf* key,const xls_entity_view* view,const XL6_AstNode* n,size_t depth)
{
 if(!n)return xlsEntityNumber(key,0);
 if(depth>128 || (n->childCount && !n->children) || xlsFrontendCancelPoll())return 0;
 uint64_t fields[]={(uint64_t)n->kind+1,n->flags,n->literalDecoded,n->literalScalar,n->literalSize,
  n->semanticTypeAbiId,n->semanticCallKind,n->semanticLvalueKind,n->semanticOwnership,n->semanticExitKind};
 if(!xlsEntityBytes(key,fields,sizeof(fields)) || !xlsEntityVersionText(key,n->text) ||
    !xlsEntityVersionText(key,xl6AstSourcePath(n)) ||
    (n->literalSize && (!n->literalData || !xlsEntityBytes(key,n->literalData,n->literalSize))) ||
    !xlsEntityVersionId(key,view,n->semanticTypeId,1) || !xlsEntityVersionId(key,view,n->semanticSymbolId,0) ||
    !xlsEntityVersionId(key,view,n->semanticDeclaredSymbolId,0) || !xlsEntityVersionId(key,view,n->semanticTargetSymbolId,0) ||
    !xlsEntityVersionId(key,view,n->semanticCompoundOperatorSymbolId,0) ||
    !xlsEntityVersionHeader(key,view,n->inferredType,depth+1) || !xlsEntityVersionHeader(key,view,n->semanticDeclaredType,depth+1))return 0;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
 /* Receiver IDs use the same proven entity/exact-native domains as other
  * symbols. Owned package text is value identity,including NULL versus empty. */
 uint64_t package[]={n->compiledPackageError,n->compiledPackageRoot};
 if(!xlsEntityBytes(key,package,sizeof(package)) ||
    !xlsEntityVersionText(key,n->compiledPackageLink) ||
    !xlsEntityVersionId(key,view,n->semanticReceiverSymbolId,0))return 0;
#endif
 size_t count=0;
 for(size_t i=0;i<n->childCount;++i)if(!(xlsEntityCallable(n) && n->children[i] && n->children[i]->kind==XL6_AST_BLOCK))++count;
 if(!xlsEntityNumber(key,count))return 0;
 for(size_t i=0;i<n->childCount;++i){const XL6_AstNode* child=n->children[i];if(xlsEntityCallable(n) && child && child->kind==XL6_AST_BLOCK)continue;if(!xlsEntityVersionHeader(key,view,child,depth+1))return 0;}
 return 1;
}
static int xlsEntityVersionGroupKey(xstrbuf* key,const XL6_IndexFact* f)
{
 const XL6_AstNode* n=f->node;const XL6_AstNode* name=xlsEntityCallable(n) && n->childCount ? n->children[0] : NULL;
 return name && name->kind==XL6_AST_NAME && name->text && *name->text && xlsEntityNumber(key,f->parent) && xlsEntityVersionText(key,name->text);
}
static void xlsEntityVersionsBuild(xls_entity_view* view,const XL6_IndexBuild* b)
{
 xarray pending={0},versions={0};xmap slots={0},groups={0};size_t used=0;
 size_t budget=xlsActiveEntityVersions ? xlsActiveEntityVersions->bytes : 8u*1024u*1024u;
 size_t count=view->symbols.Count+view->types.Count;view->versionDeclined=count;
 if(!budget || !count || count>budget/sizeof(xls_entity_version_pending) ||
    !xrtArrayInit(&pending,sizeof(xls_entity_version_pending)) || !xrtArrayResize(&pending,count) ||
    pending.Capacity>budget/sizeof(xls_entity_version_pending))goto done;
 used=pending.Capacity*sizeof(xls_entity_version_pending);
 if(!xrtArrayInit(&versions,sizeof(xls_entity_version)) || !xrtArrayResize(&versions,count) ||
    versions.Capacity>(XLS_ENTITY_VIEW_BYTES-view->bytes)/sizeof(xls_entity_version) || !xrtMapInit(&slots,sizeof(size_t)) || !xrtMapInit(&groups,sizeof(uint32_t)))goto done;
 for(unsigned type=0;type<2;++type){const xarray* rows=type ? &view->types : &view->symbols;
  for(size_t j=0;j<rows->Count;++j){size_t i=j+(type ? view->symbols.Count : 0);const xls_entity_binding* r=xrtArrayConstGet(rows,j);xls_entity_version* v=xrtArrayGet(&versions,i);v->native=r->native;v->type=type;
   uint32_t key[]={r->native,type};if(!r->ambiguous && !xrtMapSet(&slots,(xbytesview){(const unsigned char*)key,sizeof(key)},&i))goto done;
  }
 }
 /* The native checker binds only the first NAME when registering an overload
  * group. Enumerate later declarations in that same compiler lexical scope,
  * using the fully expanded compiler name and a previously PROVEN group ID.
  * This collects declaration candidates;it never binds a reference by name. */
 for(size_t i=0;i<b->count;++i){
  if(!(i%64) && xlsFrontendCancelPoll())goto done;
  const XL6_IndexFact* f=&b->facts[i];const XL6_AstNode* n=f->node;const XL6_AstNode* name=xlsEntityCallable(n) && n->childCount ? n->children[0] : NULL;
  if(!name || !name->semanticSymbolId || !xlsEntityViewSymbol(view,name->semanticSymbolId))continue;
  xstrbuf key={0};if(!xlsEntityVersionGroupKey(&key,f)){xrtStrBufFree(&key);goto done;}
  xbytesview bytes={(const unsigned char*)key.Data,key.Size};uint32_t* old=xrtMapGet(&groups,bytes);int okay=1;
  if(old){if(*old!=name->semanticSymbolId)*old=0;}
  else if(key.Size<=budget-used){okay=xrtMapSet(&groups,bytes,&name->semanticSymbolId);used+=key.Size;}else okay=0;
  xrtStrBufFree(&key);if(!okay)goto done;
 }
 for(size_t i=0;i<b->count;++i){
  if(!(i%64) && xlsFrontendCancelPoll())goto done;
  const XL6_IndexFact* f=&b->facts[i];const XL6_AstNode* n=f->node;
  const XL6_AstNode* parent=f->parent && f->parent<=b->count ? b->facts[f->parent-1].node : NULL;
  int groupName=n->kind==XL6_AST_NAME && parent && xlsEntityCallable(parent) && parent->childCount && parent->children[0]==n;
  /* Later overload NAMEs may reference the existing group without declaring
   * it again. Observe every callable's actual bound NAME, not just the one
   * declaration occurrence. Each overload contributes exactly one header. */
  for(unsigned item=0;item<3;++item){unsigned type=item==1;const XL6_AstNode* header=n;uint32_t native=0;
   if(item==2){if(!xlsEntityCallable(n))continue;xstrbuf group={0};if(!xlsEntityVersionGroupKey(&group,f)){xrtStrBufFree(&group);continue;}
    const uint32_t* bound=xrtMapConstGet(&groups,(xbytesview){(const unsigned char*)group.Data,group.Size});native=bound ? *bound : 0;xrtStrBufFree(&group);}
   else{if(!type && groupName)continue;native=type ? f->declaredType : n->semanticDeclaredSymbolId;}
   uint32_t key[]={native,type};
   const size_t* at=native ? xrtMapConstGet(&slots,(xbytesview){(const unsigned char*)key,sizeof(key)}) : NULL;if(!at)continue;
   xls_entity_version_pending* p=xrtArrayGet(&pending,*at);if(p->failed)continue;
   uint64_t module=xlsModuleGraphSource(view->modules,f->source);
   if(!module || (p->module && module!=p->module)){p->failed=1;continue;}
   size_t before=p->key.Capacity;
   if(!p->module){const xls_entity_version* v=xrtArrayConstGet(&versions,*at);uint64_t identity=type ? xlsEntityViewType(view,v->native) : xlsEntityViewSymbol(view,v->native);p->module=module;
    if(!xlsEntityNumber(&p->key,UINT64_C(0x786c736661637401)) || !xlsEntityNumber(&p->key,type) || !xlsEntityNumber(&p->key,identity))p->failed=1;}
   if(!p->failed && !xlsEntityVersionHeader(&p->key,view,header,0))p->failed=1;
   size_t added=p->key.Capacity-before;if(added>budget-used)goto done;used+=added;
  }
 }
 for(size_t i=0;i<pending.Count;++i){
  if(!(i%64) && xlsFrontendCancelPoll())goto done;
  xls_entity_version_pending* p=xrtArrayGet(&pending,i);if(!p->module || p->failed || !p->key.Size)continue;
  uint64_t id=xlsEntityStoreIntern(view->store,view->domain,p->module,p->key.Data,p->key.Size);
  if(id){xls_entity_version* v=xrtArrayGet(&versions,i);v->version=id;--view->versionDeclined;if(v->type)++view->versionTypes;else ++view->versionSymbols;}
 }
 view->versions=versions;versions=(xarray){0};view->bytes+=view->versions.Capacity*sizeof(xls_entity_version);
done:
 if(!view->versions.Count){view->versionDeclined=count;view->versionSymbols=view->versionTypes=0;}
 for(size_t i=0;i<pending.Count;++i)xrtStrBufFree(&((xls_entity_version_pending*)xrtArrayGet(&pending,i))->key);
 xrtArrayUnit(&pending);xrtArrayUnit(&versions);xrtMapUnit(&slots);xrtMapUnit(&groups);
}
