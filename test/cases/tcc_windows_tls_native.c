/* Windows AMD64 COFF static TLS. Runs only in the explicitly requested PE gate. */
#include <windows.h>
#include <stdint.h>
__thread int tls_initial=0x13579bdf;
__thread unsigned tls_zero;
__thread unsigned tls_callback_thread;
__thread unsigned tls_order;
__thread unsigned tls_slot;
__thread unsigned char tls_aligned[64] __attribute__((aligned(64)))={0x5a};
static volatile LONG checks,failures,process_attach,thread_attach,thread_detach;
static HANDLE report_file;
static unsigned main_complete;
static void expect(int condition,LONG bit){InterlockedIncrement(&checks);if(!condition)InterlockedOr(&failures,bit);}
static void CALLBACK callback_a(void*module,DWORD reason,void*reserved){(void)module;(void)reserved;
 if(reason==DLL_PROCESS_ATTACH || reason==DLL_THREAD_ATTACH){
  expect(tls_initial==0x13579bdf,1);expect(tls_zero==0,2);expect(tls_callback_thread==0,4);expect(tls_aligned[0]==0x5a,8);expect(((uintptr_t)tls_aligned&63)==0,16);expect(tls_order==0,32);
  tls_callback_thread=GetCurrentThreadId();tls_order=1;if(reason==DLL_PROCESS_ATTACH)InterlockedIncrement(&process_attach);else InterlockedIncrement(&thread_attach);
 }else if(reason==DLL_THREAD_DETACH){
  expect(tls_order==2,64);expect(tls_slot>=1 && tls_slot<=2,128);expect(tls_initial==(int)(0x5000+tls_slot),256);expect(tls_zero==0x6000+tls_slot,512);expect(tls_aligned[0]==tls_slot,1024);tls_order=3;
 }else if(reason==DLL_PROCESS_DETACH){expect(tls_order==2 && main_complete,2048);expect(process_attach==1 && thread_attach==2 && thread_detach==2,4096);tls_order=3;}
}
static void CALLBACK callback_b(void*module,DWORD reason,void*reserved){(void)module;(void)reserved;
 if(reason==DLL_PROCESS_ATTACH || reason==DLL_THREAD_ATTACH){expect(tls_order==1,8192);tls_order=2;}
 else if(reason==DLL_THREAD_DETACH){expect(tls_order==3,16384);tls_order=4;InterlockedIncrement(&thread_detach);}
 else if(reason==DLL_PROCESS_DETACH){
  expect(tls_order==3,32768);tls_order=4;
  if(report_file && report_file!=INVALID_HANDLE_VALUE){
   uint64_t base=(uintptr_t)GetModuleHandleW(NULL);
   uint32_t data[10]={0x544c5337,(uint32_t)checks,(uint32_t)failures,(uint32_t)process_attach,(uint32_t)thread_attach,(uint32_t)thread_detach,1,main_complete,(uint32_t)base,(uint32_t)(base>>32)};DWORD written=0;
   WriteFile(report_file,data,sizeof data,&written,NULL);CloseHandle(report_file);report_file=NULL;
  }
 }
}
PIMAGE_TLS_CALLBACK callback_first __attribute__((section(".CRT$XLB"),used))=callback_a;
PIMAGE_TLS_CALLBACK callback_last __attribute__((section(".CRT$XLY"),used))=callback_b;
struct Worker{unsigned slot;HANDLE ready,release;};
static DWORD WINAPI worker(void*opaque){struct Worker*p=opaque;unsigned slot=p->slot;
 expect(tls_initial==0x13579bdf && tls_zero==0 && tls_aligned[0]==0x5a,65536);expect(tls_callback_thread==GetCurrentThreadId() && tls_order==2,131072);expect(((uintptr_t)tls_aligned&63)==0,262144);
 tls_slot=slot;tls_initial=0x5000+slot;tls_zero=0x6000+slot;tls_aligned[0]=(unsigned char)slot;
 if(!SetEvent(p->ready))return GetLastError();if(WaitForSingleObject(p->release,10000)!=WAIT_OBJECT_0)return 91;
 expect(tls_initial==(int)(0x5000+slot) && tls_zero==0x6000+slot && tls_aligned[0]==slot,524288);expect(tls_callback_thread==GetCurrentThreadId(),1048576);return 0;
}
int native_tls_run(const char*path){
 report_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(report_file==INVALID_HANDLE_VALUE)return 10;
 expect(process_attach==1 && tls_order==2,2097152);expect(tls_initial==0x13579bdf && tls_zero==0 && tls_aligned[0]==0x5a,4194304);expect(tls_callback_thread==GetCurrentThreadId(),8388608);
 tls_initial=0x11111111;tls_zero=0x22222222;tls_aligned[0]=0xa5;
 HANDLE release=CreateEventW(NULL,TRUE,FALSE,NULL);if(!release)return 11;struct Worker args[2];HANDLE threads[2]={NULL,NULL},ready[2]={NULL,NULL};int result=0;
 for(unsigned i=0;i<2;i++){ready[i]=CreateEventW(NULL,TRUE,FALSE,NULL);if(!ready[i]){result=12;break;}args[i]=(struct Worker){i+1,ready[i],release};threads[i]=CreateThread(NULL,0,worker,&args[i],0,NULL);if(!threads[i]){result=13;break;}}
 if(!result && WaitForMultipleObjects(2,ready,TRUE,10000)!=WAIT_OBJECT_0)result=14;
 if(!result){expect(thread_attach==2,16777216);expect(tls_initial==0x11111111 && tls_zero==0x22222222 && tls_aligned[0]==0xa5,33554432);}
 SetEvent(release);
 for(unsigned i=0;i<2;i++){if(threads[i]){if(WaitForSingleObject(threads[i],10000)!=WAIT_OBJECT_0)result=15;DWORD code=99;if(!GetExitCodeThread(threads[i],&code)||code)result=16;CloseHandle(threads[i]);}if(ready[i])CloseHandle(ready[i]);}CloseHandle(release);
 if(!result){expect(thread_detach==2,67108864);expect(tls_initial==0x11111111 && tls_zero==0x22222222 && tls_aligned[0]==0xa5,134217728);}
 main_complete=!result && !failures;return result?result:failures?17:0;
}
int native_tls_checks(void){return checks;}int native_tls_failures(void){return failures;}
