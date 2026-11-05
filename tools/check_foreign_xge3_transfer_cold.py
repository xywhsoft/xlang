"""CPU-only cold adapter acceptance with real code leases and service factories.

Native Run/drag/clipboard publication is replaced by typed synchronous fakes.
No OS, OLE, window, network or device operation runs. Native snapshot and COM
capsule implementations have separate acceptance; this does not certify them.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','native','out'):p.add_argument('--'+name,type=Path,required=True)
    add_build_arguments(p);a=p.parse_args();a.root=a.root.resolve();a.native=a.native.resolve();out=a.out.resolve();out.mkdir(exist_ok=False)
    b=load_build(a.build,a.overlay);source=b['cohort'];runtime=b['runtime'];sdk=Path(b['base'].get('RuntimeDirectory') or b['base'].get('SdkDirectory'))
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    inputs=dict(b['inputs'])
    for relative in ('test/cases/foreign_xge3_transfer_cold.c','lib/bindings/_xge_native/src/transfer.c','lib/bindings/_xge_native/include/xge3_runtime.h','src/runtime/xl6_native_com.h'):
        inputs[str(a.root/relative)]=sha(a.root/relative)
    for relative in ('include/xge.h','include/xui.h','include/xnative_transfer.h'):
        inputs[str(a.native/relative)]=sha(a.native/relative)
    includes=out/'include/__xl_runtime';includes.mkdir(parents=True)
    # This is a cold boundary test with four typed Native fakes, not a full
    # engine SDK. A zero fingerprint cannot pass production bootstrap.
    (includes.parent/'xge_build_config.h').write_text('/* CPU-only: no matching engine delivery claimed. */\n',encoding='utf-8')
    for header in (source/'src/runtime').glob('*.h'):
        shutil.copy2(header,includes/header.name);inputs[str(header)]=sha(header)
    shutil.copy2(a.root/'src/runtime/xl6_native_com.h',includes/'xl6_native_com.h')
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    env=dict(os.environ)
    if os.name!='nt':env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,shippingDelivery=False,scope=__doc__,overlay=b['overlay'],inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,args):
        actual=command_with_response(list(map(str,args)),'gcc',out/(name+'.rsp'));start=time.monotonic();result=subprocess.run(actual,cwd=source,env=env,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr);report['rows'].append(dict(name=name,command=actual,exitCode=result.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save();print(name,result.returncode,flush=True);assert not result.returncode,log
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*['-I'+str(x) for x in (out/'include',a.root,a.root/'lib/bindings/_xge_native/include',a.native/'include',a.native,source,sdk,sdk/'include')]]
    system=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-luuid'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    try:
        for opt in ('O0','O2'):
            target=out/(opt+('.exe'if os.name=='nt'else''));run(opt+'-build',['gcc',*flags,'-'+opt,a.root/'test/cases/foreign_xge3_transfer_cold.c',*b['objects'],runtime,*system,'-o',target]);run(opt+'-run',[target])
        report.update(complete=True,passed=True)
    except Exception as error:report['error']=str(error);raise
    finally:
        report['inputsUnchanged']=all(sha(f)==expected for f,expected in inputs.items());save()
if __name__=='__main__':main()
