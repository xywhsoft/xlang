#include <xge.h>
#include "wgl_fault_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int frames,errors;
static char detail[1024];
static void error(const xge_error_info_t* info,void* user)
{(void)user;++errors;snprintf(detail,sizeof(detail),"%s",info->sMessage ? info->sMessage : "");}
static int frame(void* user) {(void)user;++frames;xgeQuit();return XGE_OK;}
int main(int argc,char** argv)
{
 if(argc!=2)return 2;
 _set_error_mode(_OUT_TO_STDERR);
 xge_desc_t desc={0};desc.iWidth=320;desc.iHeight=200;desc.sTitle="WGL startup verification";desc.iRunMode=XGE_RUN_GAME_LOOP;
 if(xgeInit(&desc)!=XGE_OK)return 3;
 xgeSetErrorCallback(error,NULL);int result=xgeRun(frame,NULL);xgeSetErrorCallback(NULL,NULL);xgeUnit();
 typedef xide_wgl_test_stats (*stats_proc)(void);
 stats_proc get=(stats_proc)GetProcAddress(GetModuleHandleW(L"xge.dll"),"xideWglTestStats");if(!get)return 4;
 xide_wgl_test_stats stats=get();int failure=strcmp(argv[1],"compat33")!=0;
 printf("result=%d frames=%d errors=%d requested=%d.%d contexts=%d/%d detail=%s\n",result,frames,errors,stats.major,stats.minor,stats.created,stats.deleted,detail);
 if(stats.created!=stats.deleted)return 5;
 if(failure)return result!=XGE_OK && frames==0 && errors==1 && strstr(detail,"OpenGL 3.3") ? 0 : 6;
 return result==XGE_OK && frames>0 && errors==0 && stats.requests==1 && stats.major==3 && stats.minor==3 ? 0 : 7;
}
