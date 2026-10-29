"""Private genuine typed Set storage, checked Value methods and snapshot projection.

Strict actual supplier and resident providers, O0/O2, 11 raw-slot and 17 boxed
whole allocation prefixes, exact domains, snapshot/sticky cursors, real cyclic/
Future references, checked entry/cleanup and absent conversion failure, plus
compiled broken implementation controls. This is not language carrier cutover.
Optional ASan/UBSan instruments this whole selected native closure, not DLLs.
"""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, os, shutil, subprocess, sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    cc=shutil.which('gcc'); assert cc and not out.exists() and (not args.sanitize or os.name!='nt')
    sources=['test/cases/stdlib_set_native_host.c','test/support/set_native.c','test/support/module_product_memory.c',
        'src/runtime/xl6_set.c','src/runtime/xl6_set_key_policy.c','src/runtime/xl6_sequence.c',
        'src/runtime/xl6_native_leaf_value.c',
        'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_array_nominal_admission.c','src/runtime/xl6_array_primitive_admission.c',
        'src/runtime/xl6_array_c_scalar_admission.c','src/runtime/xl6_array_future_admission.c',
        'src/runtime/xl6_string_value.c','src/runtime/xl6_string_type.c','src/runtime/xl6_nullable_string.c',
        'src/runtime/xl6_nullable_i64.c','src/runtime/xl6_nullable_i64_value.c',
        'src/runtime/xl6_value_admission.c','src/runtime/xl6_nominal_admission.c',
        'src/runtime/xl6_char_type.c','src/runtime/xl6_char_value.c',
        'src/runtime/xl6_ownership_plan.c','src/api/xl_foreign_error.c']
    sources+=['src/runtime/xl6_array_nullable_c_scalar_admission.c']
    sources+=['src/runtime/xl6_nullable_'+name+'.c' for name in ('bool','u64','f32','f64','time','char',
        'native_i8','native_u8','native_i16','native_u16','native_i32','native_u32')]
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable),*[root/p for p in sources]}
    for folder in ('src/runtime','src/api','release/include','lib/xrt','lib/xruntime','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    rows=[]; controls=[]; products=[]; compiled=[]; out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,NegativeControls=controls,
        Products=products,Objects=compiled,Platform=sys.platform,Sanitized=args.sanitize,PrivateNativeSet=True,
        PrivateErasedSetMethods=True,LanguageFactoryCutover=False,
        LanguageSetCutover=False,LanguageDictCutover=False,ShippingLibraryAcceptance=False,Full53Acceptance=False,
        Scope=__doc__,BoundaryInjection='GNU ld --wrap, not actual OS error-context exhaustion')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=['-Wl,--wrap=xl6ErrorScopeBegin','-Wl,--wrap=xl6ErrorScopeBeginCleanup']
    env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']; env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,command,rejected=False):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Label=label,Command=command,Exit=result.returncode,ExpectedRejection=rejected,Log=str(log),LogSha256=sha(log)))
        assert result.returncode!=0 if rejected else result.returncode==0,(label,data[-4500:])
        if rejected: assert b'Assertion' in data or b'assertion' in data,(label,data[-2000:])
        return data.decode(errors='replace')
    try:
        cached={}
        for opt in ('O0','O2'):
            objects=[out/(opt+'-'+str(i)+'.o') for i in range(len(sources))]
            def compile_one(pair):
                i,path=pair; run('build-'+opt+'-'+str(i),[cc,*flags,'-'+opt,'-c',path,'-o',objects[i]])
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: list(pool.map(compile_one,enumerate(sources)))
            cached[opt]=objects
            compiled.extend(dict(Path=str(p),Sha256=sha(p)) for p in objects)
            binary=out/('set-'+opt+('.exe' if os.name=='nt' else ''))
            run('link-'+opt,[cc,*objects,*links,'-o',binary]); data=run('run-'+opt,[binary])
            assert '[PASS] genuine resident typed Set' in data and data.count(' first-no-hit=')==28
            raw=int(data.split('native-set full-prefix-attempts=')[1].splitlines()[0])
            boxed=int(data.split('erased-set full-prefix-attempts=')[1].splitlines()[0])
            products.append(dict(Path=str(binary),Sha256=sha(binary),FullPrefixLanes=28,
                RawPrefixLanes=11,BoxedPrefixLanes=17,RawAllocationAttempts=raw,
                BoxedAllocationAttempts=boxed,AllocationAttempts=raw+boxed))
        body=(root/'src/runtime/xl6_set.c').read_text(encoding='utf-8')
        for name,old,new in (
            ('order-sensitive-hash','aggregate+=hash;','aggregate=aggregate*UINT64_C(131)+hash;'),
            ('cursor-failure-not-sticky','owner->failed=true;','owner->failed=false;'),
            ('ownership-edges-omitted','if (owner->layout.value.transfer<3) return true;','if (owner->layout.value.transfer>=1) return true;'),
            ('logical-policy-equivalence-lost','a->ops->Equivalent(a->data,b->data)','true'),
            ('projection-before-take-omitted','result=admitProjection(owner,result,&scope);\n    if (!scopeFinish(&scope,result!=NULL))',
                'if (!result) xl6ErrorScopeCaptureFailure(&scope);\n    if (!scopeFinish(&scope,result!=NULL))'),
            ('decoder-absent-failure-ignored','if (!scopeFinish(&scope,ok)) { callbackFailure(); return false; }',
                'if (!scopeFinish(&scope,ok)) { xrtClearError(); return ok; }'),
            ('clone-projection-lost','owner->projection,owner->projected);','NULL,false);'),
            ('foreign-family-probe-lies','ops==&handleOps ? 1 : 0;','ops ? 1 : 0;')):
            assert old in body,name
            altered=out/(name+'.c'); altered.write_text(body.replace(old,new),encoding='utf-8')
            obj=out/(name+'.o'); run(name+'-compile',[cc,*flags,'-O0','-c',altered,'-o',obj])
            compiled.append(dict(Path=str(obj),Sha256=sha(obj)))
            binary=out/(name+('.exe' if os.name=='nt' else '')); objects=list(cached['O0']); objects[sources.index('src/runtime/xl6_set.c')]=obj
            run(name+'-link',[cc,*objects,*links,'-o',binary]); run(name+'-reject',[binary],True)
            controls.append(dict(Name=name,Source=str(altered),SourceSha256=sha(altered),Product=str(binary),ProductSha256=sha(binary),Compiled=True,Rejected=True))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=report['InputsUnchanged'] and len(controls)==8; assert report['Complete']
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Native Set: strict O0/O2, 28 complete allocation prefixes, eight real mutants rejected',flush=True)

if __name__=='__main__': main()
