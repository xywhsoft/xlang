#include "services/catalog_lookup.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
struct xide_catalog_lookup {const xide_command* entries;size_t count;uint32_t *full,*shortName,*id;};
static const char* key(const xide_catalog_lookup* x,uint32_t i,int kind) {
 const xide_command* c=x->entries+i;
 if(kind==2)return c->id;
 const char* p=kind ? strrchr(c->name,'.'):NULL;return p ? p+1:c->name;
}
static int compare(const xide_catalog_lookup* x,uint32_t a,uint32_t b,int kind) {
 int n=strcmp(key(x,a,kind),key(x,b,kind));if(!n && kind!=2)n=strcmp(x->entries[a].language,x->entries[b].language);return n ? n:a<b ? -1:a>b;
}
static void sort(xide_catalog_lookup* x,uint32_t* a,uint32_t* tmp,size_t first,size_t end,int kind) {
 if(end-first<2)return;
 size_t mid=first+(end-first)/2;sort(x,a,tmp,first,mid,kind);sort(x,a,tmp,mid,end,kind);
 size_t l=first,r=mid,n=first;while(l<mid || r<end)tmp[n++]=(r==end || (l<mid && compare(x,a[l],a[r],kind)<=0)) ? a[l++]:a[r++];
 memcpy(a+first,tmp+first,(end-first)*sizeof(*a));
}
xide_catalog_lookup* xideCatalogLookupCreate(const xide_command_catalog* c) {
 if(!c || c->count>UINT32_MAX || c->count>SIZE_MAX/(4*sizeof(uint32_t)))return NULL;
 xide_catalog_lookup* x=calloc(1,sizeof(*x));if(!x)return NULL;x->entries=c->entries;x->count=c->count;
 size_t n=c->count ? c->count:1;uint32_t* all=malloc(3*n*sizeof(*all)),*tmp=malloc(n*sizeof(*tmp));if(!all || !tmp){free(all);free(tmp);free(x);return NULL;}
 x->full=all;x->shortName=all+n;x->id=all+2*n;
 for(size_t i=0;i<c->count;++i)x->full[i]=x->shortName[i]=x->id[i]=(uint32_t)i;
 for(int k=0;k<3;++k)sort(x,k==0 ? x->full:k==1 ? x->shortName:x->id,tmp,0,c->count,k);
 free(tmp);
 return x;
}
void xideCatalogLookupDestroy(xide_catalog_lookup* x){if(x){free(x->full);free(x);}}
static size_t bound(const xide_catalog_lookup* x,const uint32_t* a,const char* name,const char* language,int kind,int upper) {
 size_t lo=0,hi=x->count;while(lo<hi){size_t m=lo+(hi-lo)/2;int n=strcmp(key(x,a[m],kind),name);if(!n && language)n=strcmp(x->entries[a[m]].language,language);if(n<0 || (upper && !n))lo=m+1;else hi=m;}return lo;
}
size_t xideCatalogLookupName(const xide_catalog_lookup* x,const char* name,const char* language,int shortName,uint32_t* out,size_t capacity,size_t* total) {
 if(total)*total=0;
 if(!x || !name || !language)return 0;
 const uint32_t* a=shortName ? x->shortName:x->full;
 size_t first=bound(x,a,name,language,shortName,0),end=bound(x,a,name,language,shortName,1),n=end-first;
 if(total)*total=n;
 if(n>capacity)n=capacity;
 if(out && n)memcpy(out,a+first,n*sizeof(*out));
 return out ? n:0;
}
const xide_command* xideCatalogLookupId(const xide_catalog_lookup* x,const char* id) {
 if(!x || !id)return NULL;
 size_t i=bound(x,x->id,id,NULL,2,0);return i<x->count && !strcmp(key(x,x->id[i],2),id) ? x->entries+x->id[i]:NULL;
}
size_t xideCatalogLookupBytes(const xide_catalog_lookup* x){return x ? sizeof(*x)+3*(x->count ? x->count:1)*sizeof(uint32_t):0;}
