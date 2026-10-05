#include "src/runtime/xl6_pattern_internal.h"
#include "src/runtime/xl6_text_thread.h"
#include "src/runtime/xl6_array_input.h"
#include <stdlib.h>

/* identity is a private, freshly allocated immutable UINT, never a user value.
 * Keys keep just this token alive, not the builder or any language tags. */
typedef struct XL6_PatternBuilder {
    XL6_TextThreadOwner thread;
    xpatternbuilder* builder;
    xvalue* identity;
} XL6_PatternBuilder;
typedef enum XL6_PatternEditKind { ADD, SET, REMOVE, CLEAR } XL6_PatternEditKind;
typedef struct XL6_PatternEdit {
    XL6_TextThreadOwner thread;
    xpatternedit* edit;
    xvalue* identity;
    xvalue* publication;
    XL6_PatternEditKind kind;
    size_t originalCount;
} XL6_PatternEdit;
typedef struct XL6_PatternKey { xvalue* identity; xvalue* publication; xpatternid id; } XL6_PatternKey;

/* Provisional keys may be constructed before the no-fail commit. Keep a tiny
 * shared publication flag, not an edit/builder reference: cancellation must
 * never let a retained provisional key resurrect when an unused ID is reused.
 * This private leaf has no language references or graph edges. XRT owns its
 * reference count; the atomic flag permits immutable published key reads. */
static void publicationDrop(ptr handle,ptr userData) {(void)userData;xrtFree(handle);}
static const xvaluehandleops publicationOps={NULL,publicationDrop,NULL,NULL};
static xvalue* publicationCreate(void)
{
    ptr flag=xrtCalloc(1,sizeof(xatomic32));if (!flag) return NULL;
    xvalue* value=xrtValueHandleTake(&flag,&publicationOps,NULL);xrtFree(flag);return value;
}
static xatomic32* publicationFlag(const xvalue* value)
{
    ptr flag=NULL;const xvaluehandleops* ops=NULL;
    if (!xrtValueGetHandle(value,&flag,&ops,NULL) || ops!=&publicationOps || !flag) abort();
    return flag;
}
static bool keyPublished(const XL6_PatternKey* key)
{return key && xrtAtomic32Load(publicationFlag(key->publication),XMEMORY_ACQUIRE)!=0;}

static bool builderCheck(XL6_PatternBuilder* b)
{
    if (!b || !b->builder) {xrtSetErrorKind(XERR_STATE);return false;}
    return xl6TextThreadCheck(&b->thread);
}
static bool editCheck(XL6_PatternEdit* e)
{
    if (!e || !e->edit) {xrtSetErrorKind(XERR_STATE);return false;}
    if (!xl6TextThreadCheck(&e->thread)) return false;
    if (!xrtPatternEditReady(e->edit)) {xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
void xl6PatternBuilderDestroyV1(void* handle)
{
    XL6_PatternBuilder* b=handle;
    if (b) {xrtPatternBuilderFree(b->builder);xrtValueRelease(b->identity);
        xl6TextThreadDrop(&b->thread);xrtFree(b);}
}
static void* builderCreate(const xpatternconfig* config)
{
    XL6_PatternBuilder* b=xrtCalloc(1,sizeof(*b));
    if (!b) return NULL;
    if (!xl6TextThreadInit(&b->thread)) goto fail;
    b->identity=xrtValueUInt(1);if (!b->identity) goto fail;
    b->builder=xrtPatternBuilderCreateConfig(config);if (!b->builder) goto fail;
    return b;
fail: xl6PatternBuilderDestroyV1(b);return NULL;
}
void* xl6PatternBuilderCreateV1(void)
{xpatternconfig c;xrtPatternConfigInit(&c);return builderCreate(&c);}
void* xl6PatternBuilderOptionsV1(void* options)
{
    if (!options) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return builderCreate(&((XL6_PatternOptions*)options)->config);
}
int64_t xl6PatternBuilderCountV1(void* handle)
{XL6_PatternBuilder* b=handle;return builderCheck(b)?(int64_t)xrtPatternBuilderCount(b->builder):0;}
uint64_t xl6PatternBuilderVersionV1(void* handle)
{XL6_PatternBuilder* b=handle;return builderCheck(b)?xrtPatternBuilderVersion(b->builder):0;}
xvalue* xl6PatternBuilderIdentityV1(void* handle)
{XL6_PatternBuilder* b=handle;return builderCheck(b)?xrtValueRetain(b->identity):NULL;}
void xl6PatternBuilderReserveV1(void* handle,int64_t capacity)
{
    XL6_PatternBuilder* b=handle;if (!builderCheck(b)) return;
    if (capacity<0 || (uint64_t)capacity>SIZE_MAX) {xrtSetErrorKind(XERR_RANGE);return;}
    (void)xrtPatternBuilderReserve(b->builder,(size_t)capacity);
}
bool xl6PatternBuilderContainsV1(void* handle,void* key)
{
    XL6_PatternBuilder* b=handle;XL6_PatternKey* k=key;
    if (!builderCheck(b)) return false;
    return keyPublished(k) && b->identity==k->identity && xrtPatternBuilderContains(b->builder,k->id);
}
void* xl6PatternBuilderCompileV1(void* handle)
{XL6_PatternBuilder* b=handle;return builderCheck(b)?xrtPatternBuilderCompile(b->builder):NULL;}

void xl6PatternEditDestroyV1(void* handle)
{
    XL6_PatternEdit* e=handle;
    if (e) {xrtPatternEditFree(e->edit);xrtValueRelease(e->identity);xrtValueRelease(e->publication);
        xl6TextThreadDrop(&e->thread);xrtFree(e);}
}
static XL6_PatternEdit* editCreate(XL6_PatternBuilder* b,XL6_PatternEditKind kind)
{
    if (!builderCheck(b)) return NULL;
    XL6_PatternEdit* e=xrtCalloc(1,sizeof(*e));if (!e) return NULL;
    e->kind=kind;e->originalCount=xrtPatternBuilderCount(b->builder);
    if (!xl6TextThreadInit(&e->thread)) goto fail;
    e->identity=xrtValueRetain(b->identity);if (!e->identity) goto fail;
    if (kind==ADD) {e->publication=publicationCreate();if (!e->publication) goto fail;}
    return e;
fail: xl6PatternEditDestroyV1(e);return NULL;
}
static bool specInit(xpatternspec* spec,xstrview expression,int64_t priority,int64_t size)
{
    if (priority<INT32_MIN || priority>INT32_MAX) {xrtSetErrorKind(XERR_RANGE);return false;}
    if (!xl6TextSelect(&expression,size)) return false;
    *spec=(xpatternspec){expression,NULL,(int32)priority,0};return true;
}
void* xl6PatternEditAddV1(void* handle,xstrview expression,int64_t priority,int64_t size)
{
    XL6_PatternBuilder* b=handle;xpatternspec spec;
    if (!builderCheck(b) || !specInit(&spec,expression,priority,size)) return NULL;
    XL6_PatternEdit* e=editCreate(b,ADD);if (!e) return NULL;
    e->edit=xrtPatternBuilderPrepareAdd(b->builder,&spec,1);
    if (!e->edit) {xl6PatternEditDestroyV1(e);return NULL;}return e;
}
void* xl6PatternEditAddManyV1(void* handle,const xvalue* expressions,const xvalue* priorities,const xvalue* sizes)
{
    XL6_PatternBuilder* b=handle;
    if (!builderCheck(b)) return NULL;
    XL6_ArrayInputV1 inputs[3]={{0}};
    xpatternspec* specs=NULL; XL6_PatternEdit* e=NULL;
    if (!xl6ArrayInputAcquireV1(expressions,&inputs[0]) ||
        !xl6ArrayInputAcquireV1(priorities,&inputs[1]) ||
        !xl6ArrayInputAcquireV1(sizes,&inputs[2])) goto done;
    size_t count=inputs[0].count;
    if (inputs[1].count!=count || inputs[2].count!=count) {xrtSetErrorKind(XERR_ARGUMENT);goto done;}
    if (count>SIZE_MAX/sizeof(xpatternspec)) {xrtSetErrorKind(XERR_RANGE);goto done;}
    specs=count?xrtMalloc(count*sizeof(*specs)):NULL;if (count&&!specs) goto done;
    for (size_t i=0;i<count;++i) {
        xstrview text;int64 priority,size;
        if (!xrtValueGetString(inputs[0].items[i],&text) ||
            !xrtValueGetInt(inputs[1].items[i],&priority) ||
            !xrtValueGetInt(inputs[2].items[i],&size) || !specInit(&specs[i],text,priority,size)) goto done;
    }
    e=editCreate(b,ADD);
    if (e) {e->edit=xrtPatternBuilderPrepareAdd(b->builder,specs,count);
        if (!e->edit) {xl6PatternEditDestroyV1(e);e=NULL;}}
done:
    xrtFree(specs);
    for (size_t i=0;i<3;++i) xl6ArrayInputReleaseV1(&inputs[i]);
    return e;
}
static bool keyCheck(XL6_PatternBuilder* b,XL6_PatternKey* key)
{
    if (!builderCheck(b)) return false;
    if (!keyPublished(key) || key->identity!=b->identity || !xrtPatternBuilderContains(b->builder,key->id)) {
        xrtSetErrorKind(XERR_STATE);return false;
    }
    return true;
}
void* xl6PatternEditSetV1(void* handle,void* key,xstrview expression,int64_t priority,int64_t size)
{
    XL6_PatternBuilder* b=handle;XL6_PatternKey* k=key;xpatternspec spec;
    if (!keyCheck(b,k) || !specInit(&spec,expression,priority,size)) return NULL;
    XL6_PatternEdit* e=editCreate(b,SET);if (!e) return NULL;
    e->edit=xrtPatternBuilderPrepareSet(b->builder,k->id,&spec);
    if (!e->edit) {xl6PatternEditDestroyV1(e);return NULL;}return e;
}
void* xl6PatternEditRemoveV1(void* handle,void* key)
{
    XL6_PatternBuilder* b=handle;XL6_PatternKey* k=key;
    if (!keyCheck(b,k)) return NULL;
    XL6_PatternEdit* e=editCreate(b,REMOVE);if (!e) return NULL;
    e->edit=xrtPatternBuilderPrepareRemove(b->builder,k->id);
    if (!e->edit) {xl6PatternEditDestroyV1(e);return NULL;}return e;
}
void* xl6PatternEditClearV1(void* handle)
{
    XL6_PatternBuilder* b=handle;XL6_PatternEdit* e=editCreate(b,CLEAR);if (!e) return NULL;
    e->edit=xrtPatternBuilderPrepareClear(b->builder);
    if (!e->edit) {xl6PatternEditDestroyV1(e);return NULL;}return e;
}
int64_t xl6PatternEditCountV1(void* handle)
{XL6_PatternEdit* e=handle;return editCheck(e)?(int64_t)xrtPatternEditCount(e->edit):0;}
void xl6PatternEditCancelV1(void* handle)
{
    XL6_PatternEdit* e=handle;
    if (!e || !xl6TextThreadCheck(&e->thread)) return;
    xrtPatternEditFree(e->edit);e->edit=NULL;
}
void xl6PatternKeyDestroyV1(void* handle)
{XL6_PatternKey* k=handle;if (k) {xrtValueRelease(k->identity);xrtValueRelease(k->publication);xrtFree(k);}}
void* xl6PatternKeyCreateV1(void* handle,int64_t index)
{
    XL6_PatternEdit* e=handle;if (!editCheck(e)) return NULL;
    if (e->kind!=ADD) {xrtSetErrorKind(XERR_STATE);return NULL;}
    if (index<0 || (uint64_t)index>=xrtPatternEditCount(e->edit)) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_PatternKey* k=xrtCalloc(1,sizeof(*k));if (!k) return NULL;
    k->id=xrtPatternEditId(e->edit,(size_t)index);k->identity=xrtValueRetain(e->identity);
    k->publication=xrtValueRetain(e->publication);
    if (!k->identity || !k->publication) {xl6PatternKeyDestroyV1(k);return NULL;}return k;
}
uint64_t xl6PatternKeyIdV1(void* handle)
{XL6_PatternKey* k=handle;if (!k) {xrtSetErrorKind(XERR_STATE);return 0;}return k->id;}

/* Only the facade's ordinary managed state owns tags. No language reference
 * is retained in builder, key or edit native resources. */
static xvalue* tagsGet(const xvalue* state)
{
    if (!state || xrtValueType(state)!=XVALUE_OBJECT) {xrtSetErrorKind(XERR_TYPE);return NULL;}
    xvalue* tags=xrtValueObjectGet(state,XRT_STR_LITERAL("tags"));
    if (!tags || xrtValueType(tags)!=XVALUE_INT_MAP) {xrtSetErrorKind(XERR_TYPE);return NULL;}
    return tags;
}
xvalue* xl6PatternBuilderTagsV1(void* handle,const xvalue* state)
{
    XL6_PatternBuilder* b=handle;if (!builderCheck(b)) return NULL;
    xvalue* tags=tagsGet(state);if (!tags) return NULL;
    if (xrtValueObjectGet(state,XRT_STR_LITERAL("identity"))!=b->identity ||
        xrtValueCount(tags)!=xrtPatternBuilderCount(b->builder)) {xrtSetErrorKind(XERR_STATE);return NULL;}
    return xrtValueClone(tags);
}

/* Single edits modify a unique table in O(log n). A genuine batch prepares a
 * COW candidate once and installs it once, rather than copying per entry.
 * Old values are pinned through the native no-allocation commit: mutation
 * admission itself does NOT defer finalizers. Cleanup faults after this point
 * propagate with both states already committed, never one half committed. */
static void apply(XL6_PatternEdit* e,xvalue* state,const xvalue* payload,bool many)
{
    if (!editCheck(e)) return;
    xvalue* tags=tagsGet(state);if (!tags) return;
    if (xrtValueObjectGet(state,XRT_STR_LITERAL("identity"))!=e->identity ||
        xrtValueCount(tags)!=e->originalCount) {xrtSetErrorKind(XERR_STATE);return;}
    size_t count=xrtPatternEditCount(e->edit);
    XL6_ArrayInputV1 input={0};
    if (many) {
        if (e->kind!=ADD) {xrtSetErrorKind(XERR_ARGUMENT);return;}
        if (!xl6ArrayInputAcquireV1(payload,&input)) return;
        if (input.count!=count) {xrtSetErrorKind(XERR_ARGUMENT);xl6ArrayInputReleaseV1(&input);return;}
    }
    if ((e->kind==ADD || e->kind==SET) && !many && (!payload || count!=1)) {
        xrtSetErrorKind(XERR_ARGUMENT);return;
    }
    xrtownershipscope scope={0};if (!xrtOwnershipMutationBegin(&scope)) {xl6ArrayInputReleaseV1(&input);return;}
    xvalue* pin=NULL; xvalue* candidate=NULL;bool ok=false;
    if (e->kind==CLEAR || (many && count!=1)) {
        pin=xrtValueRetain(tags);if (!pin) goto done;
        candidate=e->kind==CLEAR?xrtValueIntMap():xrtValueClone(tags);if (!candidate) goto done;
        if (e->kind==ADD) for (size_t i=0;i<count;++i) {
            if (!xrtValueIntMapSet(candidate,xl6PatternTagKeyFromId(xrtPatternEditId(e->edit,i)),input.items[i])) goto done;
        }
        if (!xrtValueObjectSetTake(state,XRT_STR_LITERAL("tags"),&candidate)) goto done;
        ok=true;
    } else {
        int64_t slot=xl6PatternTagKeyFromId(xrtPatternEditId(e->edit,0));
        const xvalue* old=xrtValueIntMapGet(tags,slot);
        if (e->kind==ADD ? old!=NULL : old==NULL) {xrtSetErrorKind(XERR_STATE);goto done;}
        pin=old?xrtValueRetain(old):NULL;if (old&&!pin) goto done;
        tags=xrtValueObjectEdit(state,XRT_STR_LITERAL("tags"));if (!tags) goto done;
        if (e->kind==REMOVE) ok=xrtValueIntMapRemove(tags,slot);
        else ok=xrtValueIntMapSet(tags,slot,many?input.items[0]:payload);
    }
    if (ok && !xrtPatternEditCommit(e->edit)) abort(); /* Ready edit, same thread, no callbacks occurred. */
    if (ok && e->publication) xrtAtomic32Store(publicationFlag(e->publication),1,XMEMORY_RELEASE);
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
done:
    if (!xrtOwnershipScopeEnd(&scope)) abort();
    /* Preserve a preparation failure over cleanup of temporary references. */
    xerror* failure=xrtTakeError();xrtValueRelease(candidate);xrtValueRelease(pin);
    xl6ArrayInputReleaseV1(&input);
    if (failure) {xrtClearError();xrtSetErrorTake(failure);}
}
void xl6PatternEditApplyV1(void* handle,xvalue* state,const xvalue* payload)
{apply(handle,state,payload,false);}
void xl6PatternEditApplyManyV1(void* handle,xvalue* state,const xvalue* payload)
{apply(handle,state,payload,true);}
