/* Current native alias-reader dependency projection. Positions locate owned
 * records only; every use compares the actual live kind/declaration reads.
 * A later full read upgrades the same record, preserving the256-record cap.
 * typedComparisons counts actual owned type-value comparisons, including
 * these live presence/kind projections as well as full value snapshots. */
static size_t xlsSemAliasSlot(xls_sem_read_capture* c,const char* name,const XL6_TypeInfo* actual)
{
 size_t cached=actual ? c->aliasSlots[xlsSemReadFrontBucket(actual->id)] : 0;
 if(cached && cached<=c->reads->items.Count){
  const xls_sem_read* r=xrtArrayConstGet(&c->reads->items,cached-1);
  if((r->type==1 || r->type==2) && !strcmp(r->name,name))return cached-1;
 }
 for(size_t i=0;i<c->reads->items.Count;++i){
  const xls_sem_read* r=xrtArrayConstGet(&c->reads->items,i);
  if((r->type==1 || r->type==2) && !strcmp(r->name,name))return i;
 }
 return SIZE_MAX;
}
static void xlsSemAliasRemember(xls_sem_read_capture* c,size_t at,const char* name,const XL6_TypeInfo* actual)
{
 if(actual && actual->name && !strcmp(actual->name,name))
  c->aliasSlots[xlsSemReadFrontBucket(actual->id)]=at+1;
}
static int xlsSemAliasCaptureReady(const xls_sem_read_capture* c,XL6_SemContext* ctx,const char* name)
{return c && c->reads && c->reads->complete && ctx==c->context && name && *name && c->reads->items.Count<256;}
static int xlsSemAliasScalarSame(const xls_sem_read* r,const XL6_TypeInfo* actual)
{
 if(actual && actual->kind==XL6_TYPE_ALIAS)return 0;
 uint64_t fields[]={actual!=NULL,actual ? (uint64_t)actual->kind : 0};
 xstrbuf key={.Data=(char*)fields,.Size=sizeof(fields)};
 return xlsSemReadSame(r,&key,actual!=NULL);
}
static int xlsSemAliasValueSame(const xls_sem_read_value* old,const XL6_TypeInfo* actual)
{
 if(old->present!=(actual!=NULL))return 0;
 if(!actual)return 1;
 if(old->type.kind!=actual->kind)return 0;
 /* Full value snapshots admit no declaration. Nonaliases never read it. */
 return actual->kind!=XL6_TYPE_ALIAS || !actual->decl;
}
static int xlsSemAliasFront(xls_sem_read_capture* c,const char* name,const XL6_TypeInfo* actual)
{
 if(!actual || !c->valuesReady)return 0;
 size_t cached=c->typedSlots[xlsSemReadFrontBucket(actual->id)];
 if(!cached || cached>c->reads->items.Count)return 0;
 const xls_sem_read_value* value=xrtArrayConstGet(&c->values,cached-1);
 /* The existing full-read front admission already proves this owned name
  * belongs to the captured record. Compare every live alias-consumed field. */
 if(!value || !value->ready || !value->present || !value->type.name || strcmp(value->type.name,name))return 0;
 ++c->observations;++c->typedComparisons;++c->aliasComparisons;++c->aliasFrontComparisons;
 if(!xlsSemAliasValueSame(value,actual))c->reads->complete=0;
 return 1;
}
static void xlsSemAliasObserve(xls_sem_read_capture* c,XL6_SemContext* ctx,const char* name,const XL6_TypeInfo* actual)
{
 if(!xlsSemAliasCaptureReady(c,ctx,name)){
  xlsSemReadsObserveSlow(ctx,name,2,actual);return;
 }
 if(xlsSemAliasFront(c,name,actual))return;
 size_t at=xlsSemAliasSlot(c,name,actual);
 if(at==SIZE_MAX){
  size_t before=c->reads->items.Count;xlsSemReadsObserveSlow(ctx,name,2,actual);
  if(c->reads->complete && c->reads->items.Count>before)xlsSemAliasRemember(c,before,name,actual);
  return;
 }
 const xls_sem_read* r=xrtArrayConstGet(&c->reads->items,at);
 /* The existing value control also disables projected value comparisons.
  * Compare the retained serialized record, preserving full-read subsumption. */
 if(!c->valuesReady){xlsSemReadsObserveSlow(ctx,name,r->type,actual);return;}
 if(r->type==1){
  /* A full record can own a mutable declaration snapshot without the
   * scalar-only value optimization. Its owned primitive projection still
   * proves a nonalias result without reserializing that unread tree. */
  if(r->aliasReady && (!r->aliasPresent || r->aliasKind!=XL6_TYPE_ALIAS || !r->aliasHasDecl)){
   ++c->observations;++c->typedComparisons;++c->aliasComparisons;
   if(r->aliasPresent!=(actual!=NULL) || (actual && (r->aliasKind!=actual->kind ||
      (actual->kind==XL6_TYPE_ALIAS && actual->decl))))c->reads->complete=0;
  }else xlsSemReadsObserveSlow(ctx,name,1,actual);
 }else if(!actual || actual->kind!=XL6_TYPE_ALIAS){
  ++c->observations;++c->typedComparisons;++c->aliasComparisons;
  if(!xlsSemAliasScalarSame(r,actual))c->reads->complete=0;
 }else xlsSemReadsObserveSlow(ctx,name,2,actual);
 if(c->reads->complete)xlsSemAliasRemember(c,at,name,actual);
}
static int xlsSemAliasUpgrade(xls_sem_read_capture* c,XL6_SemContext* ctx,const char* name,const XL6_TypeInfo* actual)
{
 if(!xlsSemAliasCaptureReady(c,ctx,name))return 0;
 size_t at=xlsSemAliasSlot(c,name,actual);
 if(at==SIZE_MAX)return 0;
 xls_sem_read* r=xrtArrayGet(&c->reads->items,at);
 if(r->type!=2)return 0;
 ++c->observations;
 xstrbuf alias={0},full={0};size_t count=0;
 int same=actual && actual->kind==XL6_TYPE_ALIAS ?
  xlsSemReadAliasActual(actual,&alias,&count) && xlsSemReadSame(r,&alias,count) : xlsSemAliasScalarSame(r,actual);
 if(!same || !xlsSemReadTypeKey(ctx,name,&full,&count))goto decline;
 size_t limit=xlsSemReadBudget();if(limit>1024u*1024u)limit=1024u*1024u;
 if(c->reads->bytes<r->key.Capacity || c->reads->bytes-r->key.Capacity>limit ||
    full.Capacity>limit-(c->reads->bytes-r->key.Capacity))goto decline;
 c->reads->bytes=c->reads->bytes-r->key.Capacity+full.Capacity;
 xrtStrBufFree(&r->key);r->key=full;full=(xstrbuf){0};r->type=1;r->count=count;
 xlsSemReadAliasProjection(r,actual);
 xlsSemReadValueRemember(c,at,actual);xlsSemAliasRemember(c,at,name,actual);++c->aliasUpgrades;
 xrtStrBufFree(&alias);return 1;
decline:
 c->reads->complete=0;xrtStrBufFree(&alias);xrtStrBufFree(&full);return 1;
}
