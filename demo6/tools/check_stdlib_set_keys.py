"""Private resident typed Set key foundation, not language Set/Dict cutover.

Build actual consumer supplier bodies and resident providers under strict C,
exercise exact NUL/UTF-8/bytes/null and retained/Future reference semantics,
checked failure without a diagnostic, reentry and shared recursion limits,
and close four whole allocation-failure prefixes. Compile broken real
providers and require these same vectors to reject each implementation.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    cc=shutil.which('gcc'); assert cc and not out.exists() and (not args.sanitize or os.name!='nt')
    sources=['test/cases/stdlib_set_keys_native_host.c','test/support/set_keys_native.c',
        'test/support/module_product_memory.c','src/runtime/xl6_set_key_policy.c',
        'src/runtime/xl6_sequence.c','src/runtime/xl6_string_type.c',
        'src/runtime/xl6_nullable_string.c','src/runtime/xl6_ownership_plan.c','src/api/xl_foreign_error.c']
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable),*[root/p for p in sources]}
    for folder in ('src/runtime','src/api','release/include','lib/xrt','lib/xruntime','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    rows=[]; controls=[]; products=[]; out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,
        NegativeControls=controls,Products=products,Platform=sys.platform,Sanitized=args.sanitize,
        PrivateKeyFoundation=True,LanguageSetCutover=False,LanguageDictCutover=False,
        ShippingLibraryAcceptance=False,Full53Acceptance=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=['-Wl,--wrap=xl6ErrorScopeBegin']
    env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-no-pie']; env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,command,rejected=False):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; log=out/(label+'.log'); log.write_bytes(data)
        row=dict(Label=label,Command=command,Exit=result.returncode,ExpectedRejection=rejected,
            Log=str(log),LogSha256=sha(log)); rows.append(row)
        assert result.returncode!=0 if rejected else result.returncode==0,(label,data[-4000:])
        if rejected: assert b'Assertion' in data or b'assertion' in data,(label,data[-2000:])
        return data.decode(errors='replace')
    try:
        for opt in ('O0','O2'):
            binary=out/('keys-'+opt+('.exe' if os.name=='nt' else ''))
            run('build-'+opt,[cc,*flags,'-'+opt,*sources,*links,'-o',binary])
            data=run('run-'+opt,[binary]); assert '[PASS] resident typed Set key policies' in data
            assert 'resident-key full-prefix-attempts=15' in data
            products.append(dict(Path=str(binary),Sha256=sha(binary),FullPrefixLanes=4,AllocationAttempts=15))
        body=(root/'src/runtime/xl6_set_key_policy.c').read_text(encoding='utf-8')
        for name,old,new in (
            ('equality-payload-lost','return finish(&scope) && equal;','return finish(&scope) && equal && false;'),
            ('no-diagnostic-first-failure-lost','xl6ErrorScopePropagateFailure(result);','xrtSetErrorTake(result.error);'),
            ('canonical-null-omitted','value ? value : xrtValueNull(),&hash','value,&hash')):
            assert body.count(old)==1,name
            altered=out/(name+'.c'); altered.write_text(body.replace(old,new),encoding='utf-8')
            binary=out/(name+('.exe' if os.name=='nt' else ''))
            changed=[altered if p=='src/runtime/xl6_set_key_policy.c' else p for p in sources]
            run(name+'-build',[cc,*flags,'-O0',*changed,*links,'-o',binary])
            run(name+'-reject',[binary],True)
            controls.append(dict(Name=name,Source=str(altered),SourceSha256=sha(altered),
                Product=str(binary),ProductSha256=sha(binary),Compiled=True,Rejected=True))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=report['InputsUnchanged'] and len(controls)==3
        assert report['Complete']
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Resident Set keys: strict O0/O2, four full prefixes/15 attempts each, three real mutants rejected',flush=True)


if __name__=='__main__': main()
