/* Private native coordinate projection. -1 declines optional scratch,0 rejects invalid
 * coordinates,1 verifies every endpoint. The original sorted pass remains. */
static int convertDense(endpoint* ends,size_t used,const xide_semantic_source* sources,size_t count,size_t budget)
{
 if(!used)return 1;
 if(!sources || !count || count>512 || used>=UINT32_MAX)return -1;
 size_t offsets[512]={0},sizes[512]={0},maximum[512]={0},buckets=0;unsigned char present[512]={0};
 for(size_t i=0;i<used;++i){const endpoint* e=&ends[i];if(e->source<1 || (size_t)e->source>count || e->offset<0)return 0;
  size_t s=(size_t)e->source-1;present[s]=1;if((size_t)e->offset>maximum[s])maximum[s]=(size_t)e->offset;}
 for(size_t s=0;s<count;++s)if(present[s]){
  if(!sources[s].text)return 0;
  size_t n=strlen(sources[s].text);if(maximum[s]>n)return 0;
  if(maximum[s]==SIZE_MAX || maximum[s]+1>SIZE_MAX-buckets)return -1;
  offsets[s]=buckets;sizes[s]=n;buckets+=maximum[s]+1;
 }
 if(buckets>SIZE_MAX-used)return -1;
 size_t slots=buckets+used;
 /* Pinned XRT starts at8 slots and grows by capacity/2+8. Reserve a
  * conservative2*requested+8 bound before allocation,then verify capacity.
  * Both payload and allocation refusal keep the original sorted fallback. */
 size_t allowance=budget/sizeof(uint32_t);
 if(allowance<8 || slots>(allowance-8)/2)return -1;
 xarray scratch={0};if(!xrtArrayInit(&scratch,sizeof(uint32_t)))return -1;
 if(!xrtArrayResize(&scratch,slots) || scratch.Capacity>budget/sizeof(uint32_t)){xrtArrayUnit(&scratch);return -1;}
 uint32_t* heads=(uint32_t*)scratch.Data;uint32_t* next=heads+buckets;
 /* XRT resize zero-initializes every new element;these are fresh arrays. */
 for(size_t i=0;i<used;++i){size_t key=offsets[ends[i].source-1]+(size_t)ends[i].offset;next[i]=heads[key];heads[key]=(uint32_t)i+1;}
 size_t visited=0;int okay=1;
 for(size_t s=0;okay && s<count;++s)if(present[s]){
  const char* text=sources[s].text;size_t at=0,size=sizes[s];int line=1,column=1,character=sources[s].utf16Prefix;
  for(;;){
   for(uint32_t index=heads[offsets[s]+at];index;index=next[index-1]){
    endpoint* e=&ends[index-1];if(e->line!=line || e->column!=column){okay=0;break;}
    *e->output=(point){line-1,character};++visited;
   }
   if(!okay || at==maximum[s])break;
   unsigned char c=(unsigned char)text[at];size_t n=c<128 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
   if(c=='\r' && at+1<size && text[at+1]=='\n')n=2;
   if(n>size-at || n>maximum[s]-at){okay=0;break;}
   if(c=='\r' || c=='\n'){++line;column=1;character=0;}else{++column;character+=n==4 ? 2 : 1;}
   at+=n;
  }
 }
 /* Endpoints inside UTF8/CRLF sequences were never visited and must reject. */
 okay=okay && visited==used;xrtArrayUnit(&scratch);return okay;
}
