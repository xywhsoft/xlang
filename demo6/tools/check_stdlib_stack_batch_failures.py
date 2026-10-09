"""Actual product Stack batch caller-thread OOM and ELF closed-IR mutation proof.
Windows products do not export private compiler IR APIs; that proof is ELF only.
Not full53, worker OOM, source-runtime sanitizer or pruned GCC link acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve(); assert not out.exists()
    manifest=args.build_report.resolve(); build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged']; project=Path(__file__).resolve().parents[1]
    windows=os.name=='nt'; cc=shutil.which('gcc'); assert cc
    fixture=project/'test/cases/stdlib_stack_batch_oom.xl'
    contract=project/'test/cases/xlir_stack_batch_contract.c'
    host=project/'test/cases/xlir_module_product_oom_host.c'; memory=project/'test/support/module_product_memory.c'
    products=[root/'release'/name for name in (('xl.dll','xlc.dll','xl.exe') if windows else ('libxl.so','libxlc.so','xl'))]
    files={Path(__file__).resolve(),Path(cc),manifest,fixture,contract,host,memory,
        project/'test/support/module_product_memory.h'}
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256']; files.add(Path(row['Path']))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); headers=out/'include/__xl_runtime'; headers.mkdir(parents=True)
    for path in list((root/'release/include').glob('*.h'))+list((root/'src/runtime').glob('xl6_*.h')):
        shutil.copy2(path,headers/path.name)
    for path in products[:2]: shutil.copy2(path,out/path.name)
    rows=[]; sweeps=[]; report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,
        'Inputs':inputs,'Rows':rows,'Sweeps':sweeps,'IRProof':[],
        'OOMScope':'Complete synchronous caller-thread init/main/fini; no worker allocation',
        'Full53Acceptance':False,'PrunedGccLink':False}
    env={key:value for key,value in os.environ.items() if not key.upper().startswith('XL6_')
        and key not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=out,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode})
        assert not result.returncode,(name,data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(project),'-I'+str(out/'include')]
    if not windows: flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    def links(product):
        if windows: return [out/product.name]
        return [out/product.name,'-Wl,-rpath,$ORIGIN','-pthread','-ldl','-lm']
    try:
        generated=out/'batch.c'; run('cgen',[products[2],'cgen',fixture,'--emit',generated])
        if not windows:
            for level in ('O0','O2'):
                binary=out/('contract.'+level)
                # Existing compiler SDK inline headers are not Wshadow-clean;
                # contract host still uses Wall/Wextra/Werror with no exemptions.
                run('contract.'+level+'.build',[cc,*[flag for flag in flags if flag!='-Wshadow'],
                    '-'+level,contract,*links(products[1]),'-o',binary])
                proof=run('contract.'+level+'.run',[binary,fixture])
                match=re.search(r'Stack batch IR: (\d+) calls, (\d+) mutations refused',proof); assert match
                report['IRProof'].append({'Level':level,'Calls':int(match[1]),'RejectedMutations':int(match[2])})
        memory_objects={}
        for level in ('O0','O2'):
            object_path=out/('memory.'+level+'.o')
            run('memory.'+level+'.build',[cc,*flags,'-'+level,'-c',memory,'-o',object_path])
            memory_objects[level]=object_path
        for product in products[:2]:
            for level in ('O0','O2'):
                lane=product.stem+'.'+level; binary=out/(lane+('.exe' if windows else ''))
                run(lane+'.build',[cc,*flags,*compat,'-'+level,'-DXL6_PRODUCT_USER_ENTRY','-include',generated,
                    host,memory_objects[level],*links(product),'-o',binary])
                assert 'balanced' in run(lane+'.baseline',[binary])
                for point in range(4097):
                    outcome=run(lane+'.'+str(point),[binary,point]); assert 'balanced' in outcome
                    if 'hit=0 balanced' in outcome:
                        assert point; sweeps.append({'Lane':lane,'FirstNoHit':point}); break
                else: raise AssertionError('Unclosed allocation prefix')
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'] and len(sweeps)==4
        assert windows or len(report['IRProof'])==2; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,flush=True)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Stack batch failure contract: {len(rows)} checks, {len(sweeps)} closed prefixes, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
