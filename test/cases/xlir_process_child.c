#include "test_numeric_arg.h"
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
#include <sys/ioctl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#endif
#if defined(_WIN32)
/* ConPTY resize and input use different channels. Observe real console events
 * until both the complete input line and requested size have arrived; do not
 * turn an arbitrary sleep into a cross-channel ordering guarantee. */
static int terminalEventLine(char* line,size_t capacity,unsigned wantedColumns,unsigned wantedRows)
{
    HANDLE input=GetStdHandle(STD_INPUT_HANDLE),output=GetStdHandle(STD_OUTPUT_HANDLE);
    size_t length=0;int ready=0;ULONGLONG deadline=GetTickCount64()+5000;
    for(;;){
        CONSOLE_SCREEN_BUFFER_INFO info;
        if(!GetConsoleScreenBufferInfo(output,&info))return 97;
        unsigned columns=(unsigned)(info.srWindow.Right-info.srWindow.Left+1);
        unsigned rows=(unsigned)(info.srWindow.Bottom-info.srWindow.Top+1);
        if(ready && columns==wantedColumns && rows==wantedRows){line[length]=0;return 0;}
        ULONGLONG now=GetTickCount64();if(now>=deadline)return 99;
        if(WaitForSingleObject(input,(DWORD)(deadline-now))!=WAIT_OBJECT_0)return 99;
        INPUT_RECORD event;DWORD count=0;
        if(!ReadConsoleInputW(input,&event,1,&count) || count!=1)return 96;
        if(event.EventType!=KEY_EVENT || !event.Event.KeyEvent.bKeyDown || ready)continue;
        WCHAR character=event.Event.KeyEvent.uChar.UnicodeChar;
        if(character=='\r' || character=='\n'){
            ready=1;fputs("line-ready\n",stdout);fflush(stdout);continue;
        }
        if(!character)continue;
        if(character>127)return 96; /* This private protocol receives ASCII hello. */
        for(unsigned repeat=0;repeat<event.Event.KeyEvent.wRepeatCount;++repeat){
            if(length+1>=capacity)return 96;
            line[length++]=(char)character;
        }
    }
}
#else
static int terminalWaitSize(unsigned wantedColumns,unsigned wantedRows,const sigset_t* signals)
{
    struct timespec deadline;if(clock_gettime(CLOCK_MONOTONIC,&deadline))return 97;deadline.tv_sec+=5;
    for(;;){
        struct winsize info;if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&info))return 97;
        if(info.ws_col==wantedColumns && info.ws_row==wantedRows)return 0;
        struct timespec now,remaining;if(clock_gettime(CLOCK_MONOTONIC,&now))return 97;
        remaining.tv_sec=deadline.tv_sec-now.tv_sec;remaining.tv_nsec=deadline.tv_nsec-now.tv_nsec;
        if(remaining.tv_nsec<0){--remaining.tv_sec;remaining.tv_nsec+=1000000000;}
        if(remaining.tv_sec<0 || (!remaining.tv_sec && !remaining.tv_nsec))return 99;
        if(sigtimedwait(signals,NULL,&remaining)<0 && errno!=EINTR)return 99;
    }
}
#endif
static int childMain(int argc,char** argv)
{
    if(argc<2)return 90;
#if defined(_WIN32)
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);_setmode(_fileno(stderr),_O_BINARY);
#endif
    if(!strcmp(argv[1],"exit"))return argc>2 ? xlTestIntArg(argv[2]) : 0;
    if(!strcmp(argv[1],"terminal")){
        unsigned wantedColumns=0,wantedRows=0;
        if(argc==4){
            int columns=xlTestIntArg(argv[2]),rows=xlTestIntArg(argv[3]);
            if(columns<=0 || columns>32767 || rows<=0 || rows>32767)return 90;
            wantedColumns=(unsigned)columns;wantedRows=(unsigned)rows;
        }
#if defined(_WIN32)
        int in=_isatty(_fileno(stdin))!=0,out=_isatty(_fileno(stdout))!=0,err=_isatty(_fileno(stderr))!=0;
        if(wantedColumns){
            DWORD mode=0;HANDLE input=GetStdHandle(STD_INPUT_HANDLE);
            if(!GetConsoleMode(input,&mode) || !SetConsoleMode(input,mode|ENABLE_WINDOW_INPUT))return 97;
        }
#else
        int in=isatty(STDIN_FILENO)!=0,out=isatty(STDOUT_FILENO)!=0,err=isatty(STDERR_FILENO)!=0;
        sigset_t resized,previous;
        if(wantedColumns){
            if(sigemptyset(&resized) || sigaddset(&resized,SIGWINCH) || sigprocmask(SIG_BLOCK,&resized,&previous))return 97;
        }
#endif
        /* Application-level readiness, not a spawn/ResizePseudoConsole ack.
         * Query the attached terminal before allowing the parent to resize.
         * Windows 10 inbox ConPTY can lose requests during client startup. */
#if defined(_WIN32)
        CONSOLE_SCREEN_BUFFER_INFO initial;
        if(!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&initial))return 97;
#else
        struct winsize initial;
        if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&initial))return 97;
#endif
        printf("tty:%d:%d:%d\n",in,out,err);
        fputs("terminal-stderr\n",stderr);fflush(stdout);fflush(stderr);
        fputs("terminal-ready\n",stdout);fflush(stdout);
        char line[64];
#if defined(_WIN32)
        if(wantedColumns){int code=terminalEventLine(line,sizeof(line),wantedColumns,wantedRows);if(code)return code;}
        else
#endif
        if(!fgets(line,sizeof(line),stdin))return 96;
        size_t length=strlen(line);
        while(length && (line[length-1]=='\r' || line[length-1]=='\n'))line[--length]=0;
#if !defined(_WIN32)
        if(wantedColumns){
            fputs("line-ready\n",stdout);fflush(stdout);
            int code=terminalWaitSize(wantedColumns,wantedRows,&resized);
            if(sigprocmask(SIG_SETMASK,&previous,NULL))return 97;
            if(code)return code;
        }
#endif
        unsigned columns,rows;
#if defined(_WIN32)
        CONSOLE_SCREEN_BUFFER_INFO info;
        if(!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&info))return 97;
        columns=(unsigned)(info.srWindow.Right-info.srWindow.Left+1);
        rows=(unsigned)(info.srWindow.Bottom-info.srWindow.Top+1);
#else
        struct winsize info;
        if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&info))return 97;
        columns=info.ws_col;rows=info.ws_row;
#endif
        printf("size:%u:%u\nreply:%s\n",columns,rows,line);fflush(stdout);
        if(wantedColumns && (columns!=wantedColumns || rows!=wantedRows))return 99;
        return in && out && err && !strcmp(line,"hello") ? 0 : 98;
    }
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
