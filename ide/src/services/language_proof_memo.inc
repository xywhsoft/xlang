typedef struct proof_memo_item {xvalue* report;xls_text* text;} proof_memo_item;
struct xide_language_proof_memo {xmap index;xarray items;size_t keyBytes;int failed;};
static int proofMemoInit(struct xide_language_proof_memo* m)
{return xrtMapInit(&m->index,sizeof(size_t)) && xrtArrayInit(&m->items,sizeof(proof_memo_item));}
static void proofMemoUnit(struct xide_language_proof_memo* m)
{for(size_t i=0;i<m->items.Count;++i){proof_memo_item* p=xrtArrayGet(&m->items,i);xrtValueRelease(p->report);xlsTextRelease(p->text);}xrtArrayUnit(&m->items);xrtMapUnit(&m->index);}
static proof_memo_item* proofMemoSlot(struct xide_language_proof_memo* m,const xvalue* item,xide_model_error* error)
{
 const char* path=string(get(item,"path"));wchar_t* key=path?xidePathLexicalWide(path):NULL;
 if(!key || m->failed){free(key);m->failed=1;fail(error,"Cannot prepare bounded proof memo path.");return NULL;}
 size_t bytes=(wcslen(key)+1)*sizeof(wchar_t);bool added=false;
 size_t* index=xrtMapGetOrAdd(&m->index,(xbytesview){(const unsigned char*)key,bytes},&added);free(key);
 if(!index){m->failed=1;fail(error,"Cannot allocate proof memo index.");return NULL;}
 if(added){
  *index=SIZE_MAX;
  if(m->items.Count>=65536 || bytes>32u*1024u*1024u-m->keyBytes){m->failed=1;fail(error,"Proof memo path/entry quota exceeded.");return NULL;}
  proof_memo_item empty={0};if(!xrtArrayPush(&m->items,&empty)){m->failed=1;fail(error,"Cannot allocate proof memo rows.");return NULL;}
  m->keyBytes+=bytes;*index=m->items.Count-1;
 }
 if(*index>=m->items.Count){m->failed=1;return NULL;}return xrtArrayGet(&m->items,*index);
}
int xideLanguageProofMemoMatch(struct xide_language_proof_memo* m,const xvalue* item,const xls_text* text,xide_model_error* error)
{
 if(!m)return 0;
 proof_memo_item* row=proofMemoSlot(m,item,error);if(!row)return -1;
 return row->report && row->text==text && xrtValueEqual(row->report,item);
}
int xideLanguageProofMemoRemember(struct xide_language_proof_memo* m,const xvalue* item,const xls_text* text,xide_model_error* error)
{
 if(!m)return 1;
 proof_memo_item* row=proofMemoSlot(m,item,error);if(!row)return 0;
 xvalue* retained=xrtValueRetain(item);xls_text* body=xlsTextRetain((xls_text*)text);xrtValueRelease(row->report);xlsTextRelease(row->text);row->report=retained;row->text=body;return 1;
}
