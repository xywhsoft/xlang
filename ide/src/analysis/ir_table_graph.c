#include "analysis/ir_table_graph.h"
#include <xrt.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct table_writer {unsigned char* base;size_t used,limit,valueOffset;int failed;} table_writer;
typedef struct table_descriptor {
    const char* name;size_t width,alignment;unsigned scalar,keyAlias;
    int (*pack)(table_writer*,const void*);
    const void* (*read)(const XL6_IrModule*,size_t*,size_t*);
    void (*assign)(XL6_IrModule*,void*,size_t);
} table_descriptor;
typedef struct table_namespace {xatomic32 references;} table_namespace;
typedef struct table_range {size_t rows,first,units;} table_range;
struct xls_ir_table_unit {
    xatomic32 references;table_namespace* producer;
    const table_descriptor* descriptor;size_t bytes,count,first,valueOffset;unsigned keyAlias;
};
struct xls_ir_table_graph {
    xatomic32 references;table_namespace* producer;XL6_IrModule model;
    size_t bytes,tableCount,unitCount;table_range* ranges;xls_ir_table_unit** units;
};
struct xls_ir_table_image {xls_ir_table_graph* graph;size_t bytes;XL6_IrModule module;};

static int add(size_t* value,size_t n)
{if(n>SIZE_MAX-*value)return 0;*value+=n;return 1;}
static int region(size_t* bytes,size_t count,size_t width,size_t align,size_t* offset)
{
    if(!add(bytes,(align-*bytes%align)%align) || count>SIZE_MAX/width)return 0;
    *offset=*bytes;return add(bytes,count*width);
}
static void* writeRegion(table_writer* w,size_t count,size_t width,size_t align)
{
    size_t at=0;
    if(w->failed || !region(&w->used,count ? count : 1,width,align,&at) || w->used>w->limit){w->failed=1;return NULL;}
    return w->base ? w->base+at : NULL;
}
static char* writeText(table_writer* w,const char* source)
{
    if(!source)return NULL;
    if(w->failed || w->used>w->limit){w->failed=1;return NULL;}
    size_t room=w->limit-w->used;
    for(size_t i=0;i<room;++i)if(!source[i]){
        char* p=writeRegion(w,i+1,1,1);if(p)memcpy(p,source,i+1);return p;
    }
    w->failed=1;return NULL;
}
#define XLS_TABLE_VALUE(T) _Static_assert(_Generic((T){0}, \
    char:1,signed char:1,unsigned char:1,short:1,unsigned short:1,int:1,unsigned int:1, \
    long:1,unsigned long:1,long long:1,unsigned long long:1,float:1,double:1,_Bool:1,default:0), \
    #T " must be a native numeric value")
#define TABLE_TEXT(field) do{char* p=writeText(writer,src->field);if(dst)dst->field=p;}while(0)
#define TABLE_VECTOR(field,count,T,extra) do{ \
    if(src->field){size_t n=src->count;if(!add(&n,extra)){writer->failed=1;return 0;} \
        void* p=writeRegion(writer,n,sizeof(T),_Alignof(T)); \
        if(p && n){memcpy(p,src->field,n*sizeof(T));}if(dst){dst->field=p;}} \
}while(0)
#define TABLE_STRINGS(field,count) do{ \
    if(src->field){char** p=writeRegion(writer,src->count,sizeof(char*),_Alignof(char*)); \
        if(dst){dst->field=p;} \
        for(size_t i=0;i<src->count && !writer->failed;++i){char* text=writeText(writer,src->field[i]);if(p)p[i]=text;} \
    } \
}while(0)
#include "ir-table-schema.inc"
#undef TABLE_STRINGS
#undef TABLE_VECTOR
#undef TABLE_TEXT
#undef XLS_TABLE_VALUE

static table_namespace* namespaceRetain(table_namespace* p)
{if(p)xrtAtomic32FetchAdd(&p->references,1,XMEMORY_RELAXED);return p;}
static void namespaceRelease(table_namespace* p)
{if(p && xrtAtomic32FetchSub(&p->references,1,XMEMORY_ACQ_REL)==1)xrtFree(p);}
xls_ir_table_unit* xlsIrTableUnitRetain(xls_ir_table_unit* u)
{if(u)xrtAtomic32FetchAdd(&u->references,1,XMEMORY_RELAXED);return u;}
void xlsIrTableUnitRelease(xls_ir_table_unit* u)
{if(u && xrtAtomic32FetchSub(&u->references,1,XMEMORY_ACQ_REL)==1){namespaceRelease(u->producer);xrtFree(u);}}
xls_ir_table_graph* xlsIrTablesRetain(xls_ir_table_graph* g)
{if(g)xrtAtomic32FetchAdd(&g->references,1,XMEMORY_RELAXED);return g;}
void xlsIrTablesRelease(xls_ir_table_graph* g)
{
    if(!g || xrtAtomic32FetchSub(&g->references,1,XMEMORY_ACQ_REL)!=1)return;
    for(size_t i=0;i<g->unitCount;++i)xlsIrTableUnitRelease(g->units[i]);
    namespaceRelease(g->producer);xrtFree(g);
}
/* The native verifier reconstructs persistence keys using pointer identity.
 * Retain the exact ClassField key selector,not a string/name inference. */
static int keyAlias(const XL6_IrModule* m,const table_descriptor* d,const void* row,unsigned* code)
{
    const char* key=NULL;size_t field=0;*code=0;
    switch(d->keyAlias){
    case 1:{const XL6_IrPersistenceProjectField* r=row;key=r->targetKey;field=r->classField;break;}
    case 2:{const XL6_IrPersistenceRestoreField* r=row;key=r->sourceKey;field=r->classField;break;}
    case 3:{const XL6_IrPersistenceRestoreClaim* r=row;key=r->key;field=r->classField;break;}
    default:return 1;
    }
    if(!key)return 1;
    if(!m || field>=m->classFieldCount || !m->classFields)return 0;
    const XL6_IrClassField* target=&m->classFields[field];
    if(key==target->name)*code=1;
    else if(key==target->jsonPersistenceKeyLiteral)*code=2;
    else if(key==target->xlonPersistenceKeyLiteral)*code=3;
    else return 0;
    return 1;
}
static xls_ir_table_unit* unitCreate(const table_descriptor* d,const void* value,size_t count,
    size_t first,table_namespace* producer,size_t limit,unsigned alias,int* budget)
{
    table_writer measure={.used=sizeof(xls_ir_table_unit),.limit=limit};
    size_t offset=0;*budget=1;
    if(!region(&measure.used,count,d->width,d->alignment,&offset) || measure.used>limit)return NULL;
    measure.valueOffset=offset;
    if(d->pack && !d->pack(&measure,value))return NULL;
    if(measure.failed)return NULL;
    *budget=0;
    xls_ir_table_unit* u=xrtMalloc(measure.used);if(!u)return NULL;
    memset(u,0,sizeof(*u));u->descriptor=d;u->bytes=measure.used;u->count=count;u->first=first;u->valueOffset=offset;u->keyAlias=alias;
    table_writer fill={.base=(unsigned char*)u,.used=offset+count*d->width,.limit=measure.used,.valueOffset=offset};
    if(d->pack){
        if(!d->pack(&fill,value) || fill.failed || fill.used!=measure.used){xrtFree(u);return NULL;}
    }else if(count)memcpy((unsigned char*)u+offset,value,count*d->width);
    xrtAtomic32Init(&u->references,1);u->producer=namespaceRetain(producer);return u;
}
size_t xlsIrTableSchemaCount(void){return sizeof(tableDescriptors)/sizeof(*tableDescriptors);}
size_t xlsIrTableSchemaFind(const char* name)
{if(name)for(size_t i=0;i<xlsIrTableSchemaCount();++i)if(!strcmp(name,tableDescriptors[i].name))return i;return SIZE_MAX;}
const char* xlsIrTableSchemaName(size_t table){return table<xlsIrTableSchemaCount() ? tableDescriptors[table].name : NULL;}
size_t xlsIrTableSchemaRowBytes(size_t table){return table<xlsIrTableSchemaCount() ? tableDescriptors[table].width : 0;}
xls_ir_table_unit* xlsIrTableUnitCapture(const xls_ir_table_capture_input* input,
    const xls_ir_table_unit* cohort,size_t limit,int(*cancelled)(void*),void* user,
    xls_ir_graph_status* status,size_t* peakBytes)
{
    xls_ir_table_unit* unit=NULL;xls_ir_graph_status error=XLS_IR_GRAPH_INVALID;if(peakBytes)*peakBytes=0;
    if(!input || input->table>=xlsIrTableSchemaCount() || (cohort && !cohort->producer))goto failed;
    const table_descriptor* d=&tableDescriptors[input->table];
    if((d->scalar && input->first) || (!d->scalar && (input->count!=1 || input->first==SIZE_MAX)) ||
       (input->count && !input->rows))goto failed;
    if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
    error=XLS_IR_GRAPH_BUDGET;if(limit<sizeof(table_namespace))goto failed;
    unsigned alias=0;
    if(!d->scalar && !keyAlias(input->aliases,d,input->rows,&alias)){error=XLS_IR_GRAPH_INVALID;goto failed;}
    int budget=0;unit=unitCreate(d,input->rows,input->count,input->first,NULL,limit-sizeof(table_namespace),alias,&budget);
    if(!unit){error=budget ? XLS_IR_GRAPH_BUDGET : XLS_IR_GRAPH_MEMORY;goto failed;}
    if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
    if(cohort)unit->producer=namespaceRetain(cohort->producer);
    else{unit->producer=xrtCalloc(1,sizeof(*unit->producer));if(!unit->producer){error=XLS_IR_GRAPH_MEMORY;goto failed;}xrtAtomic32Init(&unit->producer->references,1);}
    if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
    if(peakBytes)*peakBytes=unit->bytes+sizeof(table_namespace);
    if(status)*status=XLS_IR_GRAPH_OK;
    return unit;
failed:
    xlsIrTableUnitRelease(unit);if(status)*status=error;return NULL;
}
xls_ir_table_graph* xlsIrTablesCapture(const XL6_IrModule* source,size_t limit,
    int(*cancelled)(void*),void* user,xls_ir_graph_status* status)
{
    xls_ir_graph_status error=XLS_IR_GRAPH_INVALID;xls_ir_table_graph* g=NULL;
    const size_t tables=sizeof(tableDescriptors)/sizeof(*tableDescriptors);
    size_t counts[sizeof(tableDescriptors)/sizeof(*tableDescriptors)],units=0;
    if(!source || !source->ready || source->stage<XL6_IR_STAGE_TYPED ||
       source->nodeCount>source->nodeCapacity || source->childCount>source->childCapacity ||
       (source->nodeCount && !source->nodes) || (source->childCount && !source->children))goto failed;
    if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
    for(size_t i=0;i<tables;++i){
        size_t capacity;const table_descriptor* d=&tableDescriptors[i];const void* values=d->read(source,&counts[i],&capacity);
        if(counts[i]>capacity || (counts[i] && !values))goto failed;
        if(counts[i]>SIZE_MAX/d->width){error=XLS_IR_GRAPH_BUDGET;goto failed;}
        if(!add(&units,d->scalar ? (counts[i]!=0) : counts[i]) || units>=(size_t)INT32_MAX){error=XLS_IR_GRAPH_BUDGET;goto failed;}
    }
    size_t bytes=sizeof(*g),ranges,slots,total;error=XLS_IR_GRAPH_BUDGET;
    if(!region(&bytes,tables,sizeof(table_range),_Alignof(table_range),&ranges) ||
       !region(&bytes,units,sizeof(xls_ir_table_unit*),_Alignof(xls_ir_table_unit*),&slots))goto failed;
    total=bytes;if(!add(&total,sizeof(table_namespace)) || total>limit)goto failed;
    error=XLS_IR_GRAPH_MEMORY;g=xrtCalloc(1,bytes);if(!g)goto failed;
    xrtAtomic32Init(&g->references,1);memcpy(&g->model,source,sizeof(g->model));g->tableCount=tables;g->unitCount=units;g->bytes=total;
    g->ranges=(table_range*)((unsigned char*)g+ranges);g->units=(xls_ir_table_unit**)((unsigned char*)g+slots);
    g->producer=xrtCalloc(1,sizeof(*g->producer));if(!g->producer)goto failed;xrtAtomic32Init(&g->producer->references,1);
    g->model.nodes=NULL;g->model.children=NULL;g->model.nodeCapacity=g->model.nodeCount;g->model.childCapacity=g->model.childCount;
    size_t cursor=0,visits=0;
    for(size_t i=0;i<tables;++i){
        const table_descriptor* d=&tableDescriptors[i];size_t count,capacity;const unsigned char* values=d->read(source,&count,&capacity);
        size_t number=d->scalar ? (count!=0) : count;
        g->ranges[i]=(table_range){count,cursor,number};d->assign(&g->model,NULL,count);
        for(size_t j=0;j<number;++j){
            if(!(visits++%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
            const void* row=values+(d->scalar ? 0 : j*d->width);unsigned alias;
            if(!keyAlias(source,d,row,&alias)){error=XLS_IR_GRAPH_INVALID;goto failed;}
            int budget=0;xls_ir_table_unit* u=unitCreate(d,row,d->scalar ? count : 1,j,g->producer,limit-total,alias,&budget);
            if(!u){error=budget ? XLS_IR_GRAPH_BUDGET : XLS_IR_GRAPH_MEMORY;goto failed;}
            g->units[cursor++]=u;total+=u->bytes;g->bytes=total;
        }
    }
    if(status){*status=XLS_IR_GRAPH_OK;}return g;
failed:
    xlsIrTablesRelease(g);if(status)*status=error;return NULL;
}
size_t xlsIrTablesBytes(const xls_ir_table_graph* g){return g ? g->bytes : 0;}
size_t xlsIrTablesCount(const xls_ir_table_graph* g){return g ? g->tableCount : 0;}
const char* xlsIrTablesName(const xls_ir_table_graph* g,size_t table){return g && table<g->tableCount ? tableDescriptors[table].name : NULL;}
size_t xlsIrTablesRows(const xls_ir_table_graph* g,size_t table){return g && table<g->tableCount ? g->ranges[table].rows : 0;}
size_t xlsIrTablesUnits(const xls_ir_table_graph* g,size_t table){return g && table<g->tableCount ? g->ranges[table].units : 0;}
xls_ir_table_unit* xlsIrTablesPinUnit(const xls_ir_table_graph* g,size_t table,size_t unit)
{return g && table<g->tableCount && unit<g->ranges[table].units ? xlsIrTableUnitRetain(g->units[g->ranges[table].first+unit]) : NULL;}
size_t xlsIrTableUnitBytes(const xls_ir_table_unit* u){return u ? u->bytes+sizeof(table_namespace) : 0;}
size_t xlsIrTableUnitRows(const xls_ir_table_unit* u){return u ? u->count : 0;}
size_t xlsIrTableUnitRowBytes(const xls_ir_table_unit* u){return u ? u->descriptor->width : 0;}
size_t xlsIrTableUnitFirstRow(const xls_ir_table_unit* u){return u ? u->first : 0;}
const char* xlsIrTableUnitName(const xls_ir_table_unit* u){return u ? u->descriptor->name : NULL;}
const void* xlsIrTableUnitRow(const xls_ir_table_unit* u,size_t row)
{return u && row<u->count ? (const unsigned char*)u+u->valueOffset+row*u->descriptor->width : NULL;}
const void* xlsIrTableUnitProducer(const xls_ir_table_unit* u){return u ? u->producer : NULL;}
unsigned xlsIrTableUnitKeyAlias(const xls_ir_table_unit* u){return u ? u->keyAlias : 0;}
static int restoreAlias(XL6_IrModule* m,unsigned kind,void* row,unsigned code)
{
    if(!code)return 1;
    size_t field=kind==1 ? ((XL6_IrPersistenceProjectField*)row)->classField :
                 kind==2 ? ((XL6_IrPersistenceRestoreField*)row)->classField : ((XL6_IrPersistenceRestoreClaim*)row)->classField;
    if(field>=m->classFieldCount || !m->classFields)return 0;
    const XL6_IrClassField* target=&m->classFields[field];
    const char* key=code==1 ? target->name : code==2 ? target->jsonPersistenceKeyLiteral : code==3 ? target->xlonPersistenceKeyLiteral : NULL;
    if(!key)return 0;
    if(kind==1)((XL6_IrPersistenceProjectField*)row)->targetKey=key;
    else if(kind==2)((XL6_IrPersistenceRestoreField*)row)->sourceKey=key;
    else ((XL6_IrPersistenceRestoreClaim*)row)->key=key;
    return 1;
}
xls_ir_table_image* xlsIrTablesMaterialize(xls_ir_table_graph* g,
    const XL6_IrNode* nodes,size_t nodeCount,const XL6_IrNodeId* children,size_t childCount,
    size_t limit,int(*cancelled)(void*),void* user,xls_ir_graph_status* status)
{
    xls_ir_graph_status error=XLS_IR_GRAPH_INVALID;xls_ir_table_image* image=NULL;
    size_t offsets[sizeof(tableDescriptors)/sizeof(*tableDescriptors)];
    if(!g || nodeCount!=g->model.nodeCount || childCount!=g->model.childCount || (nodeCount && !nodes) || (childCount && !children))goto failed;
    if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
    size_t bytes=sizeof(*image);error=XLS_IR_GRAPH_BUDGET;
    for(size_t i=0;i<g->tableCount;++i)if(!region(&bytes,g->ranges[i].rows,tableDescriptors[i].width,tableDescriptors[i].alignment,&offsets[i]) || bytes>limit)goto failed;
    error=XLS_IR_GRAPH_MEMORY;image=xrtMalloc(bytes);if(!image)goto failed;
    memset(image,0,sizeof(*image));image->bytes=bytes;memcpy(&image->module,&g->model,sizeof(image->module));
    image->module.nodes=(XL6_IrNode*)nodes;image->module.children=(XL6_IrNodeId*)children;
    size_t visits=0;
    for(size_t i=0;i<g->tableCount;++i){
        const table_descriptor* d=&tableDescriptors[i];const table_range* range=&g->ranges[i];
        unsigned char* data=range->rows ? (unsigned char*)image+offsets[i] : NULL;d->assign(&image->module,data,range->rows);
        for(size_t j=0;j<range->units;++j){
            if(!(visits++%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
            const xls_ir_table_unit* u=g->units[range->first+j];
            memcpy(data+(d->scalar ? 0 : j*d->width),xlsIrTableUnitRow(u,0),u->count*d->width);
        }
    }
    /* Only after every target arena exists may aliases point into it. */
    for(size_t i=0;i<g->tableCount;++i)if(tableDescriptors[i].keyAlias){
        const table_descriptor* d=&tableDescriptors[i];const table_range* range=&g->ranges[i];size_t count,capacity;
        unsigned char* data=(void*)d->read(&image->module,&count,&capacity);
        for(size_t j=0;j<range->units;++j){
            if(!(visits++%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
            if(!restoreAlias(&image->module,d->keyAlias,data+j*d->width,g->units[range->first+j]->keyAlias)){error=XLS_IR_GRAPH_INVALID;goto failed;}
        }
    }
    image->graph=xlsIrTablesRetain(g);if(status)*status=XLS_IR_GRAPH_OK;return image;
failed:
    xrtFree(image);if(status)*status=error;return NULL;
}
const XL6_IrModule* xlsIrTableImageModule(const xls_ir_table_image* image){return image ? &image->module : NULL;}
size_t xlsIrTableImageBytes(const xls_ir_table_image* image){return image ? image->bytes : 0;}
void xlsIrTableImageDestroy(xls_ir_table_image* image)
{if(image){xls_ir_table_graph* g=image->graph;xrtFree(image);xlsIrTablesRelease(g);}}
