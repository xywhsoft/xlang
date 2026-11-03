#define _POSIX_C_SOURCE 200809L
#include "update.h"
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
static DWORD saved_mode;static CONSOLE_CURSOR_INFO saved_cursor;static COORD origin;
#else
#include <unistd.h>
#include <termios.h>
#include <poll.h>
static struct termios saved_mode;
#endif
static void text(const char* s){xrtConsoleWrite(XCONSOLE_STDOUT,(xstrview){s,strlen(s)});}
/* The server may supply notes and paths, never terminal control sequences. */
static void clean(char* out,size_t cap,const char* in,unsigned columns) {
    size_t n=0;unsigned width=0;
    for(const unsigned char* p=(const unsigned char*)in;*p&&n+5<cap;) {
        unsigned c=*p,bytes=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:(c&0xf8)==0xf0?4:1;
        unsigned w=c>=0xe0?2:1;if(width+w>columns)break;
        if(c<32||c==127){p++;continue;}size_t available=strlen((const char*)p);if(available<bytes)break;
        memcpy(out+n,p,bytes);n+=bytes;p+=bytes;width+=w;
    }out[n]=0;
}
static void bar(char* out,size_t cap,uint64_t done,uint64_t total,unsigned width) {
    unsigned fill=total?(unsigned)((double)done/(double)total*width):0;if(fill>width)fill=width;
    if(width+3>cap)width=(unsigned)cap-3;out[0]='[';for(unsigned i=0;i<width;i++)out[i+1]=i<fill?'#':'-';out[width+1]=']';out[width+2]=0;
}
void up_ui_begin(up_ui* u,int plain) {
    memset(u,0,sizeof(*u));u->plain=plain;u->started=up_now();u->interactive=!plain&&xrtConsoleIsTerminal(XCONSOLE_STDOUT);
#ifdef _WIN32
    if(u->interactive){HANDLE out=GetStdHandle(STD_OUTPUT_HANDLE),in=GetStdHandle(STD_INPUT_HANDLE);CONSOLE_SCREEN_BUFFER_INFO info;
        if(!GetConsoleMode(in,&saved_mode)||!GetConsoleCursorInfo(out,&saved_cursor)||!GetConsoleScreenBufferInfo(out,&info))u->interactive=0;
        else{origin=info.dwCursorPosition;DWORD mode=saved_mode&~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_QUICK_EDIT_MODE);SetConsoleMode(in,mode|ENABLE_EXTENDED_FLAGS);CONSOLE_CURSOR_INFO cursor=saved_cursor;cursor.bVisible=FALSE;SetConsoleCursorInfo(out,&cursor);}}
#else
    const char* term=getenv("TERM");if(!term||!strcmp(term,"dumb"))u->interactive=0;
    if(u->interactive){if(tcgetattr(STDIN_FILENO,&saved_mode))u->interactive=0;else{struct termios raw=saved_mode;raw.c_lflag&=~(ICANON|ECHO);raw.c_cc[VMIN]=0;raw.c_cc[VTIME]=0;tcsetattr(STDIN_FILENO,TCSANOW,&raw);text("\033[?25l");}}
#endif
    if(!u->interactive)text("xlang 更新器\n");
}
void up_ui_draw(up_ui* u,int force) {
    uint64_t now=up_now();
    if(u->interactive&&!u->committing){
#ifdef _WIN32
        HANDLE in=GetStdHandle(STD_INPUT_HANDLE);INPUT_RECORD event;DWORD count=0;
        while(PeekConsoleInputW(in,&event,1,&count)&&count){ReadConsoleInputW(in,&event,1,&count);if(event.EventType==KEY_EVENT&&event.Event.KeyEvent.bKeyDown&&event.Event.KeyEvent.wVirtualKeyCode==VK_ESCAPE)up_cancel=1;}
#else
        struct pollfd fd={STDIN_FILENO,POLLIN,0};if(poll(&fd,1,0)>0){char c;if(read(STDIN_FILENO,&c,1)==1&&c==27)up_cancel=1;}
#endif
    }
    if(!force&&now-u->last<(u->interactive?150:1500))return;u->last=now;
    if(force){up_note(u->phase);if(u->file[0])up_note(u->file);}
    char title[256],phase[160],file[256],notes[4096],b1[80],b2[80],line[2048];uint32 cols=80,rows=24;
    if(u->interactive)xrtConsoleSize(XCONSOLE_STDOUT,&cols,&rows);if(cols<40)cols=40;if(cols>110)cols=110;
    clean(phase,sizeof(phase),u->phase,cols>14?cols-14:20);clean(file,sizeof(file),u->file,cols>14?cols-14:20);
    if(!u->interactive){if(u->version&&!u->shownNotes){u->shownNotes=1;char safe[4096];clean(safe,sizeof(safe),u->notes,2000);snprintf(line,sizeof(line),"版本 %lld -> %lld\n",(long long)u->local,(long long)u->version);text(line);text(safe);text("\n");}snprintf(line,sizeof(line),"%s | %s | %llu/%llu bytes\n",phase,file,(unsigned long long)u->done,(unsigned long long)u->total);text(line);return;}
#ifdef _WIN32
    HANDLE out=GetStdHandle(STD_OUTPUT_HANDLE);CONSOLE_SCREEN_BUFFER_INFO info;DWORD written;
    if(GetConsoleScreenBufferInfo(out,&info)){origin.Y=info.srWindow.Top;origin.X=info.srWindow.Left;FillConsoleOutputCharacterW(out,L' ',(DWORD)(info.dwSize.X*(info.srWindow.Bottom-info.srWindow.Top+1)),origin,&written);SetConsoleCursorPosition(out,origin);}
#else
    text("\033[H\033[2J");
#endif
    snprintf(title,sizeof(title),"xlang 更新器  |  %s\n版本：%lld -> %lld\n",UP_PLATFORM,(long long)u->local,(long long)u->version);text(title);
    char border[128];unsigned bw=cols>1?cols-1:39;memset(border,'-',bw);border[bw]=0;text(border);text("\n更新内容\n");
    snprintf(notes,sizeof(notes),"%s",u->notes[0]?u->notes:"正在获取更新信息...");char* start=notes;
    for(unsigned i=0;i<(rows<20?2:4)&&start&&*start;i++){char* next=strchr(start,'\n');if(next)*next++=0;char safe[512];clean(safe,sizeof(safe),start,cols-4);text("  ");text(safe);text("\n");start=next;}
    text(border);text("\n");snprintf(line,sizeof(line),"当前步骤：%s\n当前文件：%s\n文件数量：%u / %u\n\n",phase,file,u->fileIndex,u->fileCount);text(line);
    unsigned width=cols>65?32:16;bar(b1,sizeof(b1),u->fileDone,u->fileTotal,width);bar(b2,sizeof(b2),u->done,u->total,width);
    if(cols>=70)snprintf(line,sizeof(line),"当前文件 %s %3.0f%%  %.1f / %.1f MiB\n下载总计 %s %3.0f%%  %.1f / %.1f MiB\n",b1,u->fileTotal?100.0*u->fileDone/u->fileTotal:0,u->fileDone/1048576.,u->fileTotal/1048576.,b2,u->total?100.0*u->done/u->total:0,u->done/1048576.,u->total/1048576.);
    else snprintf(line,sizeof(line),"当前文件 %s %3.0f%%\n下载总计 %s %3.0f%%\n",b1,u->fileTotal?100.0*u->fileDone/u->fileTotal:0,b2,u->total?100.0*u->done/u->total:0);text(line);
    double seconds=(now-u->started)/1000.,speed=seconds>0?u->done/seconds:0;snprintf(line,sizeof(line),"\n平均速度：%.1f MiB/s  剩余：%.0f 秒\n\n%s\n",speed/1048576.,speed>0&&u->total>u->done?(u->total-u->done)/speed:0,u->failed?"R：重试  Esc / Enter：退出":u->committing?"正在提交更新，请稍候。":"Esc / Ctrl+C：取消更新");text(line);
    if(u->failed){char safe[512];clean(safe,sizeof(safe),up_last_error,cols-1);text(safe);text("\n日志：.xlang-update/update.log\n");}xrtConsoleFlush(XCONSOLE_STDOUT);
}
void up_ui_end(up_ui* u) {if(!u->interactive)return;
#ifdef _WIN32
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),saved_mode);SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE),&saved_cursor);
#else
    tcsetattr(STDIN_FILENO,TCSANOW,&saved_mode);text("\033[0m\033[?25h\n");
#endif
    u->interactive=0;
}
int up_ui_retry(up_ui* u){u->committing=0;u->failed=1;snprintf(u->phase,sizeof(u->phase),"更新失败，现有文件已保留或等待恢复");up_ui_draw(u,1);
    while(!up_cancel){
#ifdef _WIN32
        INPUT_RECORD event;DWORD n=0;HANDLE in=GetStdHandle(STD_INPUT_HANDLE);
        if(PeekConsoleInputW(in,&event,1,&n)&&n){ReadConsoleInputW(in,&event,1,&n);if(event.EventType==KEY_EVENT&&event.Event.KeyEvent.bKeyDown){WORD k=event.Event.KeyEvent.wVirtualKeyCode;if(k=='R')return 1;if(k==VK_ESCAPE||k==VK_RETURN)return 0;}}
#else
        struct pollfd fd={STDIN_FILENO,POLLIN,0};if(poll(&fd,1,100)>0){char c;if(read(STDIN_FILENO,&c,1)==1){if(c=='r'||c=='R')return 1;if(c==27||c=='\r'||c=='\n')return 0;}}
#endif
        xrtSleep(50);
    }return 0;
}
