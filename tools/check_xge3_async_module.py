"""Qualify independent async request/worker/callback code after module Close.

Uses a real native shared reference, not a synthetic ownership adapter.
Both empty and threaded/cyclic requests must keep code alive until publishers
stop, then allow checked collection and native image unload. CPU/local IO only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from full_compiler_qualification import verify_full_compiler


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','packages','stage-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--font-load',action='store_true',help='Also qualify owning Font requests with a generated valid font')
    parser.add_argument('--sound-load',action='store_true',help='Also qualify owning Sound requests with the private real no-device SDK')
    a=parser.parse_args();root,compiler,cohort,packages,stage,out=[v.resolve() for v in
        (a.root,a.compiler,a.compiler_report,a.packages,a.stage_report,a.out)]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    metadata=verify_full_compiler(cohort,compiler)
    delivery=json.loads(stage.read_text(encoding='utf8'))
    assert delivery.get('complete') and delivery.get('inputsUnchanged')
    assert all(sha(packages/name)==digest for name,digest in delivery['outputs'].items())
    private=packages/'_xge_native';manifest=json.loads((private/'xlang.package.json').read_text(encoding='utf8'))
    assert manifest['native']['linkage']=='dynamic'
    image=private/manifest['links'][0];assert sha(image)==manifest['native']['sha256']
    sdk=compiler.parent/'include';assert sdk.is_dir()
    host=root/'test/cases/foreign_xge3_async_module_host.c'
    case=root/'test/cases/foreign_xge3_async_module.xl';sidecar=case.with_suffix('.h')
    cases=[('image',case)]
    font=root/'lib/bindings/xge/examples/data/font_fixture.ttf'
    if a.font_load:cases.append(('font',root/'test/cases/foreign_xge3_font_load_module.xl'))
    sound=root/'lib/bindings/xge/examples/data/tone.wav'
    if a.sound_load:cases.append(('sound',root/'test/cases/foreign_xge3_sound_load_module.xl'))
    cc=Path(shutil.which('gcc'));assert cc.is_file()
    library=compiler.parent/('xlc.dll' if os.name=='nt' else 'libxlc.so')
    files=[Path(__file__).resolve(),Path(__file__).with_name('full_compiler_qualification.py'),Path(metadata['sourceFreeze']),cohort,stage,cc,host,case,sidecar,library]
    if a.font_load:files.extend([font,cases[1][1]])
    if a.sound_load:files.extend([sound,root/'test/cases/foreign_xge3_sound_load_module.xl'])
    files.extend(p for p in sdk.rglob('*') if p.is_file())
    files.extend(p for p in packages.rglob('*') if p.is_file())
    inputs={str(p):sha(p) for p in files}
    out.mkdir(parents=True,exist_ok=False)
    if os.name=='nt':shutil.copy2(library,out/library.name)
    env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages))
    if os.name!='nt':env['LD_LIBRARY_PATH']=str(compiler.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,
        networkTests=False,graphicsDevice=False,inputs=inputs,rows=[])
    if a.font_load:report.update(explicitFontResultResetBeforeShutdown=True,moduleGlobalResourceShutdownGate=False)
    if a.sound_load:report.update(explicitSoundResultResetBeforeShutdown=True,moduleGlobalResourceShutdownGate=False,privateNoDeviceAudioSetup=True,audioDevice=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));r=subprocess.run(command,cwd=compiler.parent,env=env,capture_output=True,timeout=90)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert not r.returncode,log
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type',
        '-I'+str(root),'-I'+str(compiler.parent),'-I'+str(sdk),'-I'+str(root/'test/cases'),
        '-I'+str(private/'include'),'-include',private/'include/xge_build_config.h']
    systems=['-DXRT_USE_SHARED'] if os.name=='nt' else ['-Wl,-rpath,'+str(compiler.parent),'-pthread','-ldl','-lm']
    for kind,case in cases:
        extra=['-DXGE_TEST_FONT_LOAD=1'] if kind=='font' else ['-DXGE_TEST_SOUND_LOAD=1'] if kind=='sound' else []
        for optimization in ('O0','O2'):
            tag=kind+'-'+optimization
            binary=out/('host-'+tag+('.exe' if os.name=='nt' else ''))
            run(tag+'-build',[cc,*flags,*extra,'-'+optimization,host,library,*systems,'-o',binary])
            for callbacks in ('0','1'):
                temp=out/(tag+'-tmp-'+callbacks);temp.mkdir()
                run(tag+'-callbacks-'+callbacks,[binary,case,compiler.parent,sdk,temp,image,callbacks,root,*([font] if kind=='font' else [sound] if kind=='sound' else [])])
    report.update(passed=True,inputsUnchanged=all(sha(p)==digest for p,digest in inputs.items()))
    assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
