#ifndef XLANG_UPDATE_H
#define XLANG_UPDATE_H
#include "xrt_profile.h"
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <signal.h>
#define UP_PATH 32768
#define UP_META_LIMIT (16u*1024u*1024u)
#define UP_FILE_LIMIT (2ull*1024u*1024u*1024u)
#if defined(_WIN64) && defined(__x86_64__)
#define UP_PLATFORM "win_x64"
#elif defined(__aarch64__) && !defined(_WIN32)
#define UP_PLATFORM "linux_arm64"
#elif defined(__arm__) && !defined(_WIN32)
#define UP_PLATFORM "linux_arm"
#elif defined(__x86_64__) && !defined(_WIN32)
#define UP_PLATFORM "linux_x64"
#else
#error "Unsupported updater architecture; add an explicit platform identity first"
#endif
typedef struct up_ui {
    int interactive,vt,plain,committing,failed,shownNotes; uint64_t started,last,done,total,fileDone,fileTotal;
    unsigned fileIndex,fileCount; char phase[128],file[2048],notes[4096],displayVersion[64]; int64_t local,version;
} up_ui;
typedef struct up_config {
    char root[UP_PATH],state[UP_PATH],base[2048],offline[UP_PATH],key[65],cleanupHelper[UP_PATH];
    int printState,check,versionOnly,cachedPolicy,recoverOnly,noLaunch,plain,staged,allowHttp,timeout; unsigned long parent,bootstrap;
} up_config;
extern volatile sig_atomic_t up_cancel;
extern char up_last_error[2048];
int up_error(const char* format,...);
void up_log_start(const char* root);
const char* up_log_path(void);
void up_log_stop(void);
void up_note(const char* text);
uint64_t up_now(void);
FILE* up_open(const char* path,const char* mode);
int up_exists(const char* path);
int up_regular(const char* path);
int up_mkdir(const char* path);
int up_parents(const char* path);
int up_join(char* dst,size_t cap,const char* base,const char* rel);
int up_safe(const char* rel);
int up_managed(const char* rel);
int up_safe_target(const char* root,const char* rel);
int up_move(const char* from,const char* to);
int up_remove(const char* path);
int up_remove_tree(const char* path);
int up_copy(const char* from,const char* to);
int up_sync(FILE* file);
int up_write(const char* path,const void* data,size_t size);
unsigned char* up_read(const char* path,size_t limit,size_t* size);
int up_hash(const char* path,char digest[65]);
uint64_t up_length(const char* path);
int up_executable(const char* path);
int up_hex(const char* text,void* bytes,size_t size);
void up_tohex(const void* bytes,size_t size,char* text);
int up_root(char* out,size_t cap,const char* selected);
int up_lock(const char* root);
int up_storage_resolve(up_config* cfg);
int up_storage_lock(up_config* cfg);
void up_unlock(void);
int up_wait_parent(unsigned long pid,int seconds);
int up_self_stage(int argc,char** argv,const char* root);
int up_needs_stage(const char* root);
int up_cleanup_stage(const char* executable,unsigned long pid);
void up_schedule_cleanup(const char* helper);
int up_launch(const char* root);
uint64_t up_space(const char* path);
void up_ui_begin(up_ui* ui,int plain);
void up_ui_draw(up_ui* ui,int force);
void up_ui_end(up_ui* ui);
int up_ui_retry(up_ui* ui);
int up_fetch(const up_config* cfg,const char* name,const char* destination,uint64_t limit,up_ui* ui);
int up_decode(const char* from,const char* to,uint64_t limit);
xvalue* up_get(const xvalue* obj,const char* key);
const char* up_text(const xvalue* obj,const char* key);
int64_t up_int(const xvalue* obj,const char* key,int64_t fallback);
#endif
