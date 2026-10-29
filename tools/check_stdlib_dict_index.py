"""Native Dict dynamic indexing and actual generated checked functions.

Strict O0/O2, exact keys, immutable domains, owned read/consuming write,
complete allocation and no-diagnostic entry/provider prefixes, real broken
implementations, independent XLIR verification, JIT and selected AOT closure.
No source Dict producer/typed direct-index/restore or shipping/full53 claim.
"""
from pathlib import Path
import argparse,ast,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--public-directory',type=Path)
    p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
    root=a.root.resolve(); out=a.out.resolve(); cc=shutil.which('gcc'); assert cc and not out.exists()
    assert not a.sanitize or os.name!='nt'
    population=root/'tools/check_stdlib_dict_native.py'; tree=ast.parse(population.read_text(encoding='utf-8'))
    sources=ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign)
        and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets)))
    sources[sources.index('test/cases/stdlib_dict_native_host.c')]='test/cases/stdlib_dict_index_native_host.c'
    sources[sources.index('test/support/dict_native.c')]='test/support/list_index_native.c'
    sources+=['src/runtime/xl6_list.c','src/runtime/xl6_list_value_read.c','src/runtime/xl6_array.c',
        'src/runtime/xl6_array_erased.c','src/runtime/xl6_array_value_read.c',
        'src/runtime/xl6_value_index_read.c','src/runtime/xl6_value_index_write.c',
        'src/runtime/xl6_value_named_read.c','src/runtime/xl6_value_named_write.c',
        'src/runtime/xl6_value_object_fields.c','src/runtime/xl6_dynamic_fields.c',
        'src/runtime/xl6_native_handle.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_code_lease.c']
    fixture=root/'test/cases/stdlib_dict_index.xl'; contract=root/'test/cases/xlir_dict_index_contract.c'
    language=root/'test/cases/stdlib_dict_index_language_host.c'
    files={Path(__file__).resolve(),population,Path(cc),Path(sys.executable),fixture,contract,language,
        root/'test/cases/stdlib_dict_native_host.c',root/'test/cases/xlir_artifact_runtime_contract.c',
        root/'test/cases/xlir_module_product_oom_host.c',*[root/s for s in sources]}
    for folder in ('src/runtime','src/api','lib/xrt','lib/xruntime','release/include','test/support'):
        files.update((root/folder).rglob('*.h'))
    build=None; public=a.public_directory.resolve() if a.public_directory else None
    if public:
        manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
        if os.name!='nt': assert build['Complete'] and build['InputsUnchanged']
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path'])==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
        driver=Path(build['Driver']); assert sha(driver)==build['DriverSha256'].lower()
        product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
        files.update((manifest,driver,product))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; prefixes=[]; controls=[]; products=[]; objects=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        NegativeControls=controls,Products=products,Objects=objects,Platform=sys.platform,Sanitized=a.sanitize,
        NativeDictDynamicIndex=True,GeneratedLanguageNativeInputs=bool(public),LanguageDictFactoryCutover=False,
        TypedDictDirectIndexCutover=False,DictRestoreCutover=False,ShippingLibraryAcceptance=False,Full53Acceptance=False,
        Scope=__doc__,ScopeInjection='GNU ld --wrap, not actual OS context exhaustion')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime']
    wraps=['-Wl,--wrap='+s for s in ('xl6ErrorScopeBegin','xl6ErrorScopeBeginCleanup','xrtTypedDictIterNext',
        'xl6DictErasedReadV1','xl6DictErasedSetV1')]
    links=[] if os.name=='nt' else ['-pthread','-lm']; env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    san=[]
    if a.sanitize:
        san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']; env.update(
            ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,command,reject=False):
        command=list(map(str,command)); r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=360)
        data=r.stdout+r.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Name=label,Command=command,Exit=r.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if reject else r.returncode==0),(label,data[-5000:])
        if reject: assert b'assert' in data.lower(),(label,data[-1000:])
        return data.decode(errors='replace')
    def sweep(label,binary,mode,language=False):
        data=run(label,[binary,mode]); prefix='Dict index language' if language else 'Dict index'
        found=re.findall(prefix+r' closed lane=(\d+) mode='+mode+r' runs=(\d+) first-no-hit=(\d+)',data)
        assert len(found)==(4 if language else 8)
        for lane,runs,first in found:
            lane,runs,first=map(int,(lane,runs,first)); assert runs==first+1
            points=re.findall(prefix+' lane='+str(lane)+' mode='+mode+r' point=(\d+) hit=(\d) balanced',data)
            assert points==[(str(i),str(int(i<first))) for i in range(runs)]
            prefixes.append(dict(Label=label,Language=language,Mode=mode,Lane=lane,Runs=runs,FirstNoHit=first))
    try:
        cached={}
        for opt in ('O0','O2'):
            compiled=[out/(opt+'-'+str(i)+'.o') for i in range(len(sources))]
            def compile_one(pair):
                i,s=pair; run(opt+'-compile-'+str(i),[cc,*flags,*san,'-'+opt,'-c',s,'-o',compiled[i]])
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: list(pool.map(compile_one,enumerate(sources)))
            cached[opt]=compiled; objects.extend(dict(Path=str(obj),Sha256=sha(obj)) for obj in compiled)
            binary=out/('native-'+opt+('.exe' if os.name=='nt' else ''))
            run(opt+'-link',[cc,*compiled,*wraps,*links,'-o',binary]); assert '[PASS]' in run(opt+'-vectors',[binary,'vectors'])
            for mode in ('oom','scopes','providers'): sweep(opt+'-'+mode,binary,mode)
            products.append(dict(Path=str(binary),Sha256=sha(binary)))
        for name,source,before,after in (
            ('native-dict-read-omitted','src/runtime/xl6_value_named_read.c','if(dict>0) {',
                'if(dict>0 && xrtValueType(source)==XVALUE_OBJECT) {'),
            ('native-dict-write-omitted','src/runtime/xl6_value_named_write.c',
                'if(dict>0) success=xl6DictErasedSetV1(source,(xstrview){name,size},item);','if(dict>0) success=true;'),
            ('read-key-nul-truncated','src/runtime/xl6_value_index_read.c',
                'if (!xrtValueGetString(key,&text)) return NULL;',
                'if (!xrtValueGetString(key,&text)) return NULL;\n        text.Size=strlen(text.Data);'),
            ('write-key-nul-truncated','src/runtime/xl6_value_index_write.c','success=xl6ValueNameSetTakeV1(source,text.Data,text.Size,item);',
                'success=xl6ValueNameSetTakeV1(source,text.Data,strlen(text.Data),item);'),
            ('scope-entry-input-not-consumed','src/runtime/xl6_value_index_write.c',
                'xrtClearError(); xrtValueRelease(item); xrtClearError(); xrtSetErrorTake(primary);',
                'xrtClearError(); xrtClearError(); xrtSetErrorTake(primary);')):
            body=(root/source).read_text(encoding='utf-8'); assert body.count(before)==1,name
            changed=out/(name+'.c'); changed.write_text('#include <string.h>\n'+body.replace(before,after),encoding='utf-8')
            obj=out/(name+'.o'); run(name+'-compile',[cc,*flags,*san,'-O0','-c',changed,'-o',obj]); objects.append(dict(Path=str(obj),Sha256=sha(obj)))
            selected=list(cached['O0']); selected[sources.index(source)]=obj; binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'-link',[cc,*selected,*wraps,*links,'-o',binary]); run(name+'-reject',[binary,'vectors'],True)
            controls.append(dict(Name=name,Compiled=True,Rejected=True,Source=str(changed),SourceSha256=sha(changed),
                Product=str(binary),ProductSha256=sha(binary)))
        if public:
            if os.name=='nt': shutil.copy2(product,out/'xl.dll')
            else:
                env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
                env['XL6_TCC_LIB']=str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
            full=flags+['-DXL_STATIC_EMBED','-Ilib/tcc','-I'+str(public/'include'),'-Wno-unused-function',
                '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
                '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
            compiler=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
                ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
            systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
            shared=[str(product),*systems]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(product.parent)])
            binary=out/('contract'+('.exe' if os.name=='nt' else '')); bindings=out/'bindings.h'
            run('contract-build',[cc,*full,'-O0',contract,*compiler,*shared,'-o',binary])
            proof=run('contract',[binary,fixture.as_posix(),bindings]); report['Plan']=json.loads(next(line for line in proof.splitlines() if line.startswith('{')))
            report['Contract']=dict(zip(('Reads','Writes','Mutations'),map(int,re.search(
                r'Dict index contract reads=(\d+) writes=(\d+) mutations=(\d+)',proof).groups())))
            generated=out/'dict-index.c'; repeated=out/'dict-index-repeat.c'
            for target in (generated,repeated): run(target.stem,[driver,fixture,'cgen-default',target])
            assert sha(generated)==sha(repeated); body=generated.read_text(encoding='utf-8-sig')
            assert 'xl6ValueIndexReadCopyV1(' in body and 'xl6ValueIndexSetTakeV1(' in body
            for opt in ('O0','O2'):
                memory=out/(opt+'-memory.o'); run(opt+'-memory',[cc,*flags,'-'+opt,'-c','test/support/module_product_memory.c','-o',memory])
                binary=out/('language-'+opt+('.exe' if os.name=='nt' else ''))
                run(opt+'-language-build',[cc,*full,'-'+opt,'-include',generated,'-include',bindings,language,memory,*compiler,*wraps,*shared,'-o',binary])
                for mode in ('oom','providers'): sweep(opt+'-language-'+mode,binary,mode,True)
                products.append(dict(Path=str(binary),Sha256=sha(binary)))
                # First execute the pure production closure. Then add only the
                # explicitly reported test-input producer to exercise native
                # external inputs, never claim those additions were selected.
                plan=report['Plan']; native=out/(opt+'-pruned-native.c'); selected=[]
                native.write_text(''.join('#define '+m+'\n' for m in plan['NativeModules'])+
                    '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
                for i,source in enumerate([native,*[root/s for s in plan['Sources']]]):
                    obj=out/(opt+'-pruned-'+str(i)+'.o'); run(opt+'-pruned-compile-'+str(i),[cc,*full,'-'+opt,'-c',source,'-o',obj]); selected.append(obj)
                    objects.append(dict(Path=str(obj),Sha256=sha(obj)))
                binary=out/('module-pruned-'+opt+('.exe' if os.name=='nt' else ''))
                run(opt+'-module-pruned-build',[cc,*full,'-'+opt,'-include',generated,
                    root/'test/cases/xlir_module_product_oom_host.c',memory,*selected,*systems,'-o',binary])
                assert 'balanced' in run(opt+'-module-pruned-run',[binary]); products.append(dict(Path=str(binary),Sha256=sha(binary)))
                needed=['src/runtime/xl6_array_primitive_admission.c','src/runtime/xl6_value_admission.c','src/runtime/xl6_string_type.c']
                extra=[source for source in needed if source not in plan['Sources']]
                report['PrunedNativeInputRequiredSources']=needed
                report['PrunedNativeInputTestOnlySources']=extra
                extra_modules=[] if 'XRUNTIME_MODULE_RUNTIME_TYPE_STRING' in plan['NativeModules'] else ['XRUNTIME_MODULE_RUNTIME_TYPE_STRING']
                report['PrunedNativeInputTestOnlyModules']=extra_modules
                for i,source in enumerate(extra):
                    obj=out/(opt+'-test-input-'+str(i)+'.o')
                    run(opt+'-test-input-compile-'+str(i),[cc,*full,'-'+opt,'-c',root/source,'-o',obj]); selected.append(obj)
                    objects.append(dict(Path=str(obj),Sha256=sha(obj)))
                input_native=out/(opt+'-test-input-native.c'); input_object=out/(opt+'-test-input-native.o')
                input_native.write_text(''.join('#define '+m+'\n' for m in plan['NativeModules']+extra_modules)+
                    '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
                run(opt+'-test-input-native',[cc,*full,'-'+opt,'-c',input_native,'-o',input_object]); selected[0]=input_object
                objects.append(dict(Path=str(input_object),Sha256=sha(input_object)))
                binary=out/('language-pruned-'+opt+('.exe' if os.name=='nt' else ''))
                run(opt+'-pruned-build',[cc,*full,'-'+opt,'-include',generated,'-include',bindings,language,memory,*selected,*wraps,*systems,'-o',binary])
                for mode in ('oom','providers'): sweep(opt+'-pruned-'+mode,binary,mode,True)
                products.append(dict(Path=str(binary),Sha256=sha(binary)))
            run('jit',[driver,fixture,'run',out/'jit-result'])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs+objects+products)
        report['Complete']=report['InputsUnchanged'] and len(controls)==5 and len(prefixes)==(80 if public else 48)
        assert report['Complete']
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Native Dict index: '+str(len(rows))+' phases, '+str(len(prefixes))+' full prefixes, five mutants rejected',flush=True)

if __name__=='__main__': main()
