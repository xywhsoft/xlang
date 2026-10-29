"""Current-source module service ownership, compiled effects and native pool graphs.

Uses a verified complete-source object set; the test TUs use O0/O2 while those
production objects retain their recorded options. No shipping/JIT/AOT or network
acceptance. The cursor negative control alone replaces one catalog object.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--compiler',default='gcc');a=p.parse_args()
    root,public,out=(v.resolve() for v in (a.root,a.public_directory,a.out));assert not out.exists()
    build,pinned,compared=verify_public_build(root,public)
    cc=Path(shutil.which(a.compiler) or a.compiler).resolve();windows=os.name=='nt'
    runtime=root/'release/xl.dll' if windows else Path(build['NativeRoot'])/'release/libxl.so'
    objects=[v['Path'] for v in build['Objects'] if Path(v['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    tests=dict(module=root/'test/cases/xlir_task_pool_graph_contract.c',pool=root/'test/cases/xlir_task_pool_graph_runtime.c',
        effects=root/'test/cases/xlir_runtime_operand_catalog.c')
    catalog=root/'test/support/ownership_catalog_contract.c'
    pinned.update((Path(__file__).resolve(),root/'tools/check_xlir_contract.ps1',root/'tools/check_xlir_task_pool_graph.ps1',
        cc,Path(sys.executable).resolve(),runtime,catalog,root/'test/cases/xlir_module_executor_mixed.xl',
        root/'test/cases/xlir_continuation_owner_test.h',root/'test/cases/xlir_synthetic_owner_edit.h',*tests.values()))
    inputs={str(v):sha(v) for v in sorted(pinned)};out.mkdir(parents=True);rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,ProductsUnchanged=False,Scope=__doc__,
        Inputs=inputs,Rows=rows,Products=products,Failures=failures,ComparedSources=compared,
        ModulePartitions=4,ModuleMutations=32,CursorControl=False,NativePoolGraph=False,EffectsCatalog=False,
        ShippingAcceptance=False,JitAotAcceptance=False,FullP1Acceptance=False,NetworkTests=False)
    env=dict(os.environ)
    if windows:shutil.copy2(runtime,out/'xl.dll')
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+os.pathsep+env.get('LD_LIBRARY_PATH','')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXL_ENABLE_EXE_COMPILER=1','-DTCC_DISABLE_BUILTIN_VFS',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(public/'include'),'-I'+str(root/'src/runtime'),
        '-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation']
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)])]
    if not windows:flags+=['-D_GNU_SOURCE']
    suffix='.exe' if windows else ''
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,reject=False):
        command=list(map(str,command));log=out/(name+'.log');started=time.monotonic()
        with log.open('wb') as stream:r=subprocess.run(command,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Command=command,Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
        print(name+': '+str(r.returncode),flush=True);text=log.read_text(encoding='utf8',errors='replace')
        assert (r.returncode!=0 if reject else r.returncode==0),(name,text[-5000:])
        if reject:assert 'assert' in text.lower(),(name,'not a runtime assertion rejection')
        return text
    def compile_to(name,sources,selected,target,opt):
        command=[*flags,'-'+opt,*sources,*selected,*libs,'-o',target];rsp=out/(name+'.rsp')
        rsp.write_text('\n'.join('"'+str(v).replace('\\','/').replace('"','\\"')+'"' for v in command)+'\n',encoding='utf8');product(rsp)
        run(name,[cc,'@'+str(rsp)]);product(target)
    try:
        save()
        mutant=out/'cursor-not-admitted.c';body=(root/'src/runtime/xl6_ownership_catalog.c').read_text(encoding='utf-8-sig')
        needle='if(!selected)selected=xrtValueCursorOwnershipAdapterV1(ref);';assert body.count(needle)==1
        mutant.write_text(body.replace(needle,'/* negative control: omit actual native cursor admission */'),encoding='utf8');product(mutant)
        other=[o for o in objects if Path(o).stem!='xl6_ownership_catalog'];assert len(other)==len(objects)-1
        for opt in ('O0','O2'):
            for name,source in tests.items():
                binary=out/(name+'-'+opt+suffix);compile_to(name+'-'+opt+'-build',[source],objects,binary,opt)
                text=run(name+'-'+opt,[binary])
                if name=='module':assert '4 independent Pool/Executor partitions, 32 C/header-gated mutations' in text
                elif name=='pool':assert '280 balanced lifetimes, 240 exact queued raw/language graphs, 520 real preparations' in text
                else:
                    match=re.search(r'operations=(\d+) effects=(\d+) consumes=(\d+) invalid=(\d+) rejected',text);assert match,text
                    operations,effects,consumes,invalid=map(int,match.groups());assert effects==operations*8 and consumes>0 and invalid==40
            binary=out/('cursor-not-admitted-'+opt+suffix)
            compile_to('cursor-'+opt+'-build',[catalog,mutant],other,binary,opt)
            text=run('cursor-'+opt+'-rejected',[binary,'basic'],True)
            assert 'xl6OwnershipCatalogResolveV1' in text,'wrong refusal point'
        report.update(CursorControl=True,NativePoolGraph=True,EffectsCatalog=True)
        report['InputsUnchanged']=all(sha(v)==h for v,h in inputs.items())
        report['LogsUnchanged']=all(sha(v['Log'])==v['Sha256'] for v in rows)
        report['ProductsUnchanged']=all(sha(v['Path'])==v['Sha256'] for v in products)
        assert report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged']
        report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise

if __name__=='__main__':main()
