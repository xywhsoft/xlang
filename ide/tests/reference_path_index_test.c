#include "services/project_references.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
static size_t checks,callocCalls,mallocCalls,failCalloc,failMalloc,comparisons;
static void* faultCalloc(size_t n,size_t size){return ++callocCalls==failCalloc ? NULL : xrtCalloc(n,size);}
static void* faultMalloc(size_t size){return ++mallocCalls==failMalloc ? NULL : malloc(size);}
static int countedEqual(const char* a,const char* b){++comparisons;return xidePathLexicalEqual(a,b);}
/* Exercise the actual private prepared-key implementation and its original
   comparator fallback,without adding fault/observation hooks to production. */
#define xrtCalloc faultCalloc
#define malloc faultMalloc
#define xidePathLexicalEqual countedEqual
#include "services/project_references.c"
#undef xrtCalloc
#undef malloc
#undef xidePathLexicalEqual
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"reference_path_index_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int run(void)
{
 const char* examples[]={"C:/xlang/目录😀/Main.xl","c:\\XLANG\\目录😀\\.\\main.xl","C:/xlang/目录😀/MAIN.xl/","//?/C:/xlang/目录😀/Main.xl","//server/share/Dir/main.xl","\\\\SERVER\\share\\Dir\\main.xl","C:/xlang/A/../B.xl","C:\\xlang\\B.xl","C:/xlang/ß.xl","C:/xlang/SS.xl","C:/xlang/I.xl","C:/xlang/ı.xl","//?/UNC/server/share/Dir/main.xl","C:/"};
 const size_t n=sizeof(examples)/sizeof(*examples);wchar_t* keys[14]={0};
 for(size_t i=0;i<n;++i){keys[i]=xidePathLexicalWide(examples[i]);CHECK(keys[i]);}
 for(size_t i=0;i<n;++i)for(size_t j=0;j<n;++j)CHECK((xidePathNativeKeyCompare(keys[i],keys[j])==0)==xidePathLexicalEqual(examples[i],examples[j]));
 CHECK(xidePathLexicalEqual(examples[0],examples[1]) && !xidePathLexicalEqual(examples[0],examples[3]));
 for(size_t i=0;i<n;++i)free(keys[i]);
 CHECK(!xidePathLexicalWide(NULL));
 char storage[64][160];const char* paths[64];reference_input in={0};reference_record record={0};
 for(size_t i=0;i<64;++i){snprintf(storage[i],sizeof(storage[i]),"C:/xlang/目录😀/member-%03zu.xl",63-i);paths[i]=i<n ? examples[i] : storage[i];in.docs[i].path=paths[i];}paths[63]=examples[0];in.docs[63].path=paths[63];in.count=64;record.overlays=(char**)paths;record.count=64;
 CHECK(pathIndexBuild(&in.paths,paths,64) && pathIndexClone(&record.paths,&in.paths));
 for(size_t i=0;i<64;++i){CHECK(in.paths.slots[i].key!=record.paths.slots[i].key && !wcscmp(in.paths.slots[i].key,record.paths.slots[i].key));}
 for(size_t round=0;round<8;++round)for(size_t i=0;i<64;++i){
  int expected=currentDocument(&in,paths[i]);comparisons=0;CHECK(indexedDocument(&in,paths[i])==expected && !comparisons);
  wchar_t* key=xidePathLexicalWide(paths[i]);CHECK(key && (pathIndexFind(&record.paths,key)>=0)==hasOverlay(&record,paths[i]));free(key);
 }
 CHECK(indexedDocument(&in,examples[0])==0 && indexedDocument(&in,examples[3])==3 && indexedDocument(&in,"C:/xlang/missing.xl")==-1);
 pathIndexUnit(&in.paths);CHECK(pathIndexFind(&record.paths,record.paths.slots[0].key)>=0);pathIndexUnit(&record.paths);
 CHECK(!in.paths.count && !in.paths.slots && !record.paths.count && !record.paths.slots);
 for(size_t i=0;i<64;++i){in.count=i+1;CHECK(pathIndexBuild(&in.paths,paths,in.count));for(size_t j=0;j<64;++j)CHECK(indexedDocument(&in,paths[j])==currentDocument(&in,paths[j]));pathIndexUnit(&in.paths);}
 in.count=64;failCalloc=callocCalls+1;CHECK(!pathIndexBuild(&in.paths,paths,64) && !in.paths.slots && !in.paths.count);failCalloc=0;comparisons=0;CHECK(indexedDocument(&in,paths[40])==currentDocument(&in,paths[40]) && comparisons>0);
 CHECK(pathIndexBuild(&in.paths,paths,64));failCalloc=callocCalls+1;CHECK(!pathIndexClone(&record.paths,&in.paths) && !record.paths.slots && !record.paths.count);failCalloc=0;
 for(size_t at=1;at<=64;++at){failMalloc=mallocCalls+at;CHECK(!pathIndexClone(&record.paths,&in.paths) && !record.paths.slots && !record.paths.count);failMalloc=0;}
 CHECK(pathIndexClone(&record.paths,&in.paths));pathIndexUnit(&record.paths);pathIndexUnit(&in.paths);
 const char* relative[]={"main.xl","C:main.xl","/rooted.xl","\\rooted.xl"};
 for(size_t i=0;i<4;++i){CHECK(!pathIndexBuild(&in.paths,relative+i,1) && !in.paths.count);in.count=1;in.docs[0].path=relative[i];CHECK(indexedDocument(&in,relative[i])==0);}
 const char* invalid[]={"C:/valid.xl","C:/invalid-\xff.xl"};CHECK(!pathIndexBuild(&in.paths,invalid,2) && !in.paths.count && !in.paths.slots);in.count=2;in.docs[0].path=invalid[0];in.docs[1].path=invalid[1];CHECK(indexedDocument(&in,invalid[1])==1);
 CHECK(!pathIndexBuild(&in.paths,paths,0) && !pathIndexBuild(&in.paths,paths,65));
 return 1;
}
int main(void){if(!run())return 1;printf("reference_path_index_test passed: %zu checks;exact original lexical/Unicode/device-prefix parity,first duplicate,1..64 documents,prepared lookup without original comparisons,independent keys,relative/invalid/fault fallbacks and partial clone cleanup\n",checks);return 0;}
