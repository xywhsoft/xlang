"""Actual-product tests for queue initialization, defaults and scalar tree keys.
Source-free JIT/module/relocated AOT, saved strict O0/O2 C, semantic negatives,
and Linux genuine-SO IR/C/header mutation gates. No source/provider overrides.
This is a bounded regression batch, not complete runtime/function coverage.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time

POSITIVE=('stdlib_slot_map_default','stdlib_array_remove_range',
          'stdlib_wait_queue_count','stdlib_concurrent_queue_capacity',
          'stdlib_concurrent_queue_initialization','stdlib_tree_char_keys',
          'stdlib_tree_scalar_pruning')
NEGATIVE=tuple('sem_'+n+'_capacity_required' for n in
               ('spsc_queue','mpsc_queue','mpmc_queue','wait_queue','queue_alias'))
DECLARATION_NEGATIVE=tuple('sem_'+n+'_initializer_required' for n in
               ('spsc_queue','mpsc_queue','mpmc_queue','wait_queue','queue_alias'))

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','build-report','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();assert not out.exists()
    task=Path(__file__).resolve().parents[1];win=os.name=='nt'
    cc=shutil.which('gcc');inspector=shutil.which('objdump' if win else 'readelf');nm=shutil.which('nm');assert cc and inspector and nm
    sys.path.insert(0,str(root/'tools'))
    from gen_tcc_vfs_resources import load_resources
    build=json.loads(a.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/n for n in (('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so'))]
    recorded={Path(r['Path']).resolve():r['Sha256'] for r in build['Products']}
    assert all(recorded.get(s)==sha(s) for s in products)
    host=task/'test/cases/stdlib_internal_container_product_host.c'
    mutation=task/'test/cases/xlir_slot_map_contract.c'
    array_mutation=task/'test/cases/xlir_array_remove_range_contract.c'
    char_mutation=task/'test/cases/xlir_tree_char_contract.c'
    cases={n:task/'test/cases'/(n+'.xl') for n in (*POSITIVE,*NEGATIVE,*DECLARATION_NEGATIVE)}
    files={Path(__file__).resolve(),a.build_report.resolve(),Path(sys.executable),Path(cc),Path(inspector),Path(nm),host,mutation,array_mutation,char_mutation,*cases.values()}
    for r in build['Inputs']+build['Products']:assert sha(r['Path'])==r['Sha256'];files.add(Path(r['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();isolated,sdk,tmp,moved=[out/n for n in ('isolated','sdk','tmp','relocated')]
    release=isolated/'release';release.mkdir(parents=True)
    for d in (sdk,tmp,moved):d.mkdir()
    for s in products:
        shutil.copy2(s,release/s.name)
        if win and s.suffix=='.dll':shutil.copy2(s,isolated/s.name)
    for s in (root/'release/include').glob('*.h'):shutil.copy2(s,sdk/s.name)
    builddir=root/('build/tmp' if win else 'build/linux')
    manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if win else 'tcc_vfs_manifest_linux_runtime.txt')
    dist=root/'res/tcc/win64/x86_64' if win else builddir/'tcc-dist'
    for r in load_resources(manifest,root,dist,builddir/'xrt.o',builddir):
        if r.virtual.startswith('include/__xl_runtime/'):
            target=sdk/Path(r.virtual).relative_to('include');target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(r.source,target)
    shutil.copy2(root/'lib/xruntime/xruntime.h',sdk/'xruntime.h')
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[];proofs=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,Proofs=proofs,
                Failures=failures,CompilerSourceObjects=False,ProviderOverrides=False,Full53Acceptance=False)
    def run(tag,argv,reject=False):
        argv=list(map(str,argv));start=time.monotonic()
        try:
            r=subprocess.run(argv,cwd=isolated,env=env,capture_output=True,timeout=180)
            code,data=r.returncode,r.stdout+r.stderr
        except subprocess.TimeoutExpired as e:code,data='timeout',(e.stdout or b'')+(e.stderr or b'')
        log=out/(tag+'.log');log.write_bytes(data)
        rows.append(dict(Name=tag,Command=argv,Exit=code,Seconds=time.monotonic()-start,
                         Log=str(log),LogSha256=sha(log),ExpectedRejection=reject))
        assert (code not in (0,'timeout') if reject else code==0),(tag,data[-5000:])
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(sdk)]
    if not win:flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
            '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    try:
        binary=isolated/('observer'+('.exe' if win else ''));compiler=release/products[2].name
        links=[compiler] if win else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
        run('host.build',[cc,*flags,'-O2',host,*links,'-o',binary])
        assert not any((isolated/n).exists() for n in ('src','lib','res','include'))
        for name in POSITIVE:
            try:
                source=isolated/(name+'.xl');shutil.copy2(cases[name],source)
                image=isolated/(name+('.exe' if win else ''))
                for mode in ('run','module','aot'):
                    work=tmp/(name+'-'+mode);work.mkdir()
                    assert 'internal-container '+mode+' complete' in run(name+'.'+mode,[binary,compiler,source,mode,work,image])
                for opt in ('O0','O2'):
                    for filename in ('module.c','xl6_xrt_impl.c'):
                        run(name+'.'+opt+'.'+filename,[cc,*flags,*compat,'-'+opt,'-c',work/filename,
                            '-o',out/(name+'.'+opt+'.'+filename+'.o')])
                relocated=moved/image.name;shutil.move(image,relocated)
                imports=run(name+'.imports',[inspector,'-p' if win else '-d',relocated])
                deps=re.findall(r'DLL Name:\s*(\S+)',imports) if win else re.findall(r'\(NEEDED\).*?\[(.*?)\]',imports)
                assert not any(n.lower() in ('xl.dll','xlc.dll') or 'libxl' in n.lower() for n in deps)
                assert win or not re.search(r'\((?:RPATH|RUNPATH)\)',imports)
                run(name+'.relocated',[relocated]);proofs.append(dict(Name=name,Jit=True,Module=True,RelocatedAot=True,StrictC=['O0','O2']))
                print(name+': passed',flush=True)
            except Exception as e:failures.append(dict(Name=name,Error=repr(e)))
        for name in NEGATIVE:
            try:
                source=isolated/(name+'.xl');shutil.copy2(cases[name],source)
                text=run(name+'.check',[release/products[0].name,'check',source,'--json'],reject=True)
                assert '队列构造函数必须显式提供一个正容量参数' in text,text
                proofs.append(dict(Name=name,SemanticRejection=True))
            except Exception as e:failures.append(dict(Name=name,Error=repr(e)))
        for name in DECLARATION_NEGATIVE:
            try:
                source=isolated/(name+'.xl');shutil.copy2(cases[name],source)
                text=run(name+'.check',[release/products[0].name,'check',source,'--json'],reject=True)
                result=json.loads(text)
                messages=[r['message'] for r in result['diagnostics']]
                assert len(messages)==3 and all('不可空队列声明必须包含初始化表达式' in m for m in messages),result
                assert all('[ir]' not in m for m in messages),result
                proofs.append(dict(Name=name,SemanticRejection=True,DeclarationContexts=3))
            except Exception as e:failures.append(dict(Name=name,Error=repr(e)))
        # TCC strips the final executable's symbols. An empty `nm` result is
        # NOT pruning evidence. Inspect actual pinned Tree provider objects:
        # the shared shape check must not reference the optional char factory.
        for opt in ('O0','O2'):
            obj=out/('tree-provider.'+opt+'.o')
            run('tree-provider.'+opt,[cc,*flags,*compat,'-'+opt,'-I'+str(root),'-I'+str(root/'release'),'-c',
                root/'src/runtime/xl6_tree.c','-o',obj])
            symbols=run('tree-provider.'+opt+'.undefined',[nm,'-u',obj])
            assert 'xrtTypedTree' in symbols and 'xl6CharTypeV1' not in symbols,symbols
        report['TreeProviderHasNoCharFactoryDependency']=True
        if not win:
            binary=out/'mutation';library=root/'release'
            run('mutation.build',[cc,*flags,'-O0','-Wno-unused-function','-I'+str(root),'-I'+str(root/'lib/tcc'),
                 mutation,'-L'+str(library),'-lxlc','-Wl,-rpath,'+str(library),'-pthread','-ldl','-lm','-o',binary])
            text=run('mutation.default',[binary,cases[POSITIVE[0]]])
            assert re.search(r'slot-map calls=\d+ mutations=[1-9]\d*',text),text
            binary=out/'array-mutation'
            run('array-mutation.build',[cc,*flags,'-O0','-Wno-unused-function','-I'+str(root),'-I'+str(root/'lib/tcc'),
                 array_mutation,'-L'+str(library),'-lxlc','-Wl,-rpath,'+str(library),'-pthread','-ldl','-lm','-o',binary])
            text=run('array-mutation.range',[binary,cases[POSITIVE[1]]])
            assert re.search(r'array-remove calls=\d+ mutations=[1-9]\d*',text),text
            binary=out/'char-mutation'
            run('char-mutation.build',[cc,*flags,'-O0','-Wno-unused-function','-I'+str(root),'-I'+str(root/'lib/tcc'),
                 char_mutation,'-L'+str(library),'-lxlc','-Wl,-rpath,'+str(library),'-pthread','-ldl','-lm','-o',binary])
            text=run('char-mutation.order',[binary,cases['stdlib_tree_char_keys']])
            assert re.search(r'char-tree calls=\d+ mutations=[1-9]\d*',text),text
            for name,expected in (('stdlib_tree_char_keys','1'),('stdlib_tree_scalar_pruning','0')):
                text=run('char-plan.'+name,[binary,'--plan',cases[name],expected])
                assert 'char-tree plan-char='+expected in text,text
            report['CharDescriptorPlanPruning']=True
            report['DefaultIrMutationAcceptance']=True
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=report['InputsUnchanged'] and len(proofs)==len(POSITIVE)+len(NEGATIVE)+len(DECLARATION_NEGATIVE) and not failures
    except Exception as e:failures.append(dict(Name='gate',Error=repr(e)))
    report['PhaseCount']=len(rows)
    report['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(out.rglob('*')) if f.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Phases=len(rows),Failures=failures),ensure_ascii=False),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
