/* Shared production signature component; included in the owning TU. */
#define XLS_SIGNATURE_SYNTAX_BYTES (32u*1024u*1024u)
static int signatureParseCancelled(void* user){return xlsSignatureScopeCancelled(user);}
static int signatureFragment(const char* raw,const char* path,XL6_ParseResult* result,XL6_CompletionQuery* query,void* user)
{
    xls_signature_scope* parent=user;xls_signature_data local=*parent->data;size_t size=strlen(raw),skip=size>=3 && !memcmp(raw,"\xef\xbb\xbf",3) ? 3 : 0;
    const char* source=raw+skip;size-=skip;
    if(signatureParseCancelled(parent) || query->offset>local.offset)return 0;
    size_t start=local.offset-query->offset;
    if(start>local.size || size>local.size-start || memcmp(source,local.source+start,size))return 0;
    size_t* required[]={&local.site.calleeStart,&local.site.calleeEnd,&local.site.argumentsStart,&local.site.argumentStart};
    for(size_t i=0;i<sizeof(required)/sizeof(*required);++i){if(*required[i]<start || *required[i]-start>size)return 0;*required[i]-=start;}
    if(local.site.closed){if(local.site.argumentsEnd<start || local.site.argumentsEnd-start>=size)return 0;local.site.argumentsEnd-=start;}
    if(local.site.nameEnd>local.site.nameStart){if(local.site.nameStart<start || local.site.nameEnd-start>size)return 0;local.site.nameStart-=start;local.site.nameEnd-=start;}
    if(local.site.namedEnd>local.site.namedStart){if(local.site.namedStart<start || local.site.namedEnd-start>size)return 0;local.site.namedStart-=start;local.site.namedEnd-=start;}
    local.path=(char*)path;local.source=source;local.size=size;local.offset=query->offset;local.projected=0;
    xls_signature_scope scope;xlsSignatureScopeBegin(&scope,&local,signatureParseCancelled,parent);scope.query=query;
    int okay=xl6ParseSourceWithCompletion(raw,path,result,query);xlsSignatureScopeEnd(&scope);return okay && !result->diags.count && !parent->cancelObserved;
}
static void signatureKeepSyntax(xls_signature_data* data,xls_syntax* syntax,size_t bytes)
{xlsSyntaxRelease(data->syntax);data->syntax=syntax;data->parseWork.cacheBytes=bytes;}
int xlsSignatureParseCached(const char* raw,const char* path,XL6_ParseResult* result,XL6_CompletionQuery* query)
{
    xls_signature_scope* scope=xlsSignatureScopeCurrent(query);
    if(!scope || !raw || !path || !result || result->root || result->diags.count)return xl6ParseSourceWithCompletion(raw,path,result,query);
    xls_signature_data* data=scope->data;const xls_text* text=xlsCallContextText(data->index);size_t size=strlen(raw),skip=size>=3 && !memcmp(raw,"\xef\xbb\xbf",3) ? 3 : 0;
    if(!text || size-skip!=data->size || memcmp(raw+skip,data->source,data->size) || !xlsCompletionPathEqual(path,data->path))return xl6ParseSourceWithCompletion(raw,path,result,query);
    if(signatureParseCancelled(scope)){query->error="signature query cancelled";return 0;}
    if(!query->prefix && !query->parsed && !query->error){
        size_t open=data->site.argumentsStart-1;const xls_text* old=xlsSyntaxText(data->syntax);
        int exact=old && xlsTextSize(old)==xlsTextSize(text) && !memcmp(xlsTextData(old),xlsTextData(text),xlsTextSize(text)) && xlsSyntaxSignatureMatches(data->syntax,path,query->offset,open);
        xls_syntax* next=NULL;xls_parse_work work={0};XL6_CompletionQuery projection={.path=path,.offset=query->offset};
        if(exact){next=xlsSyntaxRetain(data->syntax);++data->parseWork.exactHits;}
        else next=xlsSyntaxTrySignatureFunction(path,(xls_text*)text,data->syntax,&projection,signatureFragment,scope,&work);
        xl6CompletionFree(&projection);data->parseWork.fragments+=work.fragments;data->parseWork.reused+=work.reused;data->parseWork.bytes+=work.bytes;
        size_t bytes=0;int copied=next && xlsSyntaxSignatureCacheBytes(next,XLS_SIGNATURE_SYNTAX_BYTES,&bytes) && xlsRawBodyCopySignatureParse(next,query,result,open);
        if(scope->cancelObserved){xlsSyntaxRelease(next);query->error="signature query cancelled";return 0;}
        if(copied){signatureKeepSyntax(data,next,bytes);data->projected=1;return 1;}
        xlsSyntaxRelease(next);
    }
    ++data->parseWork.full;data->parseWork.bytes+=size;if(data->syntax)++data->parseWork.fallbacks;
    int okay=xl6ParseSourceWithCompletion(raw,path,result,query);
    if(okay && !scope->cancelObserved && result->root && !result->diags.count){
        xls_syntax* captured=xlsSyntaxCaptureSignature(path,(xls_text*)text,result->root,query,XLS_SIGNATURE_SYNTAX_BYTES);size_t bytes=0;
        if(captured && xlsSyntaxSignatureCacheBytes(captured,XLS_SIGNATURE_SYNTAX_BYTES,&bytes)){
            signatureKeepSyntax(data,captured,bytes);
            xlsRawBodyRegisterSyntax(captured,result->root);
        }else xlsSyntaxRelease(captured);
    }
    return okay && !scope->cancelObserved;
}
