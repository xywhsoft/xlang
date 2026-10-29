"""Real resident nested-Set domains, mixed depth, consumed inputs and full OOM prefixes.

Strict O0/O2, optional ASan/UBSan over the selected whole native closure, and
compiled broken-provider controls. Uses actual XRT/xruntime List/Set/Array
storage, never a synthetic schema engine. Not language Set carrier cutover.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,shutil,subprocess,sys,textwrap
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true'); args=parser.parse_args()
    root=args.root.resolve(); out=args.out.resolve(); cc=shutil.which('gcc'); assert cc and not out.exists()
    assert not args.sanitize or os.name!='nt'
    shared=root/'tools/check_stdlib_set_native.py'; body=shared.read_text(encoding='utf-8')
    source_fragment=textwrap.dedent(body[body.index("    sources=['"):body.index('    files=')])
    population={}; exec(compile(source_fragment,str(shared),'exec'),population)
    sources=population['sources']; sources[0]='test/cases/stdlib_set_schema_native_host.c'; sources[1]='test/support/set_schema_native.c'
    sources+=['src/runtime/xl6_set_schema.c','src/runtime/xl6_list.c','src/runtime/xl6_list_schema.c',
        'src/runtime/xl6_array_sequence_admission.c']
    files={Path(__file__).resolve(),shared,Path(cc),Path(sys.executable),*[root/p for p in sources],root/'test/support/set_native.c'}
    for folder in ('src/runtime','src/api','release/include','lib/xrt','lib/xruntime','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir(parents=True); rows=[]; products=[]; objects=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Objects=objects,
        NegativeControls=controls,Platform=sys.platform,Sanitized=args.sanitize,Scope=__doc__,
        LanguageSetCutover=False,ShippingLibraryAcceptance=False,Full53Acceptance=False,
        BoundaryInjection='GNU ld --wrap, not actual OS error-context exhaustion')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=['-Wl,--wrap=xl6ErrorScopeBegin','-Wl,--wrap=xl6ErrorScopeBeginCleanup']; env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']; env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command,reject=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; log=out/(name+'.log'); log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
        assert result.returncode!=0 if reject else result.returncode==0,(name,data[-5000:])
        if reject: assert b'Assertion' in data or b'assertion' in data,(name,data[-2000:])
        return data.decode(errors='replace')
    try:
        cached={}
        for opt in ('O0','O2'):
            compiled=[out/(opt+'-'+str(i)+'.o') for i in range(len(sources))]
            def build(pair):
                i,path=pair; run('build-'+opt+'-'+str(i),[cc,*flags,'-'+opt,'-c',path,'-o',compiled[i]])
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: list(pool.map(build,enumerate(sources)))
            cached[opt]=compiled; objects.extend(dict(Path=str(p),Sha256=sha(p)) for p in compiled)
            binary=out/('schema-'+opt+('.exe' if os.name=='nt' else '')); run('link-'+opt,[cc,*compiled,*links,'-o',binary])
            data=run('run-'+opt,[binary]); assert '[PASS] complete resident Set reference domains' in data and data.count(' first-no-hit=')==5
            attempts=int(data.split('Set schema full-prefix-attempts=')[1].splitlines()[0])
            products.append(dict(Path=str(binary),Sha256=sha(binary),FullPrefixLanes=5,AllocationAttempts=attempts))
        body=(root/'src/runtime/xl6_set_schema.c').read_text(encoding='utf-8')
        for name,old,new in (
            ('logical-equivalence-lost','p->ops->Equivalent(p->data,q->data)','true'),
            ('budget-edge-off-by-one','if (depth>64)','if (depth>=64)'),
            ('nullable-default-swapped','schema,nullable);','schema,!nullable);')):
            assert old in body,name
            altered=out/(name+'.c'); altered.write_text(body.replace(old,new),encoding='utf-8')
            obj=out/(name+'.o'); run(name+'-compile',[cc,*flags,'-O0','-c',altered,'-o',obj]); objects.append(dict(Path=str(obj),Sha256=sha(obj)))
            compiled=list(cached['O0']); compiled[sources.index('src/runtime/xl6_set_schema.c')]=obj
            binary=out/(name+('.exe' if os.name=='nt' else '')); run(name+'-link',[cc,*compiled,*links,'-o',binary]); run(name+'-reject',[binary],True)
            controls.append(dict(Name=name,Source=str(altered),SourceSha256=sha(altered),Product=str(binary),ProductSha256=sha(binary),Compiled=True,Rejected=True))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=report['InputsUnchanged'] and len(controls)==3; assert report['Complete']
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Set schema: strict O0/O2, five closed prefixes, three real mutants rejected',flush=True)
if __name__=='__main__': main()
