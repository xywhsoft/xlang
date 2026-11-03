
#include "analysis/frontend_internal.h"
#include <stdio.h>
#include "xl6_parser.c"
static void* live[8192];static size_t count,allocations;static int active,failed;
void* __real_xrtCalloc(size_t,size_t);void __real_xrtFree(void*);
void* __wrap_xrtCalloc(size_t n,size_t size)
{
 void* p=__real_xrtCalloc(n,size);
 if(active && p && n==1 && size==sizeof(XL6_AstNode)){
  size_t i=0;for(;i<count && live[i];++i){}
  if(i==8192)failed=1;else{live[i]=p;if(i==count)++count;++allocations;}
 }
 return p;
}
void __wrap_xrtFree(void* p)
{if(p)for(size_t i=0;i<count;++i)if(live[i]==p){live[i]=NULL;break;}__real_xrtFree(p);}
int main(void)
{
 const char* cases[]={
  "extern \"record\" struct Record @header(\"record.h\")\n c_int values[2]\nend\n",
  "extern \"handle*\" class Handle @header(\"handle.h\") @lifetime(\"caller\")\n native point handle\nend\n",
  "extern \"create\" @no_raise @factory(1, 0, dispose) @c_ref(1) func create(c_int mode, Record value) as Handle\n",
  "extern \"call\" @no_raise @c_static_callback(0) func call(c_callback func(c_int) as c_int callback) as c_int\n",
  "import dep as alias\nfrom dep import compute as run\nfunc main(...) as int\n return run(7)\nend\n"
 };
 size_t prefixes=0;active=1;
 for(size_t c=0;c<sizeof(cases)/sizeof(*cases);++c)for(size_t end=0;end<=strlen(cases[c]);++end){
  char source[512];memcpy(source,cases[c],end);source[end]=0;
  XL6_ParseResult parsed;xl6ParseResultInit(&parsed);xl6ParseSource(source,"foreign-stack.xl",&parsed);xl6ParseResultFree(&parsed);
  if(failed)return 1;
  for(size_t i=0;i<count;++i)if(live[i]){XL6_AstNode* n=live[i];fprintf(stderr,"foreign parser ownership prefix %zu/%zu leaks AST %s %s\n",c,end,xl6AstKindName(n->kind),n->text ? n->text : "");return 1;}
  ++prefixes;
 }
 active=0;printf("foreign parser ownership passed: %zu prefixes,%zu actual native AST allocations\n",prefixes,allocations);return 0;
}
