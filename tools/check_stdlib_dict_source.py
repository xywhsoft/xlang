"""Native language Dict factories and resident methods, exact keys and ownership.

Strict O0/O2 native vectors, full allocation/scope/provider failure prefixes,
compiled broken implementations, actual XLIR pre-output rejection, generated
native owners and independent C/JIT links. Does not certify recursive restore,
unboxed indexing, shipping DLLs, or the entire 53-module long-term objective.
"""
from pathlib import Path
import argparse,ast,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--public-directory',type=Path); p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
    root=a.root.resolve(); out=a.out.resolve(); cc=shutil.which('gcc'); assert cc and not out.exists()
    assert not a.sanitize or os.name!='nt'
    population=root/'tools/check_stdlib_dict_native.py'; tree=ast.parse(population.read_text(encoding='utf-8'))
    sources=ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign)
        and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets)))
    sources[sources.index('test/cases/stdlib_dict_native_host.c')]='test/cases/stdlib_dict_source_native_host.c'
    sources[sources.index('test/support/dict_native.c')]='test/support/dict_methods_native.c'
    sources+=['src/runtime/xl6_dict_methods.c']
    fixture=root/'test/cases/stdlib_dict_source.xl'; contract=root/'test/cases/xlir_dict_source_contract.c'
    leaf=root/'test/cases/stdlib_dict_source_leaf.xl'
    language=root/'test/cases/stdlib_dict_source_language_host.c'
    files={Path(__file__).resolve(),population,Path(cc),Path(sys.executable),fixture,contract,leaf,language,
        root/'test/cases/stdlib_dict_native_host.c',root/'test/cases/xlir_artifact_runtime_contract.c',
        root/'test/cases/xlir_module_product_oom_host.c',*[root/s for s in sources]}
    for folder in ('src/runtime','src/api','lib/xrt','lib/xruntime','release/include','test/support'):
        files.update((root/folder).rglob('*.h'))
    public=a.public_directory.resolve() if a.public_directory else None
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
        LanguageDictFactoryCutover=bool(public),ResidentDictMethods=True,LanguageNativeOwnersProbed=bool(public),
        DictRestoreCutover=False,TypedDictUnboxedIndexCutover=False,ShippingLibraryAcceptance=False,Full53Acceptance=False,
        Scope=__doc__,ScopeInjection='GNU ld --wrap, not actual OS context exhaustion')
    report['LanguageScopeInjection']='All checked scope entries inside Dict factory/projection/merge/without-key providers; excludes generated fatal unwind TLS rebinding invariant'
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime']
    wrapped=('xl6ErrorScopeBegin','xl6ErrorScopeBeginCleanup','xrtTypedDictIterNext',
        'xl6DictErasedCountV1','xl6DictErasedKeyV1','xl6DictErasedClearV1','xl6DictErasedReserveV1',
        'xl6DictErasedTrimV1','xl6DictErasedReadV1','xl6DictErasedCloneV1','xl6DictMergeV1','xl6DictErasedSetV1')
    wraps=['-Wl,--wrap='+s for s in wrapped]; links=[] if os.name=='nt' else ['-pthread','-lm']; env=dict(os.environ)
    san=[]
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if a.sanitize:
        san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']; env.update(
            ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,command,reject=False):
        command=list(map(str,command)); r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=360)
        data=r.stdout+r.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Name=label,Command=command,Exit=r.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if reject else r.returncode==0),(label,data[-6000:])
        if reject: assert b'assert' in data.lower(),(label,data[-1000:])
        return data.decode(errors='replace')
    def sweep(label,binary,mode,language=False):
        data=run(label,[binary,mode]); prefix='Dict factory language' if language else 'Dict methods'
        found=re.findall(prefix+r' closed '+('' if language else r'lane=(\d+) ')+r'mode='+mode+r' runs=(\d+) first-no-hit=(\d+)',data)
        assert len(found)==(1 if language else 15)
        for row in found:
            lane,runs,first=(0,*map(int,row)) if language else map(int,row); assert runs==first+1
            points=re.findall(prefix+(' ' if language else ' lane='+str(lane)+' ')+r'mode='+mode+r' point=(\d+) hit=(\d) balanced',data)
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
        for name,before,after in (
            ('native-count-omitted','if (native) return xl6DictErasedCountV1(source,capacity,output);',
                'if (native) { *output=0; return true; }'),
            ('native-key-truncated','if (native) return xl6DictErasedKeyV1(source,text,remove,found);',
                'if (native) { if (text.Data) text.Size=strlen(text.Data); return xl6DictErasedKeyV1(source,text,remove,found); }'),
            ('negative-reserve-accepted','if (size<0 || (uint64_t)size>SIZE_MAX) xrtSetErrorKind(XERR_RANGE);',
                'if (size<0 || (uint64_t)size>SIZE_MAX) success=true;'),
            ('without-key-omitted','if (!result || !key(result,text,true,&found) || scope.failed || xrtGetError()) {',
                'if (!result || scope.failed || xrtGetError()) { (void)text; (void)found;'),
            ('copy-alias-returned','return native>0 ? xl6DictErasedCloneV1(source) : !native ? xrtValueClone(source) : NULL;',
                'return native>0 ? xrtValueRetain(source) : !native ? xrtValueClone(source) : NULL;')):
            source='src/runtime/xl6_dict_methods.c'; body=(root/source).read_text(encoding='utf-8'); assert body.count(before)==1,name
            changed=out/(name+'.c'); changed.write_text('#include <string.h>\n'+body.replace(before,after),encoding='utf-8')
            obj=out/(name+'.o'); run(name+'-compile',[cc,*flags,*san,'-O0','-c',changed,'-o',obj]); objects.append(dict(Path=str(obj),Sha256=sha(obj)))
            selected=list(cached['O0']); selected[sources.index(source)]=obj; binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'-link',[cc,*selected,*wraps,*links,'-o',binary]); run(name+'-reject',[binary,'vectors'],True)
            controls.append(dict(Name=name,Compiled=True,Rejected=True,Source=str(changed),SourceSha256=sha(changed),
                Product=str(binary),ProductSha256=sha(binary)))
        if public:
            if os.name=='nt': shutil.copy2(product,out/'xl.dll')
            else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
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
            report['Contract']=dict(zip(('Creates','Methods','Mutations'),map(int,re.search(
                r'Dict source contract creates=(\d+) methods=(\d+) mutations=(\d+)',proof).groups())))
            leaf_proof=run('leaf-contract',[binary,leaf.as_posix()]); report['LeafPlan']=json.loads(next(line for line in leaf_proof.splitlines() if line.startswith('{')))
            generated=out/'dict-source.c'; repeated=out/'dict-source-repeat.c'
            for target in (generated,repeated): run(target.stem,[driver,fixture,'cgen-default',target])
            assert sha(generated)==sha(repeated); body=generated.read_text(encoding='utf-8-sig')
            assert 'xl6DictCreateTakeV1(' in body and 'xl6DictValueMergeRightV1(' in body and 'xl6DictValueWithoutKeyV1(' in body
            assert 'xlir_xvalue_dict_merge_right(' not in body and 'xlir_xvalue_dict_without_string_key(' not in body
            for opt in ('O0','O2'):
                memory=out/(opt+'-memory.o'); run(opt+'-memory',[cc,*flags,'-'+opt,'-c','test/support/module_product_memory.c','-o',memory])
                binary=out/('language-'+opt+('.exe' if os.name=='nt' else ''))
                language_wraps=['-Wl,--wrap='+s for s in ('xl6ErrorScopeBegin','xl6ErrorScopeBeginCleanup',
                    'xl6DictCreateTakeV1','xl6DictProjectionTakeV1','xl6DictValueMergeRightV1','xl6DictValueWithoutKeyV1')]
                run(opt+'-language-build',[cc,*full,'-'+opt,'-include',generated,'-include',bindings,language,memory,*compiler,*language_wraps,*shared,'-o',binary])
                for mode in ('oom','scopes'): sweep(opt+'-language-'+mode,binary,mode,True)
                products.append(dict(Path=str(binary),Sha256=sha(binary)))
                plan=report['Plan']; native=out/(opt+'-pruned-native.c'); selected=[]
                native.write_text(''.join('#define '+m+'\n' for m in plan['NativeModules'])+
                    '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
                for i,source in enumerate([native,*[root/s for s in plan['Sources']]]):
                    obj=out/(opt+'-pruned-'+str(i)+'.o'); run(opt+'-pruned-compile-'+str(i),[cc,*full,'-'+opt,'-c',source,'-o',obj]); selected.append(obj)
                    objects.append(dict(Path=str(obj),Sha256=sha(obj)))
                binary=out/('language-pruned-'+opt+('.exe' if os.name=='nt' else ''))
                run(opt+'-pruned-build',[cc,*full,'-'+opt,'-include',generated,'-include',bindings,language,memory,*selected,*language_wraps,*systems,'-o',binary])
                for mode in ('oom','scopes'): sweep(opt+'-pruned-'+mode,binary,mode,True)
                products.append(dict(Path=str(binary),Sha256=sha(binary)))
                module=out/('module-'+opt+('.exe' if os.name=='nt' else ''))
                run(opt+'-module-build',[cc,*full,'-'+opt,'-include',generated,
                    root/'test/cases/xlir_module_product_oom_host.c',memory,*selected,*systems,'-o',module])
                module_proof=run(opt+'-module-prefixes',[module,'--all'])
                module_runs,module_first=map(int,re.search(
                    r'module product full prefixes=(\d+) first-no-hit=(\d+) balanced',module_proof).groups())
                assert module_runs==module_first+1
                report.setdefault('ModulePrefixes',[]).append(dict(Optimization=opt,Runs=module_runs,FirstNoHit=module_first))
                products.append(dict(Path=str(module),Sha256=sha(module)))
            leaf_generated=out/'dict-leaf.c'; run('leaf-cgen',[driver,leaf,'cgen-default',leaf_generated])
            for opt in ('O0','O2'):
                plan=report['LeafPlan']; native=out/(opt+'-leaf-native.c'); selected=[]
                native.write_text(''.join('#define '+m+'\n' for m in plan['NativeModules'])+
                    '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
                for i,source in enumerate([native,*[root/s for s in plan['Sources']]]):
                    obj=out/(opt+'-leaf-'+str(i)+'.o'); run(opt+'-leaf-compile-'+str(i),[cc,*full,'-'+opt,'-c',source,'-o',obj]); selected.append(obj)
                    objects.append(dict(Path=str(obj),Sha256=sha(obj)))
                module=out/('leaf-module-'+opt+('.exe' if os.name=='nt' else ''))
                run(opt+'-leaf-build',[cc,*full,'-'+opt,'-include',leaf_generated,
                    root/'test/cases/xlir_module_product_oom_host.c',out/(opt+'-memory.o'),*selected,*systems,'-o',module])
                proof=run(opt+'-leaf-prefixes',[module,'--all'])
                runs,first=map(int,re.search(r'module product full prefixes=(\d+) first-no-hit=(\d+) balanced',proof).groups())
                assert runs==first+1
                report.setdefault('LeafModulePrefixes',[]).append(dict(Optimization=opt,Runs=runs,FirstNoHit=first))
                products.append(dict(Path=str(module),Sha256=sha(module)))
            run('jit',[driver,fixture,'run',out/'jit-result'])
            run('leaf-jit',[driver,leaf,'run',out/'leaf-jit-result'])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs+objects+products)
        report['Complete']=report['InputsUnchanged'] and len(controls)==5 and len(prefixes)==(98 if public else 90)
        assert report['Complete']
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Dict source: '+str(len(rows))+' phases, '+str(len(prefixes))+' full prefixes, five mutants rejected',flush=True)
if __name__=='__main__': main()
