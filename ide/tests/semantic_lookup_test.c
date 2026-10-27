#include "services/semantic_definition.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
/* Private real implementation: fail each XRT array operation independently;
 * allocations in the input DOM/runtime are not replaced. */
static size_t lookupOperations,lookupFail,lookupArrays;
static int lookupBadLifetime;
static int lookupOperation(void) {return ++lookupOperations!=lookupFail;}
static bool lookupInit(xarray* a,size_t size)
{if(!lookupOperation())return false;bool ok=xrtArrayInit(a,size);if(ok)++lookupArrays;return ok;}
static bool lookupAppend(xarray* a,const void* item,size_t count)
{return lookupOperation() && xrtArrayAppend(a,item,count);}
static bool lookupSort(xarray* a,xarraycompare compare)
{return lookupOperation() && xrtArraySort(a,compare);}
static bool lookupResize(xarray* a,size_t count)
{return lookupOperation() && xrtArrayResize(a,count);}
static bool lookupReserve(xarray* a,size_t count)
{return lookupOperation() && xrtArrayReserve(a,count);}
static void lookupUnit(xarray* a)
{if(!lookupArrays)lookupBadLifetime=1;else --lookupArrays;xrtArrayUnit(a);}
#define xrtArrayInit lookupInit
#define xrtArrayAppend lookupAppend
#define xrtArraySort lookupSort
#define xrtArrayResize lookupResize
#define xrtArrayReserve lookupReserve
#define xrtArrayUnit lookupUnit
#include "../src/services/semantic_definition.c"
#undef xrtArrayInit
#undef xrtArrayAppend
#undef xrtArraySort
#undef xrtArrayResize
#undef xrtArrayReserve
#undef xrtArrayUnit

static size_t checks;
#define CHECK(e) do {++checks;if(!(e)){fprintf(stderr,"semantic_lookup_test:%d: %s\n",__LINE__,#e);return 0;}}while(0)
typedef struct lookup_input {char *path,*text;size_t *starts,count;xvalue* index;} lookup_input;
static xvalue* lookupRange(size_t start,size_t end,size_t line,size_t column)
{
 xvalue* v=xrtValueObject();int ok=v && put(v,"startOffset",xrtValueInt((int64)start)) && put(v,"endOffset",xrtValueInt((int64)end)) &&
  put(v,"startLine",xrtValueInt((int64)line+1)) && put(v,"startColumn",xrtValueInt((int64)column+1)) &&
  put(v,"endLine",xrtValueInt((int64)line+1)) && put(v,"endColumn",xrtValueInt((int64)(column+end-start)+1));
 if(!ok){xrtValueRelease(v);v=NULL;}return v;
}
static xvalue* lookupNode(size_t id,size_t symbol,int declared,size_t start,size_t end,size_t line)
{
 size_t name=start+(declared ? 4 : 0),nameEnd=name+10;xvalue* v=xrtValueObject();
 int ok=v && put(v,"id",xrtValueInt((int64)id)) && put(v,"parent",xrtValueInt(0)) && put(v,"sourceId",xrtValueInt(1)) &&
  put(v,"selectionSourceId",xrtValueInt(declared ? 1 : 0)) && put(v,"kind",xrtValueString(sv(declared ? "var_decl" : "name"))) &&
  put(v,"declaredSymbolId",xrtValueInt(declared ? (int64)symbol : 0)) && put(v,"symbolId",xrtValueInt(declared ? (int64)symbol : 0)) &&
  put(v,"targetSymbolId",xrtValueInt(declared ? 0 : (int64)symbol)) && put(v,"compoundOperatorSymbolId",xrtValueInt(0)) &&
  put(v,"typeId",xrtValueInt(0)) && put(v,"typeAbiId",xrtValueString(sv("0000000000000000"))) && put(v,"type",xrtValueNull()) &&
  put(v,"declaredTypeId",xrtValueInt(0)) && put(v,"referencedTypeId",xrtValueInt(0)) &&
  put(v,"range",lookupRange(start,end,line,0)) && put(v,"tokenRange",lookupRange(name,nameEnd,line,declared ? 4 : 0)) &&
  put(v,"selectionRange",declared ? lookupRange(name,nameEnd,line,4) : xrtValueNull());
 if(!ok){xrtValueRelease(v);v=NULL;}return v;
}
static int lookupInput(lookup_input* f,size_t count)
{
 memset(f,0,sizeof(*f));f->count=count;f->path=xrtPathAbs("semantic-lookup-memory.xl");f->text=malloc(count*64+1);f->starts=calloc(count*2+1,sizeof(*f->starts));f->index=xrtValueObject();xvalue* nodes=xrtValueArray();
 CHECK(f->path && f->text && f->starts && f->index && nodes);
 f->text[0]=0;
 CHECK(put(f->index,"schema",xrtValueInt(1)) && put(f->index,"scope",xrtValueString(sv("checked-compilation"))) && put(f->index,"complete",xrtValueBool(true)) &&
  put(f->index,"offsetEncoding",xrtValueString(sv("utf-8-after-bom"))) && put(f->index,"columnEncoding",xrtValueString(sv("unicode-scalar-1-based"))) &&
  put(f->index,"rangeConvention",xrtValueString(sv("half-open"))) && put(f->index,"nodes",nodes));
 size_t used=0;
 for(size_t i=0;i<count*2;++i) {
  f->starts[i]=used;int n=snprintf(f->text+used,count*64+1-used,i<count ? "int value%05zu = 0\n" : "value%05zu\n",i%count);CHECK(n>0 && n<64);used+=(size_t)n;
  xvalue* row=lookupNode(i+1,i%count+100,i<count,f->starts[i],used-1,i);CHECK(row && xrtValueArrayAppendNew(nodes,row));
 }
 f->starts[count*2]=used;
 if(count) {
  /* One valid enclosing token spans the entire source. A prefix-maximum
     backwards scan would visit every preceding node for a late query. */
  xvalue* enclosing=lookupNode(count*2+1,0,0,0,used,0),*range=lookupRange(0,used,0,0);CHECK(enclosing && range);
  CHECK(put(range,"endLine",xrtValueInt((int64)count*2+1)) && put(range,"endColumn",xrtValueInt(1)) &&
   put(enclosing,"kind",xrtValueString(sv("program"))) && put(enclosing,"range",xrtValueDeepClone(range)) && put(enclosing,"tokenRange",range) && xrtValueArrayAppendNew(nodes,enclosing));
 }
 return 1;
}
static void lookupInputUnit(lookup_input* f)
{xrtFree(f->path);free(f->text);free(f->starts);xrtValueRelease(f->index);memset(f,0,sizeof(*f));}
static xide_semantic_definitions* lookupCreate(const lookup_input* f)
{xide_semantic_source source={f->path,f->text,1,0};xide_model_error error={0};return xideSemanticDefinitionsCreate(f->index,&source,1,&error);}
static int lookupFaults(void)
{
 lookup_input f;CHECK(lookupInput(&f,3));xvalue* original=xrtValueDeepClone(f.index);CHECK(original);
 lookupOperations=lookupFail=0;xide_semantic_definitions* d=lookupCreate(&f);CHECK(d && lookupArrays==2);size_t operations=lookupOperations;xideSemanticDefinitionsDestroy(d);CHECK(!lookupArrays && !lookupBadLifetime && operations>10);
 for(size_t fail=1;fail<=operations;++fail) {
  lookupOperations=0;lookupFail=fail;d=lookupCreate(&f);CHECK(!d && !lookupArrays && !lookupBadLifetime && xrtValueEqual(original,f.index));
  lookupOperations=lookupFail=0;d=lookupCreate(&f);CHECK(d);xideSemanticDefinitionsDestroy(d);CHECK(!lookupArrays);
 }
 lookupFail=0;xrtValueRelease(original);lookupInputUnit(&f);return 1;
}
static int lookupQueries(size_t count)
{
 lookup_input f;CHECK(lookupInput(&f,count));lookupOperations=lookupFail=0;xide_semantic_definitions* d=lookupCreate(&f);CHECK(d && lookupArrays==2 && d->references.Count==count*2);
 size_t operations=lookupOperations,maximumWork=0;lookupFail=operations+1;
 for(size_t i=0;i<count;++i) {
  xide_semantic_reference_view view={0};xide_semantic_reference a={0},b={0};
  CHECK(xideSemanticReferences(d,f.path,(int)(count+i),1,1,&view) && view.count==2);
  CHECK(xideSemanticReferenceAt(&view,0,&a) && xideSemanticReferenceAt(&view,1,&b) && !xideSemanticReferenceAt(&view,2,&b));
  CHECK(a.declaration && a.start.line==(int)i && a.start.character==4 && a.start.offset==(int)f.starts[i]+4 && a.end.offset==a.start.offset+10 && !strcmp(a.path,f.path));
  CHECK(xideSemanticReferenceAt(&view,1,&b) && !b.declaration && b.start.line==(int)(count+i) && !b.start.character && b.start.offset==(int)f.starts[count+i]);
  CHECK(xideSemanticReferences(d,f.path,(int)i,5,0,&view) && view.count==1 && xideSemanticReferenceAt(&view,0,&b) && !b.declaration && b.start.line==(int)(count+i));
  size_t declaration=SIZE_MAX,work=SIZE_MAX,selected=selectNodeIndexed(d,1,(int)(count+i),1,&declaration,&work);
  CHECK(selected==count+i && declaration==i && work<=64);if(work>maximumWork)maximumWork=work;
 }
 CHECK(lookupOperations==operations);
 xide_semantic_reference_view view={0};xide_semantic_reference item={0};CHECK(xideSemanticReferences(d,f.path,0,0,1,&view) && !view.count && !xideSemanticReferenceAt(&view,0,&item));
 CHECK(!xideSemanticReferences(d,NULL,0,0,1,&view) && !view.definitions && !view.count);
 CHECK(!xideSemanticReferences(d,f.path,-1,0,1,&view) && !xideSemanticReferences(d,f.path,0,-1,1,&view) && !xideSemanticReferences(d,f.path,0,0,2,&view));
 CHECK(!xideSemanticReferences(NULL,f.path,0,0,1,&view) && !xideSemanticReferences(d,f.path,0,0,1,NULL));
 CHECK(!xideSemanticReferenceAt(NULL,0,&item) && !xideSemanticReferenceAt(&view,0,NULL));
 CHECK(xideSemanticReferences(d,f.path,(int)count,1,0,&view) && view.count==1);xide_semantic_reference_view bad=view;bad.first=SIZE_MAX;CHECK(!xideSemanticReferenceAt(&bad,0,&item));bad=view;bad.count=SIZE_MAX;CHECK(!xideSemanticReferenceAt(&bad,0,&item));bad=view;bad.excluded=SIZE_MAX-1;CHECK(!xideSemanticReferenceAt(&bad,0,&item));
 size_t charged=d->cursors.Capacity*d->cursors.ItemSize+d->references.Capacity*d->references.ItemSize;CHECK(charged<=6u*1024u*1024u);
 /* Views borrow only the owning navigation table, never the original DOM,
    text buffer, compiler AST or caller path. */
 lookupInputUnit(&f);CHECK(xideSemanticReferenceAt(&view,0,&item) && item.start.line==(int)count && strstr(item.path,"semantic-lookup-memory.xl"));
 lookupFail=0;xideSemanticDefinitionsDestroy(d);CHECK(!lookupArrays && !lookupBadLifetime);
 printf("lookup scale: %zu declarations, %zu nodes including full-source token, maximum indexed cursor work=%zu, charged array capacity=%zu bytes.\n",count,count*2+1,maximumWork,charged);return 1;
}
#ifdef XLS_COMPILER_XRT
static int typedLimits(void)
{
 xide_semantic_source source={"/typed-capacity.xl","",1,0};
 size_t low=0,high=CURSOR_SELECTION,budget=0,small=0;
 while(low+1<high) {size_t mid=low+(high-low)/2;if(typedCapacity(mid,&source,1,&budget))low=mid;else high=mid;}
 CHECK(low>65536 && typedCapacity(low,&source,1,&budget) && !typedCapacity(low+1,&source,1,&budget));
 CHECK(!typedCapacity(SIZE_MAX,&source,1,&budget) && !typedCapacity(CURSOR_SELECTION,&source,1,&budget));
 CHECK(typedCapacity(0,&source,1,&small) && small==8u*1024u*1024u);
 CHECK(typedCapacity(low,&source,1,&budget) && budget<small);
 source.path=NULL;CHECK(!typedCapacity(0,&source,1,&budget));return 1;
}
#endif
int main(void)
{
 lookup_input empty;if(!lookupInput(&empty,0))return 1;xide_semantic_definitions* zero=lookupCreate(&empty);xide_semantic_reference_view view={0};
 if(!zero || zero->cursors.Count || zero->references.Count || !xideSemanticReferences(zero,empty.path,0,0,1,&view) || view.count)return 1;
 xideSemanticDefinitionsDestroy(zero);lookupInputUnit(&empty);
 if(!lookupFaults() || !lookupQueries(1) || !lookupQueries(3) || !lookupQueries(5000))return 1;
#ifdef XLS_COMPILER_XRT
 if(!typedLimits())return 1;
#endif
 printf("semantic_lookup_test passed: %zu checks; every XRT index operation failure/recovery, immutable input, same-span selection precedence, typed reference/declaration views, guarded bounds, independent ownership and cached lookup work.\n",checks);return 0;
}
