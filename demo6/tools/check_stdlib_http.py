"""HTTP normal vectors, lifetime/OOM, typed ABI, standalone products and trimming.

Default mode uses only genuine CLI/DLL/SO products and their embedded VFS.
--driver is a development-only source-driver check, never shipping acceptance.
No public traffic, probing, attack simulation or security certification.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,threading,sys

CASES=['stdlib_http1_core','stdlib_http_headers_v2','stdlib_http_syntax_v2','stdlib_http_fields_v2',
    'stdlib_http1_values_v2','stdlib_http1_stream_v2','stdlib_http_decode_v2',
    'stdlib_http1_net_buffer_v2','stdlib_http1_transport_declarations',
    'stdlib_http1_tcp_transport','stdlib_http1_tcp_transport_async',
    'stdlib_http1_tls_transport','stdlib_http1_tcp_transport_stops','stdlib_http_minimal_v2','stdlib_http_language_oom','stdlib_http_perf']
BAD=['bad_stdlib_http_native_handle','bad_stdlib_http_legacy_body','bad_stdlib_http_legacy_decoder',
     'bad_stdlib_http_no_raise','bad_stdlib_http_enum_result']
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--driver',type=Path);p.add_argument('--jobs',type=int,default=2);a=p.parse_args()
    root,out=a.root.resolve(),a.out.resolve();assert not out.exists() and 1<=a.jobs<=2
    windows=os.name=='nt';cc=shutil.which('gcc');assert cc
    cases=root/'test/cases';nativeSource=cases/'stdlib_http_native_host.c'
    inputs={Path(__file__).resolve(),nativeSource,*[cases/(n+'.xl') for n in CASES+BAD]}
    inputs.update(cases/n for n in ['stdlib_codec_template_language_host.c','stdlib_codec_template_language_oom_host.c',
        'stdlib_product_fixture_host.c','stdlib_pruning_product_host.c'])
    inputs.update(root/'test/support'/n for n in ['module_product_memory.c','module_product_memory.h'])
    inputs.update(root.glob('src/runtime/xl6_http*'));inputs={p for p in inputs if p.is_file()}
    out.mkdir();isolated=out/'isolated';release=isolated/'release';release.mkdir(parents=True)
    relocated=out/'relocated';relocated.mkdir();rows=[];lock=threading.Lock()
    report=dict(Complete=False,Scope=__doc__,SourceDriverOnly=bool(a.driver),ShippingAcceptance=False,
        Cases=CASES,NegativeCases=BAD,Rows=rows)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
        ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    def run(name,args,cwd=isolated,negative=False):
        args=list(map(str,args));log=out/(name+'.log')
        try:
            r=subprocess.run(args,cwd=cwd,env=env,capture_output=True,timeout=240)
            data=r.stdout+r.stderr;code=r.returncode
        except subprocess.TimeoutExpired as e:data=(e.stdout or b'')+(e.stderr or b'');code='timeout'
        log.write_bytes(data)
        with lock:rows.append(dict(Name=name,Command=args,Exit=code,Negative=negative,Log=str(log),Sha256=sha(log)))
        assert isinstance(code,int) and (code!=0 if negative else code==0),(name,data[-3500:].decode(errors='replace'))
        return data
    try:
        driver=a.driver.resolve() if a.driver else None
        if driver:assert driver.is_file();inputs.add(driver)
        else:
            names=('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so')
            for name in names:
                source=root/'release'/name;assert source.is_file();inputs.add(source);shutil.copy2(source,release/name)
                if windows and source.suffix=='.dll':shutil.copy2(source,isolated/name)
            cli=release/names[0];library=release/names[2]
            flags=['-std=c11','-Wall','-Wextra','-Werror','-O2','-DXL_STATIC_EMBED']
            links=[library] if windows else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
            host=isolated/('host.exe' if windows else 'host');observer=isolated/('observer.exe' if windows else 'observer')
            for binary,source in [(host,'stdlib_product_fixture_host.c'),(observer,'stdlib_pruning_product_host.c')]:
                inputs.add(cases/source);run(binary.stem+'.build',[cc,*flags,'-I'+str(root/'release/include'),cases/source,*links,'-o',binary])
            for name in names[1:]:
                product=release/name;binary=isolated/(product.stem+'.http-native'+('.exe' if windows else ''))
                nativeLinks=[product] if windows else ['-L'+str(release),'-l:'+name,'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
                run(product.stem+'.native.build',[cc,*flags,'-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),nativeSource,*nativeLinks,'-o',binary])
                run(product.stem+'.native.run',[binary])
        report['Inputs']=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(inputs)]
        def positive(name):
            source=cases/(name+'.xl');work=out/(name+'.saved');work.mkdir()
            if driver:
                run(name+'.run',[driver,source,'run',work/'jit'],root)
                run(name+'.cgen',[driver,source,'cgen-default',work/'generated.c'],root)
            else:
                run(name+'.run',[cli,'run',source])
                run(name+'.module',[host,library,source,'module',work/'module-tmp'])
                run(name+'.cgen',[cli,'cgen',source,'--emit',work/'generated.c'])
                image=work/('program.exe' if windows else 'program')
                run(name+'.aot-build',[observer,library,source,work,image])
                independent=relocated/(name+('.exe' if windows else ''));shutil.copy2(image,independent)
                inspect=shutil.which('objdump' if windows else 'readelf');assert inspect
                imports=run(name+'.imports',[inspect,'-p' if windows else '-d',independent])
                deps=re.findall(rb'DLL Name:\s*([^\s]+)',imports) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]',imports)
                assert deps and not {d.decode().lower() for d in deps}&{n.lower() for n in names}
                if not windows:assert b'(RPATH)' not in imports and b'(RUNPATH)' not in imports
                run(name+'.relocated',[independent],relocated)
            generated=(work/'generated.c').read_text(encoding='utf-8-sig')
            assert 'xl6_xrt_http1_' not in generated and 'xl6_xrt_http_decoder_' not in generated
            return name
        failures=[]
        with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as executor:
            for future in concurrent.futures.as_completed({executor.submit(positive,n):n for n in CASES}):
                try:future.result()
                except Exception as error:failures.append(repr(error))
        for name in BAD:
            try:
                source=cases/(name+'.xl')
                data=run(name,[driver,source,'cgen-default',out/(name+'.c')] if driver else [cli,'cgen',source,'--emit',out/(name+'.c')],root if driver else isolated,negative=True)
                if name.endswith(('no_raise','enum_result')):assert b'exact declared ABI' in data,(name,data[-1000:])
                else:assert b'[sem]' in data,(name,data[-1000:])
            except Exception as error:failures.append(repr(error))
        assert not failures,failures
        if not driver:
            # Strict generated-C and allocation-prefix hosts use the exact
            # SDK header inputs of this product's runtime VFS manifest.
            sys.path.insert(0,str(root/'tools'))
            from gen_tcc_vfs_resources import load_resources
            sdk=out/'sdk';sdk.mkdir();bd=root/('build/tmp' if windows else 'build/linux')
            dist=root/'res/tcc/win64/x86_64' if windows else bd/'tcc-dist'
            manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
            headerInputs=[]
            for resource in load_resources(manifest,root,dist,bd/'xrt.o',bd):
                if resource.virtual.startswith('include/__xl_runtime/'):
                    dest=sdk/Path(resource.virtual).relative_to('include');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(resource.source,dest)
                    headerInputs.append(dict(Path=str(resource.source),Sha256=sha(resource.source)))
            report['HeaderInputs']=headerInputs
            # The ordinary Windows runtime uses TCC; Linux providers use GCC
            # -O2. Match the handwritten C compiler to that provider compiler.
            benchmark=isolated/('http-tcc-benchmark.exe' if windows else 'libxlc.http-native')
            if windows:
                tcc=bd/'tcc_bootstrap.exe';assert tcc.is_file()
                tccFlags=['-std=c11','-Wall','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
                tccFlags+=['-I'+str(root/'lib/tcc/include'),'-I'+str(dist/'include'),'-I'+str(dist/'include/winapi'),'-L'+str(dist/'lib')]
                run('same-compiler.benchmark.build',[tcc,*tccFlags,nativeSource,root/'release/xlc.def','-o',benchmark])
                report['NativeBenchmarkCompiler']='product TCC, matching Windows resident providers'
            else:report['NativeBenchmarkCompiler']='GCC -O2, matching Linux resident providers'
            run('same-compiler.benchmark',[benchmark,'--benchmark-only'])
            strict=[*flags,'-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
                '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
                '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(sdk)]
            for name in CASES[:8]+['stdlib_http_language_oom']:
                sourceHost=cases/('stdlib_codec_template_language_oom_host.c' if name=='stdlib_http_language_oom' else 'stdlib_codec_template_language_host.c')
                module=out/(name+'.saved')/'module.c';assert module.is_file()
                for opt in ('O0','O2'):
                    binary=isolated/(name+'.'+opt+('.exe' if windows else ''))
                    run(name+'.'+opt+'.build',[cc,*strict,'-'+opt,'-DXLIR_GENERATED_MODULE="'+module.as_posix()+'"',
                        sourceHost,root/'test/support/module_product_memory.c',*links,'-o',binary])
                    run(name+'.'+opt+'.lifetime',[binary])
            assert all(sha(r['Path'])==r['Sha256'] for r in headerInputs)
            body=out/'stdlib_http_minimal_v2.saved/xl6_xrt_impl.c';assert body.is_file()
            macros=run('minimal.features',[cc,'-E','-dM','-I'+str(root/'lib/xruntime'),body])
            selected=set(re.findall(rb'^#define (XRT_FEATURE_[A-Z0-9_]+)\b',macros,re.M))
            assert b'XRT_FEATURE_HTTP' in selected
            assert not selected&{b'XRT_FEATURE_HTTP1_HEAD',b'XRT_FEATURE_HTTP_DECODE',b'XRT_FEATURE_TLS',b'XRT_FEATURE_NET_ENGINE',b'XRT_FEATURE_INFLATE',b'XRT_FEATURE_DEFLATE'}
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in report['Inputs'])
        report['Complete']=report['InputsUnchanged'];report['ShippingAcceptance']=report['Complete'] and not driver
    except Exception as error:report['Error']=repr(error)
    report['Rows'].sort(key=lambda r:r['Name'])
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k in ('Complete','Error','SourceDriverOnly','ShippingAcceptance')},ensure_ascii=False),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
