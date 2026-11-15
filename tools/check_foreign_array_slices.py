"""Qualify exact, pinned array record byte slices and disjoint record packs.

The actual complete compiler is required. No graphics, network, installation
or product runtime wrapper is introduced. Every generated fixture/log remains
in the external validation directory; no existing evidence is overwritten.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,build,out=[v.resolve() for v in (a.root,a.compiler,a.compiler_report,a.out)]
    assert not out.is_relative_to(root)
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    metadata=json.loads(build.read_text());case=root/'test/cases/foreign_array_slices.xl';helper=case.with_suffix('.c');header=case.with_suffix('.h')
    runtime=compiler.parent/('xl.dll' if win else 'libxl.so');host=root/'test/cases/foreign_decl_module_host.c'
    tools={n:Path(shutil.which(n)).resolve() for n in ('gcc','g++','objdump')}
    inputs={str(f):sha(f) for f in (Path(__file__).resolve(),compiler,build,Path(metadata['sourceFreeze']),runtime,case,helper,header,host,*extra,*tools.values(),Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'))}
    for f in sdk.rglob('*'):
        if f.is_file():inputs[str(f)]=sha(f)
    out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir();snapshots=out/'input-snapshots';snapshots.mkdir()
    for f in (case,helper,header,Path(__file__).resolve()):shutil.copy2(f,snapshots/f.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,optimization=metadata['optimization'],inputs=inputs,rows=[],programs=[],heap=[])
    def save():
        temp=out/'results.json.tmp';temp.write_text(json.dumps(report,indent=2)+'\n');os.replace(temp,out/'results.json')
    env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
    env.update(TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch))
    key='PATH' if win else 'LD_LIBRARY_PATH';env[key]=str(compiler.parent)+os.pathsep+env.get(key,'')
    def run(name,command,success=True):
        startup=None
        if win:startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup,preexec_fn=None if win else no_core)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,expectedSuccess=success,passed=(z.returncode==0)==success,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert (z.returncode==0)==success,log;return z.stdout+z.stderr
    includes=['--inc-path',root,'--inc-path',sdk]
    ci=['-I'+str(root),'-I'+str(sdk),'-I'+str(compiler.parent)]
    expected=b'Array slices: exact bytes/length/sizeof, original pointers, uint64 count, scoped pins, checked errors and OOM passed'
    def check_output(mode,output):
        assert expected in output,output
        matches=re.findall(rb'Array slice heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+) row=(\d+)',output)
        assert len(matches)==2 and [r[0] for r in matches]==[b'language',b'native'],matches
        rows={r[0].decode():dict(alloc=int(r[1]),free=int(r[2]),realloc=int(r[3]),rowBytes=int(r[4])) for r in matches}
        assert rows['language']['alloc']==rows['language']['free'] and rows['language']['realloc']==0
        assert rows['native']['alloc']==rows['native']['free']==rows['native']['realloc']==0
        report['heap'].append(dict(mode=mode,rounds=1000,counts=rows,xrtOnly=True,cpuEquivalenceClaim=False));save()
    try:
        generated=out/'generated.c';public=out/'public.h'
        run('emit',[compiler,'cgen',case,'--emit',generated,'--emit-header',public,*includes])
        code=generated.read_text(encoding='utf-8-sig')
        assert 'native_array_slice.Size' in code and 'native Array span length ABI' in code and '(uint64_t)native_array_count_' in code
        assert 'memchr(native_array_text.Data' not in code and 'native_array_record_' in code
        run('ir',[compiler,'ir',case,'--lowered','--format','json','--emit',out/'lowered.json'])
        ir=json.loads((out/'lowered.json').read_text(encoding='utf-8-sig'))
        assert any(row.get('lengthField')=='length' and row.get('lengthCName')=='uint64_t' for row in ir['foreignArrayFields'])
        assert any(row.get('sizeField')=='iSize' and row.get('sizeCName')=='uint32_t' for row in ir['foreignArrayGroups'])
        for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',public])
        # Header-associated C companions are discovered by the CLI itself.
        check_output('jit',run('jit',[compiler,'run',case,*includes]))
        executable=out/('aot.exe' if win else 'aot')
        run('aot-build',[compiler,'build',case,'-o',executable,*includes])
        check_output('aot',run('aot-run',[executable]))
        table=run('aot-imports',[tools['objdump'],'-p',executable])
        imports=re.findall(rb'DLL Name:\s*(\S+)',table) if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
        assert not {v.lower() for v in imports}.intersection({b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'})
        report['programs'].append(dict(path=str(executable),sha256=sha(executable),bytes=executable.stat().st_size,imports=[v.decode() for v in imports]))
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        for opt in ('O0','O2'):
            binary=out/('gcc-'+opt+('.exe' if win else ''))
            run('gcc-'+opt+'-build',[tools['gcc'],'-std=c11','-'+opt,*ci,generated,helper,host,runtime,*systems,'-o',binary])
            check_output('gcc-'+opt,run('gcc-'+opt+'-run',[binary]))
        declaration=case.read_text().splitlines()[0]
        minimal='\nfunc main(...) as int\n array<string> values=[]\n array<uint64> tags=[]\n textSlices(values,tags)\n return 0\nend\n'
        negatives={
            'same-fields':declaration.replace('"data","length"','"data","data"'),
            'length-collision':declaration.replace('"data","length"','"data","tag"'),
            'sizeof-collision':declaration.replace('"iSize","uint32_t"','"data","uint32_t"'),
            'mutable-pointer':declaration.replace('"const uint8_t*"','"uint8_t*"'),
            'bad-length-type':declaration.replace('"const uint8_t*","uint64_t"','"const uint8_t*","long"'),
            'nullable-source':declaration.replace('array<string>','array<string?>'),
            'wrong-source':declaration.replace('array<string>','array<int>'),
            'bad-column':declaration.replace('"data","length",0,','"data","length",2,'),
            'missing-column':declaration.replace(' @c_array_field(0,"tag",1,"value")',''),
            'bad-count-type':declaration.replace('2,"uint64_t",2','2,"uint32_t",2'),
            'no-effect-contract':declaration.replace(' @no_raise',''),
            'duplicate-column':declaration.replace(' func textSlices',' @c_array_span_field(0,"extra","extra_length",0,"const uint8_t*","uint64_t") func textSlices'),
        }
        packed=case.read_text().splitlines()[2]
        negatives['pack-physical-collision']=packed.replace('@c_array_group(1,1,2','@c_array_group(0,1,2').replace('@c_array_record(1,','@c_array_record(0,').replace('@c_array_span_field(1,','@c_array_span_field(0,').replace('@c_array_field(1,','@c_array_field(0,')
        for name,line in negatives.items():
            path=out/(name+'.xl');path.write_text(line+'\nfunc main(...) as int\n return 0\nend\n')
            target=out/(name+'.c');data=run('reject-'+name,[compiler,'cgen',path,'--emit',target,*includes],False)
            assert not target.exists() and (b'[sem]' in data or b'[ir]' in data),data
        abi={
            'length':(declaration.replace('"const uint8_t*","uint64_t"','"const uint8_t*","uint32_t"'),b'native Array span length ABI'),
            'sizeof':(declaration.replace('"iSize","uint32_t"','"iSize","size_t"'),b'native Array sizeof field ABI'),
            'pointer':(declaration.replace('"const uint8_t*"','"const char*"'),b'native Array record field ABI'),
            'count':(declaration.replace('2,"uint64_t",2','2,"int",2'),b'declared C function ABI'),
        }
        for name,(line,message) in abi.items():
            path=out/('bad-abi-'+name+'.xl');path.write_text(line+minimal);badc=out/('bad-abi-'+name+'.c');badh=badc.with_suffix('.h')
            run('emit-bad-abi-'+name,[compiler,'cgen',path,'--emit',badc,'--emit-header',badh,*includes])
            data=run('reject-bad-abi-'+name,[tools['gcc'],'-std=c11',*ci,'-fsyntax-only',badh],False)
            assert message in data,data
        report.update(complete=True,inputsUnchanged=all(sha(f)==digest for f,digest in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
