"""Full O2/VFS compiler execution of borrowed C-string pointer field snapshots.

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
    source=root/'test/cases/foreign_cstring_fields.xl'
    provider=root/'test/cases/foreign_cstring_fields_native.c'
    tools={name:Path(shutil.which(name)).resolve() for name in ('gcc','g++','objdump')}
    files=[source,provider,source.with_suffix('.h'),compiler,build,Path(__file__),
        Path(__file__).with_name('full_compiler_qualification.py'),root/'test/cases/foreign_decl_module_host.c',*tools.values()]
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
    def run(name,command,accepted=True):
        command=list(map(str,command));start=time.monotonic()
        r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,
            accepted=accepted,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert (r.returncode==0)==accepted,log
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
        # The header witness checks field address types, before array decay.
        original_header=source.with_suffix('.h').read_text(encoding='utf-8')
        oracle=out/'oracle/test/cases/foreign_cstring_fields.h'
        oracle.parent.mkdir(parents=True)
        field='const char* text;'
        shapes={'constant-pointer':'const char* const text;',
            'mutable-pointer':'char* text;', 'mutable-constant-pointer':'char* const text;',
            'array':'char text[32];', 'const-array':'const char text[32];',
            'unsigned':'const unsigned char* text;', 'wide':'const wchar_t* text;',
            'double-pointer':'char** text;', 'void-pointer':'void* text;',
            'function':'const char* (*text)(void);'}
        for shape,declaration in shapes.items():
            oracle.write_text('#include <stddef.h>\n'+original_header.replace(field,declaration),encoding='utf-8',newline='\n')
            accepted=shape in ('constant-pointer','mutable-pointer','mutable-constant-pointer')
            for std,tool,lang in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run('field-type-'+shape+'-'+std,[tools[tool],'-std='+std,'-Wall','-Wextra','-Werror',
                    '-I'+str(out/'oracle'),'-I'+str(root),'-I'+str(sdk),'-I'+str(sdk/'include'),
                    '-x',lang,'-fsyntax-only',header],accepted)
        original=source.read_text(encoding='utf-8')
        marker='@c_result("cstring")'
        negatives={
            'wrong-type':original.replace('get optional() as string?','get optional() as bytes?'),
            'field-function':original.replace('get optional() as string?','func optional() as string?'),
            'field-argument':original.replace('get optional() as string?','get optional(c_int index) as string?'),
            'field-static':original.replace(' extern "text" @c_field @c_result("cstring") get optional()', ' static extern "text" @c_field @c_result("cstring") get optional()',1),
            'owned':original.replace(marker,'@owned '+marker,1),
            'output':original.replace(marker,'@c_output(0) '+marker,1),
            'counted':original.replace(marker,'@c_length("size") '+marker,1),
            'duplicate':original.replace(marker,marker+' '+marker,1)}
        for name,content in negatives.items():
            assert content!=original
            case=out/(name+'.xl');case.write_text(content,encoding='utf-8',newline='\n')
            target=out/(name+'.c')
            data=run('reject-'+name,[compiler,'cgen',case,'--emit',target,*includes],False)
            assert b'[sem]' in data and not target.exists(),name
        # Compile the emitted implementation independently with GCC as well as TCC.
        runtime=compiler.parent/('xlc.dll' if win else 'libxlc.so')
        if win:shutil.copy2(runtime,out/runtime.name)
        warnings=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-label',
            '-Wno-unused-const-variable','-Wno-unused-but-set-variable']
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
        for opt in ('O0','O2'):
            binary=out/('gcc-'+opt+('.exe' if win else ''))
            run('gcc-'+opt+'-build',[tools['gcc'],'-std=c11','-'+opt,'-Wall','-Wextra','-Werror',
                *warnings,'-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(sdk),'-I'+str(sdk/'include'),
                generated,provider,root/'test/cases/foreign_decl_module_host.c',runtime,*systems,'-o',binary])
            assert b'CString fields:' in run('gcc-'+opt+'-execute',[binary])
        assert b'CString fields:' in run('jit',[compiler,'run',source,provider,*includes])
        binary=out/('cstring.exe' if win else 'cstring')
        run('exe-build',[compiler,'build',source,provider,'-o',binary,*includes])
        assert b'CString fields:' in run('exe-execute',[binary]);imports('exe',binary)
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
        assert b'CString fields:' in run('shared-execute',[sys.executable,'-B',host,shared,'cstring_check'])
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
            assert b'CString fields:' in run(name+'-execute',command)
            imports(name,binary)
        verify_full_compiler(build,compiler)
        report.update(complete=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error));raise
    finally:
        save()


if __name__=='__main__':main()
