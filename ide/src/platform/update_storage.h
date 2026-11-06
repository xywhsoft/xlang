#ifndef XLANG_UPDATE_STORAGE_H
#define XLANG_UPDATE_STORAGE_H
/* Shared by the independent updater and the IDE. No GUI/runtime DLL calls. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif
#define XLANG_UPDATE_PATH_MAX 32768
static inline int xlangUpdateNormalizeRoot(const char* root,char out[XLANG_UPDATE_PATH_MAX]) {
    if(!root||strlen(root)>=XLANG_UPDATE_PATH_MAX)return 0;
#ifdef _WIN32
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,root,-1,NULL,0);
    wchar_t* w=n?malloc((size_t)n*sizeof(*w)):NULL;if(!w)return 0;
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,root,-1,w,n);
    int lowerSize=LCMapStringW(LOCALE_INVARIANT,LCMAP_LOWERCASE,w,-1,NULL,0);
    wchar_t* lower=lowerSize?malloc((size_t)lowerSize*sizeof(*lower)):NULL;
    int ok=lower&&LCMapStringW(LOCALE_INVARIANT,LCMAP_LOWERCASE,w,-1,lower,lowerSize)&&WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,lower,-1,out,XLANG_UPDATE_PATH_MAX,NULL,NULL);
    free(lower);free(w);if(!ok)return 0;
#else
    strcpy(out,root);
#endif
    for(char* p=out;*p;p++)if(*p=='\\')*p='/';
#ifdef _WIN32
    if(!strncmp(out,"//?/unc/",8)){memmove(out+2,out+8,strlen(out+8)+1);out[0]='/';out[1]='/';}
    else if(!strncmp(out,"//?/",4))memmove(out,out+4,strlen(out+4)+1);
#endif
    size_t len=strlen(out);while(len>1&&out[len-1]=='/')out[--len]=0;
    return len>0;
}
static inline int xlangUpdateStorageKey(const char* root,char key[65]) {
    char* normalized=malloc(XLANG_UPDATE_PATH_MAX);if(!normalized)return 0;
    int ok=xlangUpdateNormalizeRoot(root,normalized);xsha256 hash;unsigned char bytes[32];
    if(ok){xrtSha256Init(&hash);ok=xrtSha256Update(&hash,normalized,strlen(normalized))&&xrtSha256Final(&hash,bytes);}
    free(normalized);if(!ok)return 0;
    for(int i=0;i<32;i++){snprintf(key+2*i,3,"%02x",bytes[i]);}return 1;
}
static inline int xlangUpdateStorageMarker(const char* path) {
#ifdef _WIN32
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);wchar_t* w=n?malloc((size_t)n*sizeof(*w)):NULL;
    if(!w)return 0;
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,w,n);DWORD a=GetFileAttributesW(w);free(w);return a!=INVALID_FILE_ATTRIBUTES;
#else
    struct stat s;return lstat(path,&s)==0;
#endif
}
static inline int xlangUpdateStoragePath(const char* root,const char* platform,char out[XLANG_UPDATE_PATH_MAX]) {
    const char* selected=getenv("XLANG_UPDATE_STATE_DIR");if(selected&&*selected) {
#ifdef _WIN32
        if(!((strlen(selected)>2&&selected[1]==':'&&(selected[2]=='/'||selected[2]=='\\'))||(selected[0]=='/'&&selected[1]=='/')||(selected[0]=='\\'&&selected[1]=='\\')))return 0;
#else
        if(selected[0]!='/')return 0;
#endif
        return snprintf(out,XLANG_UPDATE_PATH_MAX,"%s",selected)<XLANG_UPDATE_PATH_MAX;
    }
    char* parent=malloc(XLANG_UPDATE_PATH_MAX);char* probe=malloc(XLANG_UPDATE_PATH_MAX);char key[65];
    if(!parent||!probe||!xlangUpdateNormalizeRoot(root,parent)||!xlangUpdateStorageKey(root,key)){free(parent);free(probe);return 0;}
    char* slash=strrchr(parent,'/');int dev=0,ok=0;
    if(slash&&slash[1]) {
        int platformFolder=!strcmp(slash+1,platform);*slash=0;
        char* release=strrchr(parent,'/');
        if(platformFolder&&release&&!strcmp(release+1,"release")) {
            *release=0;
            if(snprintf(probe,XLANG_UPDATE_PATH_MAX,"%s/ide/AGENTS.md",parent)<XLANG_UPDATE_PATH_MAX&&xlangUpdateStorageMarker(probe)) {
                dev=1;ok=snprintf(out,XLANG_UPDATE_PATH_MAX,"%s/ide/build/update-runtime/%s-%s",parent,platform,key)<XLANG_UPDATE_PATH_MAX;
            }
            *release='/';
        }
        if(!dev)ok=snprintf(out,XLANG_UPDATE_PATH_MAX,"%s/.xlang-update/%s-%s",parent,platform,key)<XLANG_UPDATE_PATH_MAX;
    }
    free(parent);free(probe);return ok;
}
#endif
