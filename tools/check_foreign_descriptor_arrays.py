"""Qualify synchronous descriptor-field Array projections with a full O2 compiler.

CPU-only: JIT, static AOT/shared unload, GCC, ABI refusals and complete OOM
prefixes. No runtime delivery, device or network operations are performed.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from foreign_compiler_cohort import load_compiler_cohort
from stage_runtime_headers import stage as stage_runtime_headers


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','compiler','compiler-report','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,compiler,build,out=[v.resolve() for v in (a.root,a.compiler,a.compiler_report,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);assert not out.is_relative_to(root)
    win=os.name=='nt';sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    meta=json.loads(build.read_text('utf8'));case=root/'test/cases/foreign_descriptor_arrays.xl'
    helper=case.with_suffix('.c');header=case.with_suffix('.h');host=root/'test/cases/foreign_decl_module_host.c'
    runtime=compiler.parent/('xl.dll' if win else 'libxl.so')
    tools={n:Path(shutil.which(n)).resolve() for n in ('gcc','g++','objdump')}
    files=[Path(__file__).resolve(),Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),compiler,build,Path(meta['sourceFreeze']),runtime,case,helper,header,host,*extra,*tools.values()]
    nested=root/'test/cases/foreign_descriptor_arrays_nested.xl';files.extend((nested,nested.with_suffix('.h')))
    files.extend(f for f in sdk.rglob('*') if f.is_file());inputs={str(f):sha(f) for f in files}
    out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir();copies=out/'input-snapshots';copies.mkdir()
    for path in (case,helper,header,Path(__file__).resolve()):shutil.copy2(path,copies/path.name)
    frozen=Path(meta['source']);fresh=out/'runtime-sdk'
    staged=stage_runtime_headers(frozen,fresh,frozen/'tools'/('tcc_vfs_manifest_win64_xlc.txt' if win else 'tcc_vfs_manifest_linux_xlc.txt'))
    for f in [Path(__file__).with_name('stage_runtime_headers.py'),Path(__file__).with_name('gen_tcc_vfs_resources.py'),Path(__file__).with_name('runtime_layout.py'),*(p for p in fresh.rglob('*') if p.is_file())]:inputs[str(f)]=sha(f)
    env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
    env.update(TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch),XLANG_DESCRIPTOR_ROOT=str(root),XLANG_DESCRIPTOR_TEMP=str(out/'nested-temps'))
    key='PATH' if win else 'LD_LIBRARY_PATH';env[key]=str(compiler.parent)+os.pathsep+env.get(key,'')
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,optimization=meta['optimization'],inputs=inputs,rows=[],programs=[],budgets=[],heap=[],privateHeaders=len(staged),wholeBridgeZeroOverheadClaim=False)
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n','utf8');os.replace(tmp,out/'results.json')
    def run(name,args,accepted=True):
        startup=None
        if win:startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        args=list(map(str,args));start=time.monotonic();z=subprocess.run(args,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data);passed=(z.returncode==0)==accepted
        report['rows'].append(dict(name=name,command=args,exitCode=z.returncode,expectedSuccess=accepted,passed=passed,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert passed,log;return data
    def check(mode,data):
        counts=re.findall(rb'DESCRIPTOR_ARRAYS_OK\s+(\d+)\s+(\d+)',data);assert len(counts)==1,data[-2000:]
        counts=list(map(int,counts[0]));assert counts==[2,2],counts
        report['budgets'].append(dict(mode=mode,fullFailurePrefixes=counts,scalarRepeated=4,viewBalanced=True))
        matches=re.findall(rb'Descriptor array heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+) row=(\d+)',data)
        assert len(matches)==2 and [m[0] for m in matches]==[b'language',b'native'],matches
        rows={m[0].decode():dict(alloc=int(m[1]),free=int(m[2]),realloc=int(m[3]),rowBytes=int(m[4])) for m in matches}
        assert rows['language']['alloc']==rows['language']['free']==2000 and rows['language']['realloc']==0
        assert rows['native']['alloc']==rows['native']['free']==rows['native']['realloc']==0
        report['heap'].append(dict(mode=mode,rounds=1000,counts=rows,xrtOnly=True,cpuEquivalenceClaim=False));save()
    def imports(name,path):
        data=run(name,[tools['objdump'],'-p',path]);names=re.findall(rb'DLL Name:\s*(\S+)',data) if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        assert not {n.lower() for n in names}.intersection({b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'})
        report['programs'].append(dict(path=str(path),sha256=sha(path),imports=[n.decode() for n in names]))
    includes=['--inc-path',root,'--inc-path',fresh/'include','--inc-path',sdk];ci=['-I'+str(v) for v in (fresh/'include',root,sdk,compiler.parent)]
    original=case.read_text('utf8');single=next(l for l in original.splitlines() if 'func consume(' in l);entry='\nfunc main(...) as int\n return 0\nend\n'
    try:
        generated=out/'generated.c';public=out/'public.h'
        run('emit',[compiler,'cgen',case,'--emit',generated,'--emit-header',public,*includes]);code=generated.read_text('utf-8-sig')
        for required in ('native descriptor Array pointer ABI','native descriptor Array count ABI','native descriptor Array fields disjoint','native_pack_0.inputs=native_array_record_10','native_pack_0.ports.items=native_array_record_12','xl6SequenceReadPinBeginV1'):
            assert required in code,required
        assert 'native_array_slice.Size' in code and 'native_array_views_' in code
        run('ir',[compiler,'ir',case,'--lowered','--format','json','--emit',out/'lowered.json',*includes]);ir=json.loads((out/'lowered.json').read_text('utf-8-sig'));groups=ir['foreignArrayGroups']
        assert sum(bool(g['packPhysicalIndexPlusOne']) for g in groups)>=8 and any(g['packDataField']=='ports.items' for g in groups)
        assert any(not g['packPhysicalIndexPlusOne'] for g in groups)
        for std,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):run('header-'+std,[tools[tool],'-std='+std,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',public])
        check('jit',run('jit',[compiler,'run',case,*includes]))
        exe=out/('aot.exe' if win else 'aot');run('aot-build',[compiler,'build',case,'-o',exe,*includes]);check('aot',run('aot-run',[exe]));imports('aot-imports',exe)
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        for opt in ('O0','O2'):
            binary=out/('gcc-'+opt+('.exe' if win else ''));run(opt+'-build',[tools['gcc'],'-std=c11','-'+opt,*ci,generated,helper,host,runtime,*systems,'-o',binary]);check(opt,run(opt+'-run',[binary]))
        shared=out/'descriptor-shared.xl';shared.write_text(original.replace('func main(...) as int','export "descriptor_check" func check() as int'),'utf8');module=out/('descriptor-shared'+('.dll' if win else '.so'))
        run('shared-build',[compiler,'build',shared,helper,'--shared','-o',module,*includes]);shared_host=out/'shared-host.py'
        shared_host.write_text('import ctypes,sys,os,_ctypes\nm=ctypes.CDLL(sys.argv[1],**({"winmode":0} if os.name=="nt" else {}))\nm.xl_script_module_init.argtypes=[ctypes.c_void_p]\nm.xl_script_module_init.restype=ctypes.c_int\nassert m.xl_script_module_init(None)==1\nm.descriptor_check.restype=ctypes.c_int64\nassert m.descriptor_check()==0\nm.xl_script_module_unit_checked.restype=ctypes.c_int\nassert m.xl_script_module_unit_checked()==1\nif os.name=="nt":\n m.xl_script_runtime_retire_checked.restype=ctypes.c_int\n assert m.xl_script_runtime_retire_checked()==1\n _ctypes.FreeLibrary(m._handle)\nelse: _ctypes.dlclose(m._handle)\nprint("DESCRIPTOR_SHARED_RETIRED")\n','utf8')
        data=run('shared-run',[sys.executable,'-B',shared_host,module]);check('shared',data);assert b'DESCRIPTOR_SHARED_RETIRED' in data;imports('shared-imports',module)
        assert b'DESCRIPTOR_ARRAYS_NESTED_OK' in run('nested-jit',[compiler,'run',nested,*includes])
        nested_exe=out/('nested.exe' if win else 'nested');run('nested-build',[compiler,'build',nested,'-o',nested_exe,*includes]);assert b'DESCRIPTOR_ARRAYS_NESTED_OK' in run('nested-run',[nested_exe]);imports('nested-imports',nested_exe)
        unused=out/'unused.xl';unused.write_text(single+entry,'utf8');run('unused-emit',[compiler,'cgen',unused,'--emit',out/'unused.c',*includes]);pruned=(out/'unused.c').read_text('utf-8-sig');assert 'native_array_record_' not in pruned and 'descriptorArraysConsume(' not in pruned
        negatives={
            'duplicate':single.replace('@c_array_pack(10,0,"inputs","inputCount")','@c_array_pack(10,0,"inputs","inputCount") @c_array_pack(10,0,"inputs","inputCount")'),
            'missing-group':single.replace('@c_array_pack(10,','@c_array_pack(63,'),
            'missing-parent':single.replace('@c_array_pack(10,0,','@c_array_pack(10,1,'),
            'negative-parent':single.replace('@c_array_pack(10,0,','@c_array_pack(10,-1,'),
            'large-key':single.replace('@c_array_pack(10,','@c_array_pack(64,'),
            'injection':single.replace('"inputs","inputCount"','"inputs;evil","inputCount"'),
            'same-fields':single.replace('"inputs","inputCount"','"inputs","inputs"'),
            'cross-fields':single.replace('"outputs","outputCount"','"inputs","outputCount"'),
            'ordinary-field':single.replace('"inputs","inputCount"','"tag","inputCount"'),
            'missing-witness':single.replace('"inputs","inputCount"','"ports.items","inputCount"'),
            'lost-destination':single.replace(' @c_array_pack(10,0,"inputs","inputCount")',''),
            'wrong-parent-range':single.replace('@c_pack(0,"DescriptorArrays",0,7)','@c_pack(0,"DescriptorArrays",0,6)'),
            'reused-input':single.replace('@c_pack_field(0,"tag",0,"value")','@c_pack_field(0,"tag",1,"value")'),
            'nonrecord':single.replace(' @c_array_record(10,"DescriptorPort","iSize","uint32_t")',''),
            'no-header':single.replace(' @header("test/cases/foreign_descriptor_arrays.h")',''),
            'nullable-array':single.replace('array<string> first,','array<string>? first,'),
        }
        for name,text in negatives.items():
            source=out/('bad-'+name+'.xl');source.write_text(text+entry,'utf8');target=source.with_suffix('.c');data=run('reject-'+name,[compiler,'cgen',source,'--emit',target,*includes],False);assert not target.exists() and (b'[sem]' in data or b'[ir]' in data)
        native=header.read_text('utf8');abi={
            'count-type':native.replace('int inputCount;','size_t inputCount;'),
            'pointer-type':native.replace('const DescriptorPort* inputs;','DescriptorPort* inputs;'),
            'union-pointers':native.replace('const DescriptorPort* inputs; int inputCount; const DescriptorPort* outputs;','union { const DescriptorPort* inputs; const DescriptorPort* outputs; }; int inputCount;'),
            'union-counts':native.replace('int inputCount; const DescriptorPort* outputs; int outputCount;','union { int inputCount; int outputCount; }; const DescriptorPort* outputs;'),
        }
        for name,text in abi.items():
            assert text!=native;bad=out/('bad-native-'+name);target=bad/'test/cases';target.mkdir(parents=True);(target/header.name).write_text(text,'utf8')
            for std,tool,lang in (('c11','gcc','c'),('c++17','g++','c++')):
                data=run('reject-native-'+name+'-'+std,[tools[tool],'-std='+std,'-I'+str(bad),*ci,'-x',lang,'-fsyntax-only',public],False);assert b'native descriptor Array' in data
        report.update(complete=True,inputsUnchanged=all(sha(path)==h for path,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
