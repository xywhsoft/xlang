"""Qualify pointer-field receivers using a complete O2/TCC/XRT/VFS compiler.

CPU-only direct/bound/protocol calls, spans, outputs, region observers, callback
errors and checked expiry. Independent TCC and GCC programs plus ABI negatives.
No SDK/IDE installation, engine, window, GPU, network or security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from full_compiler_qualification import verify_full_compiler
from compiler_response import command_with_response


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,build,out=[v.resolve() for v in (a.root,a.compiler,a.compiler_report,a.out)]
    metadata=verify_full_compiler(build,compiler);snapshot=Path(metadata['source'])
    win=os.name=='nt';sdk=compiler.parent/'include'
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    fixture=root/'test/cases/foreign_field_receiver.xl';native=fixture.with_name('foreign_field_receiver_native.c')
    legacy=root/'test/cases/foreign_callable_field.xl';legacy_native=legacy.with_name('foreign_callable_field_native.c')
    tools={n:Path(shutil.which(n)).resolve() for n in ('gcc','g++','objdump')}
    files=[fixture,native,fixture.with_suffix('.h'),legacy,legacy_native,legacy.with_suffix('.h'),build,
           compiler,Path(__file__),Path(__file__).with_name('full_compiler_qualification.py'),
           Path(__file__).with_name('compiler_response.py'),*tools.values(),root/'test/cases/foreign_decl_module_host.c']
    files += [compiler.parent/n for n in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so'))]
    files += [v for v in sdk.rglob('*') if v.is_file()]
    inputs={str(v):sha(v) for v in files};out.mkdir(parents=True,exist_ok=False)
    env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
    env.update(TMP=str(out),TEMP=str(out),TMPDIR=str(out),PYTHONUTF8='1',PYTHONDONTWRITEBYTECODE='1')
    if not win:env['LD_LIBRARY_PATH']=str(compiler.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],programs=[],
                fullCompilerBuild=True,shippingDelivery=False,networkTests=False,expectedFieldContracts=12)
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n');os.replace(tmp,out/'results.json')
    def run(name,command,reject=False,contains=None):
        command=list(map(str,command));actual=command_with_response(command,str(tools['gcc']),out/(name+'.rsp'))
        start=time.monotonic();r=subprocess.run(actual,cwd=root,env=env,capture_output=True,timeout=360)
        log=out/(name+'.log');raw=r.stdout+r.stderr;log.write_bytes(raw)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,expectedFailure=reject,
                                   seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert bool(r.returncode)==reject,log
        if contains is not None:assert contains in raw,log
        return raw
    def imports(name,binary):
        raw=run(name+'-imports',[tools['objdump'],'-p',binary]).lower()
        names=re.findall(rb'dll name:\s*(\S+)',raw) if win else re.findall(rb'\bneeded\s+(\S+)',raw)
        assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(names)
        report['programs'].append(dict(path=str(binary),bytes=binary.stat().st_size,sha256=sha(binary),imports=[n.decode() for n in names]))
    try:
        stage=snapshot/'tools/stage_runtime_headers.py';headers=out/'runtime-sdk'
        for v in (stage,snapshot/'tools/gen_tcc_vfs_resources.py',snapshot/'tools'/('tcc_vfs_manifest_win64_xlc.txt' if win else 'tcc_vfs_manifest_linux_xlc.txt')):inputs[str(v)]=sha(v)
        run('stage-headers',[sys.executable,'-B',stage,'--root',snapshot,'--out',headers])
        inputs.update((str(v),sha(v)) for v in headers.rglob('*') if v.is_file())
        include=['--inc-path',root,'--inc-path',sdk]
        generated=out/'generated.c';header=out/'public.h';ir=out/'lowered.json'
        run('emit',[compiler,'cgen',fixture,'--emit',generated,'--emit-header',header,*include])
        run('ir',[compiler,'ir',fixture,'--lowered','--format','json','--emit',ir,*include])
        rows=json.loads(ir.read_text(encoding='utf-8-sig'))['foreignFieldCalls'];assert len(rows)==12
        assert all(v['receiverField'] and v['receiverCName'] for v in rows)
        text=generated.read_text(encoding='utf-8-sig')
        assert 'native callable receiver field ABI' in text and 'native_callee_receiver->pUser' in text
        assert re.search(r'native_region_query_receiver_\d+->pUser',text)
        assert 'extern int add(' not in text
        inc=['-I'+str(v) for v in (root,headers/'include',sdk,sdk/'include')]
        for std,tool,lang in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('header-'+std,[tools[tool],'-std='+std,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*inc,'-x',lang,'-fsyntax-only',header])
        run('jit',[compiler,'run',fixture,native,*include],contains=b'FIELD_RECEIVER_OK')
        binary=out/('standalone.exe' if win else 'standalone')
        run('static-build',[compiler,'build',fixture,native,'-o',binary,*include])
        run('static-run',[binary],contains=b'FIELD_RECEIVER_OK');imports('static',binary)
        shared_source=out/'shared.xl';shared_source.write_text(fixture.read_text().replace('func main(...) as int','export "field_receiver_check" func check() as int'),encoding='utf-8')
        library=out/('receiver.dll' if win else 'receiver.so')
        run('shared-build',[compiler,'build',shared_source,native,'--shared','-o',library,*include])
        host=out/'host.py';host.write_text('import ctypes,sys\nlib=ctypes.CDLL(sys.argv[1])\nf=lib.field_receiver_check\nf.restype=ctypes.c_int\nassert f()==0 and f()==0\nlib.xl_script_module_unit_checked.restype=ctypes.c_int\nassert lib.xl_script_module_unit_checked()==1\n')
        run('shared-run',[sys.executable,'-B',host,library],contains=b'FIELD_RECEIVER_OK');imports('shared',library)
        runtime=compiler.parent/('xlc.dll' if win else 'libxlc.so')
        if win:shutil.copy2(runtime,out/runtime.name)
        system=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        warnings=['-Wno-'+v for v in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
        for opt in ('O0','O2'):
            exe=out/(opt+('.exe' if win else ''))
            run(opt+'-build',[tools['gcc'],'-std=c11','-Wall','-Wextra','-Werror',*warnings,'-'+opt,'-DXL_STATIC_EMBED',*inc,generated,native,root/'test/cases/foreign_decl_module_host.c',runtime,*system,'-o',exe])
            run(opt+'-run',[exe],contains=b'FIELD_RECEIVER_OK')
        original=fixture.read_text();needle='@c_field_call("pUser","void*")'
        negatives={
            'one-argument':'@c_field_call("pUser")','three-arguments':'@c_field_call("pUser","void*",0)',
            'non-string':'@c_field_call(0,"void*")','empty-field':'@c_field_call("","void*")',
            'field-expression':'@c_field_call("pUser->table","void*")','non-pointer':'@c_field_call("pUser","int")',
            'type-expression':'@c_field_call("pUser","void*);bad(")','duplicate':needle+' '+needle}
        for name,replacement in negatives.items():
            case=out/(name+'.xl');case.write_text(original.replace(needle,replacement,1));target=out/(name+'.c')
            run('reject-'+name,[compiler,'cgen',case,'--emit',target,*include],True);assert not target.exists()
        header_text=fixture.with_suffix('.h').read_text()
        for name,old,new in (
            ('receiver-type','void* pUser;','const void* pUser;'),
            ('missing-field','void* pUser;','void* absent;'),
            ('function-type','int (*add)(void*,int);','int (*add)(ReceiverTable*,int);')):
            bad=out/name/'test/cases';bad.mkdir(parents=True);(bad/'foreign_field_receiver.h').write_text(header_text.replace(old,new))
            run('reject-'+name,[tools['gcc'],'-std=c11','-DXL_STATIC_EMBED','-I'+str(out/name),*inc,'-fsyntax-only',generated],True)
        run('legacy-jit',[compiler,'run',legacy,legacy_native,*include])
        verify_full_compiler(build,compiler)
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
