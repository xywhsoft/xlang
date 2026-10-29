"""Current-source dynamic function product count, ownership and failure boundary.
Native foundation only; language methods and full53 remain separate.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--out',type=Path,required=True); p.add_argument('--sanitize',action='store_true')
    a=p.parse_args(); root=Path(__file__).resolve().parents[1]; out=a.out.resolve(); assert not out.exists()
    cc=shutil.which('gcc'); assert cc and (os.name!='nt' or not a.sanitize)
    sources=['test/cases/stdlib_dynamic_product_native_host.c','test/support/dynamic_product_native.c',
        'src/runtime/xl6_dynamic_product.c','src/api/xl_foreign_error.c']
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable),root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',*(root/name for name in sources)}
    for folder in ('src/runtime','src/api','release/include'): files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    rows=[]; sweeps=[]; mutants=[]; out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Sweeps=sweeps,Mutations=mutants,
        RuntimeInstrumented=a.sanitize,Full53Acceptance=False,LanguageExecution=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include']
    links=['-Wl,--wrap=xrtValueInvoke']; env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    if os.name!='nt':links+=['-pthread','-lm']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if a.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command):
        r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=240)
        data=r.stdout+r.stderr; (out/(name+'.log')).write_bytes(data); rows.append(dict(Name=name,Exit=r.returncode,Command=list(map(str,command))))
        assert r.returncode==0,(name,data[-5000:].decode(errors='replace')); return data.decode(errors='replace')
    try:
        for opt in (0,2):
            binary=out/('O'+str(opt)+suffix)
            run('O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),*sources,*links,'-o',binary])
            assert 'exact-count/no-diagnostic/cleanup balanced' in run('O'+str(opt)+'.vectors',[binary])
            for count in (2,6):
                for point in range(129):
                    log=run(f'O{opt}.arity{count}.{point}',[binary,count,point]); m=re.search(r'hit=([01]) balanced',log); assert m
                    if m[1]=='0':
                        assert point>0; sweeps.append(dict(Lane='O'+str(opt),Arity=count,FirstNoHit=point)); break
                else: raise AssertionError('Unclosed fault prefix')
        changes=[
            ('drop-extra-results','if(exact ? xrtCallResultCount(&result)!=expected : xrtCallResultCount(&result)<expected)',
                'if(xrtCallResultCount(&result)<expected)'),
            ('ignore-failure-during-publication','if(scope.failed)for','if(xrtGetError())for'),
            ('skip-failed-argument-cleanup','for(size_t i=0;i<count;++i) { xrtValueRelease(values[i]);','for(size_t i=0;i<count && !scope->failed;++i) { xrtValueRelease(values[i]);'),
        ]
        selected='src/runtime/xl6_dynamic_product.c'; original=(root/selected).read_text(encoding='utf-8-sig')
        for name,before,after in changes:
            assert original.count(before)==1,name
            mutant=out/(name+'.c'); mutant.write_text(original.replace(before,after),encoding='utf-8')
            binary=out/(name+suffix); providers=[str(mutant) if path==selected else path for path in sources]
            run(name+'.build',[cc,*flags,'-O0',*providers,*links,'-o',binary])
            r=subprocess.run([str(binary)],cwd=root,env=env,capture_output=True,timeout=240)
            (out/(name+'.reject.log')).write_bytes(r.stdout+r.stderr)
            assert r.returncode!=0 and b'assert' in (r.stdout+r.stderr).lower(),name
            mutants.append(dict(Name=name,Exit=r.returncode,Rejected=True,Sha256=sha(mutant)))
        assert len(sweeps)==4 and len(mutants)==3
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Dynamic product native complete='+str(report['Complete']),flush=True); return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
