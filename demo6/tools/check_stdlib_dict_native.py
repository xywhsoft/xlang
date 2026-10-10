"""Private XRT typed_dict resident foundation; no source Dict cutover.

Exact byte-span keys, full owned value domains, stable independent snapshots,
real reference/Future edges, phased retirement and checked error boundaries.
O0/O2 whole logical allocation prefixes and compiled broken implementation
controls. Optional ASan/UBSan instruments this selected closure, not products.
"""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, os, re, shutil, subprocess, sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm')
    assert cc and nm and not out.exists() and (not args.sanitize or os.name!='nt')
    sources=['test/cases/stdlib_dict_native_host.c','test/support/dict_native.c',
        'test/support/module_product_memory.c','src/runtime/xl6_dict.c','src/runtime/xl6_sequence.c',
        'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_array_nominal_admission.c',
        'src/runtime/xl6_array_primitive_admission.c','src/runtime/xl6_array_c_scalar_admission.c',
        'src/runtime/xl6_array_future_admission.c','src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_nominal_admission.c','src/runtime/xl6_string_type.c',
        'src/runtime/xl6_nullable_string.c','src/runtime/xl6_nullable_i64.c','src/runtime/xl6_char_type.c',
        'src/api/xl_foreign_error.c']
    files={Path(__file__).resolve(),Path(cc),Path(nm),Path(sys.executable),*[root/p for p in sources]}
    for folder in ('src/runtime','src/api','release/include','lib/xrt','lib/xruntime','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    rows=[]; controls=[]; products=[]; objects=[]; prefixes=[]; out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Objects=objects,Prefixes=prefixes,NegativeControls=controls,Platform=sys.platform,
        Sanitized=args.sanitize,SelectedNativeClosureInstrumented=args.sanitize,
        WholeRuntimeInstrumented=False,LanguageDictCutover=False,ShippingLibraryAcceptance=False,
        Full53Acceptance=False,Scope=__doc__,BoundaryInjection='GNU ld --wrap, not OS context exhaustion')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=['-Wl,--wrap=xl6ErrorScopeBegin','-Wl,--wrap=xl6ErrorScopeBeginCleanup',
        '-Wl,--wrap=xrtTypedDictIterNext']; env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,command,rejected=False):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Label=label,Command=command,Exit=result.returncode,ExpectedRejection=rejected,
            Log=str(log),LogSha256=sha(log)))
        assert result.returncode!=0 if rejected else result.returncode==0,(label,data[-4500:])
        if rejected: assert b'Assertion' in data or b'assertion' in data,(label,data[-2000:])
        return data.decode(errors='replace')
    try:
        cached={}
        for opt in ('O0','O2'):
            compiled=[out/(opt+'-'+str(i)+'.o') for i in range(len(sources))]
            def compile_one(pair):
                i,path=pair; run('build-'+opt+'-'+str(i),[cc,*flags,'-'+opt,'-c',path,'-o',compiled[i]])
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
                list(pool.map(compile_one,enumerate(sources)))
            cached[opt]=compiled; objects.extend(dict(Path=str(p),Sha256=sha(p)) for p in compiled)
            binary=out/('dict-'+opt+('.exe' if os.name=='nt' else ''))
            run('link-'+opt,[cc,*compiled,*links,'-o',binary])
            symbols=run('symbols-'+opt,[nm,'--defined-only',binary])
            names={line.split()[-1].lstrip('_') for line in symbols.splitlines() if line.split()}
            assert {'xrtTypedDictSet','xrtTypedDictMerge','xrtMapVisit','xl6DictCreateTakeV1',
                'xl6DictOwnershipAdapterV2','xl6DictCursorReadV1'}<=names
            assert not {'xl6ListCreateTakeV1','xl6SetCreateTakeV1','xl6ArrayToValueV1','xl6ArrayFromValueV1'}&names
            assert '[PASS] genuine resident typed Dict vectors' in run('vectors-'+opt,[binary,'vectors'])
            data=run('prefixes-'+opt,[binary,'prefixes'])
            found=re.findall(r'native Dict (\S+) first-no-hit=(\d+)',data); assert len(found)==14
            prefixes.extend(dict(Optimization=opt,Operation=name,FirstNoHit=int(point),Attempts=int(point)+1)
                for name,point in found)
            attempts=int(data.split('native-dict full-prefix-attempts=')[1].splitlines()[0])
            assert attempts==sum(int(point)+1 for _,point in found)
            products.append(dict(Path=str(binary),Sha256=sha(binary),AllocationAttempts=attempts,PrefixLanes=14))
        body=(root/'src/runtime/xl6_dict.c').read_text(encoding='utf-8')
        for name,before,after in (
            ('nul-key-truncated','xrtTypedDictSet(owner->storage,key,item)',
             'xrtTypedDictSet(owner->storage,(xstrview){key.Data,key.Data ? strlen(key.Data) : 0},item)'),
            ('admission-skipped','item && owner->policy.ops->Slot(owner->policy.data,item)','item && owner'),
            ('policy-equivalence-lost','a->ops->Equivalent(a->data,b->data)','true'),
            ('ownership-edges-omitted','if (owner->layout.value.transfer<3) return true;',
             'if (owner->layout.value.transfer>=1) return true;')):
            assert body.count(before)==1
            source=out/(name+'.mutant.c')
            source.write_text('#include <string.h>\n'+body.replace(before,after),encoding='utf-8')
            obj=out/(name+'.o'); binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'.build',[cc,*flags,'-O0','-c',source,'-o',obj])
            linked=[obj if path==cached['O0'][sources.index('src/runtime/xl6_dict.c')] else path for path in cached['O0']]
            run(name+'.link',[cc,*linked,*links,'-o',binary]); run(name+'.reject',[binary,'vectors'],True)
            controls.append(dict(Name=name,Rejected=True,Source=str(source),SourceSha256=sha(source),
                Path=str(binary),Sha256=sha(binary)))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs+objects+products)
        report['Complete']=report['InputsUnchanged'] and len(controls)==4
        assert report['Complete']
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Native Dict accepted: '+str(len(rows))+' phases, 28 complete prefix lanes, 4 broken implementations rejected',flush=True)
if __name__=='__main__': main()
