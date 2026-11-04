"""CPU-only readonly fixed C-array reference gate over a pinned frontend overlay.

No Native engine, graphics, network or device code is exercised.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','overlay','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,overlay,out=[p.resolve() for p in (args.root,args.overlay,args.out)]
    description=json.loads(overlay.read_text());assert description['passed'] and not description['fullRebuild']
    cohort=Path(description['sourceRoot']);basePath=Path(description['parentBuild']);base=json.loads(basePath.read_text())
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    assert base['Complete'] and base['InputsUnchanged'] and sha(basePath)==description['parentBuildSha256']
    replacements={r['ParentObject']:r for r in description['Objects']};objects=[];inputs={str(overlay):sha(overlay)}
    for original in base['Objects']:
        row=replacements.get(original['Path'],original);assert sha(row['Path'])==row['Sha256'].lower();inputs[row['Path']]=sha(row['Path'])
        if Path(original['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c'):objects.append(row['Path'])
    driver=Path(description['Driver']);runtime=Path(description['RuntimeLibrary']);assert sha(driver)==description['DriverSha256']
    fixture=root/'test/cases/foreign_fixed_ref.xl';native=root/'test/cases/foreign_region_native.c';host=root/'test/cases/foreign_decl_module_host.c'
    pinned=[driver,runtime,fixture,native,host,root/'test/cases/foreign_region.h',Path(__file__).resolve()]
    irFixture=root/'test/cases/foreign_fixed_ref_ir.c'
    if irFixture.exists():pinned.append(irFixture)
    inputs.update((str(p),sha(p)) for p in pinned);out.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(root/'tools'));from compiler_response import command_with_response
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=False,overlay=str(overlay),inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,accepted=True):
        command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
        started=time.monotonic();result=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,exitCode=result.returncode,accepted=accepted,logSha256=sha(log),seconds=round(time.monotonic()-started,3)));save()
        print(name,result.returncode,flush=True);assert (result.returncode==0)==accepted,log
    generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('header',[driver,fixture,'emit',out/'public'])
    text=generated.read_text(encoding='utf-8-sig');assert 'const RegionVertex*' in text and ').Elements' in text
    headers=out/'fixture-inputs/test/cases';headers.mkdir(parents=True);shutil.copy2(root/'test/cases/foreign_region.h',headers/'foreign_region.h')
    sdk=Path(base.get('SdkDirectory') or str(Path(base['Objects'][0]['Source']).parents[2]/'release'/'win_x64'))
    includes=['-I'+str(p) for p in (out/'fixture-inputs',cohort,basePath.parent/'include',sdk,sdk/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    run('header-C',['gcc',*common,'-x','c','-fsyntax-only',out/'public.h'])
    for standard in ('c++11','c++17'):run('header-'+standard,['g++','-std='+standard,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
    warnings=['-Wno-'+s for s in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for level in ('O0','O2'):
        exe=out/(level+('.exe' if os.name=='nt' else '.out'))
        run(level+'-build',['gcc',*common,*warnings,'-'+level,generated,native,host,*objects,runtime,*systems,'-o',exe]);run(level+'-run',[exe])
        if irFixture.exists():
            gate=out/('ir-'+level+('.exe' if os.name=='nt' else '.out'))
            run('ir-'+level+'-build',['gcc',*common,'-'+level,irFixture,*objects,runtime,*systems,'-o',gate]);run('ir-'+level+'-run',[gate,fixture])
    original=fixture.read_text()
    negatives={
        'dynamic':'func sum(array<Vertex> values)',
        'point':'func sum(point values)',
        'nullable':'func sum(Vertex[4]? values)',
        'zero':'func sum(Vertex[0] values)',
        'overflow':'func sum(Vertex[4294967296] values)',
        'nested':'func sum(Vertex[4][4] values)',
        'managed-element':'func sum(string[4] values)',
        'pointer-element':'func sum(point[4] values)',
    }
    for name,value in negatives.items():
        code=original.replace('func sum(Vertex[4] values)',value)
        path=out/(name+'.xl');path.write_text(code,encoding='utf8');target=out/(name+'.c');run('reject-'+name,[driver,path,'cgen-default',target],False);assert not target.exists()
    for name,old,new in [
        ('constness','(*sumVertices)(RegionRoot*,const RegionVertex*)','(*sumVertices)(RegionRoot*,RegionVertex*)'),
        ('pointer-to-array','(*sumVertices)(RegionRoot*,const RegionVertex*)','(*sumVertices)(RegionRoot*,const RegionVertex(*)[4])'),
        ('element','(*sumVertices)(RegionRoot*,const RegionVertex*)','(*sumVertices)(RegionRoot*,const int*)')]:
        bad=out/name/'test/cases';bad.mkdir(parents=True)
        header=(root/'test/cases/foreign_region.h').read_text();assert old in header;(bad/'foreign_region.h').write_text(header.replace(old,new))
        run('reject-native-'+name,['gcc','-I'+str(out/name),*common,*warnings,'-fsyntax-only',generated],False)
    report['inputsUnchanged']=all(sha(p)==s for p,s in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
