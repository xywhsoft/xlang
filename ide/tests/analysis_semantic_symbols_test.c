/* Diagnostic TU includes the real generated semantic compiler. Normal
 * production has no test hooks. Exercise the original allocator directly. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include XLS_SEM_SYMBOL_FIXTURE
#pragma GCC diagnostic pop
#include <stdio.h>
static xatomic32 symbolChecks;
#define SYMBOL_CHECK(v) do{xrtAtomic32FetchAdd(&symbolChecks,1,XMEMORY_RELAXED);if(!(v)){fprintf(stderr,"semantic symbols:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int xlsTestSymbolValues(void)
{
    xls_sem_symbol_span span={100,120,50,1};XL6_SymbolId out;
    SYMBOL_CHECK(xlsSemSymbolsLimit(&span,1000,&out) && out==1020);
    for(XL6_SymbolId id=0;id<140;++id){
        XL6_SymbolId value=id;int okay=xlsSemSymbolsBind(&span,1000,1,&value);
        SYMBOL_CHECK(okay==(id<50 || (id>=100 && id<120)));
        if(okay)SYMBOL_CHECK(value==(id<50 ? id : id+900));
        value=id;SYMBOL_CHECK(xlsSemSymbolsBind(&span,1000,0,&value) && value==id);
    }
    span.local=0;out=110;SYMBOL_CHECK(!xlsSemSymbolsBind(&span,1000,1,&out));
    out=4;SYMBOL_CHECK(xlsSemSymbolsBind(&span,1000,1,&out) && out==4);
    SYMBOL_CHECK(!xlsSemSymbolsLimit(&span,UINT32_MAX-1,&out));
    SYMBOL_CHECK(!xlsSemSymbolsLimit(&span,0,&out));span.floor=0;SYMBOL_CHECK(!xlsSemSymbolsLimit(&span,1000,&out));
    span=(xls_sem_symbol_span){100,99,50,1};SYMBOL_CHECK(!xlsSemSymbolsLimit(&span,1000,&out));
    span=(xls_sem_symbol_span){100,120,101,1};SYMBOL_CHECK(!xlsSemSymbolsLimit(&span,1000,&out));
    span=(xls_sem_symbol_span){UINT32_MAX-2,UINT32_MAX,50,1};SYMBOL_CHECK(xlsSemSymbolsLimit(&span,UINT32_MAX-2,&out) && out==UINT32_MAX);
    SYMBOL_CHECK(!xlsSemSymbolsLimit(&span,UINT32_MAX-1,&out));out=UINT32_MAX-1;SYMBOL_CHECK(!xlsSemSymbolsBind(&span,UINT32_MAX,1,&out));
    return 1;
}
static int xlsTestSymbolScopes(void)
{
    SYMBOL_CHECK(!xlsActiveSymbols);
    XL6_AstSource source={.path="symbol-owned-中文.xl",.references=1},foreign={.path="other.xl",.references=1};
    XL6_AstNode root={.kind=XL6_AST_FUNC,.source=&source,.range={.known=1,.startOffset=100,.endOffset=1000}};
    XL6_AstNode node={.kind=XL6_AST_NAME,.source=&source,.range={.known=1,.startOffset=150,.endOffset=160}};
    for(unsigned mode=0;mode<8;++mode){
        XL6_DiagList observedDiags,originalDiags;xl6DiagInit(&observedDiags);xl6DiagInit(&originalDiags);
        XL6_SemContext observed={.nextSymbolId=mode==5 ? UINT32_MAX-1 : 100,.diags=&observedDiags};
        XL6_SemContext original={.nextSymbolId=observed.nextSymbolId,.diags=&originalDiags};
        XL6_AstNode fn=root,decl=node;if(mode==1)decl.source=&foreign;if(mode==2)decl.range.known=0;
        if(mode==3)decl.range.endOffset=1001;
        if(mode==4)decl.source=NULL;
        if(mode==7)fn.range.known=0;
        xls_sem_symbol_scope scope;xlsSemSymbolsBegin(&scope,&observed,&fn,50);
        if(mode==6){observed.nextSymbolId+=2;original.nextSymbolId+=2;}
        for(unsigned i=0;i<4;++i){
            XL6_SymbolId expected=xlsSemReserveSymbolIdOriginal(&original,&decl),actual=xl6SemReserveSymbolId(&observed,&decl);
            SYMBOL_CHECK(actual==expected && observed.nextSymbolId==original.nextSymbolId && observedDiags.count==originalDiags.count);
        }
        xls_sem_symbol_span span=xlsSemSymbolsEnd(&scope,&observed);SYMBOL_CHECK(!xlsActiveSymbols && span.local==(mode==0) && span.end==observed.nextSymbolId);
        for(size_t i=0;i<originalDiags.count;++i)SYMBOL_CHECK(!strcmp(originalDiags.items[i].message,observedDiags.items[i].message));
        xl6DiagFree(&observedDiags);xl6DiagFree(&originalDiags);
    }
    XL6_SemContext outer={.nextSymbolId=100},inner={.nextSymbolId=200};xls_sem_symbol_scope a,b;
    xlsSemSymbolsBegin(&a,&outer,&root,50);SYMBOL_CHECK(xl6SemReserveSymbolId(&inner,&node)==200 && a.next==100 && a.local);
    xlsSemSymbolsBegin(&b,&inner,&root,50);SYMBOL_CHECK(xl6SemReserveSymbolId(&inner,&node)==201);
    xls_sem_symbol_span inside=xlsSemSymbolsEnd(&b,&inner);SYMBOL_CHECK(xlsActiveSymbols==&a && inside.local && inside.end==202);
    SYMBOL_CHECK(xl6SemReserveSymbolId(&outer,&node)==100 && xl6SemReserveSymbolId(&outer,&node)==101);
    xls_sem_symbol_span outside=xlsSemSymbolsEnd(&a,&outer);SYMBOL_CHECK(!xlsActiveSymbols && outside.local && outside.start==100 && outside.end==102);
    return 1;
}
static int32 xlsTestSymbolThread(void* user)
{*(int*)user=xlsTestSymbolValues() && xlsTestSymbolScopes();return 0;}
int main(void)
{
    xrtAtomic32Init(&symbolChecks,0);if(!xlsTestSymbolValues() || !xlsTestSymbolScopes())return 1;
    int passed[4]={0};xthread* threads[4];
    for(size_t i=0;i<4;++i){threads[i]=xrtThreadCreate(xlsTestSymbolThread,&passed[i],0);if(!threads[i])return 1;}
    for(size_t i=0;i<4;++i){if(xrtThreadWait(threads[i])!=XWAIT_OK || !passed[i])return 1;xrtThreadDestroy(threads[i]);}
    printf("analysis_semantic_symbols_test passed: %u checks;original native allocator/error parity,foreign/unlocated/outside declarations,sticky invalidation,ID domains/overflow,nested contexts and4TLS threads\n",xrtAtomic32Load(&symbolChecks,XMEMORY_RELAXED));return 0;
}
