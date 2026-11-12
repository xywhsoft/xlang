/* Scoped reader comparison. Both methods use the same actual proof and XRT;
 * this is not a whole-service, IDE frame, filesystem cold-cache or RSS test. */
#define main diskGuardCorrectnessMain
#include "language_disk_guard_test.c"
#undef main
static double clockMs(void){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return(double)t.QuadPart*1000.0/(double)f.QuadPart;}
int main(int argc,char** argv)
{
 if(argc!=2||!xideIoCreateDirectories(argv[1]))return 2;
 char* path=xidePathResolve(argv[1],"bench.xl");CHECK(path);size_t sizes[]={1024,1024u*1024u,8u*1024u*1024u};char* body=malloc(sizes[2]);CHECK(body);
 for(int bom=0;bom<2;++bom)for(size_t k=0;k<3;++k){size_t n=sizes[k];memset(body,'x',n);if(bom){memcpy(body,"\xef\xbb\xbf",3);for(size_t at=3;at+10<n;at+=29)memcpy(body+at,"中文😀",10);}body[n-1]='\n';CHECK(writeBytes(path,body,n));xvalue* row=proof(path,body,n,1);CHECK(row);xide_language_check q={0};xide_model_error e={0};
  for(int i=0;i<32;++i){int method=(i%4==0||i%4==3)?0:1;reset();observing=1;double start=clockMs();int okay;
   if(method)okay=diskProofValid(&q,path,row,&e);else{char* text=xideLanguageReadDiskProof(path,row,NULL,NULL,NULL,&e);okay=text!=NULL;free(text);}double elapsed=clockMs()-start;observing=0;CHECK(okay);
   printf("%zu %d %d %.6f %zu %zu %zu %zu\n",n,bom,method,elapsed,allocations,maxAllocation,hashBytes,bytesRead);
  }xrtValueRelease(row);
 }free(body);free(path);return 0;
}
