"""Qualify scoped C-string output records and transferred resident-XRT values.

Requires a complete O2 source compiler; no shipping installation, graphics,
network or security tests. Checks real CLI JIT/TCC AOT, GCC O0/O2 execution,
C/C++ header proofs, rejected declarations and native ABI mismatches.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort
from stage_runtime_headers import stage as stage_runtime_headers


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','compiler','compiler-report','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,compiler,build,out=[v.resolve()for v in(a.root,a.compiler,a.compiler_report,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);assert not out.is_relative_to(root)
    win=os.name=='nt';sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    metadata=json.loads(build.read_text());case=root/'test/cases/foreign_output_snapshots.xl'
    helper=case.with_suffix('.c');header=case.with_suffix('.h');host=root/'test/cases/foreign_decl_module_host.c'
    runtime=compiler.parent/('xl.dll'if win else'libxl.so')
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    files=[Path(__file__).resolve(),Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),compiler,build,Path(metadata['sourceFreeze']),runtime,case,helper,header,host,*extra,*tools.values()]
    files.extend(f for f in sdk.rglob('*')if f.is_file());inputs={str(f):sha(f)for f in files}
    out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir();snapshots=out/'input-snapshots';snapshots.mkdir()
    # A copied platform SDK may predate newly emitted private headers. Stage
    # the exact full compiler's VFS-selected headers, never current-root ones.
    frozen=Path(metadata['source']);fresh=out/'runtime-sdk'
    staged=stage_runtime_headers(frozen,fresh,frozen/'tools'/('tcc_vfs_manifest_win64_xlc.txt'if win else'tcc_vfs_manifest_linux_xlc.txt'))
    for f in [Path(__file__).with_name('stage_runtime_headers.py'),Path(__file__).with_name('gen_tcc_vfs_resources.py'),*(p for p in fresh.rglob('*')if p.is_file())]:inputs[str(f)]=sha(f)
    for f in(case,helper,header,Path(__file__).resolve()):shutil.copy2(f,snapshots/f.name)
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
    env.update(TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch));key='PATH'if win else'LD_LIBRARY_PATH'
    env[key]=str(compiler.parent)+os.pathsep+env.get(key,'')
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,optimization=metadata['optimization'],inputs=inputs,stagedPrivateHeaders=len(staged),privateHeaderManifest=str(fresh/'runtime-headers.manifest'),rows=[],programs=[],heap=[],budgets=[])
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n');os.replace(tmp,out/'results.json')
    def run(name,command,accepted=True):
        startup=None
        if win:startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240,startupinfo=startup,preexec_fn=None if win else no_core)
        log=out/(name+'.log');data=z.stdout+z.stderr;log.write_bytes(data)
        passed=(z.returncode==0)==accepted
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,expectedSuccess=accepted,passed=passed,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert passed,log
        return data
    def check(mode,data):
        assert b'Output values: scoped C-string record snapshots' in data
        rows=re.findall(rb'Owned value heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+)',data)
        assert len(rows)==2 and [r[0]for r in rows]==[b'language',b'native']
        assert all(list(map(int,r[1:]))==[1000,1000,0]for r in rows),rows
        counts=re.findall(rb'Output snapshot OOM: prefixes=64 record=\s*(\d+)\s+mixed=\s*(\d+)',data)
        assert len(counts)==1 and all(0<int(n)<64 for n in counts[0])
        report['heap'].append(dict(mode=mode,rounds=1000,language=[1000,1000,0],native=[1000,1000,0],scope='xvalue output pointer identity, allocation/free/reallocation only; no CPU equivalence claim'))
        report['budgets'].append(dict(mode=mode,prefixesPerFamily=64,recordFailures=int(counts[0][0]),mixedFailures=int(counts[0][1])));save()
    includes=['--inc-path',root,'--inc-path',fresh/'include','--inc-path',sdk];ci=['-I'+str(v)for v in(fresh/'include',root,sdk,compiler.parent)]
    original=case.read_text();declarations=original.split('protocol Reader')[0]
    single=next(l for l in original.splitlines()if 'func owned('in l)
    main='\nfunc main(...) as int\n return 0\nend\n'
    try:
        generated=out/'generated.c';public=out/'public.h'
        run('emit',[compiler,'cgen',case,'--emit',generated,'--emit-header',public,*includes])
        code=generated.read_text(encoding='utf-8-sig')
        assert 'native_raw.first' in code and 'native_raw.optional' in code and 'borrowed output record field type' in code
        assert re.search(r'native_admitted(?:\.e0)?=native_raw(?:\.e0)?; native_raw(?:\.e0)?=NULL;',code)
        report['generatedC']=dict(path=str(generated),sha256=sha(generated),directValueAdoption=True,scopedCStringSnapshots=True,wholeResultRollback=True,overallZeroBridgeOverheadClaim=False)
        run('ir',[compiler,'ir',case,'--lowered','--format','json','--emit',out/'lowered.json'])
        for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',public])
        check('jit',run('jit',[compiler,'run',case,*includes]))
        exe=out/('aot.exe'if win else'aot');run('aot-build',[compiler,'build',case,'-o',exe,*includes]);check('aot',run('aot-run',[exe]))
        data=run('aot-imports',[tools['objdump'],'-p',exe]);imports=re.findall(rb'DLL Name:\s*(\S+)',data)if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        assert not {v.lower()for v in imports}.intersection({b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'})
        report['programs'].append(dict(path=str(exe),bytes=exe.stat().st_size,sha256=sha(exe),imports=[v.decode()for v in imports]))
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']if win else['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        for opt in('O0','O2'):
            exe=out/('gcc-'+opt+('.exe'if win else''));run(opt+'-build',[tools['gcc'],'-std=c11','-'+opt,*ci,generated,helper,host,runtime,*systems,'-o',exe]);check(opt,run(opt+'-execute',[exe]))
        negatives={
            'owned-type':single.replace('as var','as string'),
            'owned-nullable':single.replace('as var','as var?'),
            'owned-may-raise':single.replace(' @no_raise',''),
            'owned-pure':single.replace(' @no_raise',' @no_raise @pure'),
            'owned-kind':single.replace('"owned_value"','"owned_values"'),
            'field-type':declarations.replace('@c_output_field(0,"number")','@c_output_field(0,"number","cstring")'),
            'field-owned':declarations.replace('@c_output_field(1,"first","cstring")','@c_output_field(1,"first","owned_value")'),
            'field-missing-snapshot':declarations.replace('@c_output_field(1,"first","cstring")','@c_output_field(1,"first")'),
            'field-expression':declarations.replace('@c_output_field(1,"first","cstring")','@c_output_field(1,"first[0]","cstring")'),
            'projection-effect':declarations.replace('@c_output_field(0,"first","cstring")','@c_output_field(0,"first","cstring") @pure',1),
        }
        for name,text in negatives.items():
            path=out/(name+'.xl');path.write_text(text+main);target=out/(name+'.c')
            data=run('reject-'+name,[compiler,'cgen',path,'--emit',target,*includes],False);assert not target.exists() and(b'[sem]'in data or b'[ir]'in data),data
        for name,text,message,entry in(
            ('value-slot',single.replace('@c_output(1,0)','@c_output(0,0)').replace('@c_output_view(1,','@c_output_view(0,'),b'declared C function ABI','\nfunc main(...) as int\n var value=owned()\n return 0\nend\n'),
            ('field-pointer',declarations.replace('@c_output_field(1,"first","cstring")','@c_output_field(1,"number","cstring")').replace('@c_output_field(0,"number")','@c_output_field(0,"first")'),b'borrowed output record field type','\nfunc main(...) as int\n auto root=create()\n auto n,t,o,s=root.read()\n s.close()\n root.close()\n return 0\nend\n')):
            path=out/('bad-abi-'+name+'.xl');path.write_text(text+entry);c=out/('bad-abi-'+name+'.c');h=c.with_suffix('.h')
            run('emit-bad-abi-'+name,[compiler,'cgen',path,'--emit',c,'--emit-header',h,*includes])
            data=run('reject-bad-abi-'+name,[tools['gcc'],'-std=c11',*ci,'-fsyntax-only',h],False);assert message in data,data
        report.update(complete=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
