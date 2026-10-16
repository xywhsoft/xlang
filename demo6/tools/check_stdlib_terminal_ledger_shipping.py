"""Terminal physical-resource ledger through both actual rebuilt shared products.
Keep all 13 error guards, TTY/stderr/exact-size/reply, independent stream leases,
real waiter join and complete heap/owned-handle assertions. No source fallback,
runtime replacement, opaque HPCON access or Windows global handle-zero claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from stage_stdlib_process_observer import stage


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sessions',type=int,default=16)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists() and 1<=args.sessions<=64
    windows=os.name=='nt';cc=shutil.which('gcc');inspector=shutil.which('objdump' if windows else 'readelf');assert cc and inspector
    task=Path(__file__).resolve().parents[1];host=task/'test/cases/xlir_process_terminal_host.c'
    child_source=task/'test/cases/xlir_process_child.c'
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/name for name in (('xl.dll','xlc.dll') if windows else ('libxl.so','libxlc.so'))]
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded.get(path)==sha(path) for path in products)
    files={Path(__file__).resolve(),task/'tools/stage_stdlib_process_observer.py',Path(sys.executable),Path(cc),Path(inspector),
           args.build_report.resolve(),host,child_source}
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256'],row['Path'];files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True);isolated=out/'host';release=isolated/'release';release.mkdir(parents=True)
    temporary=out/'tmp';temporary.mkdir();observer=stage(root,out/'observer')
    for product in products:
        shutil.copy2(product,release/product.name)
        if windows:shutil.copy2(product,isolated/product.name)
    env={key:value for key,value in os.environ.items() if not key.upper().startswith('XL6_') and key not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(temporary),TEMP=str(temporary),TMP=str(temporary))
    rows=[];proofs=[];failures=[];counterexamples=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,Proofs=proofs,Failures=failures,
        Counterexamples=counterexamples,Observer=observer,RuntimeInstrumented=False,ProviderOverrides=False,
        PrivateLayoutFromPinnedSnapshot=True,NativeProductLedgerAcceptance=False,OsHandleZeroGrowthAcceptance=False,
        WorkerWideOomAcceptance=False,Full53Acceptance=False)
    def run(name,command,reject=False):
        command=list(map(str,command));start=time.monotonic()
        try:
            result=subprocess.run(command,cwd=isolated,env=env,capture_output=True,timeout=120)
            code,data=result.returncode,result.stdout+result.stderr
        except subprocess.TimeoutExpired as error:code,data='timeout',(error.stdout or b'')+(error.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=code,ExpectedRejection=reject,Seconds=time.monotonic()-start,Log=str(log),LogSha256=sha(log)))
        print(name+': '+str(code),flush=True)
        assert (code not in (0,'timeout')) if reject else code==0,(name,data[-6000:])
        return data
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(out/'observer'),'-Wno-unused-function']
    if not windows:flags+=['-D_GNU_SOURCE']
    def libraries(product):
        return [release/product.name] if windows else ['-L'+str(release),'-l'+product.stem[3:],'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
    try:
        child=isolated/('process-child'+('.exe' if windows else ''))
        run('child-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2',
            *(['-municode'] if windows else ['-D_GNU_SOURCE']),child_source,'-o',child])
        for product in products:
            for level in ('O0','O2'):
                name=product.stem+'.'+level;binary=isolated/(name+('.exe' if windows else ''))
                try:
                    run(name+'.build',[cc,*flags,'-'+level,host,*libraries(product),'-o',binary])
                    data=run(name+'.imports',[inspector,'-p' if windows else '-d',binary])
                    deps=re.findall(rb'DLL Name:\s*([^\s]+)',data) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]',data)
                    selected={d.decode().lower() for d in deps}&{p.name.lower() for p in products}
                    assert selected=={product.name.lower()},selected
                    data=run(name+'.run',[binary,child,args.sessions])
                    assert b'13 guards, heap/owned handles balanced' in data and b'no extra handles' in data
                    assert ('terminal product ledger: '+str(args.sessions)+' sessions, native exit code=0').encode() in data
                    match=re.search(rb'raw Win32 platform retention=(\d+)',data);assert match
                    retention=int(match.group(1));assert windows or retention==0
                    proofs.append(dict(Product=product.name,Optimization=level,Sessions=args.sessions,GuardsPerSession=13,
                        PlatformRetentionPerSession=retention,HeapBalanced=True,OwnedHandlesClosed=True,NativeExitCode=0))
                except Exception as error:failures.append(dict(Name=name,Error=repr(error)))
        for label,replacement in (('missing-resize','true'),('wrong-resize','xl6ProcessResizeV1(process,95,31)')):
            changed,count=re.subn(r'xl6ProcessResizeV1\(process,96,32\)',replacement,host.read_text());assert count==1
            source=out/(label+'.c');source.write_text(changed)
            for product in products:
                name=label+'.'+product.stem;binary=isolated/(name+('.exe' if windows else ''))
                try:
                    run(name+'.build',[cc,*flags,'-O2',source,*libraries(product),'-o',binary])
                    data=run(name+'.run',[binary,child,1],reject=True)
                    assert re.search(rb'assertion.*failed|assert.*failed',data,re.I),data[-4000:]
                    assert b'strstr(data' in data,data[-4000:]
                    counterexamples.append(dict(Name=label,Product=product.name,AssertionRejected=True))
                except Exception as error:failures.append(dict(Name=name,Error=repr(error)))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert sha(observer['Header']['Path'])==observer['Header']['Sha256']
        assert all(sha(release/product.name)==sha(product) for product in products)
        report['Complete']=report['InputsUnchanged'] and not failures and len(proofs)==4 and len(counterexamples)==4
        report['NativeProductLedgerAcceptance']=report['Complete']
    except Exception as error:failures.append(dict(Name='gate',Error=repr(error)))
    report['PhaseCount']=len(rows)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Proofs=len(proofs),Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
