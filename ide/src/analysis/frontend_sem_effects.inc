/* Original checker declarations are emitted by the original implementation.
 * Keep one exact owned receipt per actual emission; never replay a mutation
 * as scalar facts or rescan all module declarations for every function. */
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
typedef struct xls_sem_effect_scope {
    struct xls_sem_effect_scope* previous;
    const XL6_SemContext* context;
    int pure;
} xls_sem_effect_scope;
static _Thread_local xls_sem_effect_scope* xlsActiveSemEffects;
static void xlsSemEffectScopeBegin(xls_sem_effect_scope* scope,const XL6_SemContext* ctx)
{*scope=(xls_sem_effect_scope){xlsActiveSemEffects,ctx,1};xlsActiveSemEffects=scope;}
static int xlsSemEffectScopeEnd(xls_sem_effect_scope* scope)
{xlsActiveSemEffects=scope->previous;return scope->pure;}
static void xlsSemFunctionMutation(XL6_SemContext* ctx)
{
    for(xls_sem_effect_scope* s=xlsActiveSemEffects;s;s=s->previous)if(s->context==ctx)s->pure=0;
    xls_sem_build* b=xlsActiveSemBuild;
    if(b && b->enabled && b->effectContext==ctx)b->memo->inputSame=0;
}
static int xlsSemEffectsCapture(const xls_sem_build* b)
{return b->memo->effectsComplete && !b->effectPending;}
static int xlsSemEffectsAdmit(const xls_sem_build* b,const xls_sem_entry* old)
{return xlsSemEffectsCapture(b) && b->effectsCompatible && old->effectCount==b->memo->effects.Count;}
static void xlsSemEffectsFree(xls_sem_memo* m)
{
    for(size_t i=0;i<m->effects.Count;++i)xrtStrBufFree(xrtArrayGet(&m->effects,i));
    xrtArrayUnit(&m->effects);
}
static void xlsSemDeclarationEffectBegin(XL6_SemContext* ctx)
{
    xlsSemFunctionMutation(ctx);
    xls_sem_build* b=xlsActiveSemBuild;
    if(!b || !b->enabled || b->effectContext!=ctx)return;
    if(b->effectPending==SIZE_MAX)b->memo->effectsComplete=0;else ++b->effectPending;
    xlsSemReadInvalidate(b);
}
static int xlsSemEffectAppend(xls_sem_memo* m,xstrbuf* key)
{
    /* Reserve in a separate array only on growth. Bound the transient old
     * plus new arrays as well as retained bytes by the existing memo limit. */
    xarray next={0};xarray* target=&m->effects;
    size_t old=m->effects.Capacity*sizeof(xstrbuf);
    if(m->bytes>XLS_SEM_MEMO_BYTES || key->Capacity>XLS_SEM_MEMO_BYTES-m->bytes)return 0;
    size_t remaining=XLS_SEM_MEMO_BYTES-m->bytes-key->Capacity;
    if(!target->ItemSize || target->Count==target->Capacity){
        if(target->Count==SIZE_MAX || remaining<sizeof(xstrbuf) ||
           !xrtArrayInit(&next,sizeof(xstrbuf)) || !xrtArrayReserve(&next,target->Count+1) ||
           next.Capacity>remaining/sizeof(xstrbuf)){xrtArrayUnit(&next);return 0;}
        if(target->Count)memcpy(next.Data,target->Data,target->Count*sizeof(xstrbuf));
        next.Count=target->Count;target=&next;
    }
    if(!xrtArrayPush(target,key)){xrtArrayUnit(&next);return 0;}
    if(target==&next){xrtArrayUnit(&m->effects);m->effects=next;}
    m->bytes+=key->Capacity+m->effects.Capacity*sizeof(xstrbuf)-old;
    *key=(xstrbuf){0};return 1;
}
static void xlsSemDeclarationEffectEnd(XL6_SemContext* ctx,const XL6_AstNode* owner,const XL6_AstNode* decl)
{
    xls_sem_build* b=xlsActiveSemBuild;
    if(!b || !b->enabled || b->effectContext!=ctx)return;
    xls_sem_memo* m=b->memo;
    if(!b->effectPending){m->effectsComplete=0;b->effectsCompatible=0;return;}
    --b->effectPending;
    if(!m->effectsComplete)return;
    xstrbuf key={0};size_t slot=m->effects.Count;
    /* The initial context already proves class declarations in full. Record
     * the container identity and the complete newly emitted declaration,
     * including its body and native allocation state, once at this seam. */
    int okay=xlsSemKeyNumber(&key,owner!=NULL) &&
        (!owner || (xlsSemKeyNumber(&key,owner->kind) && xlsSemKeyNumber(&key,owner->semanticSymbolId) &&
                    xlsSemKeyText(&key,owner->text) &&
                    xlsSemKeyNode(&key,owner->childCount ? owner->children[0] : NULL,0,0))) &&
        xlsSemKeyNumber(&key,ctx->nextSymbolId) && xlsSemKeyNumber(&key,ctx->types.nextId) &&
        xlsSemKeyNode(&key,decl,0,0) && xlsSemEffectAppend(m,&key);
    xrtStrBufFree(&key);
    if(!okay){m->effectsComplete=0;b->effectsCompatible=0;return;}
    const xstrbuf* old=b->baseline ? xrtArrayConstGet(&b->baseline->effects,slot) : NULL;
    if(!old || !xlsSemEqualKey(old,xrtArrayConstGet(&m->effects,slot)))b->effectsCompatible=0;
    xlsSemReadInvalidate(b);
}
#else
#define xlsSemEffectsAdmit(build,old) 1
#define xlsSemEffectsCapture(build) 1
#define xlsSemEffectsFree(memo) ((void)0)
#endif
