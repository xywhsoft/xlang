#define main xl6ExistingSetConsumersMain
#include "stdlib_set_consumers_native_host.c"
#undef main
#include "src/runtime/xl6_set_methods.h"
#include "src/runtime/xl6_set_import_chain.h"
#include "src/runtime/xl6_collection_count_all.h"

static xvalue* importSeed(xvalue** seed,const xvalue* source)
{
    const XL6_CollectionImportProviderV1* providers[]={xl6SetImportProviderV1()};
    const XL6_CollectionImportPlanV1 plan={.size=sizeof(plan),.providers=providers,
        .flags=XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1};
    xvalue* result=xl6CollectionImportChainTakeV1(seed,1,source,&plan);
    assert(!*seed); return result;
}
static xvalue* import(const xvalue* source) { xvalue* seed=create(false); return importSeed(&seed,source); }
static void sourceVectors(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* source=input(false),*query=text();
    assert(xl6SetValueCountV1(source)==2 && xl6CollectionCountAllV1(source)==2);
    assert(xl6SetValueAddTakeV1(source,xrtValueRetain(query)) && xl6SetValueCountV1(source)==2);
    assert(xl6SetValueHasV1(source,query));
    xvalue* missing=xrtValueString(XRT_STR_LITERAL("missing")); assert(missing);
    assert(!xl6SetValueHasV1(source,missing) && !xrtGetError());
    assert(!xl6SetValueRemoveV1(source,missing) && !xrtGetError()); xrtValueRelease(missing);
    assert(xl6SetValueReserveV1(source,40) && xl6SetValueCapacityV1(source)>=40);
    assert(!xl6SetValueReserveV1(source,-1) && xrtGetError()); xrtClearError();
    int64_t unchanged=91;
    assert(!xl6SetValueCountIfPresentV1(xrtValueNull(),&unchanged) && unchanged==91 && !xrtGetError());
    xvalue* nullable=xl6SetValueCountOrNullV1(xrtValueNull());
    assert(nullable && xrtValueType(nullable)==XVALUE_NULL); xrtValueRelease(nullable);
    xvalue* clone=xl6SetValueCloneV1(source); assert(clone && clone!=source);
    assert(xl6SetValueRemoveV1(clone,query) && xl6SetValueCountV1(source)==2 && xl6SetValueCountV1(clone)==1);
    assert(xl6SetValueRelationV1(source,clone,XL6_SET_SUPERSET_V1,true)); xrtValueRelease(clone);
    xvalue* erased=create(true);
    assert(xl6SetValueAddTakeV1(erased,xrtValueRetain(query)));
    xvalue* imported=import(erased); assert(imported && imported!=erased && xl6SetValueHasV1(imported,query));
    assert(xl6SetValueClearV1(imported) && xl6SetValueCountV1(erased)==1); xrtValueRelease(imported);
    imported=import(source); assert(imported && imported!=source && xl6SetValueHasV1(imported,query));
    xrtValueRelease(imported);
    xvalue* bad=create(true); assert(xl6SetValueAddTakeV1(bad,xrtValueInt(5)));
    assert(!import(bad) && xrtGetError()); xrtClearError(); xrtValueRelease(bad);
    xvalue* wire=xrtValueSet(); assert(wire);
    assert(!import(wire) && xrtGetError()); xrtClearError(); xrtValueRelease(wire);
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL; XL6_SetGraphViewV1 pin={0};
    assert(xrtValueGetHandle(erased,&raw,&ops,&user) &&
        xl6SetGraphBeginV1(ops,raw,user,xrtValueTypeId(erased),&pin)==1);
    assert(!import(erased) && xrtGetError()); xrtClearError(); xl6SetGraphEndV1(&pin);
    /* Entry and cleanup refusal together still consume the owned argument and
     * preserve a checked failure without requiring a diagnostic allocation. */
    xl6_error_scope outer={0}; assert(xl6ErrorScopeBegin(&outer));
    xvalue* owned=text(); scopeArmed=true; scopeSeen=scopePoint=0; cleanupRefused=true;
    assert(!xl6SetValueAddTakeV1(source,owned)); scopeArmed=false; assert(!cleanupRefused);
    xl6_error_result outcome=xl6ErrorScopeEnd(&outer); assert(!outcome.ok); xrtErrorFree(outcome.error);
    xrtClearError(); xrtValueRelease(erased); xrtValueRelease(query); xrtValueRelease(source);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
}
static bool sourceAttempt(unsigned lane,size_t point,bool scopes)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* source=input(false),*query=text(),*other=input(false),*erased=create(true);
    assert(xl6SetValueAddTakeV1(erased,xrtValueRetain(query)));
    xvalue* owned=xrtValueString(XRT_STR_LITERAL("third")),*seed=create(false); assert(owned);
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    scopeSeen=0; scopePoint=point; scopeArmed=scopes;
    if (!scopes) assert(xrtMemDebugFailAfter(point));
    bool success=false; xvalue* result=NULL;
    switch (lane) {
    case 0: success=xl6SetValueAddTakeV1(source,owned); owned=NULL; break;
    case 1: success=xl6SetValueHasV1(source,query); break;
    case 2: success=xl6SetValueRemoveV1(source,query); break;
    case 3: success=xl6SetValueClearV1(source); break;
    case 4: success=xl6SetValueReserveV1(source,100); break;
    case 5: success=xl6SetValueTrimV1(source); break;
    case 6: success=xl6SetValueCountV1(source)==2; break;
    case 7: result=xl6SetValueCountOrNullV1(source); break;
    case 8: result=xl6SetValueCloneV1(source); break;
    case 9: result=xl6SetValueAlgebraV1(source,other,XL6_SET_UNION_V1); break;
    case 10: success=xl6SetValueRelationV1(source,other,XL6_SET_EQUALS_V1,false); break;
    case 11: result=importSeed(&seed,erased); break;
    case 12: result=xl6CollectionCountAllOrNullV1(source); break;
    default: assert(false);
    }
    bool hit=scopes ? scopeSeen>point : xrtMemDebugFailTriggered();
    scopeArmed=false; xrtMemDebugFailClear(); success=success || result;
    if (!success) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(success==!hit && outcome.ok==!hit); xrtErrorFree(outcome.error);
    if (lane==8 && success) {
        assert(result!=source && xl6SetValueRemoveV1(result,query) && xl6SetValueCountV1(source)==2);
    }
    if (lane==11 && success) assert(result!=erased && xl6SetValueHasV1(result,query));
    xrtValueRelease(result); xrtValueRelease(seed); xrtValueRelease(owned); xrtValueRelease(erased);
    xrtValueRelease(other); xrtValueRelease(query); xrtValueRelease(source); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Set source lane=%u mode=%s point=%zu hit=%d balanced\n",lane,scopes ? "scopes" : "oom",point,hit);
    return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) sourceVectors();
    else {
        bool scopes=!strcmp(argv[1],"scopes"); assert(scopes || !strcmp(argv[1],"oom"));
        for (unsigned lane=0;lane<13;++lane) {
            size_t point=0; while(point<4096 && sourceAttempt(lane,point,scopes)) ++point;
            assert(point<4096); printf("Set source closed lane=%u mode=%s runs=%zu first-no-hit=%zu\n",
                lane,scopes ? "scopes" : "oom",point+1,point);
        }
    }
    puts("[PASS] native Set source methods/import"); return 0;
}
