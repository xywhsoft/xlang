"""ParticleWorld declarations: real owned callbacks, scoped views and CPU simulation.

Requires complete compiler and matched O2 hosted/static SDKs, plus the native
collision ownership gate. Runs Windows/Linux JIT and dynamic/static AOT. No
GPU, network, final delivery or whole-engine zero-overhead claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    names=('root','compiler','compiler-report','native-root','engine','static-build',
           'native-report','dynamic-packages','static-packages','out')
    for name in names:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    root,compiler,build,native,engine,static_build,native_report,dynamic,static,out=[
        getattr(a,n.replace('-','_')).resolve()for n in names]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xge3_particle_world.xl'
    fixture=case.with_suffix('.c');example=root/'lib/bindings/xge/examples/particle_world.xl'
    h=json.loads((engine/'build-report.json').read_text(encoding='utf-8'))
    s=json.loads((static_build/'build-report.json').read_text(encoding='utf-8'))
    gate=json.loads(native_report.read_text(encoding='utf-8'))
    assert h['linked']and h['xrt_mode']=='host'and s['passed']and s['sourceUnchanged']and s['xrt_mode']=='direct-external'
    assert gate['complete']and gate['passed']and gate['inputsUnchanged']and gate['persistentOwnedCollisionQualified']
    assert len(gate['rows'])==16 and all(row['exitCode']==0 for row in gate['rows'])
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    assert all(sha(v)==digest for v,digest in gate['inputs'].items())
    image=engine/h['runtime_image'];archive=static_build/s['archive']
    runtime=compiler.parent/('xlc.dll'if win else'libxlc.so')
    files=[Path(__file__).resolve(),compiler,build,*extra,*tools.values(),case,fixture,case.with_suffix('.h'),example,
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        native_report,image,archive,engine/'build-report.json',static_build/'build-report.json',
        engine/'xge_build_config.h',static_build/'xge_build_config.h',*sorted((engine/'host-xrt').glob('*'))]
    files.extend(Path(v)for v in gate['inputs'])
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in(sdk,dynamic,static):files.extend(v for v in directory.rglob('*')if v.is_file())
    inputs={str(v):sha(v)for v in files}
    out.mkdir(parents=True,exist_ok=False);snapshots=out/'input-snapshots';snapshots.mkdir()
    for v in(Path(__file__).resolve(),case,fixture,case.with_suffix('.h'),example):shutil.copy2(v,snapshots/v.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        networkTests=False,cpuEquivalenceClaim=False,inputs=inputs,rows=[],programs=[],heap=[])
    def save():
        target=out/'results.json.tmp';target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(target,out/'results.json')
    def run(name,command,packages):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(runtime.parent),env.get(key,'')))
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log;return z.stdout+z.stderr
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    c_includes=['-I'+str(v)for v in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    try:
        for level in('O0','O2'):
            run('fixture-'+level,[tools['gcc'],'-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type',
                '-'+level,*c_includes,'-fsyntax-only',fixture],dynamic)
        for source in(case,example):
            name=source.stem;lesson=source==example;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            generated=c.read_text(encoding='utf-8-sig')
            symbols=['xgeParticlePlay','xgeParticleEmit','xgeParticleUpdate','xgeParticleStats','xgeParticleWorldSetCollisionOwned']
            if not lesson:symbols+=['xgeParticleVisit','xgeParticlePrewarm','xgeParticleWorldShareOwned','xgeParticleWorldReserve']
            for symbol in symbols:assert re.search(r'\b'+symbol+r'\(',generated),symbol
            if not lesson:
                assert 'xl6ForeignViewCloseV1('in generated
                assert 'xl6ForeignScopeViewDeriveV2('in generated
                assert 'xl6ForeignScopeViewMakeV2('in generated
                assert 'xl6NativeCallbackRuntimeV1Get()->Enter('in generated
                assert re.search(r'\(\(xge_particle_hit_t\*\)native_view_0.raw\)->tPosition =',generated)
                assert re.search(r'\(\(xge_particle_hit_t\*\)native_view_0.raw\)->fFraction =',generated)
                report['generatedC']=dict(path=str(c),sha256=sha(c),directCalls={symbol:len(re.findall(r'(?<![\w&])'+symbol+r'\(',generated))for symbol in symbols},
                    callbackLifetimeViews=True,ownedCollisionRegistration=True,zeroBridgeOverheadClaim=False)
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_includes,'-x',language,'-fsyntax-only',header],dynamic)
            for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':output=run(name+'-'+kind,[compiler,'run',source,*includes],packages)
                else:
                    binary=out/(name+'-'+kind+('.exe'if win else''))
                    run(name+'-'+kind+'-build',[compiler,'build',source,'-o',binary,*includes],packages)
                    output=run(name+'-'+kind+'-execute',[binary],packages)
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',binary],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={v.lower()for v in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host'in v for v in lower)==(kind=='dynamic')
                    report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode()for v in imports]))
                if lesson:assert b'Collision removed the particle'in output
                else:
                    assert b'ParticleWorld: direct simulation, owned collision, scoped hit/snapshot/definition, checked failure, sharing and thread passed'in output
                    counts=re.findall(rb'ParticleWorld heap kind=(\d+) alloc=(\d+) free=(\d+) realloc=(\d+)',output)
                    assert len(counts)==4 and {int(row[0])for row in counts}=={0,1,2,3}
                    assert all(tuple(map(int,row[1:]))==(0,0,0)for row in counts)
                    report['heap'].append(dict(mode=kind,rounds=1000,allocationPrefixes=96,
                        counts={row[0].decode():dict(zip(('alloc','free','realloc'),map(int,row[1:])))for row in counts}))
                save()
        report.update(complete=True,inputsUnchanged=all(sha(v)==digest for v,digest in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
