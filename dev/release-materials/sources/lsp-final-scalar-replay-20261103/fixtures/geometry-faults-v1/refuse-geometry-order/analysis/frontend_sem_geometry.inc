static int xlsGeometryRefuse(xarray* a){(void)a;fputs("actual geometry producer refusal\n",stderr);return 0;}
#define xrtArraySort(a,b) ((void)(b),xlsGeometryRefuse(a))
/* Immutable geometry only. Header values/ordering are proved separately by
 * the existing complete semantic context key before any correspondence read. */
#define XLS_SEM_GEOMETRY_BYTES (8u*1024u*1024u)
static int xlsSemSameRange(XL6_AstRange,XL6_AstRange);
typedef struct xls_sem_geometry_row {xls_sem_position position;size_t source;XL6_AstKind kind;XL6_SymbolId owner,callable;} xls_sem_geometry_row;
typedef struct xls_sem_geometry_order {const xls_sem_geometry_row* row;size_t ordinal;} xls_sem_geometry_order;
typedef struct xls_sem_geometry {xatomic32 references;xarray rows,order,callables;xmap paths;size_t bytes;} xls_sem_geometry;
typedef struct xls_sem_geometry_capture {
    const xls_sem_geometry* previous;xls_sem_geometry* fresh;
    size_t count,pathBytes,limit;int failed,pathsSame;XL6_SymbolId owner;
} xls_sem_geometry_capture;
static _Thread_local xls_sem_geometry_control* xlsActiveGeometryControl;
static size_t xlsSemGeometryBudget(void)
{return xlsActiveGeometryControl ? xlsActiveGeometryControl->bytes : XLS_SEM_GEOMETRY_BYTES;}
void xlsFrontendSemGeometryBegin(xls_sem_geometry_control* control,size_t bytes)
{if(control){size_t limit=xlsSemGeometryBudget();*control=(xls_sem_geometry_control){xlsActiveGeometryControl,bytes<limit ? bytes : limit};xlsActiveGeometryControl=control;}}
void xlsFrontendSemGeometryEnd(xls_sem_geometry_control* control)
{if(control && xlsActiveGeometryControl==control)xlsActiveGeometryControl=control->previous;}
static void xlsSemGeometryRelease(xls_sem_geometry* headers)
{if(headers && xrtAtomic32FetchSub(&headers->references,1,XMEMORY_ACQ_REL)==1){xrtArrayUnit(&headers->callables);xrtArrayUnit(&headers->order);xrtArrayUnit(&headers->rows);xrtMapUnit(&headers->paths);xrtFree(headers);}}
static int xlsSemGeometryPositionEqual(xls_sem_position a,xls_sem_position b)
{return a.line==b.line && a.column==b.column && xlsSemSameRange(a.range,b.range) && xlsSemSameRange(a.token,b.token);}
static int xlsSemGeometryCompare(const xls_sem_geometry_row* a,const xls_sem_geometry_row* b)
{
#define XLS_HEADER_ORDER(field) do{if(a->field!=b->field)return a->field<b->field ? -1 : 1;}while(0)
    XLS_HEADER_ORDER(source);XLS_HEADER_ORDER(kind);XLS_HEADER_ORDER(owner);XLS_HEADER_ORDER(callable);
    XLS_HEADER_ORDER(position.range.startOffset);XLS_HEADER_ORDER(position.range.endOffset);XLS_HEADER_ORDER(position.range.known);
    XLS_HEADER_ORDER(position.token.startOffset);XLS_HEADER_ORDER(position.token.endOffset);XLS_HEADER_ORDER(position.token.known);
    XLS_HEADER_ORDER(position.line);XLS_HEADER_ORDER(position.column);
    XLS_HEADER_ORDER(position.range.startLine);XLS_HEADER_ORDER(position.range.startColumn);XLS_HEADER_ORDER(position.range.endLine);XLS_HEADER_ORDER(position.range.endColumn);
    XLS_HEADER_ORDER(position.token.startLine);XLS_HEADER_ORDER(position.token.startColumn);XLS_HEADER_ORDER(position.token.endLine);XLS_HEADER_ORDER(position.token.endColumn);
#undef XLS_HEADER_ORDER
    return 0;
}
static int xlsSemGeometryOrderCompare(const void* a,const void* b)
{return xlsSemGeometryCompare(((const xls_sem_geometry_order*)a)->row,((const xls_sem_geometry_order*)b)->row);}
static int xlsSemGeometrySymbolCompare(const void* a,const void* b)
{XL6_SymbolId x=*(const XL6_SymbolId*)a,y=*(const XL6_SymbolId*)b;return (x>y)-(x<y);}
static int xlsSemGeometryCallable(const xls_sem_geometry* geometry,XL6_SymbolId symbol)
{
    if(!geometry || !symbol)return 0;
    size_t low=0,high=geometry->callables.Count;
    while(low<high){size_t mid=low+(high-low)/2;XL6_SymbolId at=*(const XL6_SymbolId*)xrtArrayConstGet(&geometry->callables,mid);
        if(at<symbol)low=mid+1;else high=mid;
    }
    return low<geometry->callables.Count && *(const XL6_SymbolId*)xrtArrayConstGet(&geometry->callables,low)==symbol;
}
static int xlsSemGeometryFresh(xls_sem_geometry_capture* capture)
{
    if(capture->fresh)return 1;
    if(capture->limit<sizeof(xls_sem_geometry))return 0;
    xls_sem_geometry* fresh=xrtCalloc(1,sizeof(*fresh));if(!fresh)return 0;
    xrtAtomic32Init(&fresh->references,1);capture->fresh=fresh;
    if(!xrtArrayInit(&fresh->rows,sizeof(xls_sem_geometry_row)) || !xrtArrayInit(&fresh->order,sizeof(xls_sem_geometry_order)) ||
       !xrtArrayInit(&fresh->callables,sizeof(XL6_SymbolId)) ||
       !xrtArrayResize(&fresh->rows,capture->count) || fresh->rows.Capacity>(capture->limit-sizeof(*fresh))/sizeof(xls_sem_geometry_row))return 0;
    /* A prefix is copied only after every previous row matched this walk.
     * There are no AST/source pointers or mutable lifetime counters in it. */
    if(capture->count)memcpy(fresh->rows.Data,capture->previous->rows.Data,capture->count*sizeof(xls_sem_geometry_row));
    return 1;
}
static void xlsSemGeometryPath(xls_sem_geometry_capture* capture,const char* path,size_t id)
{
    if(capture->failed)return;
    size_t bytes=strlen(path),overhead=128+sizeof(size_t)+1;
    if(capture->pathBytes>capture->limit || bytes>capture->limit-capture->pathBytes || overhead>capture->limit-capture->pathBytes-bytes){capture->failed=1;return;}
    capture->pathBytes+=bytes+overhead;
    const size_t* old=capture->previous ? xrtMapConstGet(&capture->previous->paths,(xbytesview){(const unsigned char*)path,bytes}) : NULL;
    if(!old || *old!=id)capture->pathsSame=0;
}
static void xlsSemGeometryRecord(xls_sem_geometry_capture* capture,const XL6_AstNode* node,size_t source,XL6_SymbolId callable)
{
    if(capture->failed)return;
    if(capture->count>=65536 || ((capture->count&63u)==0 && xlsFrontendCancelPoll())){capture->failed=1;return;}
    xls_sem_geometry_row row={{node->range,node->tokenRange,node->line,node->column},source,node->kind,capture->owner,callable};
    const xls_sem_geometry_row* before=capture->previous ? xrtArrayConstGet(&capture->previous->rows,capture->count) : NULL;
    if(!capture->fresh && before && !xlsSemGeometryCompare(before,&row)){++capture->count;return;}
    if(!xlsSemGeometryFresh(capture) || !xrtArrayPush(&capture->fresh->rows,&row) ||
       capture->fresh->rows.Capacity>(capture->limit-sizeof(xls_sem_geometry))/sizeof(row)){capture->failed=1;return;}
    ++capture->count;
}
static xls_sem_geometry* xlsSemGeometryFinish(xls_sem_geometry_capture* capture,xmap* paths,size_t pathCount)
{
    if(capture->failed || !capture->count || xlsFrontendCancelObserved())goto failed;
    if(!capture->fresh && capture->previous && capture->count==capture->previous->rows.Count && capture->pathsSame &&
       pathCount==capture->previous->paths.Count && capture->previous->bytes<=capture->limit){
        xls_sem_geometry* old=(xls_sem_geometry*)capture->previous;xrtAtomic32FetchAdd(&old->references,1,XMEMORY_RELAXED);return old;
    }
    if(!xlsSemGeometryFresh(capture))goto failed;
    xls_sem_geometry* fresh=capture->fresh;
    size_t bytes=sizeof(*fresh);
    if(fresh->rows.Capacity>(capture->limit-bytes)/sizeof(xls_sem_geometry_row))goto failed;
    bytes+=fresh->rows.Capacity*sizeof(xls_sem_geometry_row);
    if(capture->pathBytes>capture->limit-bytes)goto failed;
    bytes+=capture->pathBytes;
    if(paths->BucketCount>(capture->limit-bytes)/sizeof(xmapentry*))goto failed;
    bytes+=paths->BucketCount*sizeof(xmapentry*);
    for(size_t i=0;i<fresh->rows.Count;++i){const xls_sem_geometry_row* row=xrtArrayConstGet(&fresh->rows,i);
        if(row->callable && (!xrtArrayPush(&fresh->callables,&row->callable) || fresh->callables.Capacity>(capture->limit-bytes)/sizeof(XL6_SymbolId)))goto failed;
    }
    bytes+=fresh->callables.Capacity*sizeof(XL6_SymbolId);
    for(size_t i=0;i<fresh->rows.Count;++i){const xls_sem_geometry_row* row=xrtArrayConstGet(&fresh->rows,i);
        if(row->source && (row->position.range.known || row->position.token.known)){
            xls_sem_geometry_order entry={row,i};
            if(!xrtArrayPush(&fresh->order,&entry) || fresh->order.Capacity>(capture->limit-bytes)/sizeof(entry))goto failed;
        }
    }
    if(!xrtArraySort(&fresh->order,xlsSemGeometryOrderCompare) || !xrtArraySort(&fresh->callables,xlsSemGeometrySymbolCompare) || xlsFrontendCancelObserved())goto failed;
    fresh->bytes=bytes+fresh->order.Capacity*sizeof(xls_sem_geometry_order);
    fresh->paths=*paths;*paths=(xmap){0};capture->fresh=NULL;return fresh;
failed:xlsSemGeometryRelease(capture->fresh);capture->fresh=NULL;return NULL;
}
static int xlsSemGeometryPosition(const xls_sem_geometry* before,const xls_sem_geometry* current,
    const XL6_AstNode* node,XL6_SymbolId owner,xls_sem_position* mapped)
{
    const char* path=xl6AstSourcePath(node);
    if(!before || !current || before->rows.Count!=current->rows.Count || !path || !mapped || !owner ||
       !xlsSemGeometryCallable(before,owner) || !xlsSemGeometryCallable(current,owner))return 0;
    const size_t* source=xrtMapConstGet(&before->paths,(xbytesview){(const unsigned char*)path,strlen(path)});if(!source)return 0;
    const size_t* target=xrtMapConstGet(&current->paths,(xbytesview){(const unsigned char*)path,strlen(path)});
    if(!target || *target!=*source)return 0;
    xls_sem_geometry_row key={{node->range,node->tokenRange,node->line,node->column},*source,node->kind,owner,0};
    size_t low=0,high=before->order.Count;
    while(low<high){size_t mid=low+(high-low)/2;const xls_sem_geometry_order* at=xrtArrayConstGet(&before->order,mid);
        if(xlsSemGeometryCompare(at->row,&key)<0)low=mid+1;else high=mid;
    }
    int found=0;xls_sem_position result={0};
    for(size_t i=low;i<before->order.Count;++i){const xls_sem_geometry_order* at=xrtArrayConstGet(&before->order,i);
        if(xlsSemGeometryCompare(at->row,&key))break;
        const xls_sem_geometry_row* next=xrtArrayConstGet(&current->rows,at->ordinal);
        if(!next || next->source!=*source || next->kind!=node->kind || next->owner!=owner || (found && !xlsSemGeometryPositionEqual(result,next->position)))return 0;
        result=next->position;found=1;
    }
    if(found)*mapped=result;
    return found;
}

#undef xrtArraySort
