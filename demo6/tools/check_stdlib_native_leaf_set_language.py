"""Actual public source/JIT and strict generated C/native key argument smoke.

This test links source-built resident objects, not a pruned shipping artifact.
Native keys enter through var; direct source SlotMapKey/frames are not claimed.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    cc=shutil.which('gcc'); assert cc and not out.exists()
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig')); driver=Path(build['Driver'])
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    source=root/'test/cases/stdlib_native_leaf_set_language.xl'; host=root/'test/cases/stdlib_native_leaf_set_language_host.c'
    memory=root/'test/support/module_product_memory.c'
    files={Path(__file__),manifest,driver,runtime,source,host,memory,Path(cc)}
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]; out.mkdir(); rows=[]; products=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        PrunedShippingArtifact=False,LanguageSlotMapKeyCutover=False,Full53Acceptance=False,Scope=__doc__)
    env=dict(os.environ)
    if os.name!='nt': env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    else: shutil.copy2(runtime,out/'xl.dll')
    def run(name,command):
        r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log'); log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(Name=name,Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
        assert not r.returncode,(name,(r.stdout+r.stderr)[-8000:])
    try:
        generated=out/'set.c'; repeated=out/'set-repeat.c'
        run('generate',[driver,source,'cgen-default',generated]); run('repeat',[driver,source,'cgen-default',repeated])
        assert sha(generated)==sha(repeated)
        products.extend(dict(Path=str(f),Sha256=sha(f)) for f in (generated,repeated))
        run('public-jit',[driver,source,'run',out/'jit'])
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
            ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
        flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime',
            '-I'+str(public/'include'),'-DXL_STATIC_EMBED','-Wno-unused-function','-Wno-unused-variable',
            '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
            '-Wno-missing-field-initializers','-Wno-pointer-sign']
        links = ['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        if os.name!='nt': flags+=['-D_GNU_SOURCE']
        memory_object=out/'memory.o'; run('memory-build',[cc,*flags,'-O0','-c',memory,'-o',memory_object])
        products.append(dict(Path=str(memory_object),Sha256=sha(memory_object)))
        for opt in ('O0','O2'):
            product=out/(opt+('.exe' if os.name=='nt' else ''))
            run(opt+'-build',[cc,*flags,'-'+opt,'-include',generated,host,memory_object,*objects,runtime,*links,'-o',product])
            products.append(dict(Path=str(product),Sha256=sha(product))); run(opt+'-native-keys',[product])
        report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    assert report['Complete'] and report['InputsUnchanged']; print('Actual generated Value Set: 8 phases, public JIT and O0/O2 native key arguments')
if __name__=='__main__': main()
