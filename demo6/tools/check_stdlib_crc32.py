"""CRC32 actual-product tests, standalone relocation and saved native pruning.
Uses rebuilt CLI and both runtime/compiler libraries. No replacement providers,
source fallback in the isolated compiler, release promotion or full-suite claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','out'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists()
    task=Path(__file__).resolve().parents[1];win=os.name=='nt'
    cc,nm,inspect=[shutil.which(n) for n in ('gcc','nm','objdump' if win else 'readelf')]
    assert cc and nm and inspect
    names=('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so')
    products=[root/'release'/n for n in names];assert all(p.is_file() for p in products)
    cases=task/'test/cases'
    inputs={Path(__file__).resolve(),*products,*[cases/n for n in ('stdlib_crc32.xl','stdlib_crc32_text_only.xl','stdlib_crc32_baseline.xl',
        'bad_xlir_crc32_no_raise.xl','bad_xlir_crc32_result.xl','bad_xlir_crc32_buffer_pure.xl','stdlib_crc32_host.c',
        'stdlib_product_fixture_host.c','stdlib_pruning_product_host.c','stdlib53_pruning_plan.c')]}
    for name in ('xl6_crc32_text.c','xl6_crc32_bytes.c','xl6_crc32_buffer.c','xl6_crc32_internal.h'):
        inputs.add(root/'src/runtime'/name)
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(inputs)]
    out.mkdir();isolated=out/'isolated';release=isolated/'release';release.mkdir(parents=True)
    relocate=out/'relocated';relocate.mkdir();sdk=out/'sdk';sdk.mkdir();rows=[]
    report=dict(Complete=False,Scope=__doc__,Inputs=inputs,Rows=rows,ExplicitSubset=True)
    for product in products:
        shutil.copy2(product,release/product.name)
        if win and product.suffix=='.dll':shutil.copy2(product,isolated/product.name)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    def run(name,command,cwd=isolated,negative=False):
        r=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,timeout=240)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=r.returncode,Log=str(log),Sha256=sha(log)))
        assert (r.returncode!=0 if negative else r.returncode==0),(name,data[-5000:].decode(errors='replace'))
        return data
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED']
    links=[release/names[2]] if win else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
    try:
        host=isolated/('host.exe' if win else 'host');observer=isolated/('observer.exe' if win else 'observer')
        for binary,source in ((host,'stdlib_product_fixture_host.c'),(observer,'stdlib_pruning_product_host.c')):
            run(binary.stem+'.build',[cc,*flags,'-O2','-I'+str(root/'release/include'),cases/source,*links,'-o',binary])
        for product in products[1:]:
            binary=isolated/(product.stem+'.native'+('.exe' if win else ''))
            nativeLinks=[release/product.name] if win else ['-L'+str(release),'-l:'+product.name,'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
            run(product.stem+'.native.build',[cc,*flags,'-O2','-I'+str(root),'-I'+str(root/'release'),cases/'stdlib_crc32_host.c',*nativeLinks,'-o',binary])
            run(product.stem+'.native.run',[binary])
        cli=release/names[0]
        for name in ('stdlib_crc32','stdlib_crc32_text_only'):
            source=isolated/(name+'.xl');shutil.copy2(cases/source.name,source)
            run(name+'.jit',[cli,'run',source])
            run(name+'.module',[host,release/names[2],source,'module',out/(name+'.module-tmp')])
            work=out/(name+'.saved');work.mkdir();image=work/('program.exe' if win else 'program')
            run(name+'.aot-build',[observer,release/names[2],source,work,image])
            independent=relocate/(name+('.exe' if win else ''));shutil.copy2(image,independent)
            imports=run(name+'.imports',[inspect,'-p' if win else '-d',independent],relocate)
            deps=re.findall(rb'DLL Name:\s*([^\s]+)',imports) if win else re.findall(rb'Shared library:\s*\[([^\]]+)\]',imports)
            assert deps and not {n.decode().lower() for n in deps}&{n.lower() for n in names}
            if not win:assert b'(RPATH)' not in imports and b'(RUNPATH)' not in imports
            run(name+'.relocated',[independent],relocate)
            run(name+'.cgen',[cli,'cgen',source,'--emit',work/'generated.c'])
        for name in ('bad_xlir_crc32_no_raise','bad_xlir_crc32_result','bad_xlir_crc32_buffer_pure'):
            data=run(name,[cli,'cgen',cases/(name+'.xl'),'--emit',out/(name+'.c')],negative=True)
            assert b'XLIR direct runtime bridge has no exact declared ABI' in data,data
        sys.path.insert(0,str(root/'tools'));from gen_tcc_vfs_resources import load_resources
        bd=root/('build/tmp' if win else 'build/linux')
        manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if win else 'tcc_vfs_manifest_linux_runtime.txt')
        dist=root/'res/tcc/win64/x86_64' if win else bd/'tcc-dist'
        for resource in load_resources(manifest,root,dist,bd/'xrt.o',bd):
            if resource.virtual.startswith('include/__xl_runtime/'):
                dest=sdk/Path(resource.virtual).relative_to('include');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(resource.source,dest)
        for header in (root/'release/include').glob('*.h'):shutil.copy2(header,sdk/header.name)
        shutil.copy2(root/'lib/xruntime/xruntime.h',sdk/'xruntime.h')
        body=out/'stdlib_crc32_text_only.saved/xl6_xrt_impl.c'
        features=run('text.features',[cc,*flags,'-I'+str(sdk),'-E','-dM',body])
        selected=set(re.findall(rb'^#define (XRT_FEATURE_[A-Z0-9_]+)\b',features,re.M))
        assert b'XRT_FEATURE_CRC32' in selected
        assert not selected&{b'XRT_FEATURE_INFLATE',b'XRT_FEATURE_DEFLATE'}
        # Assertion formatting and ordinary runtime ownership already need
        # value/Buffer internally. Measure CRC's increment over that baseline,
        # not an absolute promise about unrelated language infrastructure.
        baseline=isolated/'baseline.xl';shutil.copy2(cases/'stdlib_crc32_baseline.xl',baseline)
        baselineWork=out/'baseline.saved';baselineWork.mkdir()
        run('baseline.emit',[observer,release/names[2],baseline,baselineWork,baselineWork/('baseline.exe' if win else 'baseline')])
        baselineFeatures=run('baseline.features',[cc,*flags,'-I'+str(sdk),'-E','-dM',baselineWork/'xl6_xrt_impl.c'])
        baselineSelected=set(re.findall(rb'^#define (XRT_FEATURE_[A-Z0-9_]+)\b',baselineFeatures,re.M))
        assert selected-baselineSelected=={b'XRT_FEATURE_CRC32'},sorted(selected-baselineSelected)
        if not win:
            planner=isolated/'planner'
            run('plan.build',[cc,*flags,'-O2','-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),cases/'stdlib53_pruning_plan.c',*links,'-o',planner])
            plan=json.loads(run('text.plan',[planner,isolated/'stdlib_crc32_text_only.xl']))
            crcSources={Path(s).name for s in plan['Sources'] if 'crc32' in s}
            assert crcSources=={'xl6_crc32_text.c'},crcSources
        for opt in ('O0','O2'):
            obj=out/(opt+'.o')
            run(opt+'.compile',[cc,*flags,'-I'+str(sdk),'-'+opt,'-c',body,'-o',obj])
            symbols=run(opt+'.symbols',[nm,'-S','--defined-only',obj])
            for symbol in (b'xrtCrc32',b'xrtCrc32Update',b'__xrtCrc32UpdateRaw'):
                assert len(re.findall(rb'\b'+symbol+rb'\r?$',symbols,re.M))==1
            assert not re.search(rb'\b(?:xrtInflate|xrtDeflate)\w*$',symbols,re.M)
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=report['InputsUnchanged']
    except Exception as e:report['Error']=repr(e)
    report['Artifacts']=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k in ('Complete','Error')},ensure_ascii=False),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
