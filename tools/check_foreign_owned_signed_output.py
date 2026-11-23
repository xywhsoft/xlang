"""Exact signed-int owned byte/text counts and paired scalar outputs.

Negative counts reject before reading output memory and release the allocation;
native errors stay primary. Covers payload identity, NUL, empty values, carrier
OOM, function values, C/C++ headers and incompatible physical count pointers.
Existing unsigned bytes/text consumers run as regressions. CPU only.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,build,out=[v.resolve()for v in(a.root,a.compiler,a.compiler_report,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    cohort=Path(json.loads(build.read_text(encoding='utf-8-sig'))['source'])
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    cases=('foreign_owned_signed_output','foreign_owned_bytes','foreign_owned_text')
    files=[Path(__file__),compiler,build,*extra,*tools.values(),
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        cohort/'tools/stage_runtime_headers.py',cohort/'tools/gen_tcc_vfs_resources.py',
        cohort/'tools'/('tcc_vfs_manifest_win64_xlc.txt'if win else'tcc_vfs_manifest_linux_xlc.txt')]
    for name in cases:files.extend(root/'test/cases'/(name+suffix)for suffix in('.xl','.c','.h'))
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    files.extend(v for v in sdk.rglob('*')if v.is_file())
    sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
    inputs={str(q):sha(q)for q in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,shippingDelivery=False,fullCompilerBuild=True,
        scope=__doc__,inputs=inputs,rows=[],programs=[])
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
    key='PATH'if win else'LD_LIBRARY_PATH';env[key]=str(compiler.parent)+os.pathsep+env.get(key,'')
    def save():
        q=out/'results.json.tmp';q.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(q,out/'results.json')
    def run(name,cmd,reject=False):
        cmd=list(map(str,cmd));start=time.monotonic();z=subprocess.run(cmd,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=cmd,exitCode=z.returncode,expectedFailure=reject,
            seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert bool(z.returncode)==reject,log;return z.stdout+z.stderr
    try:
        private=out/'runtime-sdk'
        run('stage-runtime-headers',[sys.executable,'-B',cohort/'tools/stage_runtime_headers.py','--root',cohort,'--out',private])
        inputs.update((str(v),sha(v))for v in private.rglob('*')if v.is_file())
        include=['--inc-path',root,'--inc-path',sdk]
        c_include=['-I'+str(root),'-I'+str(private/'include'),'-I'+str(sdk)]
        for name in cases:
            source=root/'test/cases'/(name+'.xl');c=out/(name+'.c');h=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',h,*include])
            text=c.read_text(encoding='utf-8-sig')
            if name=='foreign_owned_signed_output':
                assert 'struct { void* e0; int e1; } native_raw' in text
                assert 'struct { char* e0; int e1; } native_raw' in text
                assert 'struct { char* e0; int e1; int e2; } native_raw' in text
                assert 'signedOutputPair(' in text and '&native_raw.e2' in text
                assert 'xl6COutputBytesTakeV1(native_raw.e0,(uint64_t)native_raw.e1' in text
                assert 'xl6COutputTextTakeV1(native_raw.e0,(uint64_t)native_raw.e1' in text
                for level in ('O0','O2'):
                    run('signed-provider-'+level,[tools['gcc'],'-std=c11','-'+level,'-Wall','-Wextra','-Werror',*c_include,'-fsyntax-only',source.with_suffix('.c')])
            for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_include,'-x',language,'-fsyntax-only',h])
            for kind in ('jit','standalone'):
                if kind=='jit':result=run(name+'-jit',[compiler,'run',source,*include])
                else:
                    program=out/(name+('.exe'if win else''))
                    run(name+'-build',[compiler,'build',source,'-o',program,*include])
                    result=run(name+'-execute',[program]);table=run(name+'-imports',[tools['objdump'],'-p',program])
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                    assert not any(n.lower()in {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}or b'xge3_host'in n.lower()for n in imports)
                    report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[n.decode()for n in imports]));save()
                if name=='foreign_owned_signed_output':assert b'negative bounds, error priority and rollback passed'in result
        original=(root/'test/cases/foreign_owned_signed_output.h').read_text(encoding='utf-8')
        generated=out/'foreign_owned_signed_output.c'
        for count in ('unsigned int','long'):
            folder=out/('wrong-'+count.replace(' ','-'));header=folder/'test/cases/foreign_owned_signed_output.h';header.parent.mkdir(parents=True)
            header.write_text(original.replace('void** data,int* count','void** data,'+count+'* count',1),encoding='utf-8')
            errors=run('reject-prototype-'+count.replace(' ','-'),[tools['gcc'],'-std=c11','-I'+str(folder),*c_include,'-fsyntax-only',generated],True)
            assert b'declared C function ABI'in errors
        folder=out/'wrong-scalar'
        header=folder/'test/cases/foreign_owned_signed_output.h'
        header.parent.mkdir(parents=True)
        header.write_text(original.replace('int* scalar','unsigned int* scalar'),encoding='utf-8')
        errors=run('reject-prototype-scalar',[tools['gcc'],'-std=c11','-I'+str(folder),*c_include,'-fsyntax-only',generated],True)
        assert b'declared C function ABI' in errors
        original=(root/'test/cases/foreign_owned_signed_output.xl').read_text(encoding='utf-8')
        needle='@c_output_bytes(1,2,"void*","int","xrt",0)'
        for count in ('unsigned int','int32_t','int64_t'):
            source=out/(count.replace(' ','-')+'.xl');target=source.with_suffix('.c')
            source.write_text(original.replace(needle,needle.replace('"int"','"'+count+'"'),1),encoding='utf-8')
            run('reject-count-'+count.replace(' ','-'),[compiler,'cgen',source,'--emit',target,*include],True)
            assert not target.exists()
        pair='@c_output_bytes(1,2,"char*","int","xrt",0,3)'
        for name,invalid in (
            ('overlap',original.replace(pair,pair[:-2]+'2)',1)),
            ('out-of-range',original.replace(pair,pair[:-2]+'64)',1)),
            ('wrong-type',original.replace('as (bytes,c_int)','as (bytes,c_uint)',1)),
            ('missing-result',original.replace('as (bytes,c_int)','as bytes',1)),
            ('missing-slot',original.replace(pair,pair[:-3]+')',1)),
        ):
            source=out/('reject-pair-'+name+'.xl')
            target=source.with_suffix('.c')
            source.write_text(invalid,encoding='utf-8')
            run('reject-pair-'+name,[compiler,'cgen',source,'--emit',target,*include],True)
            assert not target.exists()
        # Exercise the new descriptor inside the embedded compiler, too. A
        # parent executable and an actual shared caller must use the new VFS /
        # precompiled compiler inputs, rather than the outer CLI's frontend.
        case=(root/'test/cases/foreign_owned_signed_output.xl').as_posix()
        child='#include <xlc.h>\nint main(void){XLC_Context* c=xlcCreateContext(0);if(!c)return 1;int ok=xlcAddIncludePath(c,'+json.dumps(root.as_posix())+') && xlcAddSourceFile(c,'+json.dumps(case)+') && xlcCompile(c) && xlcRun(c,0,0) && xlcGetLastExitCode(c)==0;xlcDestroyContext(c);return ok?0:2;}\n'
        for shared in (False,True):
            name='nested-signed-shared'if shared else'nested-signed-parent'
            source=out/(name+'.xl')
            body=('export "nested_signed_output" func check() as int\n return tcc.run('+json.dumps(child)+')\nend\n')if shared else('assert tcc.run('+json.dumps(child)+')==0\n')
            source.write_text('import xlang.compiler\nimport tcc\n'+body,encoding='utf-8')
            program=out/(name+(('.dll'if win else'.so')if shared else('.exe'if win else'')))
            run(name+'-build',[compiler,'build',source,*(['--shared']if shared else[]),'-o',program])
            if shared:
                client='import ctypes,sys; m=ctypes.CDLL(sys.argv[1]); m.nested_signed_output.restype=ctypes.c_int; assert m.nested_signed_output()==0; m.xl_script_module_unit_checked.restype=ctypes.c_int; assert m.xl_script_module_unit_checked()==1; print("Nested signed outputs passed")'
                run(name+'-execute',[sys.executable,'-c',client,program])
            else:run(name+'-execute',[program])
            table=run(name+'-imports',[tools['objdump'],'-p',program])
            imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
            assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(n.lower()for n in imports)
            report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[n.decode()for n in imports]));save()
        report.update(complete=True,inputsUnchanged=all(sha(q)==h for q,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as e:report.update(complete=True,passed=False,error=str(e));raise
    finally:save()


if __name__=='__main__':main()
