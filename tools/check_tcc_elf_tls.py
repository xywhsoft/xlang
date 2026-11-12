"""Real AMD64 ELF TLS file output: loader, threads, template and host isolation.

This source-only gate does not qualify rebuilt xlang/VFS/embedded compilers.
Native ELF TLS in JIT input and local-exec TLS in a DSO must be refused.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,struct,subprocess,sys,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','tcc-lib','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();assert sys.platform=='linux'
    root,lib,out=(v.resolve() for v in (a.root,a.tcc_lib,a.out));out.mkdir(parents=True,exist_ok=False)
    source=out/'source';sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    files=[v for v in (root/'lib/tcc').rglob('*') if v.is_file() and '__pycache__' not in v.parts]
    files += [root/'test/cases'/n for n in ('tcc_elf_tls_driver.c','tcc_elf_tls_native.c','tcc_elf_tls_host.c')]+[Path(__file__).resolve()]
    inputs={}
    for old in files:
        new=source/old.relative_to(root);new.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(old,new)
        inputs[str(old)]=sha(old);assert sha(new)==inputs[str(old)]
    report=dict(complete=False,passed=False,fullCompilerBuild=False,scope=__doc__,inputs=inputs,rows=[],templates=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,args,refused=False):
        args=list(map(str,args));start=time.monotonic();r=subprocess.run(args,cwd=out,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=args,exitCode=r.returncode,seconds=time.monotonic()-start,refused=refused,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert (r.returncode!=0)==refused,log
        return r.stdout+r.stderr
    def template(path,zero):
        data=path.read_bytes();assert data[:6]==b'\x7fELF\x02\x01'
        off=struct.unpack_from('<Q',data,32)[0];size,count=struct.unpack_from('<HH',data,54)
        rows=[struct.unpack_from('<IIQQQQQQ',data,off+i*size)for i in range(count)]
        tls=[v for v in rows if v[0]==7];assert len(tls)==1
        _,_,offset,address,_,filesz,memsz,align=tls[0]
        assert filesz<=memsz and offset+filesz<=len(data) and align>=64 and memsz>=136
        assert address%align==offset%align
        if zero:assert filesz==0
        else:assert filesz>=8 and filesz<memsz
        report['templates'].append(dict(path=str(path),sha256=sha(path),fileSize=filesz,memorySize=memsz,alignment=align))
    try:
        cc=shutil.which('gcc');assert cc
        host=out/'host'
        run('host-build',[cc,'-std=c11','-O2','-Wall','-Wextra','-Werror',source/'test/cases/tcc_elf_tls_host.c','-Wl,--export-dynamic','-pthread','-ldl','-o',host])
        native=source/'test/cases/tcc_elf_tls_native.c'
        py=out/'python-host.py';py.write_text('''import ctypes,sys
p=sys.argv[1];initial=int(sys.argv[2]);lib=ctypes.CDLL(p)
lib.elf_tls_read.restype=ctypes.c_int
assert lib.elf_tls_read()==initial
for i in range(1000):
 lib.elf_tls_write(42);assert lib.elf_tls_read()==88
assert ctypes.c_int.in_dll(lib,"elf_public_tls").value==42
print("Python host returned safely with intact thread state")
''')
        for opt in ('O0','O2'):
            driver=out/('driver-'+opt)
            run('driver-'+opt,[cc,'-std=c11','-'+opt,'-g','-DTCC_DISABLE_BUILTIN_VFS','-DCONFIG_TRIPLET="x86_64-linux-gnu"','-I'+str(source/'lib/tcc'),source/'test/cases/tcc_elf_tls_driver.c','-ldl','-lm','-pthread','-o',driver])
            for model in ('global-dynamic','initial-exec'):
                for zero in (False,True):
                    tag=opt+'-'+model+('-bss'if zero else'-data');obj=out/(tag+'.o');so=out/(tag+'.so');other=out/(tag+'-other.so')
                    run(tag+'-input',[cc,'-'+opt,'-fPIC','-ftls-model='+model,*(['-DTLS_ZERO_ONLY']if zero else[]),'-c',native,'-o',obj])
                    run(tag+'-link',[driver,lib,'dll',obj,so]);template(so,zero);shutil.copy2(so,other)
                    run(tag+'-native-host',[host,so,other,0 if zero else 140])
                    run(tag+'-python-host',[sys.executable,'-B',py,so,0 if zero else 140])
                tag=opt+'-'+model+'-imported';obj=out/(tag+'.o');so=out/(tag+'.so');other=out/(tag+'-other.so')
                run(tag+'-input',[cc,'-'+opt,'-fPIC','-ftls-model='+model,'-DTLS_IMPORTED','-c',native,'-o',obj])
                run(tag+'-link',[driver,lib,'dll',obj,so]);template(so,False);shutil.copy2(so,other)
                run(tag+'-native-host',[host,so,other,140])
            exeobj=out/(opt+'-exe.o');exe=out/(opt+'-exe')
            run(opt+'-exe-input',[cc,'-'+opt,'-fPIC','-DTLS_EXECUTABLE_MAIN','-c',native,'-o',exeobj])
            run(opt+'-exe-link',[driver,lib,'exe',exeobj,exe]);template(exe,False);run(opt+'-exe-execute',[exe])
            local=out/(opt+'-local.o');invalid=out/(opt+'-invalid.so')
            run(opt+'-local-input',[cc,'-'+opt,'-fPIC','-ftls-model=local-exec','-c',native,'-o',local])
            run(opt+'-reject-local-shared',[driver,lib,'dll',local,invalid],True);assert not invalid.exists()
            run(opt+'-reject-native-jit',[driver,lib,'jit',exeobj,invalid],True)
            assert not invalid.exists()
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(k)==v for k,v in inputs.items()))
        assert report['inputsUnchanged']
    except Exception as e:report.update(complete=True,error=str(e));raise
    finally:save()
if __name__=='__main__':main()
