#define _POSIX_C_SOURCE 200809L
#include "update.h"
#include "public_key.h"
#include "license.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
static xstrview sv(const char* s){return (xstrview){s,strlen(s)};}
xvalue* up_get(const xvalue* o,const char* k){return o?xrtValueObjectGet(o,sv(k)):NULL;}
const char* up_text(const xvalue* o,const char* k){xstrview v;xvalue* value=up_get(o,k);if(!value||!xrtValueGetString(value,&v)||memchr(v.Data,0,v.Size))return NULL;return v.Data;}
int64_t up_int(const xvalue* o,const char* k,int64_t fallback){int64_t n;return up_get(o,k)&&xrtValueGetInt(up_get(o,k),&n)?n:fallback;}
static int set(xvalue* o,const char* key,xvalue* value){return value&&xrtValueObjectSetNew(o,sv(key),value);}
static int number(xvalue* o,const char* key,int64_t n){return set(o,key,xrtValueInt(n));}
static int string(xvalue* o,const char* key,const char* text){return set(o,key,xrtValueString(sv(text)));}
static int save(const char* path,const xvalue* value){size_t n;char* json=xrtJsonStringify(value,false,&n);int ok=json&&up_write(path,json,n);xrtFree(json);return ok;}
static xvalue* load(const char* path){size_t n;unsigned char* bytes=up_read(path,UP_META_LIMIT,&n);xvalue* value=bytes?xrtJsonParse((xstrview){(char*)bytes,n}):NULL;free(bytes);return value;}
static void cancel(int sig){(void)sig;up_cancel=1;}
static int path(char out[UP_PATH],const up_config* cfg,const char* rel){return up_join(out,UP_PATH,cfg->root,rel);}
static int matches(const char* file,const char* hash){char actual[65];return hash&&up_regular(file)==1&&up_hash(file,actual)&&!strcmp(actual,hash);}
static int valid_hash(const char* s){unsigned char b[32];return up_hex(s,b,32);}
static int force_flag(const xvalue* m,int* force){bool b=false;xvalue* v=up_get(m,"forceUpdate");if(v&&!xrtValueGetBool(v,&b))return 0;*force=b;return 1;}
static int marker(const char* path,int64_t* version,int* force){
    xvalue* value=load(path);int64_t n=0;int ok=0;*force=0;
    if(value&&xrtValueIs(value,XVALUE_OBJECT))ok=up_get(value,"forceUpdate")&&force_flag(value,force)&&(n=up_int(value,"version",0))>0;
    else if(value)ok=xrtValueGetInt(value,&n)&&n>0;
    *version=n;xrtValueRelease(value);return ok&&n<=2147483647;
}
static int signature_ok(const up_config* c,const xvalue* envelope,const char* payload){
    unsigned char key[32],sig[64];if(!up_hex(c->key,key,32))return 0;
    const char* legacy=up_text(envelope,"signature");if(up_hex(legacy,sig,64)&&xrtEd25519Verify(key,payload,strlen(payload),sig))return 1;
    /* A release may carry both the previous and next key's signature during
       rotation. Only the locally pinned key is authoritative. */
    xvalue* signatures=up_get(envelope,"signatures");if(!signatures||!xrtValueIs(signatures,XVALUE_ARRAY)||xrtValueCount(signatures)>4)return 0;
    for(unsigned i=0;i<xrtValueCount(signatures);i++){xvalue* item=xrtValueArrayGet(signatures,i);const char* pub=up_text(item,"publicKey");if(pub&&!strcmp(pub,c->key)&&up_hex(up_text(item,"signature"),sig,64)&&xrtEd25519Verify(key,payload,strlen(payload),sig))return 1;}return 0;
}
static xvalue* manifest(const up_config* c,up_ui* ui) {
    char package[UP_PATH],json[UP_PATH],versionfile[UP_PATH];path(package,c,".xlang-update/cache/update.lzma");path(json,c,".xlang-update/cache/update.json");path(versionfile,c,".xlang-update/cache/version.txt");
    for(int retry=0;retry<3&&!up_cancel;retry++){
        int64_t first=0,last=0;int firstForce=0,lastForce=0,signedForce=0;
        if(!up_fetch(c,"version.txt",versionfile,2048,NULL))return NULL;
        if(!marker(versionfile,&first,&firstForce)){up_error("Invalid version.txt");return NULL;}
        if(!up_fetch(c,"update.lzma",package,UP_META_LIMIT,NULL)||!up_decode(package,json,UP_META_LIMIT))return NULL;
        xvalue* envelope=load(json);const char* payload=up_text(envelope,"payload");
        const char* format=up_text(envelope,"format");
        if(!format||strcmp(format,"xlang-update-signed-1")||!payload||!signature_ok(c,envelope,payload)){xrtValueRelease(envelope);up_error("Manifest signature rejected; no installation files were changed");return NULL;}
        xvalue* m=xrtJsonParse(sv(payload));xrtValueRelease(envelope);
        if(!m||up_int(m,"schema",0)!=4||!up_text(m,"platform")||strcmp(up_text(m,"platform"),UP_PLATFORM)){xrtValueRelease(m);up_error("Wrong platform or unsupported manifest schema");return NULL;}
        if(!up_fetch(c,"version.txt",versionfile,2048,NULL)){xrtValueRelease(m);return NULL;}
        if(!marker(versionfile,&last,&lastForce)||!force_flag(m,&signedForce)){xrtValueRelease(m);up_error("Invalid update policy");return NULL;}
        if(first==last&&firstForce==lastForce&&up_int(m,"version",0)==first&&firstForce==signedForce){ui->version=first;const char* notes=up_text(m,"notes");snprintf(ui->notes,sizeof(ui->notes),"%s",notes?notes:"");return m;}
        xrtValueRelease(m);snprintf(ui->phase,sizeof(ui->phase),"发布正在切换，重新获取清单");up_ui_draw(ui,1);xrtSleep(700);
    }up_error("Version and manifest are inconsistent; retry later");return NULL;
}
/* Persist the signed envelope, never an unverified network boolean. */
static int64_t required_version(const up_config* c){
    char filename[UP_PATH];path(filename,c,".xlang-update/required.json");xvalue* envelope=load(filename);const char* payload=up_text(envelope,"payload");int64_t version=0;
    if(payload&&signature_ok(c,envelope,payload)){
        xvalue* m=xrtJsonParse(sv(payload));int force=0;
        if(m&&up_int(m,"schema",0)==4&&up_text(m,"platform")&&!strcmp(up_text(m,"platform"),UP_PLATFORM)&&force_flag(m,&force)&&force)version=up_int(m,"version",0);
        xrtValueRelease(m);
    }
    xrtValueRelease(envelope);return version;
}
static int remember_policy(const up_config* c,int force){
    char target[UP_PATH],source[UP_PATH];path(target,c,".xlang-update/required.json");path(source,c,".xlang-update/cache/update.json");
    if(!up_safe_target(c->root,".xlang-update/required.json"))return 0;
    if(!force)return !up_exists(target)||up_remove(target);
    size_t n=0;unsigned char* data=up_read(source,UP_META_LIMIT,&n);int ok=data&&up_write(target,data,n);free(data);return ok;
}
static int verify_manifest(const up_config* c,xvalue* m) {
    xvalue* files=up_get(m,"files"),*deletes=up_get(m,"delete");
    if(!files||!xrtValueIs(files,XVALUE_ARRAY)||xrtValueCount(files)>50000||!deletes||!xrtValueIs(deletes,XVALUE_ARRAY)||xrtValueCount(deletes)>50000)return up_error("Invalid manifest lists");
    xvalue* seen=xrtValueObject();int ok=seen!=NULL;
    for(unsigned i=0;ok&&i<xrtValueCount(files);i++){
        xvalue* item=xrtValueArrayGet(files,i);const char* p=up_text(item,"path"),*hash=up_text(item,"sha256"),*comp=up_text(item,"compression");
        int64_t n=up_int(item,"size",-1),packed=up_int(item,"archiveSize",-1),mode=up_int(item,"mode",0644);
        ok=up_managed(p)&&up_safe_target(c->root,p)&&valid_hash(hash)&&valid_hash(up_text(item,"archiveSha256"))&&n>=0&&(uint64_t)n<=UP_FILE_LIMIT&&packed>=0&&(uint64_t)packed<=UP_FILE_LIMIT&&comp&&(!strcmp(comp,"none")||!strcmp(comp,"lzma-alone"))&&(mode==0644||mode==0755);
        if(ok){char* lower=strdup(p);if(!lower){ok=0;break;}for(char* q=lower;*q;q++)if(*q>='A'&&*q<='Z')*q+=32;
            ok=!up_get(seen,lower)&&number(seen,lower,1);free(lower);}
        xvalue* parts=up_get(item,"archiveParts");if(parts){unsigned count=xrtValueCount(parts);uint64_t sum=0;ok=ok&&xrtValueIs(parts,XVALUE_ARRAY)&&count>=2&&count<=256&&up_int(item,"archivePartCount",0)==count;
            for(unsigned j=0;ok&&j<count;j++){xvalue* part=xrtValueArrayGet(parts,j);int64_t sz=up_int(part,"size",-1);ok=sz>0&&sz<=50*1024*1024&&valid_hash(up_text(part,"sha256"));sum+=(uint64_t)(sz>0?sz:0);}ok=ok&&sum==(uint64_t)packed;
        }else ok=ok&&!up_get(item,"archivePartCount");
    }
    for(unsigned i=0;ok&&i<xrtValueCount(deletes);i++){xstrview v;xvalue* value=xrtValueArrayGet(deletes,i);ok=xrtValueGetString(value,&v)&&!memchr(v.Data,0,v.Size)&&up_managed(v.Data)&&strcmp(v.Data,"update.exe")&&strcmp(v.Data,"update")&&up_safe_target(c->root,v.Data);
        if(ok){char* lower=strdup(v.Data);if(!lower){ok=0;break;}for(char* q=lower;*q;q++)if(*q>='A'&&*q<='Z')*q+=32;ok=!up_get(seen,lower)&&number(seen,lower,1);free(lower);}}
    xrtValueRelease(seen);return ok?1:up_error("Manifest contains protected, duplicate, linked or invalid file paths/objects");
}
static int cache_object(const up_config* c,const char* name,const char* hash,uint64_t size,char out[UP_PATH],up_ui* ui) {
    uint64_t fileBefore=ui->fileDone;
    char rel[512],temp[UP_PATH];snprintf(rel,sizeof(rel),".xlang-update/cache/%s",name);path(out,c,rel);if(!up_safe_target(c->root,rel))return 0;
    if(up_length(out)==size&&matches(out,hash)){ui->done+=size;ui->fileDone=fileBefore+size;return 1;}
    snprintf(temp,sizeof(temp),"%s.part",out);char remote[512];snprintf(remote,sizeof(remote),"update/%s",name);
    char partial[600];snprintf(partial,sizeof(partial),"%s.part",rel);if(!up_safe_target(c->root,partial))return 0;
    uint64_t before=ui->done;
    if(!up_fetch(c,remote,temp,size,ui)||up_length(temp)!=size||!matches(temp,hash)){up_remove(temp);return up_error("Object hash mismatch: %s",name);}
    ui->done=before+size;ui->fileDone=fileBefore+size;return up_move(temp,out);
}
static int prepare_file(const up_config* c,xvalue* item,const char* target,up_ui* ui) {
    ui->fileTotal=(uint64_t)up_int(item,"archiveSize",0);ui->fileDone=0;
    const char* hash=up_text(item,"sha256");char name[256],packed[UP_PATH];xvalue* parts=up_get(item,"archiveParts");
    if(!parts){snprintf(name,sizeof(name),"%s.lzma",hash);if(!cache_object(c,name,up_text(item,"archiveSha256"),(uint64_t)up_int(item,"archiveSize",0),packed,ui))return 0;}
    else{snprintf(name,sizeof(name),".xlang-update/cache/%s.join",hash);path(packed,c,name);if(!up_safe_target(c->root,name))return 0;FILE* out=up_open(packed,"wb");if(!out)return 0;int ok=1;
        for(unsigned j=0;ok&&j<xrtValueCount(parts);j++){char partpath[UP_PATH];xvalue* part=xrtValueArrayGet(parts,j);snprintf(name,sizeof(name),"%s-%u.lzma",hash,j+1);
            if(!cache_object(c,name,up_text(part,"sha256"),(uint64_t)up_int(part,"size",0),partpath,ui)){ok=0;break;}
            FILE* in=up_open(partpath,"rb");if(!in){ok=0;break;}unsigned char buffer[65536];size_t n;while((n=fread(buffer,1,sizeof(buffer),in))>0)if(fwrite(buffer,1,n,out)!=n){ok=0;break;}if(ferror(in))ok=0;fclose(in);}
        if(!up_sync(out))ok=0;if(fclose(out))ok=0;if(!ok||!matches(packed,up_text(item,"archiveSha256")))return up_error("Joined archive verification failed");
    }
    snprintf(ui->phase,sizeof(ui->phase),"解压并校验文件");up_ui_draw(ui,1);
    int ok=!strcmp(up_text(item,"compression"),"none")?up_copy(packed,target):up_decode(packed,target,(uint64_t)up_int(item,"size",0));
    if(!ok||up_length(target)!=(uint64_t)up_int(item,"size",0)||!matches(target,hash))return up_error("Unpacked file verification failed: %s",up_text(item,"path"));
#ifndef _WIN32
    if(chmod(target,(mode_t)up_int(item,"mode",0644)))return 0;
#endif
    return 1;
}
static int recover(const up_config* c,up_ui* ui) {
    char journal[UP_PATH],txn[UP_PATH];path(journal,c,".xlang-update/txn/journal.json");path(txn,c,".xlang-update/txn");
    if(!up_exists(journal)){return !up_exists(txn)||up_remove_tree(txn);}
    xvalue* state=load(journal);xvalue* ops=up_get(state,"ops");const char* phase=up_text(state,"phase");
    if(!ops||!xrtValueIs(ops,XVALUE_ARRAY)||!phase||xrtValueCount(ops)>50000){xrtValueRelease(state);return up_error("Recovery journal is invalid; preserved: %s",journal);}
    if(!strcmp(phase,"committed")){char receipt[UP_PATH];path(receipt,c,".xlang-update/installed.json");int ok=save(receipt,state);xrtValueRelease(state);return ok&&up_remove_tree(txn);}
    if(strcmp(phase,"applying")&&strcmp(phase,"prepared")){xrtValueRelease(state);return up_error("Unknown recovery phase");}
    snprintf(ui->phase,sizeof(ui->phase),"恢复上次未完成的安装");up_ui_draw(ui,1);
    for(unsigned n=xrtValueCount(ops);n;n--){xvalue* op=xrtValueArrayGet(ops,n-1);const char* p=up_text(op,"path");char dest[UP_PATH],backup[UP_PATH],rel[128];
        if(!up_managed(p)||!up_safe_target(c->root,p)){xrtValueRelease(state);return up_error("Unsafe recovery path");}
        path(dest,c,p);snprintf(rel,sizeof(rel),".xlang-update/txn/backup/%u",n-1);path(backup,c,rel);
        if(up_exists(backup)){
            if(up_exists(dest)&&!matches(dest,up_text(op,"newHash"))){xrtValueRelease(state);return up_error("Recovery preserved an externally changed file: %s",p);}
            if(!matches(backup,up_text(op,"oldHash"))||!up_parents(dest)||!up_move(backup,dest)){xrtValueRelease(state);return up_error("Close applications and retry recovery: %s",p);}}
        else if(!up_int(op,"existed",0)&&up_exists(dest)){
            if(!matches(dest,up_text(op,"newHash"))||!up_remove(dest)){xrtValueRelease(state);return up_error("Recovery preserved an externally changed file: %s",p);}}
        else if(up_int(op,"existed",0)&&!matches(dest,up_text(op,"oldHash"))){xrtValueRelease(state);return up_error("Recovery backup missing or destination changed; journal preserved: %s",p);}
    }
    xrtValueRelease(state);return up_remove_tree(txn);
}
static int run(up_config* c,up_ui* ui) {
    if(c->bootstrap&&!up_wait_parent(c->bootstrap,30))return up_error("Original updater did not release its executable");
    if(!c->check&&c->parent){snprintf(ui->phase,sizeof(ui->phase),"等待 IDE 正常退出");int exited=0;for(int i=0;i<120&&!up_cancel;i++){up_ui_draw(ui,0);if(up_wait_parent(c->parent,1)){exited=1;break;}}if(!exited)return up_error("IDE did not exit; installation preserved");}
    if(!up_lock(c->root))return up_error("Install directory is locked, linked or not writable: %s",c->root);
    up_log_start(c->root);
    char pendingJournal[UP_PATH];path(pendingJournal,c,".xlang-update/txn/journal.json");
    if(c->check){
        if(!up_safe_target(c->root,".xlang-update/required.json")||!up_safe_target(c->root,".xlang-update/installed.json"))return up_error("Unsafe local update policy");
        char local[UP_PATH];path(local,c,".xlang-update/installed.json");xvalue* receipt=load(local);int64_t installed=up_int(receipt,"version",0);xrtValueRelease(receipt);
        int blocked=required_version(c)>installed;
        if(c->cachedPolicy)return blocked?4:1;
        if(up_exists(pendingJournal))return blocked?4:up_error("Unfinished installation: close IDE and run updater to recover first");
    }

    if(!up_safe_target(c->root,".xlang-update/cache/update.json")||!up_safe_target(c->root,".xlang-update/txn/journal.json")||!up_safe_target(c->root,".xlang-update/txn/stage/0")||!up_safe_target(c->root,".xlang-update/txn/backup/0")||!up_safe_target(c->root,".xlang-update/installed.json")||!recover(c,ui))return 0;
    if(c->recoverOnly)return 1;
    if(!up_safe_target(c->root,".xlang-update/required.json"))return up_error("Unsafe required-update policy path");
    char receipt[UP_PATH];path(receipt,c,".xlang-update/installed.json");xvalue* installed=load(receipt);ui->local=up_int(installed,"version",0);xrtValueRelease(installed);
    int64_t required=required_version(c);
    snprintf(ui->phase,sizeof(ui->phase),"检查更新清单");up_ui_draw(ui,1);xvalue* m=manifest(c,ui);if(!m)return c->check&&required>ui->local?4:0;
    if(!verify_manifest(c,m)||ui->version<ui->local){xrtValueRelease(m);return up_error("Manifest rejected or server version is older than installed version");}
    if(ui->version<required&&required>ui->local){xrtValueRelease(m);return c->check?4:up_error("Server is older than the verified required update");}
    int force=0;if(!force_flag(m,&force)){xrtValueRelease(m);return up_error("Invalid signed update policy");}
    if(!remember_policy(c,force&&ui->version>ui->local)){int block=c->check&&force&&ui->version>ui->local;xrtValueRelease(m);up_error("Cannot persist verified update policy");return block?4:0;}
    if(c->versionOnly){int newer=ui->version>ui->local;printf("CHECK version=%lld local=%lld forceUpdate=%d\n",(long long)ui->version,(long long)ui->local,force);xrtValueRelease(m);return newer?(force?4:2):1;}
    xvalue* files=up_get(m,"files"),*deletes=up_get(m,"delete"),*ops=xrtValueArray(),*state=xrtValueObject();int ok=ops&&state;uint64_t space=16u*1024u*1024u;
    for(unsigned i=0;ok&&i<xrtValueCount(files);i++){xvalue* item=xrtValueArrayGet(files,i);char dest[UP_PATH];path(dest,c,up_text(item,"path"));
        if(matches(dest,up_text(item,"sha256"))&&(up_int(item,"mode",0644)!=0755||up_executable(dest)))continue;xvalue* op=xrtValueObject();char old[65]="";int existed=up_exists(dest);
        ok=(!existed||up_hash(dest,old))&&string(op,"path",up_text(item,"path"))&&number(op,"existed",existed)&&string(op,"oldHash",old)&&string(op,"newHash",up_text(item,"sha256"))&&number(op,"file",i)&&xrtValueArrayAppendNew(ops,op);
        ui->total+=(uint64_t)up_int(item,"archiveSize",0);space+=(uint64_t)up_int(item,"archiveSize",0)+(uint64_t)up_int(item,"size",0);ui->fileCount++;
    }
    for(unsigned i=0;ok&&i<xrtValueCount(deletes);i++){xstrview v;xrtValueGetString(xrtValueArrayGet(deletes,i),&v);char dest[UP_PATH],old[65];path(dest,c,v.Data);if(!up_exists(dest))continue;xvalue* op=xrtValueObject();
        ok=up_hash(dest,old)&&string(op,"path",v.Data)&&number(op,"existed",1)&&string(op,"oldHash",old)&&number(op,"file",-1)&&xrtValueArrayAppendNew(ops,op);}
    unsigned count=xrtValueCount(ops);printf("\nPLAN version=%lld changes=%u download=%llu\n",(long long)ui->version,count,(unsigned long long)ui->total);
    if(c->check){xrtValueRelease(ops);xrtValueRelease(state);xrtValueRelease(m);return ok?(count?2:1):0;}
    if(!ok||up_cancel||up_space(c->root)<space){xrtValueRelease(ops);xrtValueRelease(state);xrtValueRelease(m);return up_error("Planning failed, cancelled or insufficient free space (need %llu bytes)",(unsigned long long)space);}
    ok=number(state,"version",ui->version)&&string(state,"phase","prepared")&&set(state,"ops",ops); /* state owns ops */
    char txn[UP_PATH],journal[UP_PATH];path(txn,c,".xlang-update/txn");path(journal,c,".xlang-update/txn/journal.json");
    for(unsigned i=0;ok&&i<count;i++){xvalue* op=xrtValueArrayGet(ops,i);int64_t index=up_int(op,"file",-1);if(index<0)continue;char stage[UP_PATH],rel[128];snprintf(rel,sizeof(rel),".xlang-update/txn/stage/%u",i);path(stage,c,rel);
        snprintf(ui->phase,sizeof(ui->phase),"下载更新文件");snprintf(ui->file,sizeof(ui->file),"%s",up_text(op,"path"));ui->fileIndex++;up_ui_draw(ui,1);
        ok=prepare_file(c,xrtValueArrayGet(files,(unsigned)index),stage,ui);
    }
    if(ok&&!up_cancel)ok=save(journal,state);
    if(ok&&!up_cancel){ui->committing=1;snprintf(ui->phase,sizeof(ui->phase),"安装更新");up_ui_draw(ui,1);ok=string(state,"phase","applying")&&save(journal,state);
        for(unsigned i=0;ok&&i<count;i++){xvalue* op=xrtValueArrayGet(ops,i);const char* p=up_text(op,"path");char dest[UP_PATH],backup[UP_PATH],stage[UP_PATH],rel[128];path(dest,c,p);
            snprintf(rel,sizeof(rel),".xlang-update/txn/backup/%u",i);path(backup,c,rel);snprintf(rel,sizeof(rel),".xlang-update/txn/stage/%u",i);path(stage,c,rel);
            ok=up_safe_target(c->root,p)&&((up_int(op,"existed",0)&&matches(dest,up_text(op,"oldHash")))||(!up_int(op,"existed",0)&&!up_exists(dest)));
            if(ok&&up_int(op,"existed",0))ok=up_parents(backup)&&up_move(dest,backup);
            if(ok&&up_int(op,"file",-1)>=0)ok=up_parents(dest)&&up_move(stage,dest);
            if(!ok)up_error("File changed or in use: %s",p);
            /* Bounded diagnostic fault point, used by the crash-recovery fixture. */
            const char* fault=getenv("XLANG_UPDATE_TEST_CRASH_AFTER");if(ok&&fault&&strtoul(fault,NULL,10)==i+1)_Exit(86);
        }
        for(unsigned i=0;ok&&i<xrtValueCount(files);i++){xvalue* item=xrtValueArrayGet(files,i);char p[UP_PATH];path(p,c,up_text(item,"path"));ok=matches(p,up_text(item,"sha256"));}
        if(ok)ok=string(state,"phase","committed")&&save(journal,state)&&save(receipt,state);
    }else ok=0;
    xrtValueRelease(state);xrtValueRelease(m);
    if(!ok){ui->committing=1;if(up_exists(journal)){if(!recover(c,ui))up_error("Recovery pending; keep .xlang-update and run updater again");}else up_remove_tree(txn);return 0;}
    if(!up_remove_tree(txn))return up_error("Update committed; cleanup pending, run updater again");
    char cache[UP_PATH];path(cache,c,".xlang-update/cache");if(!up_remove_tree(cache))up_error("Update succeeded; cache cleanup pending");
    snprintf(ui->phase,sizeof(ui->phase),"更新完成");ui->committing=0;up_ui_draw(ui,1);return 1;
}
static int execute(int argc,char** argv) {
    if(argc==5&&!strcmp(argv[1],"--cleanup-stage")&&!strcmp(argv[3],"--wait-pid"))return up_cleanup_stage(argv[2],strtoul(argv[4],NULL,10))?0:1;
    up_config c={0};c.timeout=30;snprintf(c.base,sizeof(c.base),"https://xcompiler.com/download/update_%s",UP_PLATFORM);snprintf(c.key,sizeof(c.key),"%s",UP_PUBLIC_KEY);const char* selected=NULL;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--license")){puts(up_license);return 0;}
        if(!strcmp(argv[i],"--help")){puts("update [--root DIR] [--check] [--offline DIR] [--no-launch] [--plain]\n  --check-version --cached-policy\n  --url HTTPS_BASE --public-key HEX --timeout SECONDS\n  --allow-http permits signed loopback test servers only\nexit: 0 success/up-to-date; 2 optional update; 4 required update; 1 failure; 3 cancelled");return 0;}
        if(!strcmp(argv[i],"--version")){printf("xlang-update 2 %s\n",UP_PLATFORM);return 0;}
        if(!strcmp(argv[i],"--root")&&i+1<argc)selected=argv[++i];
        else if(!strcmp(argv[i],"--offline")&&i+1<argc)snprintf(c.offline,sizeof(c.offline),"%s",argv[++i]);
        else if(!strcmp(argv[i],"--url")&&i+1<argc)snprintf(c.base,sizeof(c.base),"%s",argv[++i]);
        else if(!strcmp(argv[i],"--public-key")&&i+1<argc)snprintf(c.key,sizeof(c.key),"%s",argv[++i]);
        else if(!strcmp(argv[i],"--parent-pid")&&i+1<argc)c.parent=strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--bootstrap-pid")&&i+1<argc)c.bootstrap=strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--cleanup-helper")&&i+1<argc)snprintf(c.cleanupHelper,sizeof(c.cleanupHelper),"%s",argv[++i]);
        else if(!strcmp(argv[i],"--timeout")&&i+1<argc)c.timeout=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--check"))c.check=1;
        else if(!strcmp(argv[i],"--check-version")){c.check=1;c.versionOnly=1;}
        else if(!strcmp(argv[i],"--cached-policy")){c.check=1;c.versionOnly=1;c.cachedPolicy=1;}
        else if(!strcmp(argv[i],"--recover-only")){c.recoverOnly=1;c.noLaunch=1;}
        else if(!strcmp(argv[i],"--plain"))c.plain=1;
        else if(!strcmp(argv[i],"--staged"))c.staged=1;
        else if(!strcmp(argv[i],"--allow-http"))c.allowHttp=1;
        else if(!strcmp(argv[i],"--no-launch"))c.noLaunch=1;
        else return up_error("Unknown or incomplete argument: %s",argv[i])?0:1;
    }
    unsigned char key[32];if(c.timeout<1||c.timeout>3600||!up_hex(c.key,key,32))return 1;
    if(selected&&!up_exists(selected)&&(!up_parents(selected)||!up_mkdir(selected)))return 1;
    if(!up_root(c.root,sizeof(c.root),selected)||up_regular(c.root)!=2)return 1;
    size_t len=strlen(c.base);while(len&&c.base[len-1]=='/')c.base[--len]=0;
    if(c.offline[0]){char full[UP_PATH];if(!up_root(full,sizeof(full),c.offline))return 1;snprintf(c.offline,sizeof(c.offline),"%s",full);}
#ifdef _WIN32
    if(!c.staged&&!c.check&&up_needs_stage(c.root)){int result=up_self_stage(argc,argv,c.root);return result?0:1;}
#endif
    signal(SIGINT,cancel);signal(SIGTERM,cancel);up_ui ui;up_ui_begin(&ui,c.plain);
    int result;
    for(;;){result=run(&c,&ui);up_log_stop();up_unlock();int retry=result==0&&!up_cancel&&ui.interactive?up_ui_retry(&ui):0;up_ui_end(&ui);if(!retry)break;up_ui_begin(&ui,c.plain);}
    if(result==1&&!up_cancel&&!c.check&&!c.noLaunch&&!up_launch(c.root))fprintf(stderr,"Update succeeded; start the IDE manually.\n");
    up_schedule_cleanup(c.cleanupHelper);
    return result==1?0:up_cancel?3:result==2?2:result==4?4:1;
}
#ifdef _WIN32
int main(void){int argc;wchar_t** w=CommandLineToArgvW(GetCommandLineW(),&argc);if(!w)return 1;char** argv=calloc((size_t)argc,sizeof(*argv));if(!argv){LocalFree(w);return 1;}int ok=1;for(int i=0;i<argc;i++){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w[i],-1,NULL,0,NULL,NULL);argv[i]=n?malloc((size_t)n):NULL;if(!argv[i]){ok=0;break;}WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w[i],-1,argv[i],n,NULL,NULL);}LocalFree(w);int result=ok?execute(argc,argv):1;for(int i=0;i<argc;i++)free(argv[i]);free(argv);return result;}
#else
int main(int argc,char** argv){return execute(argc,argv);}
#endif
