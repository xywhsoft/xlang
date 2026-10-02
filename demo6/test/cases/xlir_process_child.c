/* Private, deterministic subprocess. No shell, desktop app, network or user file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif
static int childMain(int argc,char** argv)
{
    if(argc<2)return 90;
#if defined(_WIN32)
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);_setmode(_fileno(stderr),_O_BINARY);
#endif
    if(!strcmp(argv[1],"exit"))return argc>2 ? atoi(argv[2]) : 0;
    if(!strcmp(argv[1],"sleep")){
        unsigned delay=argc>2 ? (unsigned)strtoul(argv[2],NULL,10) : 1000;
#if defined(_WIN32)
        Sleep(delay);
#else
        usleep(delay*1000u);
#endif
        return 0;
    }
    if(!strcmp(argv[1],"args")){
        for(int i=2;i<argc;++i){fwrite(argv[i],1,strlen(argv[i]),stdout);fputc(0,stdout);}
        return 0;
    }
    if(!strcmp(argv[1],"arg0")){fwrite(argv[0],1,strlen(argv[0]),stdout);return 0;}
    if(!strcmp(argv[1],"env")){
#if defined(_WIN32)
        if(argc<3)return 17;
        int names=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[2],-1,NULL,0);
        wchar_t* name=names ? malloc((size_t)names*sizeof(*name)) : NULL;
        if(!name)return 95;
        if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[2],-1,name,names)){free(name);return 95;}
        SetLastError(ERROR_SUCCESS);
        DWORD count=GetEnvironmentVariableW(name,NULL,0);
        if(!count){DWORD error=GetLastError();free(name);return error==ERROR_ENVVAR_NOT_FOUND ? 17 : 0;}
        wchar_t* wide=malloc((size_t)count*sizeof(*wide));if(!wide){free(name);return 95;}
        DWORD actual=GetEnvironmentVariableW(name,wide,count);free(name);
        if(actual>=count){free(wide);return 95;}
        int bytes=WideCharToMultiByte(CP_UTF8,0,wide,(int)actual,NULL,0,NULL,NULL);
        char* value=bytes ? malloc((size_t)bytes) : NULL;
        if(bytes && (!value || !WideCharToMultiByte(CP_UTF8,0,wide,(int)actual,value,bytes,NULL,NULL))){free(wide);free(value);return 95;}
        if(bytes)fwrite(value,1,(size_t)bytes,stdout);
        free(wide);free(value);return 0;
#else
        const char* value=argc>2 ? getenv(argv[2]) : NULL;
        if(!value)return 17;
        fwrite(value,1,strlen(value),stdout);return 0;
#endif
    }
    if(!strcmp(argv[1],"echo")){
        unsigned char block[4096];size_t size;
        while((size=fread(block,1,sizeof(block),stdin))!=0)
            if(fwrite(block,1,size,stdout)!=size)return 91;
        if(ferror(stdin))return 92;
        fwrite("E\0R",1,3,stderr);return 0;
    }
    if(!strcmp(argv[1],"both")){
        unsigned count=argc>2 ? (unsigned)strtoul(argv[2],NULL,10) : 1024*1024;
        unsigned char block[4096];memset(block,'Q',sizeof(block));
        while(count){size_t size=count>sizeof(block)?sizeof(block):count;
            if(fwrite(block,1,size,stdout)!=size || fwrite(block,1,size,stderr)!=size)return 93;
            count-=(unsigned)size;
        }
        return 0;
    }
    if(!strcmp(argv[1],"invalid")){const unsigned char data[]={0xff,0,0xfe};fwrite(data,1,3,stdout);return 0;}
    return 94;
}
#if defined(_WIN32)
int wmain(int argc,wchar_t** wide)
{
    char** argv=calloc((size_t)argc+1,sizeof(*argv));if(!argv)return 95;
    int result=95;
    for(int i=0;i<argc;++i){int n=WideCharToMultiByte(CP_UTF8,0,wide[i],-1,NULL,0,NULL,NULL);
        if(!n || !(argv[i]=malloc((size_t)n)))goto done;
        if(!WideCharToMultiByte(CP_UTF8,0,wide[i],-1,argv[i],n,NULL,NULL))goto done;
    }
    result=childMain(argc,argv);
done:for(int i=0;i<argc;++i)free(argv[i]);free(argv);return result;
}
#else
int main(int argc,char** argv){return childMain(argc,argv);}
#endif
