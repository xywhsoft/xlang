#!/usr/bin/env python3
"""Roundtrip shared solid VFS reads, cache eviction, seeking and nested catalogs."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
from gen_tcc_vfs_resources import Resource,write_catalog,c_string


def run(command,cwd):
    p=subprocess.run(list(map(str,command)),cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if p.returncode:raise RuntimeError(" ".join(map(str,command))+"\n"+p.stdout.decode("utf-8","replace"))
    return p.stdout.decode("utf-8","replace")


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("--root",type=Path,default=Path(__file__).resolve().parents[1]);ap.add_argument("--out",type=Path,required=True);ap.add_argument("--cc",type=Path,required=True);ap.add_argument("--pack-tool",type=Path,required=True);args=ap.parse_args()
    root=args.root.resolve();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    resources=[];expected=[]
    def add(name,data):
        source=out/(str(len(resources))+".bin");source.write_bytes(data);resources.append(Resource(name,source))
        h=1469598103934665603
        for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
        expected.append((name,len(data),h))
    add("include/empty.h",b"");add("include/nul.h",b"a\0b\xff")
    add("include/alias.h",b"a\0b\xff")
    add("include/数据.h",b"utf8 path")
    add("sdk.h",b"suffix path");add("include/sdk.h",b"suffix path")
    # More than 32 decoded blocks ensures real eviction, then revisit the first.
    for i in range(40):add("include/block%02d.h"%i,bytes([i])*262144)
    add("include/large.h",bytes(range(256))*3000)
    add("__xl_objects/hidden.o",b"private object")
    object_data=bytes(range(256))*100
    add("__xl_objects/static-compiler/src/example.c.o",object_data)
    object_variant=bytearray(object_data);object_variant[10000]=77;object_variant[11000]=55
    add("__xl_objects/shared-compiler/src/example.c.o",bytes(object_variant))
    add("__xl_objects/shared-runtime/src/example.c.o",bytes(object_variant))
    catalog=out/"catalog.c";write_catalog(catalog,resources,args.pack_tool.resolve(),9,False,"solid",262144)
    rows="\n".join('{"%s",%du,UINT64_C(%d)},'%(c_string(name),size,hash_value) for name,size,hash_value in expected)
    probe=out/"probe.c"
    probe.write_text(r'''#include "tcc_builtin_vfs.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
static const struct {const char* name;size_t size;uint64_t hash;} expected[]={
'''+rows+r'''};
static uint64_t hash(const unsigned char* p,size_t n){uint64_t h=UINT64_C(1469598103934665603);while(n--){h^=*p++;h*=UINT64_C(1099511628211);}return h;}
int main(int argc,char** argv){
    size_t i,n=0;unsigned char* data;int fd;char byte;
    assert(argc==2);int held=tcc_vfs_open("include/block00.h",O_RDONLY);assert(held>=0);
    for(i=0;i<sizeof(expected)/sizeof(expected[0]);++i){
        assert(tcc_vfs_read_builtin(expected[i].name,NULL,0,&n)&&n==expected[i].size);
        data=malloc(n?n:1);assert(data);
        assert(tcc_vfs_read_builtin(expected[i].name,data,n,&n));assert(hash(data,n)==expected[i].hash);
        fd=tcc_vfs_open(expected[i].name,O_RDONLY);assert(fd>=0);
        size_t offset=0;while(offset<n){int got=tcc_vfs_read(fd,data+offset,n-offset>4093?4093:n-offset);assert(got>0);offset+=(size_t)got;}
        assert(hash(data,n)==expected[i].hash&&tcc_vfs_read(fd,&byte,1)==0);
        if(n){assert(tcc_vfs_lseek(fd,0,SEEK_SET)==0);assert(tcc_vfs_read(fd,&byte,1)==1&&(unsigned char)byte==data[0]);}
        assert(tcc_vfs_close(fd)==0);free(data);
    }
    assert(tcc_vfs_read(held,&byte,1)==1&&byte==0);tcc_vfs_clear_dynamic();assert(tcc_vfs_lseek(held,0,SEEK_SET)==0&&tcc_vfs_read(held,&byte,1)==1&&byte==0);assert(tcc_vfs_close(held)==0);
    assert(tcc_vfs_mount_memory("__xl_objects/hidden.o","shadow",6));
    fd=tcc_vfs_open("__xl_objects/hidden.o",O_RDONLY);assert(fd>=0);
    char fixed[14]={0};assert(tcc_vfs_read(fd,fixed,13)==13&&!memcmp(fixed,"private object",13));assert(!tcc_vfs_close(fd));
    assert(!tcc_vfs_read_builtin("include/nul.h",&byte,1,&n)&&errno==ENOSPC&&n==4);
    assert(tcc_vfs_write_catalog_source(1,NULL,0,&n));char* source=malloc(n+1);assert(source);
    assert(!tcc_vfs_write_catalog_source(1,source,n,&n)&&errno==ENOSPC);
    assert(tcc_vfs_write_catalog_source(1,source,n+1,&n));FILE* f=fopen(argv[1],"wb");assert(f&&fwrite(source,1,n,f)==n&&!fclose(f));free(source);
    assert(tcc_vfs_write_catalog_source(0,NULL,0,&n));source=malloc(n+1);assert(source&&tcc_vfs_write_catalog_source(0,source,n+1,&n));assert(!strstr(source,"__xl_objects/hidden.o"));free(source);
    tcc_vfs_clear_dynamic();puts("catalog roundtrip, aliases, NUL, streaming, eviction, nested serialization: ok");return 0;
}
''',encoding="utf-8")
    flags=["-std=c11","-I"+str(root/"lib/tcc"),"-I"+str(root/"lib/lzma")]
    if args.cc.stem.lower() in ("tcc","tcc_bootstrap"):flags+=["-B"+str(root/"res/tcc/win64/x86_64")]
    output=[]
    for i in range(2):
        exe=out/("probe"+str(i)+".exe");generated=out/("nested"+str(i)+".c")
        run([args.cc,*flags,probe,catalog,root/"lib/tcc/tcc_builtin_vfs.c",root/"lib/tcc/tcc_utf8_io.c",root/"lib/lzma/LzmaDec.c","-o",exe],root)
        output.append(run([exe,generated],out));catalog=generated
    report=dict(Complete=True,Resources=len(resources),Output=output,GeneratedSha256=hashlib.sha256(catalog.read_bytes()).hexdigest())
    (out/"results.json").write_text(json.dumps(report,indent=2),encoding="utf-8");print(output[-1],end="")
if __name__=="__main__":main()
