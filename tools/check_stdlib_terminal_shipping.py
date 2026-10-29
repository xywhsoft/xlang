"""Real product terminal readiness contract, not a Windows startup-race fix.

The private application acknowledges attachment before its first dynamic resize.
Original TTY/stderr/dimensions/reply/exit assertions and wrong/missing-resize
counterexamples remain normative. No retry, sleep, provider or source fallback.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys,time


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--trials',type=int,default=256)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists() and 4<=args.trials<=512 and args.trials%4==0
    windows=os.name=='nt';cc=shutil.which('gcc');inspector=shutil.which('objdump' if windows else 'readelf')
    assert cc and inspector
    task=Path(__file__).resolve().parents[1]
    sys.path.insert(0,str(root/'tools'))
    from gen_tcc_vfs_resources import load_resources
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/name for name in (('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so'))]
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded.get(path)==sha(path) for path in products)
    cases=[task/'test/cases'/(stem+'.xl') for stem in ('stdlib_process_terminal_session','stdlib_process_terminal_event_order')]
    child_source=task/'test/cases/xlir_process_child.c'
    host_source=task/'test/cases/stdlib_product_fixture_host.c'
    build_dir=root/('build/tmp' if windows else 'build/linux')
    manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
    dist=root/'res/tcc/win64/x86_64' if windows else build_dir/'tcc-dist'
    resources=load_resources(manifest,root,dist,build_dir/'xrt.o',build_dir)
    declarations=[r for r in resources if r.virtual.startswith('include/__xl_runtime/')]
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),Path(inspector),args.build_report.resolve(),
           root/'tools/gen_tcc_vfs_resources.py',manifest,child_source,host_source,*cases}
    files.update(r.source for r in declarations)
    files.update((root/'release/include').glob('xl*.h'))
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256'],row['Path'];files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True);isolated=out/'compiler';release=isolated/'release';release.mkdir(parents=True)
    relocated=out/'relocated';relocated.mkdir();temporary=out/'tmp';temporary.mkdir();sdk=out/'sdk'
    for product in products:
        shutil.copy2(product,release/product.name)
        if windows:shutil.copy2(product,isolated/product.name)
    for resource in declarations:
        target=sdk/Path(resource.virtual).relative_to('include');target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(resource.source,target)
    for header in (root/'release/include').glob('xl*.h'):
        sdk.mkdir(parents=True,exist_ok=True);shutil.copy2(header,sdk/header.name)
    fixtures=[]
    for source in cases:
        target=isolated/source.name;shutil.copy2(source,target);fixtures.append(target)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(temporary),TEMP=str(temporary),TMP=str(temporary))
    rows=[];trials=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,Trials=trials,Failures=failures,
        PlatformStartupConstraintAccepted=True,PlatformStartupRaceFixed=False,ApplicationReadinessRequired=True,
        EmbeddedVfsWithoutFallback=True,ProviderOverrides=False,IndependentAot=False,CounterexamplesRejected=False,
        NativeSourceLedgerAcceptance=False,OsHandleZeroGrowthAcceptance=False,TerminalReadinessAcceptance=False,Full53Acceptance=False)
    def command(name,argv,directory=isolated):
        argv=list(map(str,argv));start=time.monotonic()
        try:
            result=subprocess.run(argv,cwd=directory,env=env,capture_output=True,timeout=120)
            code,data=result.returncode,result.stdout+result.stderr
        except subprocess.TimeoutExpired as error:code,data='timeout',(error.stdout or b'')+(error.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        return dict(Name=name,Command=argv,Directory=str(directory),Exit=code,Seconds=time.monotonic()-start,
                    Log=str(log),LogSha256=sha(log)),data
    def run(name,argv,directory=isolated,reject=False):
        row,data=command(name,argv,directory);rows.append(row);row['ExpectedRejection']=reject
        print(name+': '+str(row['Exit']),flush=True)
        assert (row['Exit'] not in (0,'timeout')) if reject else row['Exit']==0,(name,data[-5000:])
        return data
    strict=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(sdk),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    if not windows:strict+=['-D_GNU_SOURCE']
    try:
        child=isolated/('process-child'+('.exe' if windows else ''))
        run('child-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2',
            *(['-municode'] if windows else ['-D_GNU_SOURCE']),child_source,'-o',child])
        env['XL6_PROCESS_CHILD']=str(child)
        host=isolated/('fixture-host'+('.exe' if windows else ''))
        links=[release/products[2].name] if windows else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
        run('host-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2','-DXL_STATIC_EMBED','-I'+str(sdk),host_source,*links,'-o',host])
        cli=release/products[0].name
        for source in fixtures:
            name=source.stem;generated=out/(name+'.c');repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[cli,'cgen',source,'--emit',generated])
            run(name+'.repeat',[cli,'cgen',source,'--emit',repeat]);assert generated.read_bytes()==repeat.read_bytes()
            for level in ('O0','O2'):run(name+'.'+level,[cc,*strict,'-'+level,'-c',generated,'-o',out/(name+'.'+level+'.o')])
            for mode in ('run','module'):
                data=run(name+'.'+mode,[host,release/products[2].name,source,mode,temporary/(name+'.'+mode)])
                assert ('product fixture '+mode+' complete').encode() in data
            image=isolated/(name+('.exe' if windows else '.aot'))
            run(name+'.aot.build',[cli,'build',source,'-o',image])
            independent=relocated/image.name;shutil.copy2(image,independent)
            data=run(name+'.aot.imports',[inspector,'-p' if windows else '-d',independent],relocated)
            deps=re.findall(rb'DLL Name:\s*([^\s]+)',data) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]',data)
            assert deps and not {d.decode().lower() for d in deps}&{p.name.lower() for p in products}
            if not windows:assert b'(RPATH)' not in data and b'(RUNPATH)' not in data
            run(name+'.aot.run',[independent],relocated)
        report['IndependentAot']=True
        # Mutation changes only the request, never the child's size observation
        # or original assertions. Both real compiler API modes must reject it.
        for label,replacement in (('missing-resize','true'),('wrong-resize','handle.resize(95, 31)')):
            changed,count=re.subn(r'handle\.resize\(96, 32\)',replacement,fixtures[1].read_text());assert count==1
            mutant=isolated/(label+'.xl');mutant.write_text(changed)
            for mode in ('run','module'):
                data=run(label+'.'+mode,[host,release/products[2].name,mutant,mode,temporary/(label+'.'+mode)],reject=True)
                assert b'AssertionError' in data
        report['CounterexamplesRejected']=True
        def trial(i):
            source=fixtures[i%2];mode=('run','module')[(i//2)%2]
            row,data=command('trial-'+str(i),[host,release/products[2].name,source,mode,temporary/('trial-'+str(i))])
            row.update(Fixture=source.stem,Mode=mode,Passed=row['Exit']==0 and ('product fixture '+mode+' complete').encode() in data)
            return row
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            for i,row in enumerate(pool.map(trial,range(args.trials))):
                trials.append(row)
                if not row['Passed'] or (i+1)%16==0:print(str(i+1)+'/'+str(args.trials)+' '+str(row['Exit']),flush=True)
        failures.extend(dict(Name=row['Name'],Exit=row['Exit']) for row in trials if not row['Passed'])
        assert len(trials)==args.trials
        assert all(sum(row['Fixture']==source.stem and row['Mode']==mode for row in trials)==args.trials//4
                   for source in fixtures for mode in ('run','module'))
        assert not any((isolated/name).exists() for name in ('src','lib','res','include'))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'] and not failures
        report['TerminalReadinessAcceptance']=True;report['Complete']=True
    except Exception as error:failures.append(dict(Name='gate',Error=repr(error)));print(repr(error),flush=True)
    report['PhaseCount']=len(rows)+len(trials)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
