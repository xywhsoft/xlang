"""Real isolated Linux shipping CLI builds and independent ELF execution.
The xlang/TCC inputs are only copied shipping CLI/shared libraries and fixture
text; libc headers, CRT objects and the linker are target-system inputs.
The consumer has no xlang shared-library dependency and retains the
ELF image until process exit. This is four synchronization fixtures, not safe
dlclose, terminal TLS retirement, full53 acceptance or ELF symbol pruning.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve()
    manifest=args.build_report.resolve()
    assert os.name!='nt' and not out.exists()
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    project=Path(__file__).resolve().parents[1]
    cc,readelf=[shutil.which(name) for name in ('gcc','readelf')]; assert cc and readelf
    host=project/'test/cases/stdlib_sync_artifact_host.c'
    fixtures=[project/'test/cases'/('stdlib_sync_artifact_'+name+'.xl')
        for name in ('mutex','cond','rwlock','policy')]
    products=[root/'release'/name for name in ('xl','libxl.so','libxlc.so')]
    files={Path(__file__).resolve(),Path(cc).resolve(),Path(readelf).resolve(),manifest,host,*fixtures,*products}
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded[path]==sha(path) for path in products)
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256']; files.add(Path(row['Path']))
    files.update(path for path in (root/'release/include').rglob('*') if path.is_file())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); compiler=out/'compiler'; (compiler/'release').mkdir(parents=True)
    (compiler/'inputs').mkdir()
    for path in products: shutil.copy2(path,compiler/'release'/path.name)
    for fixture in fixtures: shutil.copy2(fixture,compiler/'inputs'/fixture.name)
    assert not any((compiler/name).exists() for name in ('src','lib','res','include'))
    consumer=out/'consumer'; consumer.mkdir(); tmp=out/'tmp'; tmp.mkdir()
    # Do not inherit provider/source or dynamic-loader injection from a dev run.
    env={key:value for key,value in os.environ.items() if not key.startswith('XL6_')
        and key not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[]; failures=[]; artifacts=[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Inputs':inputs,
        'Rows':rows,'Failures':failures,'Artifacts':artifacts,
        'ExternalXlangCompilerSourcesUsed':False,'ExternalXlangCompilerHeadersUsed':False,
        'TargetSystemLibcHeadersCrtAndLinkerRequired':True,
        'TerminalRuntimeRetirementAcceptance':False,'SafeElfUnloadAcceptance':False,
        'NonExecutableStackAcceptance':False,
        'StandaloneElfExecutionAcceptance':False,'StandaloneStaticXrtPruningAcceptance':False,
        'ShippingLibraryAcceptance':False,'Full53Acceptance':False}
    def save(): (out/'sync.shipping.elf.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,cwd=consumer):
        command=list(map(str,command)); result=subprocess.run(command,cwd=cwd,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode})
        if result.returncode: failures.append({'Name':name,'Tail':data[-6000:].decode(errors='replace')})
        save(); return result.returncode==0,data.decode(errors='replace')
    def no_xlang_imports(text):
        needed=re.findall(r'\(NEEDED\).*?\[([^\]]+)\]',text)
        assert not any(re.match(r'libxlc?\.(so|dll)',name) for name in needed),needed
        return needed
    try:
        hosts=[]
        for level in ('O0','O2'):
            binary=consumer/('host.'+level)
            good,_=run('host.'+level+'.build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wshadow',
                '-DXL_STATIC_EMBED','-I'+str(root/'release/include'),'-'+level,host,'-ldl','-o',binary])
            if not good: continue
            good,text=run('host.'+level+'.dependencies',[readelf,'-dW',binary])
            if good: no_xlang_imports(text); hosts.append(binary)
        for fixture in fixtures:
            name=fixture.stem; generated=compiler/(name+'.so')
            good,_=run(name+'.compile',[compiler/'release/xl','build',compiler/'inputs'/fixture.name,
                '-shared','-o',generated,'--save-temps'],compiler)
            if not good: continue
            module=consumer/generated.name; shutil.copy2(generated,module)
            record={'Fixture':str(fixture),'Path':str(module),'Sha256':sha(module),
                'Bytes':module.stat().st_size,'Needed':[]}; artifacts.append(record)
            good,text=run(name+'.dependencies',[readelf,'-dW',module])
            if good: record['Needed']=no_xlang_imports(text)
            good,text=run(name+'.segments',[readelf,'-lW',module])
            if good:
                stack=[line for line in text.splitlines() if 'GNU_STACK' in line]
                assert len(stack)==1 and stack[0].split()[-2]=='RW',stack
                record['StackFlags']='RW'
            good,text=run(name+'.exports',[readelf,'--dyn-syms','-W',module])
            if good:
                symbols={line.split()[-1] for line in text.splitlines()
                    if re.match(r'\s*\d+:',line) and len(line.split())>=8 and line.split()[6]!='UND'}
                expected={'xl_script_module_init','xl_script_module_unit_checked','xl6_export_syncProbe'}
                assert expected.issubset(symbols),sorted(expected-symbols)
            for binary in hosts:
                phase=name+'.'+binary.name
                good,text=run(phase,[binary,module])
                if good and 'actual exported program and checked module cleanup passed; image retained until process exit' not in text:
                    failures.append({'Name':phase,'Tail':'Missing actual exported-program/cleanup proof'})
        assert not any(path.name.startswith(('libxl.so','libxlc.so')) for path in consumer.iterdir())
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; assert not failures and len(rows)==28
        report['Complete']=True; report['StandaloneElfExecutionAcceptance']=True
        report['NonExecutableStackAcceptance']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Shipping ELF: 28 phases; four real non-executable-stack CLI products executed by O0/O2 independent consumers',flush=True)
if __name__=='__main__': main()
