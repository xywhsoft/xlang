"""Fast same-source finite Channel public/IR/lifetime gate before ordinary builds.
No provider override. The optional native notification barrier is explicitly
source-only --wrap instrumentation, never normal-product or security evidence.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    assert os.name=='nt' and not out.exists();out.mkdir()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'];driver=Path(build['Driver'])
    assert sha(driver)==build['DriverSha256'].lower()
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    positives=('stdlib_channel_finite_future','stdlib_channel_finite_select','stdlib_channel_finite_ready',
        'stdlib_channel_finite_dynamic','stdlib_channel_direct_future','stdlib_channel_future')
    controls=('stdlib_concurrent_import_only','stdlib_channel_direct_shadow','stdlib_channel_finite_shadow','stdlib_channel_cases')
    private=('stdlib_channel_finite_unstarted',)
    negatives=('timeout_type','until_type','send_type','cancel_type','private','select_array','status_width')
    inputs={Path(__file__).resolve(),public/'build.json',driver,Path(shutil.which('gcc')),
        *[root/'test/cases'/(name+'.xl') for name in positives+controls+private],
        *[root/'test/cases'/('bad_channel_finite_'+name+'.xl') for name in negatives],
        root/'test/cases/xlir_channel_finite_contract.c',root/'test/cases/stdlib_channel_frame_quiescence.c',
        root/'test/cases/stdlib53_pruning_plan.c'}
    before={str(path):sha(path) for path in sorted(inputs)}
    report=dict(Complete=False,Inputs=before,Rows=[],Failures=[],Scope=__doc__,NormalProductAcceptance=False,
        NetworkTests=False,NetworkSecurityTests=False)
    env=dict(os.environ);env['PATH']=str(root/'release')+os.pathsep+env['PATH']
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,negative=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['Rows'].append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,Sha256=sha(log)));save()
        ok=(result.returncode!=0 if negative else result.returncode==0)
        if not ok:report['Failures'].append(name);save()
        return ok
    try:
        for name in positives+controls:
            source=root/'test/cases'/(name+'.xl');target=out/(name+'.json')
            if run(name+'.lowered',[driver,source,'ir-lowered',target]):
                ir=json.loads(target.read_text(encoding='utf8'))
                finite=name in positives[:4]
                assert ir['moduleDeadlineOwnershipAbi']==int(finite),(name,ir['moduleDeadlineOwnershipAbi'])
                if finite:assert ir['moduleContinuationAbi']==2
                assert all(t['nativeServicesRequired'] in (0,1,3) for t in ir['callTargets'])
                if name in positives:run(name+'.run',[driver,source,'run',out/(name+'.run')])
        for name in private:
            source=root/'test/cases'/(name+'.xl');target=out/(name+'.json')
            if run(name+'.lowered',[driver,source,'ir-lowered',target]):
                ir=json.loads(target.read_text(encoding='utf8'))
                assert ir['moduleDeadlineOwnershipAbi']==0 and ir['moduleContinuationAbi']==0
                run(name+'.run',[driver,source,'run',out/(name+'.run')])
        for name in negatives:
            target=out/('bad-'+name+'.c')
            run('bad-'+name,[driver,root/'test/cases'/('bad_channel_finite_'+name+'.xl'),'emit',target],True)
            assert not target.exists()
        cc=shutil.which('gcc');objects=[Path(row['Path']) for row in build['Objects']
            if Path(row['Source']).name!='xlir_public_source_driver.c']
        assert len(objects)==len(build['Objects'])-1
        for fixture,extra in (('xlir_channel_finite_contract',[]),('stdlib53_pruning_plan',[]),
                              ('stdlib_channel_frame_quiescence',['-Wl,--wrap=xrtChannelSubscribeResult'])):
            binary=out/(fixture+'.exe');response=out/(fixture+'.rsp')
            flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter',
                '-Wno-missing-field-initializers','-DXL_STATIC_EMBED','-O0',
                *['-I'+str(root/folder) for folder in ('.','release','release/include','lib/tcc')]]
            args=[*flags,root/'test/cases'/(fixture+'.c'),*objects,root/'release/xl.dll',
                '-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32',*extra,'-o',binary]
            response.write_text(''.join('"'+str(v).replace('\\','/').replace('"','\\"')+'"\n' for v in args),encoding='utf8')
            if run(fixture+'.build',[cc,'@'+str(response)]):
                if fixture=='stdlib53_pruning_plan':
                    for name in positives+controls+private:
                        if run(name+'.plan',[binary,root/'test/cases'/(name+'.xl')]):
                            plan=json.loads((out/(name+'.plan.log')).read_bytes())
                            files={Path(path).name for path in plan['Sources']}
                            finite=name in positives[:4]
                            if name not in private:assert ('xl6_deadline_service.c' in files)==finite,(name,files)
                            if name in ('stdlib_channel_finite_future','stdlib_channel_finite_ready','stdlib_channel_finite_dynamic'):
                                assert not {'xl6_channel_future.c','xl6_channel_select.c','xl6_channel_case.c','xl6_channel_case_array.c'}&files,(name,files)
                            report.setdefault('ActualPlans',{})[name]=plan
                else:
                    operands=[root/'test/cases'/(name+'.xl') for name in positives[:3]]+[
                        root/'test/cases/stdlib_channel_direct_future.xl'] if fixture.startswith('xlir_') else []
                    run(fixture+'.run',[binary,*operands])
        report['InputsUnchanged']=all(sha(path)==value for path,value in before.items())
        report['LogsUnchanged']=all(sha(out/(r['Name']+'.log'))==r['Sha256'] for r in report['Rows'])
        assert report['InputsUnchanged'] and report['LogsUnchanged'] and not report['Failures'],report['Failures']
        report['Complete']=True
    finally:save()
    print('Same-source finite Channel gate passed:',len(report['Rows']),'phases',flush=True)
if __name__=='__main__':main()
