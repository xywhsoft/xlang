"""Full O2/VFS compiler execution of borrowed C-string return contracts.

Includes independent executables/shared modules and real nested compiler hosts.
No network/GPU tests or final SDK/IDE installation.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from full_compiler_qualification import verify_full_compiler


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    root,compiler,build,out=[p.resolve() for p in
        (args.root,args.compiler,args.compiler_report,args.out)]
    verify_full_compiler(build,compiler)
    win=os.name=='nt';sdk=compiler.parent/'include'
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    source=root/'test/cases/foreign_cstring_result.xl'
    provider=root/'test/cases/foreign_cstring_result_native.c'
    tools={name:Path(shutil.which(name)).resolve() for name in ('gcc','g++','objdump')}
    files=[source,provider,source.with_suffix('.h'),compiler,build,Path(__file__),
        Path(__file__).with_name('full_compiler_qualification.py'),*tools.values()]
    files += [compiler.parent/n for n in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so'))]
    files.extend(p for p in sdk.rglob('*') if p.is_file())
    inputs={str(p):sha(p) for p in files}
    out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,fullCompilerBuild=True,
        shippingDelivery=False,networkTests=False,inputs=inputs,rows=[],programs=[])
    env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
    def save():
        temporary=out/'results.json.tmp'
        temporary.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        os.replace(temporary,out/'results.json')
    def run(name,command):
        command=list(map(str,command));start=time.monotonic()
        r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,
            seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert not r.returncode,log
        return r.stdout+r.stderr
    def imports(name,path):
        data=run(name+'-imports',[tools['objdump'],'-p',path])
        names=re.findall(rb'DLL Name:\s*(\S+)',data) if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(n.lower() for n in names)
        report['programs'].append(dict(path=str(path),sha256=sha(path),bytes=path.stat().st_size,
            imports=[n.decode() for n in names]))
        save()
    try:
        includes=['--inc-path',root,'--inc-path',sdk]
        header=out/'cstring.h';generated=out/'cstring.c'
        run('emit',[compiler,'cgen',source,'--emit',generated,'--emit-header',header,*includes])
        assert 'const char*' in generated.read_text(encoding='utf-8-sig')
        for std,tool,lang in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('header-'+std,[tools[tool],'-std='+std,'-Wall','-Wextra','-Werror',
                '-I'+str(root),'-I'+str(sdk),'-I'+str(sdk/'include'),'-x',lang,'-fsyntax-only',header])
        assert b'CSTRING_RESULT_OK' in run('jit',[compiler,'run',source,provider,*includes])
        binary=out/('cstring.exe' if win else 'cstring')
        run('exe-build',[compiler,'build',source,provider,'-o',binary,*includes])
        assert b'CSTRING_RESULT_OK' in run('exe-execute',[binary]);imports('exe',binary)
        shared_source=out/'cstring-shared.xl'
        shared_source.write_text(source.read_text(encoding='utf-8').replace(
            'func main(...) as int','export "cstring_check" func check() as int'),encoding='utf-8')
        shared=out/('cstring.dll' if win else 'cstring.so')
        run('shared-build',[compiler,'build',shared_source,provider,'--shared','-o',shared,*includes])
        host=out/'shared-host.py'
        host.write_text('import ctypes,sys\nlib=ctypes.CDLL(sys.argv[1])\n'
            'f=getattr(lib,sys.argv[2]);f.restype=ctypes.c_int\nassert f()==0\n'
            'lib.xl_script_module_unit_checked.restype=ctypes.c_int\n'
            'assert lib.xl_script_module_unit_checked()==1\n',encoding='utf-8')
        assert b'CSTRING_RESULT_OK' in run('shared-execute',[sys.executable,'-B',host,shared,'cstring_check'])
        imports('shared',shared)
        c='#include <xlc.h>\nint main(void){XLC_Context*c=xlcCreateContext(0);if(!c)return 1;int ok='
        c+='xlcAddIncludePath(c,'+json.dumps(root.as_posix())+')&&'
        c+='xlcAddSourceFile(c,'+json.dumps(source.as_posix())+')&&'
        c+='xlcAddCSourceFile(c,'+json.dumps(provider.as_posix())+')&&'
        c+='xlcCompile(c)&&xlcRun(c,0,0)&&xlcGetLastExitCode(c)==0;xlcDestroyContext(c);return ok?0:2;}\n'
        for shared_mode in (False,True):
            name='nested-shared' if shared_mode else 'nested-exe'
            script=out/(name+'.xl')
            text='import xlang.compiler\nimport tcc\n'
            text+=('export "cstring_check" func check() as int\n return tcc.run('+json.dumps(c)+')\nend\n'
                if shared_mode else 'assert tcc.run('+json.dumps(c)+')==0\n')
            script.write_text(text,encoding='utf-8')
            binary=out/(name+('.dll' if win else '.so') if shared_mode else name+('.exe' if win else ''))
            run(name+'-build',[compiler,'build',script,*(['--shared'] if shared_mode else []),'-o',binary])
            command=[sys.executable,'-B',host,binary,'cstring_check'] if shared_mode else [binary]
            assert b'CSTRING_RESULT_OK' in run(name+'-execute',command)
            imports(name,binary)
        verify_full_compiler(build,compiler)
        report.update(complete=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error));raise
    finally:
        save()


if __name__=='__main__':main()
