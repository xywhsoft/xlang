"""Exact Array/Stack projections through sealed IR and real source products.
Not all container casts, full53 or shipping library acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from xlir_public_build_inputs import verify_public_build
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true',help='Instrument generated C, hosts and changed schema helper on Linux; other providers remain uninstrumented.')
    args=parser.parse_args(); root=args.root.resolve(); public=args.public_directory.resolve(); out=args.out.resolve()
    assert not out.exists(); build,pinned,compared=verify_public_build(root,public)
    cc=shutil.which('gcc'); nm=shutil.which('nm'); assert cc and nm and not (args.sanitize and os.name=='nt')
    names=('stdlib_sequence_projection','stdlib_sequence_projection_domains','stdlib_sequence_projection_status','stdlib_stack_factory_only')
    fixtures=[root/'test/cases'/(name+'.xl') for name in names]
    contract=root/'test/support/sequence_projection_contract.c'; runtime=root/'test/support/sequence_projection_runtime.c'
    statusHost=root/'test/support/sequence_projection_status_host.c'; host=root/'test/cases/xlir_module_product_oom_host.c'
    memory=root/'test/support/module_product_memory.c'; helper=root/'src/runtime/xl6_sequence_projection_schema.c'
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),*fixtures,contract,runtime,statusHost,host,memory,native,helper))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True); rows=[]; prefixes=[]; products=[]; env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,Products=products,
        ComparedSources=compared,SequenceProjectionAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,
        GeneratedInstrumented=args.sanitize,SchemaHelperInstrumented=args.sanitize,OtherProvidersInstrumented=False,Scope=__doc__)
    if os.name=='nt':shutil.copy2(native,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    if args.sanitize:
        flags+=['-g','-fno-var-tracking-assignments','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    links=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    links += [str(native),*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)])]
    def run(name,command,reject=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=600)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,ExpectedRejection=reject))
        print(name+': '+str(result.returncode),flush=True)
        assert result.returncode!=0 if reject else result.returncode==0,(name,data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        binary=out/('contract'+suffix); run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
        for fixture in fixtures[:2]:assert 'rejected before output' in run(fixture.stem+'.contract',[binary,fixture])
        objects={}
        for opt in (0,2):
            objects[opt]=out/('memory.O'+str(opt)+'.o')
            run('memory.O'+str(opt),[cc,*flags,'-O'+str(opt),'-c',memory,'-o',objects[opt]])
            binary=out/('native.O'+str(opt)+suffix)
            run('native.O'+str(opt)+'.build',[cc,*flags,'-I'+str(root/'release'),'-O'+str(opt),
                '-Wl,--wrap=xl6SequenceSchemaCopyV1','-Wl,--wrap=xl6SequenceAdmissionDropV3',runtime,helper,objects[opt],
                *[path for path in links if Path(path).stem!='xl6_sequence_projection_schema'],'-o',binary])
            assert 'balanced; native prefixes=' in run('native.O'+str(opt)+'.run',[binary])
        for fixture in fixtures:
            name=fixture.stem; generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(name+'.repeat',[build['Driver'],fixture,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
            body=generated.read_text(encoding='utf-8')
            if name.startswith('stdlib_sequence_'):
                assert 'xl6SequenceProjectionWitnessTakeV1(' in body
                calls=re.findall(r'bool projection_ok_(\d+) = xl6SequenceProjectionWitnessTakeV1\(',body); assert calls
                for value in calls:assert re.search(r'\bv'+value+r'\s*=\s*NULL;',body)
            for opt in (0,2):
                binary=out/(name+'.O'+str(opt)+suffix)
                selected=statusHost if name.endswith('_status') else host
                extras=['-Wl,--wrap=xl6SequenceProjectionWitnessTakeV1'] if selected==statusHost else []
                if name=='stdlib_stack_factory_only':extras+=['-DXL6_PRODUCT_USER_ENTRY']
                run(name+'.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),'-include',generated,*extras,selected,objects[opt],*links,'-o',binary])
                assert 'balanced' in run(name+'.O'+str(opt)+'.run',[binary])
                if fixture in fixtures[:2]:
                    log=run(name+'.O'+str(opt)+'.oom',[binary,'--all'])
                    match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log); assert match
                    runs,first=map(int,match.groups()); assert runs==first+1
                    prefixes.append(dict(Fixture=name,Lane='O'+str(opt),Runs=runs,FirstNoHit=first))
            for mode in ('run','module'):run(name+'.'+mode,[build['Driver'],fixture,mode,out/(name+'.'+mode+'.result')])
            target=out/(name+'.aot'); run(name+'.aot.build',[build['Driver'],fixture,'exe',target])
            executable=Path(str(target)+'.exe') if os.name=='nt' else target
            run(name+'.aot.run',[executable]); symbols=run(name+'.aot.symbols',[nm,'--defined-only',executable])
            if name=='stdlib_stack_factory_only':assert not re.search(r'\bxl6SequenceProjection(?:Prepare|Apply|Dispose|WitnessTake)V1$',symbols,re.M)
        original=(out/'stdlib_sequence_projection_status.c').read_text(encoding='utf-8')
        status=re.findall(r'bool projection_ok_(\d+) = xl6SequenceProjectionWitnessTakeV1\(',original); assert len(status)==1
        changed,count=re.subn(r'if \(projection_ok_'+status[0]+r'\)','if (true)',original); assert count==1
        mutant=out/'lost-status.c'; mutant.write_text(changed,encoding='utf-8'); binary=out/('lost-status'+suffix)
        run('lost-status.build',[cc,*flags,'-O0','-include',mutant,'-Wl,--wrap=xl6SequenceProjectionWitnessTakeV1',statusHost,objects[0],*links,'-o',binary])
        rejected=run('lost-status.reject',[binary],True); assert 'sequence_projection_status_host.c' in rejected or 'runtime error' in rejected
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir())
            if path.is_file() and (path.suffix in ('.h','.c','.o','.exe') or os.access(path,os.X_OK)))
        report['SequenceProjectionAcceptance']=True; report['Complete']=True
    except Exception as error:report['Error']=repr(error); print(error,file=sys.stderr)
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
