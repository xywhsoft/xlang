static int xl6IrGrow(void**,size_t*,size_t,size_t);
/* Original native allocations only. Optional overlay memo/index/cache allocations are
 * not routed here. Success adds no error-state read or allocator replacement;
 * failure latches in the synchronous native build scope. */
typedef struct xls_ir_type_resource_scope {
 struct xls_ir_type_resource_scope* previous;int failed;
} xls_ir_type_resource_scope;
static _Thread_local xls_ir_type_resource_scope* xlsActiveTypeResource;
static void* xlsIrTypePointer(void* p,int requested)
{if(!p&&requested&&xlsActiveTypeResource)xlsActiveTypeResource->failed=1;return p;}
static void* xlsIrTypeMalloc(size_t n){return xlsIrTypePointer(xrtMalloc(n),n!=0);}
static void* xlsIrTypeCalloc(size_t n,size_t width){return xlsIrTypePointer(xrtCalloc(n,width),n!=0&&width!=0);}
static void* xlsIrTypeRealloc(void* p,size_t n){return xlsIrTypePointer(xrtRealloc(p,n),n!=0);}
static char* xlsIrTypeStrDup(const char* s){return xlsIrTypePointer(xrtStrDup(s),s!=NULL);}
static char* xlsIrTypeStrDupN(const char* s,size_t n){return xlsIrTypePointer(xrtStrDupN(s,n),s!=NULL);}
static int xlsIrTypeGrow(void** p,size_t* capacity,size_t count,size_t width)
{int okay=xl6IrGrow(p,capacity,count,width);if(!okay&&xlsActiveTypeResource)xlsActiveTypeResource->failed=1;return okay;}
