/* Only immutable final-function identity plus complete source-coordinate proof
 * authorizes a construction loan. Both old and new checked owners stay alive
 * in the caller. This payload retains no previous owner/index/report/AST. */
#define NATIVE_POINT_PROOF_BYTES (8u*1024u*1024u)
static int nativeFunctionKeyCompare(const void* a,const void* b)
{const native_function_key *x=a,*y=b;return (x->identity>y->identity)-(x->identity<y->identity);}
static int nativeFunctionSameSources(const native_points* a,const native_points* b)
{
 if(!a || !b || a->sourcesCount!=b->sourcesCount)return 0;
 for(size_t i=0;i<a->sourcesCount;++i)if(a->sources[i].kind!=b->sources[i].kind ||
  a->sources[i].utf16Prefix!=b->sources[i].utf16Prefix || strcmp(a->sources[i].path,b->sources[i].path))return 0;
 return 1;
}
static void nativeFunctionsBegin(xide_semantic_definitions* d,const xide_semantic_definitions* old,size_t extra,native_reuse* reuse)
{
 native_points* p=d->native;size_t count=xlsSemanticIndexSegmentCount(p->index);
 if(!count || count>extra/sizeof(native_function_points))return;
 p->functions=calloc(count,sizeof(*p->functions));if(!p->functions)return;
 p->functionCapacity=count;size_t expected=0;
 for(size_t slot=0;slot<count;++slot){xls_index_segment info;
  if(!xlsSemanticIndexSegment(p->index,slot,&info))goto decline;
  if(!info.count)continue;
  if(info.first!=expected || info.count>p->nodesCount-expected)goto decline;
  p->functions[p->functionCount++].info=info;expected+=info.count;
 }
 if(expected!=p->nodesCount)goto decline;
 extra-=count*sizeof(native_function_points);
 if(!old || !nativeFunctionSameSources(p,old->native) || !old->native->functions)return;
 const native_points* prior=old->native;size_t available=extra/sizeof(native_function_key);
 if(available<8 || prior->functionCount>(available-8)/2)return;
 if(!xrtArrayInit(&reuse->keys,sizeof(native_function_key)) || !xrtArrayReserve(&reuse->keys,prior->functionCount))return;
 if(reuse->keys.Capacity>available){xrtArrayUnit(&reuse->keys);memset(&reuse->keys,0,sizeof(reuse->keys));return;}
 for(size_t i=0;i<prior->functionCount;++i)if(prior->functions[i].ready){
  native_function_key key={(uintptr_t)prior->functions[i].info.identity,i};
  if(!xrtArrayPush(&reuse->keys,&key)){xrtArrayUnit(&reuse->keys);memset(&reuse->keys,0,sizeof(reuse->keys));return;}
 }
 if(!xrtArraySort(&reuse->keys,nativeFunctionKeyCompare))return;
 /* A changed symbol allocation can replace every immutable function. Do
  * not build a second cursor cache or sort its segments when no old ready
  * function can be reused. This generation uses the original complete path;
  * a later generation can seed the optional cache again. */
 int shared=0;
 for(size_t i=0;i<p->functionCount && !shared;++i){const xls_index_segment* info=&p->functions[i].info;
  if(!info->reusable)continue;
  native_function_key key={(uintptr_t)info->identity,0};
  const native_function_key* match=bsearch(&key,reuse->keys.Data,reuse->keys.Count,sizeof(key),nativeFunctionKeyCompare);
  if(match){const xls_index_segment* before=&prior->functions[match->slot].info;
   shared=before->count==info->count && before->source==info->source && before->proofFirst==info->proofFirst && before->proofEnd==info->proofEnd;}
 }
 if(!shared){xrtArrayUnit(&reuse->keys);memset(&reuse->keys,0,sizeof(reuse->keys));goto decline;}
 reuse->previous=prior;return;
decline:
 free(p->functions);p->functions=NULL;p->functionCount=p->functionCapacity=0;
}
static const native_function_points* nativeFunctionFind(const native_reuse* reuse,const void* identity,size_t* slot)
{
 if(!reuse->previous || !identity)return NULL;
 native_function_key key={(uintptr_t)identity,0};
 const native_function_key* found=bsearch(&key,reuse->keys.Data,reuse->keys.Count,sizeof(key),nativeFunctionKeyCompare);
 if(!found)return NULL;
 *slot=found->slot;return &reuse->previous->functions[found->slot];
}
static int nativeFunctionNodeIn(const native_function_points* part,size_t node)
{return node>=part->info.first && node-part->info.first<part->info.count;}
static int nativeFunctionReuseFacts(xide_semantic_definitions* d,native_function_points* part,native_reuse* reuse,size_t* textCharge)
{
 native_points* p=d->native;const native_points* previous=reuse->previous;size_t slot=0;
 if(!part->info.reusable || !part->info.first || !previous)return 0;
 const native_function_points* old=nativeFunctionFind(reuse,part->info.identity,&slot);
 if(!old || !old->ready || old->info.count!=part->info.count || old->info.source!=part->info.source ||
  old->info.proofFirst!=part->info.proofFirst || old->info.proofEnd!=part->info.proofEnd ||
  old->parentShape!=p->shapes[0] || old->info.first>previous->nodesCount || old->info.count>previous->nodesCount-old->info.first ||
  old->declFirst>previous->cachedDeclarationCount || old->declCount>previous->cachedDeclarationCount-old->declFirst ||
  old->typeFirst>previous->cachedTypeCount || old->typeCount>previous->cachedTypeCount-old->typeFirst ||
  old->cursorFirst>previous->cachedCursorCount || old->cursorCount>previous->cachedCursorCount-old->cursorFirst ||
  old->declCount>p->nodesCount+1-d->declCount || old->typeCount>p->nodesCount+1-d->typeCount ||
  part->info.canonicalCharge>4u*1024u*1024u-*textCharge)return 0;
 unsigned source=part->info.source;if(!source || source>p->sourcesCount)return 0;
 if(old->proofStart>part->info.proofEnd || part->info.proofEnd>xlsTextSize(p->texts[source-1]))return 0;
 size_t bytes=part->info.proofEnd-old->proofStart;
 if(bytes>NATIVE_POINT_PROOF_BYTES-reuse->proofBytes)return 0;
 /* Charge attempts before either Unicode scan,including failed proofs. */
 reuse->proofBytes+=bytes;
 xls_position start,end;
 if(!xlsTextPosition(p->texts[source-1],old->proofStart,&start) || start.line!=old->proofStartPosition.line || start.character!=old->proofStartPosition.character ||
  !xlsTextPosition(p->texts[source-1],part->info.proofEnd,&end) || end.line!=old->proofEndPosition.line || end.character!=old->proofEndPosition.character)return 0;
 p->reuse.proofBytes+=bytes;
 if(memcmp(xlsTextData(p->texts[source-1])+old->proofStart,xlsTextData(previous->texts[source-1])+old->proofStart,bytes))return 0;
 for(size_t i=0;i<old->declCount;++i)if(!nativeFunctionNodeIn(old,previous->cachedDeclarations[old->declFirst+i].node))return 0;
 for(size_t i=0;i<old->typeCount;++i)if(!nativeFunctionNodeIn(old,previous->cachedTypes[old->typeFirst+i].node))return 0;
 for(size_t i=0;i<old->cursorCount;++i)if(!nativeFunctionNodeIn(old,previous->cachedCursors[old->cursorFirst+i].node&~CURSOR_SELECTION))return 0;
 memcpy(p->shapes+part->info.first,previous->shapes+old->info.first,part->info.count*sizeof(*p->shapes));
 for(size_t i=0;i<old->declCount;++i){declaration row=previous->cachedDeclarations[old->declFirst+i];row.node=part->info.first+row.node-old->info.first;d->declarations[d->declCount++]=row;}
 for(size_t i=0;i<old->typeCount;++i){declaration row=previous->cachedTypes[old->typeFirst+i];row.node=part->info.first+row.node-old->info.first;d->typeDeclarations[d->typeCount++]=row;}
 part->declCount=old->declCount;part->typeCount=old->typeCount;part->nameCharge=old->nameCharge;part->borrowed=(int)slot+1;
 *textCharge+=part->info.canonicalCharge;++p->reuse.functions;p->reuse.facts+=part->info.count;return 1;
}
static int nativeFunctionReuseCursors(xide_semantic_definitions* d,native_function_points* part,const native_reuse* reuse)
{
 const native_points* prior=reuse->previous;const native_function_points* old=&prior->functions[part->borrowed-1];
 if(!old->cursorCount)return 1;
 cursor_row* rows=xrtArrayAdd(&d->cursors,old->cursorCount);if(!rows)return 0;
 for(size_t i=0;i<old->cursorCount;++i){cursor_row row=prior->cachedCursors[old->cursorFirst+i];
  row.node=(row.node&CURSOR_SELECTION)|(uint32_t)(part->info.first+(row.node&~CURSOR_SELECTION)-old->info.first);row.maximumLine=row.maximumCharacter=0;rows[i]=row;}
 d->native->reuse.cursors+=old->cursorCount;return 1;
}
static void nativeFunctionsSeal(xide_semantic_definitions* d)
{
 native_points* p=d->native;if(!p->functions)return;
 size_t bytes=d->cursors.Count*sizeof(cursor_row)+(d->declCount+d->typeCount)*sizeof(declaration),resident=nativePointsBytes(d);
 if(resident>TYPED_DEFINITION_BYTES || bytes>TYPED_DEFINITION_BYTES-resident)return;
 p->cachedCursors=d->cursors.Count ? malloc(d->cursors.Count*sizeof(cursor_row)) : NULL;
 p->cachedDeclarations=d->declCount ? malloc(d->declCount*sizeof(declaration)) : NULL;
 p->cachedTypes=d->typeCount ? malloc(d->typeCount*sizeof(declaration)) : NULL;
 if((d->cursors.Count && !p->cachedCursors) || (d->declCount && !p->cachedDeclarations) || (d->typeCount && !p->cachedTypes)){
  free(p->cachedCursors);free(p->cachedDeclarations);free(p->cachedTypes);p->cachedCursors=NULL;p->cachedDeclarations=p->cachedTypes=NULL;return;
 }
 p->cachedCursorCount=d->cursors.Count;p->cachedDeclarationCount=d->declCount;p->cachedTypeCount=d->typeCount;
 if(d->cursors.Count)memcpy(p->cachedCursors,d->cursors.Data,d->cursors.Count*sizeof(cursor_row));
 if(d->declCount)memcpy(p->cachedDeclarations,d->declarations,d->declCount*sizeof(declaration));
 if(d->typeCount)memcpy(p->cachedTypes,d->typeDeclarations,d->typeCount*sizeof(declaration));
 for(size_t i=0;i<p->functionCount;++i){native_function_points* part=&p->functions[i];xls_position first;
  if(!part->info.reusable || !part->info.source || part->info.source>p->sourcesCount)continue;
  const xls_text* text=p->texts[part->info.source-1];
  if(!xlsTextPosition(text,part->info.proofFirst,&first) || !xlsTextOffset(text,(xls_position){first.line,0},&part->proofStart) ||
   !xlsTextPosition(text,part->proofStart,&part->proofStartPosition) || !xlsTextPosition(text,part->info.proofEnd,&part->proofEndPosition))continue;
  part->ready=1;
 }
}
