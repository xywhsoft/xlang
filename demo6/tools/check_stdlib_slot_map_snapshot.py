"""Independent owning SlotMap key/value snapshots, not language or full53 acceptance.

Actual supplier slot iteration and typed storage, strict O0/O2, complete OOM
prefixes, no-diagnostic/partial-copy/reentrant refusal, compiled negative
controls. Optional Linux ASan/UBSan covers this selected native closure only.
"""
from pathlib import Path
import argparse,ast,hashlib,json,os,re,shutil,subprocess,sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true'); args=p.parse_args()
    root=args.root.resolve(); out=args.out.resolve(); cc,nm=shutil.which('gcc'),shutil.which('nm')
    assert cc and nm and not out.exists() and (not args.sanitize or os.name!='nt')
    population=root/'tools/check_stdlib_dict_native.py'
    tree=ast.parse(population.read_text(encoding='utf-8'))
    sources=ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign) and
        any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets)))
    value_sources=sources.copy(); value_sources[0]='test/cases/stdlib_slot_map_values_host.c'
    value_sources[1]='test/support/slot_map_native.c'
    value_sources=[s for s in value_sources if s!='src/runtime/xl6_dict.c']
    value_sources+=['src/runtime/xl6_slot_map.c','src/runtime/xl6_slot_map_key.c']
    sources[0]='test/cases/stdlib_slot_map_snapshot_host.c'; sources[1]='test/support/native_value_native.c'
    sources=[s for s in sources if s!='src/runtime/xl6_dict.c']
    sources+=['src/runtime/xl6_slot_map.c','src/runtime/xl6_slot_map_key.c','src/runtime/xl6_slot_map_key_value.c',
        'src/runtime/xl6_native_leaf_value.c','src/runtime/xl6_array_native_value_admission.c','src/runtime/xl6_slot_map_keys.c',
        'src/runtime/xl6_string_value.c']
    files={Path(__file__).resolve(),population,Path(cc),Path(nm),Path(sys.executable),*[root/s for s in sources+value_sources],
        root/'test/support/slot_map_key_value_native.c'}
    for folder in ('src/runtime','src/api','src/type','release/include','lib/xrt','lib/xruntime','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(parents=True); rows=[]; objects=[]; products=[]; prefixes=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Objects=objects,Products=products,
        Prefixes=prefixes,NegativeControls=controls,SelectedNativeClosureSanitized=args.sanitize,
        LanguageSlotMapCutover=False,ShippingLibraryAcceptance=False,Full53Acceptance=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=['-Wl,--wrap=xrtTypedArrayPush','-Wl,--wrap=xrtSlotMapIterNext']; env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,command,reject=False):
        r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=240)
        data=r.stdout+r.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Label=label,Command=list(map(str,command)),Exit=r.returncode,ExpectedRejection=reject,
            Log=str(log),LogSha256=sha(log)))
        assert r.returncode!=0 if reject else r.returncode==0,(label,data[-6000:])
        if reject: assert b'Assertion' in data or b'assertion' in data or b'runtime error:' in data
        return data.decode(errors='replace')
    try:
        compiled={}; suffix='.exe' if os.name=='nt' else ''
        for opt in ('O0','O2'):
            current=[]
            for at,source in enumerate(sources):
                obj=out/(opt+'-'+str(at)+'.o'); run(opt+'-compile-'+str(at),[cc,*flags,'-'+opt,'-c',source,'-o',obj])
                objects.append(dict(Path=str(obj),Sha256=sha(obj))); current.append(obj)
            compiled[opt]=current; product=out/(opt+suffix)
            run(opt+'-link',[cc,*current,*links,'-o',product]); run(opt+'-vectors',[product,'vectors'])
            data=run(opt+'-prefixes',[product,'prefixes'])
            found=re.findall(r'slot-map-snapshot (\S+) runs=(\d+) first-no-hit=(\d+)',data); assert len(found)==2
            for operation,runs,last in found:
                assert int(runs)==int(last)+1
                prefixes.append(dict(Optimization=opt,Operation=operation,Runs=int(runs),FirstNoHit=int(last)))
            products.append(dict(Path=str(product),Sha256=sha(product)))
            lean=[]
            for at,source in enumerate(value_sources):
                obj=out/(opt+'-values-'+str(at)+'.o')
                # Reuse only exact source/options from the same invocation.
                if source in sources: shutil.copy2(current[sources.index(source)],obj)
                else: run(opt+'-values-compile-'+str(at),[cc,*flags,'-'+opt,'-c',source,'-o',obj])
                objects.append(dict(Path=str(obj),Sha256=sha(obj))); lean.append(obj)
            product=out/(opt+'-values'+suffix)
            run(opt+'-values-link',[cc,*lean,*[flag for flag in links if '--wrap=' not in flag],'-o',product])
            run(opt+'-values-execute',[product]); symbols=run(opt+'-values-symbols',[nm,'--defined-only',product])
            for name in ('xl6SlotMapKeysV1','xl6SlotMapKeyBoxV1','xl6SlotMapKeyHashV1','xl6SequenceSlotMapKeyCreateV1'):
                assert not re.search(r'\b'+name+r'\b',symbols),name
            products.append(dict(Path=str(product),Sha256=sha(product)))
        source='src/runtime/xl6_slot_map.c'; body=(root/source).read_text(encoding='utf-8')
        mutations=[('key-generation-omitted','const XL6_SlotMapKeyV1 key={owner->instance,slot};','const XL6_SlotMapKeyV1 key={owner->instance,1};'),
            ('key-boxed-domain-omitted','policy->boxedIdentity!=XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID ||','false ||'),
            ('short-iteration-accepted','success=success && copied==owner->storage.Count && !xrtGetError();',
             'success=success && !xrtGetError(); (void)copied;')]
        for name,before,after in mutations:
            assert body.count(before)==1
            bad=out/(name+'.c'); bad.write_text(body.replace(before,after),encoding='utf-8')
            obj=out/(name+'.o'); product=out/(name+suffix)
            run(name+'-compile',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj])
            current=compiled['O0'].copy(); current[sources.index(source)]=obj
            run(name+'-link',[cc,*current,*links,'-o',product]); run(name+'-refused',[product,'vectors'],True)
            controls.append(dict(Name=name,Compiled=True,Rejected=True,Source=str(bad),SourceSha256=sha(bad),
                Product=str(product),ProductSha256=sha(product)))
        report['Complete']=True
        report['ValueOnlyPrunedExecution']=True; report['ValueOnlyLifetimeRoundsPerOptimization']=1000
    finally:
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']
    print('SlotMap snapshots: '+str(len(rows))+' phases, '+str(sum(r['Runs'] for r in prefixes))+' complete prefix runs')
if __name__=='__main__': main()
