/* Original complete ordered conversion;independent verification/fallback. */
static int convert(endpoint* ends,size_t used,const xide_semantic_source* sources)
{
 qsort(ends,used,sizeof(*ends),compareEndpoint); int source=0,line=1,column=1,character=0; size_t at=0,size=0; const char* text=NULL;
 for(size_t i=0;i<used;++i) {
  endpoint* e=&ends[i];
  if(e->source!=source) { source=e->source; text=sources[source-1].text; if(!text) return 0; size=strlen(text); at=0; line=column=1; character=sources[source-1].utf16Prefix; }
  if((size_t)e->offset>size) return 0;
  while(at<(size_t)e->offset) {
   unsigned char c=(unsigned char)text[at]; size_t n=c<128 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
   if(c=='\r' && at+1<size && text[at+1]=='\n') n=2;
   if(n>size-at || at+n>(size_t)e->offset) return 0;
   if(c=='\r' || c=='\n') { ++line; column=1; character=0; } else { ++column; character+=n==4 ? 2 : 1; }
   at+=n;
  }
  if(e->line!=line || e->column!=column) return 0;
  *e->output=(point){line-1,character};
 }
 return 1;
}
