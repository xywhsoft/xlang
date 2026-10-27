/* Independent candidate/source data produced by the original semantic query.
 * No prior completion/check/typed AST/IR owner is retained. The optional memo
 * owns only its pre-existing independent facts/readsets/coordinate records. */
#define XLS_WORD_UNIT_BYTES (256u*1024u*1024u)
static _Thread_local xls_completion_unit_control* wordControl;
static size_t wordBudget(void){return wordControl ? wordControl->bytes : XLS_WORD_UNIT_BYTES;}
void xlsCompletionUnitBegin(xls_completion_unit_control* control,size_t bytes)
{
    if(!control)return;
    size_t available=wordBudget();if(bytes>available)bytes=available;
    *control=(xls_completion_unit_control){wordControl,bytes};wordControl=control;
}
void xlsCompletionUnitEnd(xls_completion_unit_control* control)
{if(control && wordControl==control)wordControl=control->previous;}
static int wordRangeEqual(const XL6_AstRange* a,const XL6_AstRange* b)
{return a->known==b->known && a->startOffset==b->startOffset && a->endOffset==b->endOffset && a->startLine==b->startLine && a->startColumn==b->startColumn && a->endLine==b->endLine && a->endColumn==b->endColumn;}
struct xls_completion_word_unit {
    xatomic32 references;
    XL6_CompletionQuery query;
    XL6_SourceReport report;
    xls_input_batch* batch;
    xls_syntax* syntax;
    xls_sem_memo* memo;
    xls_input_topology* topology;
    XL6_SourceOverlay overlay;
    uint64_t usedInputs;int usageKnown;
    unsigned char* querySelections;
    size_t bytes;
};
static int wordSyntaxInput(const xls_syntax* syntax,const xls_input_batch* batch,const char* key)
{
    const xls_analysis_document* doc=xlsInputBatchFind(batch,key);const xls_text* text=xlsSyntaxText(syntax);
    if(!doc || !text || !xlsSyntaxPath(syntax))return 0;
    int pathMatches=!strcmp(xlsSyntaxPath(syntax),doc->path);
    if(!pathMatches){
        /* The IDE keeps URI-style separators; the original parser uses
         * xrtPathAbs. Bind with that same normalization, never by basename,
         * body equality alone or an allocation failure treated as a match. */
        char* absolute=xrtPathAbs(doc->path);
        pathMatches=absolute && !strcmp(xlsSyntaxPath(syntax),absolute);xrtFree(absolute);
    }
    return pathMatches &&
        (text==doc->text || (xlsTextSize(text)==xlsTextSize(doc->text) && !memcmp(xlsTextData(text),xlsTextData(doc->text),xlsTextSize(text))));
}
static int wordBatchTopology(const xls_input_batch* a,const xls_input_batch* b,const char* queryKey,int compareOthers)
{
    if(!a || !b || !queryKey || !xlsInputBatchFind(a,queryKey) || !xlsInputBatchFind(b,queryKey) ||
       !xlsFrontendContextsEqual(xlsInputBatchContext(a),xlsInputBatchContext(b)) ||
       strcmp(xlsInputBatchEntry(a),xlsInputBatchEntry(b)) || strcmp(xlsInputBatchDirectory(a),xlsInputBatchDirectory(b)))return 0;
    const xls_analysis_document *at=xlsInputBatchTarget(a),*bt=xlsInputBatchTarget(b);
    if((at!=NULL)!=(bt!=NULL) || (at && strcmp(at->key,bt->key)))return 0;
    size_t count=0;
    for(;;++count){
        const xls_analysis_document* old=xlsInputBatchAt(a,count);if(!old)break;
        const xls_analysis_document* next=xlsInputBatchFind(b,old->key);
        if(!next || old->identity!=next->identity || strcmp(old->path,next->path) ||
           (compareOthers && strcmp(old->key,queryKey) && !xlsInputSameBody(old,next)))return 0;
    }
    return xlsInputBatchAt(b,count)==NULL;
}
static int wordCharge(size_t* bytes,size_t amount)
{size_t limit=wordBudget();if(*bytes>limit || amount>limit-*bytes)return 0;*bytes+=amount;return 1;}
static int wordRemapUsage(const xls_completion_word_unit* unit,const xls_input_batch* current,uint64_t* mapped)
{
    *mapped=0;if(!unit->usageKnown)return 0;
    for(size_t i=0;;++i){
        const xls_analysis_document* doc=xlsInputBatchAt(current,i);if(!doc)break;
        size_t j=0;const xls_analysis_document* old;
        while((old=xlsInputBatchAt(unit->batch,j)) && strcmp(old->key,doc->key))++j;
        if(!old || j>=64)return 0;
        if(unit->usedInputs&(UINT64_C(1)<<j))*mapped|=UINT64_C(1)<<i;
    }
    return 1;
}
/* Metadata always owns paths/probes/environment. copyText owns all bodies for
 * the independent original unit; current views borrow unit/new batch bodies. */
static int wordReportCopy(const XL6_SourceReport* in,XL6_SourceReport* out,int copyText,size_t* charged,
    int (*cancelled)(void*),void* user)
{
    if(!in || in->count>XL6_SOURCE_REPORT_FILES || in->probeCount>65536 ||
       !wordCharge(charged,in->count*sizeof(*in->items)) || !wordCharge(charged,in->probeCount*sizeof(*in->probes)))return 0;
    xl6SourceReportInit(out);
    out->items=in->count ? xrtCalloc(in->count,sizeof(*in->items)) : NULL;
    out->probes=in->probeCount ? xrtCalloc(in->probeCount,sizeof(*in->probes)) : NULL;
    if((in->count && !out->items) || (in->probeCount && !out->probes))return 0;
    out->capacity=in->count;out->probeCapacity=in->probeCount;out->probeBytes=in->probeBytes;
    out->packagePathObserved=in->packagePathObserved;
    if(in->packagePath){
        if(!wordCharge(charged,strlen(in->packagePath)+1) || !(out->packagePath=xrtStrDup(in->packagePath)))return 0;
    }
    for(size_t i=0;i<in->count;++i){
        if(cancelled && cancelled(user))return 0;
        const XL6_SourceReportItem* from=&in->items[i];XL6_SourceReportItem* to=&out->items[i];
        if(!from->path || !from->text || !from->origin || from->size>XL6_SOURCE_REPORT_FILE_BYTES ||
           from->size>XL6_SOURCE_REPORT_TOTAL_BYTES-out->bytes ||
           (strcmp(from->origin,"disk") && strcmp(from->origin,"overlay") && strcmp(from->origin,"memory")) ||
           !wordCharge(charged,strlen(from->path)+1) || (copyText && !wordCharge(charged,from->size+1)))return 0;
        *to=*from;to->path=NULL;to->text=NULL;++out->count;
        to->path=xrtStrDup(from->path);to->text=copyText ? xrtStrDupN(from->text,from->size) : from->text;
        if(!to->path || !to->text)return 0;
        out->bytes+=to->size;
    }
    for(size_t i=0;i<in->probeCount;++i){
        if(cancelled && cancelled(user))return 0;
        const XL6_ResolutionProbe* from=&in->probes[i];XL6_ResolutionProbe* to=&out->probes[i];
        if(!from->path || !wordCharge(charged,strlen(from->path)+1))return 0;
        to->exists=from->exists;to->path=xrtStrDup(from->path);++out->probeCount;if(!to->path)return 0;
    }
    return 1;
}
static xls_completion_word_unit* wordUnitRetain(xls_completion_word_unit* unit)
{if(unit)xrtAtomic32FetchAdd(&unit->references,1,XMEMORY_RELAXED);return unit;}
static void wordUnitRelease(xls_completion_word_unit* unit)
{
    if(!unit || xrtAtomic32FetchSub(&unit->references,1,XMEMORY_ACQ_REL)!=1)return;
    for(size_t i=0;i<unit->query.count;++i){
        xl6AstFree((XL6_AstNode*)unit->query.items[i].selection);xrtFree((void*)unit->query.items[i].kind);
    }
    xl6CompletionFree(&unit->query);xl6SourceReportFree(&unit->report);xrtFree(unit->querySelections);
    xlsInputTopologyDestroy(unit->topology);xlsFrontendOverlayClear(&unit->overlay,unit->batch);
    xlsInputBatchDestroy(unit->batch);xlsSyntaxRelease(unit->syntax);xlsFrontendSemMemoDestroy(unit->memo);xrtFree(unit);
}
static void wordQueryFree(xls_semantic_completion* r)
{
    if(r->wordUnit){
        for(size_t i=0;i<r->query.count;++i){
            XL6_CompletionItem* item=&r->query.items[i];xl6AstFree((XL6_AstNode*)item->selection);
            item->name=item->canonical=NULL;
        }
        /* Metadata belongs to this result; all bodies are independently pinned
         * by its unit or current batch. Never pass borrowed bytes to Free. */
        for(size_t i=0;i<r->report.count;++i)r->report.items[i].text=NULL;
    }
    xl6CompletionFree(&r->query);
}
static xls_completion_word_unit* wordUnitCapture(xls_semantic_completion* r,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user)
{
    size_t syntaxBytes=0,inputBytes=xlsInputBatchStorageBytes(r->inputOwner),bytes=sizeof(xls_completion_word_unit);
    if(!r->inputOwner || !r->cursorSyntaxApplied || !r->query.prefix ||
       !xlsSyntaxCursorMatches(syntax,r->path,r->query.offset) || !xlsSyntaxCursorRetainedBytes(syntax,wordBudget(),&syntaxBytes) ||
       !wordCharge(&bytes,inputBytes) || !wordCharge(&bytes,syntaxBytes) || !wordCharge(&bytes,xlsFrontendSemMemoOwnedBytes(r->memo)))return NULL;
    xls_completion_word_unit* unit=xrtCalloc(1,sizeof(*unit));if(!unit)return NULL;
    xrtAtomic32Init(&unit->references,1);unit->batch=xlsInputBatchRetain(r->inputOwner);unit->syntax=xlsSyntaxRetain((xls_syntax*)syntax);
    unit->memo=xlsFrontendSemMemoRetain(r->memo);
    unit->usedInputs=r->usedInputs;unit->usageKnown=r->usageKnown;
    if(xlsFrontendOverlayBorrow(&unit->overlay,unit->batch,cancelled,user)!=1)goto failed;
    for(size_t i=0;i<unit->overlay.count;++i)if(!wordCharge(&bytes,strlen(unit->overlay.items[i].path)+1))goto failed;
    if(r->topology){
        unit->topology=xlsInputTopologyClone(r->topology,wordBudget()-bytes,cancelled,user);
        if(!unit->topology || !wordCharge(&bytes,xlsInputTopologyInfo(unit->topology).bytes))goto failed;
    }
    if(!wordReportCopy(&r->report,&unit->report,1,&bytes,cancelled,user) || r->query.count>XL6_COMPLETION_ITEMS ||
       !wordCharge(&bytes,r->query.count*sizeof(*r->query.items)) || !wordCharge(&bytes,r->query.count) || !wordCharge(&bytes,strlen(r->query.prefix)+1))goto failed;
    unit->query.prefix=xrtStrDup(r->query.prefix);if(!unit->query.prefix)goto failed;
    unit->query.replacement=r->query.replacement;unit->query.offset=r->query.offset;
    unit->query.items=r->query.count ? xrtCalloc(r->query.count,sizeof(*r->query.items)) : NULL;
    unit->querySelections=r->query.count ? xrtCalloc(r->query.count,1) : NULL;
    if(r->query.count && (!unit->query.items || !unit->querySelections))goto failed;
    unit->query.capacity=r->query.count;
    for(size_t i=0;i<r->query.count;++i){
        if(cancelled && cancelled(user))goto failed;
        const XL6_CompletionItem* from=&r->query.items[i];XL6_CompletionItem* to=&unit->query.items[i];
        if(!from->name || !from->kind || !wordCharge(&bytes,strlen(from->name)+1) || !wordCharge(&bytes,strlen(from->kind)+1) ||
           (from->canonical && !wordCharge(&bytes,strlen(from->canonical)+1)))goto failed;
        ++unit->query.count;to->name=xrtStrDup(from->name);to->kind=xrtStrDup(from->kind);
        to->canonical=from->canonical ? xrtStrDup(from->canonical) : NULL;to->symbolId=from->symbolId;
        if(!to->name || !to->kind || (from->canonical && !to->canonical))goto failed;
        if(from->selection){
            const char* path=xl6AstSourcePath(from->selection);
            if(path){
                int found=0;
                /* Exact captured spelling proves provenance without treating
                 * an allocating platform matcher's refusal as non-equality.
                 * Unknown/alternate-spelling located selections decline. */
                for(size_t j=0;j<unit->report.count;++j)if((unit->report.items[j].kind&XL6_SOURCE_REPORT_SOURCE) && !strcmp(path,unit->report.items[j].path)){
                    found=1;unit->querySelections[i]=!strcmp(path,r->path);break;
                }
                if(!found && from->selection->tokenRange.known)goto failed;
            }
            XL6_AstNode value={.kind=from->selection->kind,.source=from->selection->source,.tokenRange=from->selection->tokenRange};
            xls_ast_clone_options options={.bytes=wordBudget()-bytes};size_t owned=0;
            if(!options.bytes || !(to->selection=xlsAstClone(&value,&options,&owned)) || !wordCharge(&bytes,owned))goto failed;
        }
    }
    unit->bytes=bytes;return unit;
failed:wordUnitRelease(unit);return NULL;
}
xls_semantic_completion* xlsSemanticCompletionTryWordView(xls_semantic_completion* previous,const xls_input_batch* batch,
    const char* queryKey,const xls_syntax* previousSyntax,const xls_syntax* syntax,int (*cancelled)(void*),void* user)
{
    xls_semantic_completion* result=NULL;xls_completion_word_unit* unit=NULL;xls_cursor_word_view edit;
    xls_input_batch* inputLease=NULL;xls_syntax *syntaxLease=NULL,*originLease=NULL;
    if(!previous || !batch || !queryKey || !wordBudget())return NULL;
    xlsSemanticCompletionRetain(previous);
    const xls_input_batch* origin=previous->wordUnit ? previous->wordUnit->batch : previous->inputOwner;
    const xls_syntax* oldSyntax=previous->wordUnit ? previous->wordUnit->syntax : previousSyntax;
    const XL6_CompletionQuery* oldQuery=previous->wordUnit ? &previous->wordUnit->query : &previous->query;
    const xls_analysis_document* query=xlsInputBatchFind(batch,queryKey);
    const char* queryPath=syntax ? xlsSyntaxPath(syntax) : NULL;
    if(!ready(previous) || previous->signature || !query || !queryPath || strcmp(previous->path,queryPath) ||
       !wordBatchTopology(origin,batch,queryKey,1) || !wordSyntaxInput(oldSyntax,origin,queryKey) || !wordSyntaxInput(syntax,batch,queryKey))goto done;
    /* All caller-owned snapshots are pinned before the first callback. */
    inputLease=xlsInputBatchRetain(batch);syntaxLease=xlsSyntaxRetain((xls_syntax*)syntax);originLease=xlsSyntaxRetain((xls_syntax*)oldSyntax);
    if(!xlsSyntaxCursorWordProof(oldSyntax,syntax,&edit,cancelled,user) ||
       !wordRangeEqual(&edit.before,&oldQuery->replacement) || !oldQuery->prefix ||
       strlen(oldQuery->prefix)>edit.prefixSize || memcmp(oldQuery->prefix,edit.prefix,strlen(oldQuery->prefix)) ||
       stopped(cancelled,user) || !xlsSemanticCompletionValidate(previous))goto done;
    unit=previous->wordUnit ? wordUnitRetain(previous->wordUnit) : wordUnitCapture(previous,previousSyntax,cancelled,user);
    if(!unit)goto done;
    result=xrtCalloc(1,sizeof(*result));if(!result)goto done;
    xrtAtomic32Init(&result->references,1);xrtAtomic32Init(&result->invalid,0);
    result->context=xlsContextRetain((xls_context*)xlsInputBatchContext(batch));result->inputOwner=xlsInputBatchRetain(batch);
    result->wordUnit=wordUnitRetain(unit);result->wordKey=query->key;
    xl6ParseResultInit(&result->parse);xl6ImportDepsInit(&result->deps);xl6SourceReportInit(&result->report);
    if(!xrtMutexInit(&result->lock))goto failed;
    result->lockInitialized=1;result->path=xrtStrDup(queryPath);
    size_t viewBytes=unit->bytes;
    if(!result->path || !wordCharge(&viewBytes,sizeof(*result)) || !wordCharge(&viewBytes,strlen(result->path)+1) ||
       !wordCharge(&viewBytes,edit.prefixSize+1) || !wordCharge(&viewBytes,xlsInputBatchStorageBytes(batch)) ||
       !wordReportCopy(&unit->report,&result->report,0,&viewBytes,cancelled,user))goto failed;
    const char* text=xlsTextData(query->text);size_t size=xlsTextSize(query->text);
    if(size>=3 && !memcmp(text,"\xef\xbb\xbf",3)){text+=3;size-=3;}
    int replaced=0;
    for(size_t i=0;i<result->report.count;++i){
        XL6_SourceReportItem* item=&result->report.items[i];
        if(strcmp(item->path,queryPath))continue;
        if(replaced || item->kind!=XL6_SOURCE_REPORT_SOURCE || strcmp(item->origin,"overlay") ||
           size>XL6_SOURCE_REPORT_TOTAL_BYTES-(result->report.bytes-item->size))goto failed;
        result->report.bytes=result->report.bytes-item->size+size;item->text=(char*)text;item->size=item->rawSize=size;
        if(!xrtSha256(text,size,item->textSha256))goto failed;
        memcpy(item->rawSha256,item->textSha256,32);replaced=1;
    }
    if(!replaced || !wordCharge(&viewBytes,unit->query.count*sizeof(*result->query.items)))goto failed;
    result->query=(XL6_CompletionQuery){.path=result->path,.offset=edit.offset,.replacement=edit.after,.parsed=1,.reached=1,.available=1};
    result->query.prefix=xrtStrDupN(edit.prefix,edit.prefixSize);
    result->query.items=unit->query.count ? xrtCalloc(unit->query.count,sizeof(*result->query.items)) : NULL;
    if(!result->query.prefix || (unit->query.count && !result->query.items))goto failed;
    result->query.capacity=unit->query.count;
    for(size_t i=0;i<unit->query.count;++i){
        if(stopped(cancelled,user))goto failed;
        const XL6_CompletionItem* from=&unit->query.items[i];if(strncmp(from->name,result->query.prefix,edit.prefixSize))continue;
        XL6_CompletionItem* to=&result->query.items[result->query.count++];*to=*from;to->selection=NULL;
        if(from->selection){
            XL6_AstNode value={.kind=from->selection->kind,.source=from->selection->source,.tokenRange=from->selection->tokenRange};
            if(unit->querySelections[i] && value.tokenRange.known){
                if(value.tokenRange.startOffset<edit.before.endOffset && value.tokenRange.endOffset>edit.before.startOffset)goto failed;
                if(!xlsCursorWordMapRange(&edit,&value.tokenRange,&value.tokenRange))goto failed;
            }
            xls_ast_clone_options options={.bytes=wordBudget()-viewBytes};size_t owned=0;
            if(!options.bytes || !(to->selection=xlsAstClone(&value,&options,&owned)) || !wordCharge(&viewBytes,owned))goto failed;
        }
        result->query.bytes+=strlen(from->name)+1+(from->canonical ? strlen(from->canonical)+1 : 0);
    }
    result->available=1;result->cursorSyntaxApplied=1;
    result->usageKnown=wordRemapUsage(unit,batch,&result->usedInputs);result->topology=unit->topology;
    result->wordWork=(xls_completion_unit_work){.projected=1,.proofNodes=edit.nodes,.candidates=unit->query.count,.knownBytes=viewBytes};
    if(!xlsSemanticCompletionValidate(previous) || !xlsSemanticCompletionValidate(result) || stopped(cancelled,user))goto failed;
    goto done;
failed:xlsSemanticCompletionDestroy(result);result=NULL;
done:wordUnitRelease(unit);xlsInputBatchDestroy(inputLease);xlsSyntaxRelease(syntaxLease);xlsSyntaxRelease(originLease);xlsSemanticCompletionDestroy(previous);return result;
}
static const XL6_AstNode* wordDemandAst(xls_semantic_completion* r)
{
    if(!validateLocked(r))return NULL;
    if(!r->demanded)r->demanded=xlsSemanticCompletionCreateBatch(NULL,r->inputOwner,r->wordKey,r->query.offset,NULL,NULL,NULL,NULL,NULL,0);
    if(!ready(r->demanded)){
        xlsSemanticCompletionDestroy(r->demanded);r->demanded=NULL;++r->wordWork.astFailures;return NULL;
    }
    const XL6_AstNode* root=xlsSemanticCompletionAst(r->demanded);
    if(!root){++r->wordWork.astFailures;return NULL;}
    if(!validateLocked(r)){xlsSemanticCompletionDestroy(r->demanded);r->demanded=NULL;return NULL;}
    r->wordWork.astMaterialized=1;return root;
}
xls_completion_unit_work xlsSemanticCompletionUnitWork(const xls_semantic_completion* result)
{
    xls_semantic_completion* r=(xls_semantic_completion*)result;
    if(!r || !xrtMutexLock(&r->lock))return (xls_completion_unit_work){0};
    xls_completion_unit_work work=r->wordWork;xrtMutexUnlock(&r->lock);return work;
}
