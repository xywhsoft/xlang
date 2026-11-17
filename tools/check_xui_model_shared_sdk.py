"""Qualify Shared value model APIs against actual hosted/static O2 SDKs.

CPU clients use the one full compiler XRT and no production unit replacements.
Check native reference identity, original Clone isolation, history snapshots,
checked lifetimes, OOM/Busy and zero heap operations in warm Shared calls.
The proposed generic language snapshot declaration is not qualified here.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from full_compiler_qualification import verify_full_compiler
from check_xui_flow_busy_sdk import DRIVER


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('native-root','native-source','compiler','compiler-report','sdk-build','source-gate','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();native,source,cc,build,sdk,gate,out=[v.resolve() for v in
        (a.native_root,a.native_source,a.compiler,a.compiler_report,a.sdk_build,a.source_gate,a.out)]
    win=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    verify_full_compiler(build,cc)
    sdk_report=json.loads((sdk/'results.json').read_text());assert sdk_report['complete'] and sdk_report['passed']
    assert Path(sdk_report['source']).resolve()==source
    frozen=json.loads(Path(sdk_report['freeze']).read_text());assert sha(sdk_report['freeze'])==sdk_report['freezeSha256']
    overlays=frozen['overlays'] if 'overlays' in frozen else frozen['overrides']
    for name,h in overlays.items():assert sha(source/name)==h and sha(native/name)==h
    g=json.loads(gate.read_text());assert g['complete'] and g['passed'] and g['inputsUnchanged']
    for name,h in g['inputs'].items():assert sha(name)==h
    host=sdk/'host';static=sdk/'static'
    h=json.loads((host/'build-report.json').read_text());s=json.loads((static/'build-report.json').read_text())
    assert h['linked'] and h['xrt_mode']=='host' and s['passed'] and s['xrt_mode']=='direct-external'
    assert len(h['compiled'])==164 and len(s['compiled'])==166 and not h['reused'] and not s['reused']
    runtime=cc.parent/('xlc.dll' if win else 'libxlc.so')
    archive=static/s['archive'];assert sha(archive)==s['archive_sha256']
    library=host/(h['import_library'] if win else h['runtime_image'])
    cases=[native/'tests/xui'/n for n in ('xui_model_shared_values_test.c','xui_workflow_value_reentry_test.c',
        'xui_workflow_owned_test.c','xui_workflow_xson_test.c')]
    gcc=Path(shutil.which('gcc')).resolve();objdump=Path(shutil.which('objdump')).resolve()
    files=[Path(__file__).resolve(),Path(__file__).with_name('check_xui_flow_busy_sdk.py'),
        Path(__file__).with_name('full_compiler_qualification.py'),build,cc,runtime,gate,sdk/'results.json',
        Path(sdk_report['freeze']),host/'build-report.json',static/'build-report.json',archive,library,
        host/h['runtime_image'],gcc,objdump,*cases,*[native/'tests/xui'/n for n in
        ('xui_workflow_busy_probes.inc','xui_flow_graph_busy_probes.inc')]]
    files.extend(cc.parent/n for n in (('xl.dll',) if win else ('libxl.so',)))
    for folder in (source/'include',source/'extlibs/xrt',host/'host-xrt',cc.parent/'include'):
        files.extend(p for p in folder.rglob('*') if p.is_file())
    files.extend([host/'xge_build_config.h',static/'xge_build_config.h',static/'xge_static_xrt_contract.h'])
    inputs={str(p):sha(p) for p in files};out.mkdir(parents=True,exist_ok=False)
    (out/'build').mkdir() # Existing native XSON fixture writes build/workflow_roundtrip.xson.
    report=dict(complete=False,passed=False,scope=__doc__,genericSnapshotDeclarationImplemented=False,inputs=inputs,rows=[],programs=[],
        freshNativeUnits=330,productionObjectOverrides=False,networkTests=False,shippingDelivery=False)
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');os.replace(tmp,out/'results.json')
    def run(name,command):
        env=dict(os.environ,TMP=str(out),TEMP=str(out),TMPDIR=str(out));key='PATH' if win else 'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(host),str(cc.parent),env.get(key,'')))
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=out,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');data=z.stdout+z.stderr;log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,
            seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert z.returncode==0,log;return data
    driver=out/'driver.c';driver.write_text(DRIVER,encoding='utf8')
    try:
        for case in cases:
            body=out/case.name;text=case.read_text(encoding='utf8');assert text.count('int main(void)')==1
            body.write_text(text.replace('int main(void)','int fixtureNativeMain(void)'),encoding='utf8')
            for kind,folder,lib in (('host',host,library),('static',static,archive)):
                flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function',
                    '-Wno-cast-function-type','-DXGE_XRT_HOST=1','-include',folder/'xge_build_config.h']
                flags+=['-I'+str(p) for p in (source,source/'include',native/'tests/xui',host/'host-xrt',folder,cc.parent/'include')]
                if kind=='static':flags+=['-DXGE_TEST_STATIC=1','-DXL6_XGE_STATIC=1']
                elif win:flags+=['-DXGE_DLL=1','-DXUI_DLL=1']
                systems=['-l'+n for n in s['system_libraries']] if kind=='static' else []
                if not win:systems+=['-Wl,-rpath,'+str(host),'-Wl,-rpath,'+str(cc.parent),'-ldl','-lm','-pthread']
                for opt in ('O0','O2'):
                    name=case.stem+'-'+kind+'-'+opt;exe=out/(name+('.exe' if win else ''))
                    run(name+'-build',[gcc,*flags,'-'+opt,body,driver,lib,runtime,*systems,'-o',exe])
                    run(name+'-run',[exe])
                    data=run(name+'-imports',[objdump,'-p',exe])
                    assert (b'xge3_host' in data)==(kind=='host')
                    report['programs'].append(dict(path=str(exe),sha256=sha(exe)));save()
        verify_full_compiler(build,cc)
        report.update(complete=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except BaseException as error:report.update(complete=True,error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
