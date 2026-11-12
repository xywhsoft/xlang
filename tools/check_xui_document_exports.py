"""Source-compiler Document string/bytes, JIT and independent native artifacts.

Uses matching hosted/static XGE SDKs. This source preflight does not certify a
rebuilt shipping compiler/VFS or install final SDK/IDE products.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','source','public','sdk','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--resume',type=Path,help='Reuse only verified successful emit/JIT evidence from this exact cohort')
    a=p.parse_args();root,source,public,sdk,out=[v.resolve()for v in(a.root,a.source,a.public,a.sdk,a.out)]
    out.mkdir(parents=True,exist_ok=False);win=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    d=json.loads((public/'build.json').read_text());s=json.loads((sdk/'results.json').read_text());assert d['Complete']and d['InputsUnchanged']and s['complete']and s['passed']and s['inputsUnchanged']
    driver=Path(d['Driver']);assert sha(driver)==d['DriverSha256'];assert all(sha(v['Path'])==v['Sha256'].lower()for v in d['Inputs']+d['Headers']+d['Objects'])
    case=root/'test/cases/foreign_xui_document_exports.xl';example=root/'lib/bindings/xui/examples/document_exports.xl';inputs={str(v):sha(v)for v in(driver,case,example,public/'build.json',sdk/'results.json',Path(__file__))}
    for kind in('host','static'):
        stage=json.loads((sdk/('packages-'+kind+'.json')).read_text())
        assert stage['complete']and stage.get('inputsUnchanged',True)
        for n,h in stage['outputs'].items():
            v=sdk/('packages-'+kind)/n;assert sha(v)==h;inputs[str(v)]=h
    prior=json.loads(a.resume.read_text())if a.resume else None
    if prior:
        assert all(sha(v)==h for v,h in prior['inputs'].items()if Path(v).resolve()!=Path(__file__).resolve())
        assert all(any(v['name']==n and v['exitCode']==0 for v in prior['rows'])for n in('emit','model-jit','example-jit'))
    report=dict(complete=False,passed=False,fullCompilerBuild=False,shippingDelivery=False,scope=__doc__,inputs=inputs,rows=[])
    if prior:report['reusedPreflight']=dict(report=str(a.resume.resolve()),sha256=sha(a.resume),phases=['emit','model-jit','example-jit'])
    save=lambda:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    save()
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'};env['XL6_SOURCE_NATIVE_ROOT']=str(source)
    if win:shutil.copy2(sdk/'host/xge3_host.dll',out/'xge3_host.dll')
    else:env['LD_LIBRARY_PATH']=str(sdk/'host')+':'+str(Path(d['RuntimeLibrary']).parent)+':'+env.get('LD_LIBRARY_PATH','')
    def run(name,cmd,refused=False):
        cmd=list(map(str,cmd));start=time.monotonic();z=subprocess.run(cmd,cwd=root,env=env,capture_output=True,timeout=600);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,command=cmd,exitCode=z.returncode,refused=refused,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert (z.returncode!=0)==refused,log
    try:
        env['XLANG_PACKAGE_PATH']=str(sdk/'packages-host')
        if prior:
            for n in('document.c','document.h'):shutil.copy2(a.resume.parent/n,out/n)
            for name in('emit','model-jit','example-jit'):
                row=next(v for v in prior['rows']if v['name']==name);log=Path(row.get('logPath',a.resume.parent/(name+'.log')));assert sha(log)==row['logSha256']
                report['rows'].append(dict(row,reused=True,logPath=str(log)))
        else:run('emit',[driver,case,'emit',out/'document'])
        text=(out/'document.c').read_text(encoding='utf-8-sig')
        for native in ('xuiDocumentSerialize','xuiDocumentExportHtml','xuiDocumentSnapshotCopyPlainText','xuiDocumentFragmentSerialize','xuiDocumentFragmentExportHtml','xuiDocumentDeserializeOwned'):
            assert native+'('in text,native
        assert 'xl6COutputTextTakeV1('in text and 'xl6COutputBytesTakeV1('in text
        if not prior:
            for name,path in [('model',case),('example',example)]:
                run(name+'-jit',[driver,path,'run',out/(name+'-jit')])
        for kind in('host','static'):
            env['XLANG_PACKAGE_PATH']=str(sdk/('packages-'+kind));prefix=out/kind
            binary=prefix.with_suffix('.exe')if win else prefix
            artifact=next((v for v in prior.get('artifacts',[])if v['linkage']==kind),None)if prior else None
            if artifact:
                assert sha(artifact['path'])==artifact['sha256'];shutil.copy2(artifact['path'],binary)
                row=next(v for v in prior['rows']if v['name']==kind+'-native-build'and v['exitCode']==0)
                log=Path(row.get('logPath',a.resume.parent/(kind+'-native-build.log')));assert sha(log)==row['logSha256']
                report['rows'].append(dict(row,reused=True,logPath=str(log)))
            else:run(kind+'-native-build',[driver,case,'exe',prefix])
            run(kind+'-native-execute',[binary])
            tool=shutil.which('objdump');assert tool;z=subprocess.check_output([tool,'-p',str(binary)]);(out/(kind+'-imports.log')).write_bytes(z)
            forbidden=[b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so']
            if kind=='static':forbidden +=[b'xge3_host.dll',b'libxge3_host.so']
            assert not any(n.lower()in z.lower()for n in forbidden)
            report.setdefault('artifacts',[]).append(dict(linkage=kind,path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size))
        cc=shutil.which('gcc');cpp=shutil.which('g++');assert cc and cpp
        runtime_sdk=Path(d.get('SdkDirectory',d.get('RuntimeDirectory','')))
        includes=['-I'+str(v)for v in(root,public/'include',public,runtime_sdk,runtime_sdk/'include',sdk/'packages-host/_xge_native/include')]
        for std,compiler,language in [('c11',cc,'c'),('c++11',cpp,'c++'),('c++17',cpp,'c++')]:
            run('header-'+std,[compiler,'-std='+std,'-Wall','-Wextra','-Werror',*includes,'-x',language,'-fsyntax-only',out/'document.h'])
        stale=out/'stale-sdk';stale.mkdir();(stale/'xge3_runtime.h').write_text('/* old SDK lacks the allocator promise */\n')
        guard=stale/'xge3_document_xrt.h';shutil.copy2(root/'lib/bindings/_xge_native/include/xge3_document_xrt.h',guard)
        run('reject-old-allocator-sdk',[cc,'-I'+str(stale),*includes,'-x','c','-fsyntax-only',guard],True)
        assert b'matched XRT-allocator XGE SDK'in(out/'reject-old-allocator-sdk.log').read_bytes()
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));assert report['inputsUnchanged']
    except Exception as e:report.update(complete=True,error=str(e));raise
    finally:save()

if __name__=='__main__':main()
