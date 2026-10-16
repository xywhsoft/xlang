"""Current-product module-role and native pruning audit for all 53 modules.
Real public compiler emission, embedded VFS, strict O0/O2 saved native objects
and symbols; Linux also observes the genuine SO resident plan. Existing exact
product/AOT execution receipts prove fixture behavior, not this object rebuild.
No replacement provider, compiler source objects, function-level minimality,
whole stripped-executable symbol claim or full53 functional completion claim.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys,threading
import stdlib53_pruning_contract as contract


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','inventory-report','aot-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=3)
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve();assert not out.exists() and 1<=args.jobs<=4
    task=Path(__file__).resolve().parents[1];win=os.name=='nt'
    cc,nm=[shutil.which(n) for n in ('gcc','nm')];assert cc and nm
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/n for n in (('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so'))]
    recorded={Path(r['Path']).resolve():r['Sha256'].lower() for r in build['Products']}
    assert all(recorded.get(p)==sha(p) for p in products)
    host=task/'test/cases/stdlib_pruning_product_host.c';planner=task/'test/cases/stdlib53_pruning_plan.c'
    scope=root/'docs/三组标准库接入长期任务.md';catalog=root/'src/compile/xl6_artifact_native_symbols.inc'
    section=scope.read_text(encoding='utf-8-sig').split('## 53 模块覆盖表',1)[1].split('## 起始缺陷证据',1)[0]
    names=set(re.findall(r'^\| ([a-z0-9_]+) \|',section,re.M))
    assert names==set(contract.MODULES) and len(names)==53,('Module scope mismatch',names^set(contract.MODULES))
    symbols={}
    for symbol,module in re.findall(r'XL6_ARTIFACT_NATIVE_SYMBOL\("([^"]+)", "([^"]+)"\)',catalog.read_text()):
        symbols.setdefault(module,set()).add(symbol)
    sources={name:root/'test/cases'/(c['Fixture']+'.xl') for name,c in contract.PROFILES.items()}
    support=root/'test/cases/stdlib_string_support.c'
    files={Path(__file__).resolve(),Path(contract.__file__).resolve(),Path(sys.executable),Path(cc),Path(nm),host,planner,scope,catalog,support,
           args.build_report.resolve(),args.inventory_report.resolve(),args.aot_report.resolve(),*sources.values()}
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path'];files.add(Path(row['Path']))
    receipts={}
    for kind,path in (('product',args.inventory_report),('aot',args.aot_report)):
        data=json.loads(path.read_text(encoding='utf-8-sig'))
        assert data['Complete'] and data['InputsUnchanged'] and data['AllExecutablePassed'] and not data.get('ExplicitSubset') and not data.get('Failures',[])
        inputs={Path(r['Path']).resolve():r['Sha256'].lower() for r in data['Inputs']}
        assert inputs.get(args.build_report.resolve())==sha(args.build_report)
        assert all(inputs.get(p)==sha(p) for p in products)
        cases={c['Name']:c for c in data['Cases']}
        for name,source in sources.items():
            if name=='baseline':continue
            case=cases[source.stem]
            assert case['Passed'] and case['Executed'] and case['Status']=='passed',(kind,name)
            digest=sha(source)
            matches=[Path(r['Path']) for r in data['Inputs'] if Path(r['Path']).name==source.name and r['Sha256'].lower()==digest]
            assert matches and all(sha(p)==digest for p in matches),(kind,'fixture source differs',source)
            files.update(matches)
            rows=case['Rows'];assert all(r['Exit']==0 for r in rows),(kind,name)
            assert any(r['Name']==source.stem+'-run' for r in rows)
            assert any(r['Name']==source.stem+('-module' if kind=='product' else '-build') for r in rows)
            compiler_rows=rows if kind=='product' else [r for r in rows if r['Name'].endswith('-build')]
            for row in compiler_rows:
                staged=Path(row['Command'][2]);assert sha(staged)==digest,(kind,'executed source differs',staged);files.add(staged)
            for row in rows:assert sha(row['Log'])==row['LogSha256'];files.add(Path(row['Log']))
            if kind=='aot':assert sha(case['Image']['Path'])==case['Image']['Sha256'];files.add(Path(case['Image']['Path']))
        receipts[kind]=dict(Path=str(path.resolve()),Sha256=sha(path))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir();isolated=out/'isolated';release=isolated/'release';release.mkdir(parents=True)
    sdk=out/'sdk';sdk.mkdir();objects=out/'objects';objects.mkdir()
    for product in products:
        shutil.copy2(product,release/product.name)
        if win and product.suffix=='.dll':shutil.copy2(product,isolated/product.name)
    for header in (root/'release/include').glob('*.h'):shutil.copy2(header,sdk/header.name)
    sys.path.insert(0,str(root/'tools'));from gen_tcc_vfs_resources import load_resources
    bd=root/('build/tmp' if win else 'build/linux');manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if win else 'tcc_vfs_manifest_linux_runtime.txt')
    dist=root/'res/tcc/win64/x86_64' if win else bd/'tcc-dist'
    for resource in load_resources(manifest,root,dist,bd/'xrt.o',bd):
        if resource.virtual.startswith('include/__xl_runtime/'):
            target=sdk/Path(resource.virtual).relative_to('include');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(resource.source,target)
    shutil.copy2(root/'lib/xruntime/xruntime.h',sdk/'xruntime.h')
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    rows=[];proofs=[];failures=[];controls=[];lock=threading.Lock();cache_locks={};cache={}
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Proofs=proofs,Failures=failures,Controls=controls,
                Scope=__doc__,ExecutionReceipts=receipts,CompilerSourceObjects=False,ProviderOverrides=False,
                ProfileCount=len(sources),ModuleCount=53,Full53Acceptance=False,StrippedExecutableSymbolAcceptance=False,
                GenuineSoPlanAcceptance=False,NativeModuleRoleAcceptance=False)
    def run(tag,command):
        result=subprocess.run(list(map(str,command)),cwd=isolated,env=env,capture_output=True,timeout=240)
        log=out/(tag+'.log');log.write_bytes(result.stdout+result.stderr)
        row=dict(Name=tag,Command=list(map(str,command)),Exit=result.returncode,Log=str(log),LogSha256=sha(log))
        with lock:rows.append(row)
        assert result.returncode==0,(tag,(result.stdout+result.stderr)[-5000:])
        return (result.stdout+result.stderr).decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(sdk)]+([] if win else ['-D_GNU_SOURCE'])
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    def native(source):
        digest=sha(source)
        with lock:guard=cache_locks.setdefault(digest,threading.Lock())
        with guard:
            if digest in cache:return cache[digest]
            selected=set(re.findall(r'^#define ((?:XRT|XRUNTIME)_FEATURE_[A-Z0-9_]+)\b',run(digest+'.features',[cc,*flags,'-E','-dM',source]),re.M))
            result=dict(Digest=digest,Features=sorted(selected),Objects=[])
            for opt in ('O0','O2'):
                obj=objects/(digest+'.'+opt+'.o')
                run(digest+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',source,'-o',obj])
                defined={line.split()[-1] for line in run(digest+'.'+opt+'.symbols',[nm,'--defined-only',obj]).splitlines() if line.split()}
                result['Objects'].append(dict(Optimization=opt,Path=str(obj),Sha256=sha(obj),Symbols=sorted(defined)))
            cache[digest]=result;return result
    def evaluate(name,result):
        selected=set(result['Features']);contract.check_features(name,selected)
        module=contract.PROFILES[name]['SymbolModule'];witnesses={}
        for obj in result['Objects']:
            defined=set(obj['Symbols'])
            if module:
                actual=defined&symbols[module];assert actual,(name,module,'no implementation symbols');witnesses[obj['Optimization']]=sorted(actual)
            for owner,declared in symbols.items():
                unrelated=(owner.startswith('XRT_MODULE_REGEX') and contract.PROFILES[name]['Family']!='regex') or (owner=='XRT_MODULE_PATTERN' and contract.PROFILES[name]['Family']!='pattern') or (owner.startswith('XRT_MODULE_PROCESS') and contract.PROFILES[name]['Family']!='process')
                if unrelated:assert not defined&declared,(name,'unrelated symbols',owner)
            if name in ('baseline','array','list','tree','stack','fixed-stack'):
                assert not any(s.startswith(prefix) for s in defined for prefix in contract.RAW_STACKS.values()),(name,'legacy raw stack implementation')
        return witnesses
    def profile(item):
        name,source=item
        try:
            work=out/name;work.mkdir();local=isolated/source.name;shutil.copy2(source,local)
            text=source.read_text(encoding='utf-8-sig');extra=[support] if re.search(r'^extern\s+"(?:expect_i64|expect_str)"',text,re.M) else []
            image=work/('program.exe' if win else 'program')
            run(name+'.emit',[observer,release/products[2].name,local,work,image,*extra])
            body=work/'xl6_xrt_impl.c';assert not re.search(r'^#define (?:XRT|XRUNTIME)_MODULE_ALL\b',body.read_text(),re.M)
            result=native(body);witnesses=evaluate(name,result)
            plan=None
            if not win:
                plan=json.loads(run(name+'.plan',[plan_binary,source]));available={Path(p).name for p in plan['Sources']}
                assert set(contract.PROFILES[name]['RequiredSources'])<=available,(name,'resident source missing')
            if name=='baseline':run('baseline.execute',[image])
            print(name+': passed',flush=True)
            return dict(Name=name,Fixture=source.stem,NativeSource=dict(Path=str(body),Sha256=sha(body)),
                        NativeObjectDigest=result['Digest'],Features=result['Features'],SymbolWitnesses=witnesses,Plan=plan)
        except Exception as error:
            with lock:failures.append(dict(Name=name,Error=repr(error)))
            print(name+': FAILED '+repr(error),flush=True);return None
    try:
        observer=isolated/('observer.exe' if win else 'observer')
        links=[release/products[2].name] if win else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
        run('observer.build',[cc,*flags,'-O2',host,*links,'-o',observer])
        plan_binary=isolated/'plan'
        if not win:run('plan.build',[cc,*flags,'-O2','-I'+str(root),'-I'+str(root/'lib/tcc'),planner,*links,'-o',plan_binary])
        assert not any((isolated/n).exists() for n in ('src','lib','res','include'))
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            proofs.extend(p for p in pool.map(profile,sources.items()) if p)
        indexed={p['Name']:p for p in proofs}
        assert len(indexed)==len(sources) and not failures
        for module,symbol in contract.INTERNAL.items():
            baseline=cache[indexed['baseline']['NativeObjectDigest']]
            assert 'XRT_FEATURE_'+module in baseline['Features']
            assert all(symbol in obj['Symbols'] for obj in baseline['Objects']),(module,'internal implementation missing')
        # Compiled retained-code controls, not compiler failures or text matches.
        seed=Path(indexed['baseline']['NativeSource']['Path'])
        for module in ('PTR_STACK','BLOCK_STACK','PTR_FIXED_STACK','REGEX_MATCH','PROCESS_RUN'):
            changed=out/('retained-'+module+'.c');changed.write_text('#define XRT_MODULE_'+module+'\n'+seed.read_text(),encoding='utf-8')
            actual=native(changed);owner='XRT_MODULE_'+module
            assert all(set(obj['Symbols'])&symbols[owner] for obj in actual['Objects'])
            try:evaluate('baseline',actual)
            except AssertionError:controls.append(dict(Module=module,Compiled=True,UnwantedImplementationRejected=True))
            else:raise AssertionError(('Retained implementation accepted',module))
        try:evaluate('regex-core',cache[indexed['baseline']['NativeObjectDigest']])
        except AssertionError:controls.append(dict(Module='REGEX_CORE',MissingImplementationRejected=True))
        else:raise AssertionError('Missing regex implementation accepted')
        report['Modules']=[dict(Name=name,Mode=mode,Profiles=ids.split(),Verified=all(p in indexed for p in ids.split())) for name,(mode,ids) in contract.MODULES.items()]
        report['NativeObjects']=list(cache.values());report['NativeModuleRoleAcceptance']=all(r['Verified'] for r in report['Modules']) and len(controls)==6
        report['GenuineSoPlanAcceptance']=not win
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=report['InputsUnchanged'] and report['NativeModuleRoleAcceptance'] and not failures
    except Exception as error:failures.append(dict(Name='gate',Error=repr(error)))
    report['PhaseCount']=len(rows);report['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Profiles=len(proofs),Failures=failures),ensure_ascii=False),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
