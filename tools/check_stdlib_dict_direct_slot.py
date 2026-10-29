"""Closed Dict direct slots: native ownership/rollback and actual compiled IR.

Fresh source compiler, strict O0/O2 vectors and pruned products, exact keys,
zero-allocation native scalar reads, complete current-thread logical allocation
prefixes, actual compiled bad implementations, pre-C/H malformed IR refusal,
and sync/async/generator nominal execution. No shipping/full53 acceptance.
"""
from pathlib import Path
import argparse,ast,hashlib,json,os,re,shutil,subprocess,sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out','public-directory'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
    root=a.root.resolve(); out=a.out.resolve(); public=a.public_directory.resolve()
    cc=shutil.which('gcc'); assert cc and not out.exists() and (not a.sanitize or os.name!='nt')
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    driver=Path(build['Driver']); assert sha(driver)==build['DriverSha256'].lower()
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    population=root/'tools/check_stdlib_dict_native.py'; tree=ast.parse(population.read_text(encoding='utf-8'))
    sources=ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign)
        and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets)))
    sources[0]='test/cases/stdlib_dict_direct_slot_native_host.c'
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable),population,manifest,driver,runtime,
        root/'test/cases/stdlib_dict_native_host.c',root/'test/cases/xlir_dict_direct_slot_contract.c',
        root/'test/cases/xlir_artifact_runtime_contract.c',root/'test/cases/xlir_module_product_oom_host.c',
        *[root/s for s in sources]}
    fixtures=('stdlib_dict_direct_slot.xl','stdlib_dict_direct_slot_leaf.xl','stdlib_dict_direct_slot_continuation.xl')
    files.update(root/'test/cases'/s for s in fixtures)
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    for folder in ('src/runtime','src/api','lib/xrt','lib/xruntime','release/include','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; objects=[]; products=[]; prefixes=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Objects=objects,Products=products,
        Prefixes=prefixes,NegativeControls=controls,Platform=sys.platform,Sanitized=a.sanitize,
        Full53Acceptance=False,ShippingLibraryAcceptance=False,Scope=__doc__,
        SanitizedProductOptimizations=['O0'] if a.sanitize else [],
        SanitizedNativeOptimizations=['O0','O2'] if a.sanitize else [],
        ScopeInjection='GNU ld --wrap, not actual OS context exhaustion')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime']
    san=[]; env=dict(os.environ); native_links=[] if os.name=='nt' else ['-pthread','-lm']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    if a.sanitize:
        san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        native_links+=['-fsanitize=address,undefined','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    wraps=['-Wl,--wrap='+s for s in ('xl6ErrorScopeBegin','xl6ErrorScopeBeginCleanup','xrtTypedDictIterNext','xrtTypeCopyValue')]
    def run(label,command,reject=False):
        command=list(map(str,command)); r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=600)
        data=r.stdout+r.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Name=label,Command=command,Exit=r.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if reject else r.returncode==0),(label,data[-6500:])
        if reject: assert b'assert' in data.lower(),(label,data[-1000:])
        return data.decode(errors='replace')
    def record_object(path): objects.append(dict(Path=str(path),Sha256=sha(path)))
    def record_product(path): products.append(dict(Path=str(path),Sha256=sha(path)))
    full=flags+['-DXL_STATIC_EMBED','-Ilib/tcc','-I'+str(public/'include'),'-Wno-unused-function',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    compiler=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    shared=[str(runtime),*systems]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    if os.name=='nt': shutil.copy2(runtime,out/'xl.dll')
    try:
        cached={}
        for opt in ('O0','O2'):
            selected=[]
            for i,source in enumerate(sources):
                obj=out/(opt+'-native-'+str(i)+'.o'); run(obj.stem,[cc,*flags,*san,'-'+opt,'-c',source,'-o',obj])
                selected.append(obj); record_object(obj)
            cached[opt]=selected; binary=out/('native-'+opt+('.exe' if os.name=='nt' else ''))
            run(opt+'-native-link',[cc,*selected,*wraps,*native_links,'-o',binary]); record_product(binary)
            assert '[PASS]' in run(opt+'-native-vectors',[binary,'vectors'])
            data=run(opt+'-native-prefixes',[binary,'prefixes'])
            for write,runs,first in re.findall(r'direct slot closed write=(\d+) runs=(\d+) first-no-hit=(\d+)',data):
                write,runs,first=map(int,(write,runs,first)); assert runs==first+1
                points=re.findall(r'direct slot write='+str(write)+r' point=(\d+) hit=(\d) balanced',data)
                assert points==[(str(i),str(int(i<first))) for i in range(runs)]
                prefixes.append(dict(Label=opt+'-native',Write=write,Runs=runs,FirstNoHit=first))
        for name,before,after in (
            ('exact-key-truncated','DictOwner* owner=begin(value,layout,0); bool success=false,copied=false;',
                'if (key.Data) key.Size=strlen(key.Data);\n    DictOwner* owner=begin(value,layout,0); bool success=false,copied=false;'),
            ('missing-required-suppressed','if (required) xrtSetErrorKind(XERR_RANGE);','if (required) success=true;'),
            ('native-slot-copy-omitted','if (slot) success=copied=xrtTypeCopyValue(type,output,slot);','if (slot) success=true;'),
            ('failed-copy-owner-leaked','if (!success && copied) {','if (!success && copied && false) {')):
            source='src/runtime/xl6_dict.c'; body=(root/source).read_text(encoding='utf-8'); assert body.count(before)==1,name
            changed=out/(name+'.c'); changed.write_text(body.replace(before,after),encoding='utf-8')
            obj=out/(name+'.o'); run(name+'-compile',[cc,*flags,*san,'-O0','-c',changed,'-o',obj]); record_object(obj)
            selected=list(cached['O0']); selected[sources.index(source)]=obj
            binary=out/(name+('.exe' if os.name=='nt' else '')); run(name+'-link',[cc,*selected,*wraps,*native_links,'-o',binary])
            run(name+'-reject',[binary,'vectors'],True); record_product(binary)
            controls.append(dict(Name=name,Compiled=True,Rejected=True,Source=str(changed),SourceSha256=sha(changed),
                Product=str(binary),ProductSha256=sha(binary)))
        contract=out/('contract'+('.exe' if os.name=='nt' else ''))
        run('contract-build',[cc,*full,'-O0','test/cases/xlir_dict_direct_slot_contract.c',*compiler,*shared,'-o',contract]); record_product(contract)
        plans={}
        for fixture in fixtures:
            stem=Path(fixture).stem; path=root/'test/cases'/fixture
            proof=run(stem+'-contract',[contract,path.as_posix()]); plans[stem]=json.loads(next(line for line in proof.splitlines() if line.startswith('{')))
            reads,writes,mutations=map(int,re.search(r'direct slot reads=(\d+) writes=(\d+) mutations=(\d+)',proof).groups())
            report.setdefault('Contracts',[]).append(dict(Fixture=fixture,Reads=reads,Writes=writes,Mutations=mutations))
            generated=out/(stem+'.c'); repeated=out/(stem+'-repeat.c')
            for target in (generated,repeated): run(target.stem,[driver,path,'cgen-default',target])
            assert sha(generated)==sha(repeated)
            body=generated.read_text(encoding='utf-8-sig'); assert 'xl6DictReadSlotV1(' in body and 'xl6DictSetV1(' in body
            if fixture.endswith('_leaf.xl'):
                assert 'xl6DictValueRead' not in body and 'xl6DictValueSetTakeV1(' not in body
                assert 'xl6DictCreateTakeV1(' not in body
                assert not re.search(r'(?:=|return)\s*xrtValueInt\s*\(',body)
                assert 'src/runtime/xl6_dict.c' in plans[stem]['Sources']
                assert all(s not in plans[stem]['Sources'] for s in ('src/runtime/xl6_dict_value_index.c',
                    'src/runtime/xl6_dict_schema.c','src/runtime/xl6_list.c','src/runtime/xl6_set.c'))
            run(stem+'-jit',[driver,path,'run',out/(stem+'-jit')])
            for opt in ('O0','O2'):
                product_san=san if opt=='O0' else []
                product_links=native_links if opt=='O0' else [s for s in native_links if not s.startswith(('-fsanitize','-no-pie'))]
                plan=plans[stem]; native=out/(stem+'-'+opt+'-pruned.c')
                native.write_text(''.join('#define '+m+'\n' for m in plan['NativeModules'])+
                    '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
                selected=[]
                for i,source in enumerate([native,*[root/s for s in plan['Sources']],root/'test/support/module_product_memory.c']):
                    obj=out/(stem+'-'+opt+'-'+str(i)+'.o')
                    # Focused products/native closures only, not compiler/vendor DLLs.
                    run(obj.stem,[cc,*full,*product_san,'-'+opt,'-c',source,'-o',obj]); selected.append(obj); record_object(obj)
                binary=out/(stem+'-'+opt+('.exe' if os.name=='nt' else ''))
                run(binary.stem+'-link',[cc,*full,*product_san,'-'+opt,'-include',generated,
                    'test/cases/xlir_module_product_oom_host.c',*selected,*systems,*product_links,'-o',binary]); record_product(binary)
                data=run(binary.stem+'-prefixes',[binary,'--all'])
                runs,first=map(int,re.search(r'module product full prefixes=(\d+) first-no-hit=(\d+) balanced',data).groups())
                assert runs==first+1; prefixes.append(dict(Label=stem+'-'+opt,Runs=runs,FirstNoHit=first))
        report['Plans']=plans
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs+objects+products)
        report['Complete']=report['InputsUnchanged'] and len(prefixes)==10 and len(controls)==4 and len(report['Contracts'])==3
        assert report['Complete']
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Dict direct slots complete: '+str(len(rows))+' phases, '+str(sum(p['Runs'] for p in prefixes))+' prefix runs',flush=True)

if __name__=='__main__': main()
