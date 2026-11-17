"""Qualify one matched hosted/static debug delivery candidate.

Compose the existing cold and live graphics gates with the readable lesson
and actual normal-SDK feature rejection. Keep current source, compiler,
package and log identities; do not install release files. This does not
certify concurrent global calls, arbitrary module cycles or other backends.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from full_compiler_qualification import verify_full_compiler
from stage_xge3_replay_contracts import verify_replay_package


def verify_stage(packages,receipt):
    """Check the complete staged population, not only files used by a case."""
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    d=json.loads(receipt.read_text(encoding='utf8'))
    if not d.get('complete') or not d.get('passed',d.get('inputsUnchanged')) or d.get('shippingDelivery') is not False:
        raise ValueError('Not a complete local stage receipt')
    inputs,outputs=d.get('inputs'),d.get('outputs')
    if not isinstance(inputs,dict) or not inputs or not isinstance(outputs,dict) or not outputs:
        raise ValueError('Stage receipt has no complete input/output pins')
    for name,h in inputs.items():
        if not Path(name).is_file() or sha(name)!=h:raise ValueError('Stage input changed: '+name)
    present={p.relative_to(packages).as_posix():p for p in packages.rglob('*') if p.is_file()}
    expected={name.replace('\\','/'):h for name,h in outputs.items()}
    if len(expected)!=len(outputs) or set(present)!=set(expected):raise ValueError('Stage file population changed')
    for name,h in expected.items():
        if sha(present[name])!=h:raise ValueError('Stage output changed: '+name)
    return [receipt,*map(Path,inputs),*present.values()]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','packages','stage-report','static-packages',
                 'static-stage-report','normal-packages','normal-static-packages','normal-replay-report','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    root,cc,build,dynamic,stage,static,static_stage,normal,normal_static,normal_receipt,out=[v.resolve() for v in
        (a.root,a.compiler,a.compiler_report,a.packages,a.stage_report,a.static_packages,a.static_stage_report,
         a.normal_packages,a.normal_static_packages,a.normal_replay_report,a.out)]
    if out.is_relative_to(root/'release') or out.is_relative_to(dynamic) or out.is_relative_to(static):
        p.error('Keep qualification outputs outside runtime/package inputs')
    cohort=verify_full_compiler(build,cc);win=os.name=='nt';sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    names=('test/cases/foreign_xge3_debug_live.xl','test/cases/foreign_xge3_debug_live_native.c',
        'test/cases/foreign_xge3_debug_live.h','test/cases/foreign_xge3_debug_capabilities.xl',
        'lib/bindings/xge/debug/examples/capabilities.xl','lib/bindings/xge/debug/examples/live_stats.xl')
    tools_dir=Path(__file__).resolve().parent
    files=[Path(__file__).resolve(),tools_dir/'full_compiler_qualification.py',tools_dir/'foreign_compiler_cohort.py',
        tools_dir/'check_xge3_debug_live.py',tools_dir/'check_xge3_debug_capabilities.py',cc,build,
        *[root/n for n in names]]
    for pkg,receipt in ((dynamic,stage),(static,static_stage)):files.extend(verify_stage(pkg,receipt))
    for pkg in (normal,normal_static):files.extend(verify_replay_package(pkg,normal_receipt))
    files.extend(cc.parent/n for n in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so')))
    objdump=Path(shutil.which('objdump')).resolve();files.append(objdump)
    inputs={str(f):sha(f) for f in files};out.mkdir(parents=True,exist_ok=False)
    fixture=out/'fixture'
    for name in names:
        dest=fixture/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/name,dest)
    frozen={str(f):sha(f) for f in fixture.rglob('*') if f.is_file()}
    report=dict(complete=False,passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,
        releaseInstalled=False,networkTests=False,gpuTests=True,cpuEquivalenceClaim=False,inputs=inputs,
        frozenFixtures=frozen,rows=[],childGates=[],programs=[])
    env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
    env.update(XLANG_PACKAGE_PATH=str(dynamic),XL6_SOURCE_NATIVE_ROOT=cohort['source'],TMP=str(out),TEMP=str(out),
        TMPDIR=str(out),LIBGL_ALWAYS_SOFTWARE='1',PYTHONUTF8='1',PYTHONDONTWRITEBYTECODE='1')
    variable='PATH' if win else 'LD_LIBRARY_PATH'
    env[variable]=os.pathsep.join((str(dynamic/'_xge_native/native'),str(cc.parent),env.get(variable,'')))
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');os.replace(tmp,out/'results.json')
    def run(name,command,window=False,rejected=False):
        command=list(map(str,command))
        if window and not win:command=['xvfb-run','-a',*command]
        started=time.monotonic();z=subprocess.run(command,cwd=fixture,env=env,capture_output=True,timeout=600)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,expectedRejection=rejected,
            seconds=time.monotonic()-started,logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert bool(z.returncode)==rejected,log
        return data
    def imports(name,program,hosted):
        data=run(name+'-imports',[objdump,'-p',program])
        parsed=re.findall(rb'DLL Name:\s*(\S+)',data) if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        lowered={n.lower() for n in parsed}
        assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lowered)
        assert any(b'xge3_hostdbg' in n for n in lowered)==hosted
        report['programs'].append(dict(path=str(program),bytes=program.stat().st_size,sha256=sha(program),
            imports=[n.decode() for n in parsed]));save()
    def child(name,command,count):
        run(name,command)
        path=out/name/'results.json';d=json.loads(path.read_text(encoding='utf8'))
        assert d['complete'] and d['passed'] and d['inputsUnchanged'] and len(d['rows'])==count,path
        for filename,h in d['inputs'].items():assert sha(filename)==h,filename
        for row in d['rows']:
            assert not row['exitCode']
            assert sha(path.parent/(row['name']+'.log'))==row['logSha256']
        for program in d.get('programs',[]):
            assert sha(program['path'])==program['sha256'] and Path(program['path']).stat().st_size==program['bytes']
            if 'staticNative' in program:
                assert any('xge3_hostdbg' in n.lower() for n in program['imports'])!=program['staticNative']
        report['childGates'].append(dict(path=str(path),sha256=sha(path),commands=count));save()
    try:
        common=['--root',fixture,'--compiler',cc,'--compiler-report',build]
        child('live',[sys.executable,'-B',tools_dir/'check_xge3_debug_live.py',*common,'--packages',dynamic,
            '--stage-report',stage,'--static-packages',static,'--static-stage-report',static_stage,'--out',out/'live'],18)
        child('cold',[sys.executable,'-B',tools_dir/'check_xge3_debug_capabilities.py',*common,
            '--dynamic-packages',dynamic,'--static-packages',static,'--out',out/'cold'],19)
        lesson=fixture/'lib/bindings/xge/debug/examples/live_stats.xl'
        for kind,selected in (('host',dynamic),('static',static)):
            env['XLANG_PACKAGE_PATH']=str(selected)
            def lesson_run(name,cmd):
                data=run(name,cmd,window=True)
                match=re.search('纹理数：(\\d+)，纹理字节：(\\d+)'.encode(),data)
                assert match and int(match[1])>0 and int(match[2])>=16,out/(name+'.log')
            if kind=='host':lesson_run('lesson-jit',[cc,'run',lesson])
            binary=out/('lesson-'+kind+('.exe' if win else '.out'))
            run('lesson-'+kind+'-build',[cc,'build',lesson,'-o',binary])
            lesson_run('lesson-'+kind+'-run',[binary]);imports('lesson-'+kind,binary,kind=='host')
        debug_only=out/'debug-only';shutil.copytree(dynamic/'xgeDebug',debug_only/'xgeDebug')
        probe=out/'requires-debug.xl';probe.write_text('import xge\nimport xgeDebug\nauto stats=xgeDebug.debugGetStats()\n',encoding='utf8')
        for kind,selected in (('host',normal),('static',normal_static)):
            env['XLANG_PACKAGE_PATH']=str(debug_only)+os.pathsep+str(selected)
            for operation in ('cgen','build'):
                artifact=out/('rejected-'+kind+('.c' if operation=='cgen' else '.exe' if win else '.out'))
                command=[cc,operation,probe,*(['--emit',artifact] if operation=='cgen' else ['-o',artifact])]
                data=run('reject-'+kind+'-'+operation,command,rejected=True)
                assert b'XGE_HAS_DEBUGMODE' in data and b'xgeDebug' in data and not artifact.exists()
        verify_full_compiler(build,cc)
        for pkg,receipt in ((dynamic,stage),(static,static_stage)):verify_stage(pkg,receipt)
        for pkg in (normal,normal_static):verify_replay_package(pkg,normal_receipt)
        report.update(complete=True,inputsUnchanged=all(sha(f)==h for f,h in {**inputs,**frozen}.items()),
            leafCommands=sum(c['commands'] for c in report['childGates'])+len(report['rows'])-len(report['childGates']))
        assert report['leafCommands']==48
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except BaseException as error:report.update(complete=True,passed=False,error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
