"""Qualify native-feature manifest parsing and closure with real XRT at O0/O2.

This is the native parser/validator prerequisite, not full compiler, compiled
interface or runtime SDK/IDE acceptance. No network or installed files change.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,shutil,subprocess,time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','runtime','out'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();root,runtime,out=[v.resolve()for v in(a.root,a.runtime,a.out)]
    out.mkdir(parents=True,exist_ok=False)
    cc=Path(shutil.which('gcc')).resolve();sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    paths=[root/p for p in ('test/cases/package_native_features.c','src/compile/xl6_import_native_features.inc',
        'src/compile/xl6_imports.h','src/compile/xl6_imports.c','src/package/xl6_package.h','src/package/xl6_package.c',
        'src/package/xl6_package_native_variants.inc','src/package/xl6_package_native_link.inc','src/package/xl6_package_c_source_groups.inc',
        'src/package/xl6_compiled_package.h','src/package/xl6_compiled_interface.inc','src/package/xl6_compiled_native_features.inc',
        'src/compile/xl6_import_compiled_features.inc',
        'src/diag/xl6_diag.c','src/diag/xl6_diag.h','src/compile/xl6_source_report.c',
        'src/common/xl6_console.c','src/common/xl6_console.h','src/common/xl6_output.c','src/common/xl6_output.h')]
    files=[Path(__file__).resolve(),runtime,cc,*paths]
    files.extend(p for p in (runtime.parent/'include').rglob('*.h'))
    inputs={str(p):sha(p)for p in files};report=dict(complete=False,passed=False,scope=__doc__,
        fullCompilerQualification=False,shippingDelivery=False,inputs=inputs,rows=[])
    def save():
        p=out/'results.json.tmp';p.write_text(json.dumps(report,indent=2)+'\n');p.replace(out/'results.json')
    def run(name,command):
        command=list(map(str,command));started=time.monotonic()
        env=os.environ.copy();key='PATH'if os.name=='nt'else'LD_LIBRARY_PATH';env[key]=str(runtime.parent)+os.pathsep+env.get(key,'')
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=result.returncode,seconds=time.monotonic()-started,logSha256=sha(log)))
        save();print(name,result.returncode,flush=True);assert not result.returncode,log
    base=dict(format='xlang-package-1',name='feature_test',version='1',abiVersion='1',artifact={})
    cases=[('absent',{},[],[]),('empty',dict(requiresNativeFeatures=[],native=dict(features=[])),[],[]),
           ('both',dict(requiresNativeFeatures=['GPU3D','org.debug:enabled-1'],native=dict(features=['GPU3D','org.debug:enabled-1'])),['GPU3D','org.debug:enabled-1'],['GPU3D','org.debug:enabled-1'])]
    for scope in ('required','provided'):
        for label,value in [('null',None),('bool',True),('number',1),('text','GPU3D'),('object',{}),
            ('item-null',[None]),('item-bool',[True]),('empty-key',['']),('space',['GPU 3D']),('nul',['GPU3D\0off']),
            ('duplicate',['GPU3D','GPU3D']),('unicode',['三维']),('punctuation',['A/B'])]:
            patch=dict(requiresNativeFeatures=value)if scope=='required'else dict(native=dict(features=value))
            cases.append((scope+'-'+label,patch,None,None))
    manifests=[]
    for name,patch,required,provided in cases:
        p=out/(name+'.json');p.write_text(json.dumps(dict(base,**patch))+'\n');manifests.append((name,p,required,provided))
    dynamic=dict(base,native=dict(linkage='dynamic',features=['JIT'],variants=dict(static='selected.json')),requiresNativeFeatures=['GPU3D','DEBUG'])
    primary=out/'base.json';primary.write_text(json.dumps(dynamic)+'\n');selected=out/'selected.json'
    ci=['-I'+str(p)for p in(root,runtime.parent,runtime.parent/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*ci]
    for opt in ('O0','O2'):
        exe=out/(opt+('.exe'if os.name=='nt'else''))
        command=[cc,*common,'-'+opt,root/'test/cases/package_native_features.c',root/'src/package/xl6_package.c',
                 root/'src/compile/xl6_source_report.c',root/'src/diag/xl6_diag.c',
                 root/'src/common/xl6_console.c',root/'src/common/xl6_output.c',runtime,'-Wl,--gc-sections','-o',exe]
        if os.name!='nt':command+=['-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
        run(opt+'-build',command)
        run(opt+'-import-unit-syntax',[cc,*common,'-'+opt,'-fsyntax-only',root/'src/compile/xl6_imports.c'])
        run(opt+'-closure',[exe])
        for name,path,required,provided in manifests:
            args=[exe,path,1 if required is not None else 0,len(required or [])]
            if required is not None:args+=[len(provided),*required,*provided]
            run(opt+'-'+name,args)
        for label,requirements,expected in [('same',['GPU3D','DEBUG'],1),('reordered',['DEBUG','GPU3D'],1),
            ('dropped',[], -1),('missing',['GPU3D'],-1),('added',['GPU3D','DEBUG','OTHER'],-1)]:
            candidate=copy.deepcopy(dynamic);candidate['native']=dict(linkage='static',features=['STATIC']);candidate['requiresNativeFeatures']=requirements
            selected.write_text(json.dumps(candidate)+'\n');run(opt+'-variant-'+label,[exe,'--variant',primary,expected,'STATIC'])
    report.update(complete=True,inputsUnchanged=all(sha(p)==v for p,v in inputs.items()));report['passed']=report['inputsUnchanged'];save();assert report['passed']


if __name__=='__main__':main()
