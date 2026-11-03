#include "update.h"
#include "trust_roots.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
typedef struct request {
    FILE* file;uint64_t limit;atomic_uint_fast64_t received;atomic_int finished;
    int secure,failed,header,complete,status,sending;char error[512],wire[8192];size_t wireSize,sent;
    xbuffer pending;xhttpfield fields[100],trailers[32];xhttp1head head;xhttp1body body;
    _Atomic(void*) active;xtlsstream* tlsOwned;xnetstream* tcpOwned;
} request;
static void fail(request* r,const char* why){r->failed=1;snprintf(r->error,sizeof(r->error),"%s",why);}
static void consume(request* r,size_t n){if(n<r->pending.Size)memmove(r->pending.Data,r->pending.Data+n,r->pending.Size-n);r->pending.Size-=n;}
static int feed(request* r,const void* bytes,size_t count,int end) {
    if(r->failed)return 0;
    if(!xrtBufferAppend(&r->pending,(xbytesview){bytes,count})){fail(r,"HTTP input allocation failed");return 0;}
    if(!r->header){
        xhttp1status status=xrtHttp1ResponseParse((xbytesview){r->pending.Data,r->pending.Size},&r->head,NULL,NULL);
        if(status==XHTTP1_MORE){if(end||r->pending.Size>65536)fail(r,"HTTP header incomplete or too large");return !r->failed;}
        if(status!=XHTTP1_READY){fail(r,"Invalid HTTP response header");return 0;}
        r->status=r->head.Status;if(r->status!=200){snprintf(r->error,sizeof(r->error),"HTTP status %d",r->status);r->failed=1;return 0;}
        const xhttpfield* encoding=xrtHttp1Field(&r->head,(xstrview){"Content-Encoding",16});
        if(encoding&&!(encoding->Value.Size==8&&!memcmp(encoding->Value.Data,"identity",8))){fail(r,"Unexpected HTTP content encoding");return 0;}
        xhttp1bodyplan plan;xhttp1bodylimits limits;xrtHttp1BodyLimitsInit(&limits);limits.MaxBody=r->limit;
        if(!xrtHttp1ResponseBodyPlan(&r->head,(xstrview){"GET",3},&plan)||!xrtHttp1BodyInit(&r->body,&plan,r->trailers,32,&limits)){fail(r,"Invalid HTTP body framing");return 0;}
        size_t n=r->head.Bytes;consume(r,n);r->header=1;
    }
    while(!r->complete){size_t used=0;xbytesview data={0};
        xhttp1bodystatus s=xrtHttp1BodyRead(&r->body,(xbytesview){r->pending.Data,r->pending.Size},end,&used,&data,NULL);
        if(s==XHTTP1_BODY_DATA){uint64_t current=atomic_load(&r->received);if(data.Size>r->limit-current||fwrite(data.Data,1,data.Size,r->file)!=data.Size){fail(r,"HTTP file write or response size limit failed");return 0;}atomic_fetch_add(&r->received,data.Size);}
        consume(r,used);
        if(s==XHTTP1_BODY_DONE){r->complete=1;break;}
        if(s==XHTTP1_BODY_ERROR||s==XHTTP1_BODY_FIELDS){fail(r,"Invalid, truncated or excessive HTTP body");return 0;}
        if(s==XHTTP1_BODY_MORE||(!used&&s!=XHTTP1_BODY_DATA))break;
    }
    if(r->pending.Size>131072){fail(r,"HTTP framing buffer limit exceeded");return 0;}return 1;
}
static void tls_send(xtlsstream* s,ptr data){request* r=data;atomic_store(&r->active,s);if(r->sending)return;r->sending=1;while(r->sent<r->wireSize){size_t n=0;xtlsresult result=xrtTlsStreamSend(s,r->wire+r->sent,r->wireSize-r->sent,&n);r->sent+=n;if(result==XTLS_AGAIN)break;if(result!=XTLS_OK){fail(r,"TLS request send failed");xrtTlsStreamAbort(s);break;}}r->sending=0;}
static void tcp_send(xnetstream* s,ptr data){request* r=data;atomic_store(&r->active,s);if(r->sending||r->sent==r->wireSize)return;r->sending=1;xnetresult result=xrtNetStreamSend(s,r->wire+r->sent,r->wireSize-r->sent);if(result==XNET_RESULT_OK)r->sent=r->wireSize;else if(result!=XNET_RESULT_AGAIN){fail(r,"HTTP request send failed");xrtNetStreamAbort(s);}r->sending=0;}
static void tls_read(xtlsstream* s,const xnetbuf* b,ptr data){(void)b;request* r=data;while(xrtTlsStreamAvailable(s)){xnetspan span;if(!xrtNetBufFront(xrtTlsStreamBuffer(s),&span)||!feed(r,span.Data,span.Size,0)){xrtTlsStreamAbort(s);return;}if(!xrtTlsStreamConsume(s,span.Size)){fail(r,"TLS consume failed");xrtTlsStreamAbort(s);return;}if(r->complete){xrtTlsStreamAbort(s);return;}}}
static void tcp_read(xnetstream* s,xnetbuf* b,ptr data){request* r=data;while(xrtNetBufSize(b)){xnetspan span;if(!xrtNetBufFront(b,&span)||!feed(r,span.Data,span.Size,0)){xrtNetStreamAbort(s);return;}if(!xrtNetStreamConsume(s,span.Size)){fail(r,"HTTP consume failed");xrtNetStreamAbort(s);return;}if(r->complete){xrtNetStreamAbort(s);return;}}}
static void closed(request* r,xnetresult result,const xerror* error){if(!r->complete&&!r->failed){if(result==XNET_RESULT_OK)feed(r,NULL,0,1);else fail(r,error?xrtErrorMessage(error):"Network connection failed");}atomic_store_explicit(&r->finished,1,memory_order_release);}
static void tls_close(xtlsstream* s,xnetresult result,const xerror* e,ptr data){(void)s;closed(data,result,e);}
static void tcp_close(xnetstream* s,xnetresult result,const xerror* e,ptr data){(void)s;closed(data,result,e);}
static void tls_done(xtlsdial* d,xnetresult result,xtlsstream* s,const xerror* e,ptr data){(void)d;request* r=data;if(result==XNET_RESULT_OK)r->tlsOwned=s;else{fail(r,e?xrtErrorMessage(e):"TLS dial failed");atomic_store_explicit(&r->finished,1,memory_order_release);}}
static void tcp_done(xnetdial* d,xnetresult result,xnetstream* s,const xerror* e,ptr data){(void)d;request* r=data;if(result==XNET_RESULT_OK)r->tcpOwned=s;else{fail(r,e?xrtErrorMessage(e):"HTTP dial failed");atomic_store_explicit(&r->finished,1,memory_order_release);}}
static int download(const up_config* cfg,const char* name,const char* dst,uint64_t max,up_ui* ui) {
    char url[4096],host[512],target[3072];if(snprintf(url,sizeof(url),"%s/%s",cfg->base,name)>=(int)sizeof(url))return 0;
    int secure=!strncmp(url,"https://",8);const char* authority=url+(secure?8:7);
    if(!secure&&strncmp(url,"http://",7))return up_error("Only HTTPS URLs are accepted");
    const char* slash=strchr(authority,'/');if(!slash||slash-authority>500)return up_error("Invalid update URL");memcpy(host,authority,(size_t)(slash-authority));host[slash-authority]=0;snprintf(target,sizeof(target),"%s",slash);
    for(const char* p=target;*p;p++)if((unsigned char)*p<33||*p=='\177')return up_error("Invalid URL request target");
    unsigned long port=secure?443:80;char* colon=strchr(host,':');if(colon){*colon++=0;char* end;port=strtoul(colon,&end,10);if(*end||!port||port>65535)return 0;}
    if(!secure&&(!cfg->allowHttp||(strcmp(host,"127.0.0.1")&&strcmp(host,"localhost"))))return up_error("HTTP is permitted only with --allow-http on loopback");
    for(const char* p=host;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='.'&&*p!='-')return up_error("Invalid server host");
    request r={0};r.limit=max;r.secure=secure;xrtBufferInit(&r.pending);xrtHttp1HeadInit(&r.head,r.fields,100);
    r.wireSize=(size_t)snprintf(r.wire,sizeof(r.wire),"GET %s HTTP/1.1\r\nHost: %s:%lu\r\nUser-Agent: xlang-update/1\r\nAccept-Encoding: identity\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n",target,host,port);
    if(!up_parents(dst)||!(r.file=up_open(dst,"wb")))return up_error("Cannot write download: %s",dst);
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Workers=1;ec.ThreadStack=1024*1024;
    xnetresolverconfig rc;xrtNetResolverConfigInit(&rc);rc.Workers=1;rc.ThreadStack=1024*1024;
    xnetengine* engine=xrtNetEngineCreate(&ec);xnetresolver* resolver=NULL;xtlsverifier* verifier=NULL;xx509store* store=NULL;xtlsdial* td=NULL;xnetdial* nd=NULL;int ok=0;
    if(!engine||!xrtNetEngineStart(engine)||(resolver=xrtNetResolverCreate(&rc))==NULL)goto cleanup;
    if(secure){store=xrtX509StoreSystem();if(!store)store=xrtX509StoreCreate();if(!store||!xrtX509StoreAddPem(store,up_trust_roots,strlen(up_trust_roots),NULL))goto cleanup;
        xtlsverifierconfig vc;xrtTlsVerifierConfigInit(&vc);vc.Store=store;verifier=xrtTlsVerifierCreate(&vc);if(!verifier)goto cleanup;
        xtlsclientconfig tc;xtlsdialconfig dc;xtlsstreamevents events={0};xrtTlsClientConfigInit(&tc);tc.Verifier=verifier;xrtTlsDialConfigInit(&dc);dc.Timeout=15000;
        events.Open=tls_send;events.Writable=tls_send;events.Read=tls_read;events.Close=tls_close;
        td=xrtTlsDial(engine,resolver,host,(uint16)port,&tc,&dc,&events,&r,tls_done,&r);if(!td)goto cleanup;
    }else{xnetdialconfig dc;xnetstreamevents events={0};xrtNetDialConfigInit(&dc);dc.Timeout=15000;events.Open=tcp_send;events.LowWater=NULL;events.Drain=tcp_send;events.Read=tcp_read;events.Close=tcp_close;
        nd=xrtNetDial(engine,resolver,host,(uint16)port,&dc,&events,&r,tcp_done,&r);if(!nd)goto cleanup;}
    uint64_t last=up_now(),previous=0,base=ui?ui->done:0,fileBase=ui?ui->fileDone:0;
    while(!atomic_load_explicit(&r.finished,memory_order_acquire)){
        uint64_t received=atomic_load(&r.received);if(received!=previous){last=up_now();previous=received;}
        if(ui){ui->fileDone=fileBase+received;ui->done=base+received;up_ui_draw(ui,0);}
        if(up_cancel||up_now()-last>(uint64_t)cfg->timeout*1000){void* active=atomic_load(&r.active);if(active){if(secure)xrtTlsStreamAbort(active);else xrtNetStreamAbort(active);}else{if(td)xrtTlsDialCancel(td);if(nd)xrtNetDialCancel(nd);}last=up_now();if(!up_cancel)up_error("Download idle timeout: %s",name);}
        xrtSleep(25);
    }
    ok=!r.failed&&r.complete&&!up_cancel;
cleanup:
    if(td)xrtTlsDialDestroy(td);if(nd)xrtNetDialDestroy(nd);
    if(engine)xrtNetEngineStop(engine);
    xrtTlsStreamDestroy(r.tlsOwned);xrtNetStreamDestroy(r.tcpOwned);
    if(resolver&&!xrtNetResolverDestroy(resolver))ok=0;
    if(engine&&!xrtNetEngineDestroy(engine))ok=0;
    xrtTlsVerifierRelease(verifier);xrtX509StoreFree(store);xrtBufferUnit(&r.pending);
    if(!up_sync(r.file))ok=0;if(fclose(r.file))ok=0;
    if(!ok){up_remove(dst);up_error("Download %s failed: %s",name,r.error[0]?r.error:xrtGetError()?xrtErrorMessage(xrtGetError()):"cancelled or network unavailable");}return ok;
}
int up_fetch(const up_config* cfg,const char* name,const char* dst,uint64_t limit,up_ui* ui) {
    if(!up_safe(name))return up_error("Invalid object path");
    if(cfg->offline[0]){char from[UP_PATH];if(!up_join(from,sizeof(from),cfg->offline,name)||!up_safe_target(cfg->offline,name)||up_length(from)>limit||!up_copy(from,dst))return up_error("Offline object missing or oversized: %s",name);return 1;}
    uint64_t before=ui?ui->done:0,fileBefore=ui?ui->fileDone:0;
    for(int attempt=0;attempt<3&&!up_cancel;attempt++){if(ui){ui->done=before;ui->fileDone=fileBefore;}if(download(cfg,name,dst,limit,ui))return 1;xrtSleep((attempt+1)*500);}if(ui){ui->done=before;ui->fileDone=fileBefore;}return 0;
}
