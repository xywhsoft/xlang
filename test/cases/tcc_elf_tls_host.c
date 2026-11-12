#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static __thread unsigned char sentinel[256] __attribute__((aligned(64)));
static pthread_key_t key;
__thread int elf_host_tls=311;
typedef struct Api {
    int (*read)(void);
    void (*write)(int);
    uintptr_t (*aligned)(void);
    int (*imported_read)(void);
    void (*imported_write)(int);
} Api;
typedef struct Worker { Api api; int value; int initial; } Worker;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"ELF TLS assertion failed at %d: %s\n",__LINE__,#x); abort(); } } while (0)
static void mark(void)
{
    memset(sentinel, 0xA7, sizeof(sentinel));
    CHECK(!pthread_setspecific(key, (void *)(uintptr_t)0xC0FFEE));
    errno = EDOM;
}
static void preserved(void)
{
    for (size_t i=0; i<sizeof(sentinel); ++i) CHECK(sentinel[i]==0xA7);
    CHECK(pthread_getspecific(key)==(void *)(uintptr_t)0xC0FFEE);
    CHECK(errno==EDOM);
}
static void *worker(void *opaque)
{
    Worker *w=opaque;
    mark();
    CHECK(w->api.read()==w->initial);
    if(w->api.imported_read) {
        CHECK(w->api.imported_read()==311);
        w->api.imported_write(w->value);
        CHECK(elf_host_tls==w->value);
    }
    for (int i=0; i<10000; ++i) {
        w->api.write(w->value);
        CHECK(w->api.read()==w->value*2+4);
        CHECK((w->api.aligned()&63)==0);
        preserved();
    }
    return NULL;
}
static Api load(const char *path,void **handle)
{
    Api a;
    *handle=dlopen(path,RTLD_NOW|RTLD_LOCAL);
    if(!*handle){fprintf(stderr,"%s\n",dlerror());exit(4);}
    *(void **)(&a.read)=dlsym(*handle,"elf_tls_read");
    *(void **)(&a.write)=dlsym(*handle,"elf_tls_write");
    *(void **)(&a.aligned)=dlsym(*handle,"elf_tls_aligned");
    *(void **)(&a.imported_read)=dlsym(*handle,"elf_tls_imported_read");
    *(void **)(&a.imported_write)=dlsym(*handle,"elf_tls_imported_write");
    CHECK(a.read && a.write && a.aligned);
    return a;
}
int main(int argc,char **argv)
{
    void *left,*right;
    if(argc!=4)return 2;
    int initial=atoi(argv[3]);
    CHECK(!pthread_key_create(&key,NULL));
    Api a=load(argv[1],&left),b=load(argv[2],&right);
    if(a.imported_read) {
        CHECK(a.imported_write && a.imported_read()==311);
        a.imported_write(987);CHECK(elf_host_tls==987 && b.imported_read()==987);
    }
    mark();
    CHECK(a.read()==initial && b.read()==initial);
    CHECK((a.aligned()&63)==0 && (b.aligned()&63)==0);
    a.write(42);CHECK(a.read()==88 && b.read()==initial);
    b.write(73);CHECK(a.read()==88 && b.read()==150);
    CHECK(*(int *)dlsym(left,"elf_public_tls")==42);
    CHECK(*(int *)dlsym(right,"elf_public_tls")==73);
    preserved();
    Worker w[2]={{a,91,initial},{a,127,initial}};
    pthread_t threads[2];
    for(int i=0;i<2;++i)CHECK(!pthread_create(&threads[i],NULL,worker,&w[i]));
    for(int i=0;i<2;++i)CHECK(!pthread_join(threads[i],NULL));
    CHECK(a.read()==88 && b.read()==150);
    if(a.imported_read)CHECK(a.imported_read()==987 && elf_host_tls==987);
    CHECK(!dlclose(right) && !dlclose(left));
    a=load(argv[1],&left);CHECK(a.read()==initial);CHECK(!dlclose(left));
    CHECK(!pthread_key_delete(key));
    puts("ELF TLS: independent modules, new threads, alignment, host sentinels and reload passed");
    return 0;
}
