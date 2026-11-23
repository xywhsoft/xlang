/* Bounded, exact in-process input serialization shared by native phase caches. */
typedef struct xls_ir_input_read {uint32_t kind,id;} xls_ir_input_read;
typedef struct xls_ir_input_trace {
 const XL6_IrModule* module;uint32_t *seen,epoch;xls_ir_input_read* ids;
 size_t reads,readCapacity,used,capacity;unsigned char* key;int failed;
} xls_ir_input_trace;
static int xlsIrInputPut(xls_ir_input_trace* t,const void* p,size_t n)
{
 if(t->failed||n>t->capacity-t->used||(n&&!p)){t->failed=1;return 0;}
 if(n)memcpy(t->key+t->used,p,n);
 t->used+=n;return 1;
}
static int xlsIrInputText(xls_ir_input_trace* t,const char* p)
{
 uint64_t n=0;
 if(p){size_t room=t->capacity-t->used;for(;n<room&&p[n];++n){}if(n==room){t->failed=1;return 0;}++n;}
 return xlsIrInputPut(t,&n,sizeof(n))&&xlsIrInputPut(t,p,(size_t)n);
}
static int xlsIrInputRegion(size_t* bytes,size_t count,size_t width,size_t align,size_t* at)
{
 size_t pad=(align-*bytes%align)%align;if(pad>SIZE_MAX-*bytes)return 0;*bytes+=pad;*at=*bytes;
 if(count>(SIZE_MAX-*bytes)/width)return 0;
 *bytes+=count*width;return 1;
}
