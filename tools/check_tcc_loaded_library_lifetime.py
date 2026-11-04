"""Isolated Linux TinyCC provider and actual ELF load/unload acceptance.

The original provider and VFS remain unchanged. This initial kernel uses an
empty catalog and the supplied native distribution for standard link inputs;
it does not claim current embedded-VFS or complete compiler acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','native-root','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,native,out=[v.resolve() for v in (a.root,a.native_root,a.out)]
    assert os.name!='nt';out.mkdir(parents=True,exist_ok=False)
    cc=shutil.which('gcc');assert cc
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    source=out/'tcc-source';shutil.copytree(root/'lib/tcc',source)
    shutil.copy2(native/'build/linux/tcc-source/config.h',source/'config.h')
    shutil.copy2(native/'build/linux/tcc-source/tccdefs_.h',source/'tccdefs_.h')
    with (source/'config.h').open('a') as f:f.write('\n#define CONFIG_TCC_BUILTIN_VFS 1\n')
    fixture=root/'test/cases/tcc_loaded_library_lifetime.c'
    inputs={str(path):sha(path) for path in [Path(__file__).resolve(),fixture,Path(cc),
        root/'tools/tcc_empty_builtin_resources.c',*[p for p in source.rglob('*') if p.is_file()],
        root/'lib/lzma/LzmaDec.c',root/'lib/lzma/LzmaDec.h',root/'lib/lzma/7zTypes.h',root/'lib/lzma/Compiler.h']}
    report=dict(scope=__doc__,complete=False,passed=False,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,env=None):
        start=time.monotonic();r=subprocess.run(list(map(str,command)),capture_output=True,env=env,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log
    provider=out/'libtcc-lifetime.so'
    run('provider-build',[cc,'-std=c11','-O2','-fPIC','-shared','-Wl,-Bsymbolic','-DLIBTCC_AS_DLL',
        '-I'+str(source),'-I'+str(root/'lib/lzma'),source/'libtcc.c',source/'tcctools.c',
        source/'tcc_builtin_vfs.c',root/'lib/lzma/LzmaDec.c',root/'tools/tcc_empty_builtin_resources.c','-ldl','-lm','-o',provider])
    publisher=out/'libtcc-lifetime-native.so';publisher_source=out/'native.c'
    publisher_source.write_text('int tcc_lifetime_value(void){return 37;}\n')
    run('publisher-build',[cc,'-std=c11','-O2','-fPIC','-shared',publisher_source,'-o',publisher])
    for level in ('O0','O2'):
        program=out/('lifetime-'+level)
        run(level+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+level,'-I'+str(source),fixture,
            provider,'-Wl,-rpath,'+str(out),'-ldl','-o',program])
        run(level+'-run',[program,publisher,native/'build/linux/tcc-dist'])
    report.update(complete=True,passed=all(sha(path)==value for path,value in inputs.items()),
        provider=str(provider),providerSha256=sha(provider));save();assert report['passed']
if __name__=='__main__':main()
