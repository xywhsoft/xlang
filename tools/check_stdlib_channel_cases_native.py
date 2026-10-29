"""Ordinary actual-product proof for owned Channel case/result providers.

This is a native foundation gate, not the unfinished public source-language
case/select cutover. No network, probing, security or discovered test suites.
"""
from pathlib import Path
import argparse,ctypes,hashlib,json,os,re,shutil,subprocess

def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True); parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--native-plan-report',type=Path)
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve(); task=Path(__file__).resolve().parents[1]
    win=os.name=='nt'; cc=shutil.which('gcc'); inspector=shutil.which('objdump' if win else 'readelf')
    assert cc and inspector and not out.exists()
    products=[root/'release'/name for name in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so'))]
    fixture=task/'test/cases/stdlib_channel_cases_native.c'; memory=task/'test/support/module_product_memory.c'
    plannerSource=task/'test/cases/stdlib_channel_select_native_plan.c'; pruned=task/'test/cases/stdlib_channel_cases_pruned.c'
    inputs={Path(__file__).resolve(),Path(cc).resolve(),Path(inspector).resolve(),fixture,memory,
            task/'test/support/module_product_memory.h',plannerSource,pruned,*products}
    for folder in ('src','lib','tools','release/include'):
        inputs.update(path for path in (root/folder).rglob('*') if path.is_file())
    donor=None
    if win:
        assert args.native_plan_report and args.native_plan_report.is_file()
        donor=json.loads(args.native_plan_report.read_text(encoding='utf8')); inputs.add(args.native_plan_report.resolve())
        assert donor['Complete'] and donor['InputsUnchanged'] and donor['LogsUnchanged'] and not donor['Failures']
        assert donor['PlanOrigin']=='actual-linux-compiler' and not donor['NetworkTests'] and not donor['NetworkSecurityTests']
        normalized={row['RelativePath']:row['NormalizedSha256'] for row in donor['SourceInputs']}
        sources={path.relative_to(root).as_posix():path for path in (root/'src').rglob('*') if path.is_file()}
        assert sources.keys()==normalized.keys()
        for name,path in sources.items(): assert hashlib.sha256(path.read_bytes().replace(b'\r\n',b'\n')).hexdigest()==normalized[name],name
        for symbol,plan in donor['Plans'].items():
            row=next(row for row in donor['Rows'] if row['Name']==symbol+'.plan')
            log=args.native_plan_report.parent/(symbol+'.plan.log')
            assert digest(log)==row['Sha256'] and json.loads(log.read_bytes())==plan
            inputs.add(log.resolve())
    else: assert not args.native_plan_report
    out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Failures=[],NetworkTests=False,
                NetworkSecurityTests=False,PublicChannelSelectCutover=False,PlanOrigin='verified-linux-donor' if win else 'actual-linux-compiler',
                Inputs=[dict(Path=str(path),Sha256=digest(path)) for path in sorted(inputs)],Rows=[],Plans={},Prefixes={},Products=[])
    report['SourceInputs']=[dict(RelativePath=path.relative_to(root).as_posix(),
        NormalizedSha256=hashlib.sha256(path.read_bytes().replace(b'\r\n',b'\n')).hexdigest())
        for path in sorted((root/'src').rglob('*')) if path.is_file()]
    env=dict(os.environ)
    env['PATH']=str(root/'release')+os.pathsep+env.get('PATH','')
    if not win: env['LD_LIBRARY_PATH']=str(root/'release')+os.pathsep+env.get('LD_LIBRARY_PATH','')
    def save(): (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,timeout=180):
        result=subprocess.run(list(map(str,command)),env=env,cwd=out,capture_output=True,timeout=timeout)
        log=out/(name+'.log'); log.write_bytes(result.stdout+result.stderr)
        report['Rows'].append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,Log=str(log),Sha256=digest(log)))
        if result.returncode: report['Failures'].append(name)
        save(); assert result.returncode==0,(name,result.returncode)
        return result.stdout
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
           '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    def links(product): return [str(product)] if win else ['-L'+str(root/'release'),'-l:'+product.name,'-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
    save()
    try:
        symbols=re.findall(r'XL6_TEXT_EXPORT\((\w+), CHANNEL_CASE(?:_SELECT(?:_CANCEL)?)?\)',
            (root/'src/runtime/xl6_text_symbols.inc').read_text(encoding='utf8'))
        assert len(symbols)==17
        report['Exports']={}
        for product in products:
            library=ctypes.CDLL(str(product)); assert all(getattr(library,name,None) for name in symbols)
            assert getattr(library,'xl6ChannelSchemaCopyV1',None) and getattr(library,'xl6OwnershipDomainChannelCaseV1Get',None)
            report['Exports'][product.name]=symbols+['xl6ChannelSchemaCopyV1','xl6OwnershipDomainChannelCaseV1Get']
            for level in ('0','2'):
                lane=product.stem+'.O'+level; binary=out/(lane+('.exe' if win else ''))
                run(lane+'.build',[cc,*flags,'-O'+level,fixture,memory,*links(product),'-o',binary])
                report['Products'].append(dict(Path=str(binary),Sha256=digest(binary)))
                run(lane+'.run',[binary],60); prefixes={}
                for operation in ('create','read','small','large','send','send-large','cancel'):
                    for point in range(128):
                        output=run(lane+'.'+operation+'.'+str(point),[binary,operation,point],60)
                        if b'hit=0 complete-prefix balanced' in output: break
                    else: raise AssertionError('Unfinished allocation prefix: '+operation)
                    prefixes[operation]=point
                report['Prefixes'][lane]=prefixes
        planner=out/'planner'
        if not win: run('planner.build',[cc,*flags,'-O2','-I'+str(root/'lib/tcc'),plannerSource,*links(products[1]),'-o',planner])
        selected={'xl6_channel_case.c','xl6_channel_cases_select.c','xl6_channel_cases_select_cancel.c',
                  'xl6_channel_select.c','xl6_channel_select_cancel.c','xl6_channel_wait.c','xl6_channel_cancel.c'}
        roots=(('xl6ChannelReceiveCaseCreateV1',{'xl6_channel_case.c'},set()),
               ('xl6ChannelCasesSelectV1',{'xl6_channel_case.c','xl6_channel_cases_select.c','xl6_channel_select.c'},{'XRT_MODULE_CHANNEL_SELECT'}),
               ('xl6ChannelCasesSelectCancelV1',{'xl6_channel_case.c','xl6_channel_cases_select_cancel.c','xl6_channel_select.c',
                                              'xl6_channel_select_cancel.c'},{'XRT_MODULE_CHANNEL_SELECT','XRT_MODULE_CHANNEL_SELECT_CANCEL'}))
        registry=(root/'src/compile/xl6_compile_unit.c').read_text(encoding='utf8').split('xl6ArtifactComponents[] = {',1)[1].split('\n};',1)[0]
        catalog=set(re.findall(r'^\s*\{\s*"([^"\n]+\.c)"',registry,re.M))
        for kind,(symbol,expected,modules) in enumerate(roots,1):
            plan=donor['Plans'][symbol] if win else json.loads(run(symbol+'.plan',[planner,symbol]))
            report['Plans'][symbol]=plan
            assert {Path(path).name for path in plan['Sources']} & selected==expected
            assert all(path in catalog and (root/path).resolve().is_relative_to(root/'src') for path in plan['Sources'])
            assert all(re.fullmatch(r'(?:XRT|XRUNTIME)_MODULE_[A-Z0-9_]+',name) for name in plan['NativeModules'])
            assert modules<=set(plan['NativeModules'])
            assert not ({'XRT_MODULE_CHANNEL_CANCEL','XRT_MODULE_CHANNEL_SELECT','XRT_MODULE_CHANNEL_SELECT_CANCEL'}-modules)&set(plan['NativeModules'])
            native=out/(symbol+'.native.c'); native.write_text(''.join('#define '+name+'\n' for name in plan['NativeModules'])+
                '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf8')
            report['Products'].append(dict(Path=str(native),Sha256=digest(native)))
            binary=out/(symbol+'.pruned'+('.exe' if win else ''))
            systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-pthread','-ldl','-lm']
            run(symbol+'.pruned.build',[cc,*flags,'-O2','-DTEST_KIND='+str(kind),*([] if win else ['-D_GNU_SOURCE']),
                pruned,native,*[root/path for path in plan['Sources']],*systems,'-o',binary],360)
            report['Products'].append(dict(Path=str(binary),Sha256=digest(binary)))
            run(symbol+'.pruned.run',[binary],60)
            imports=run(symbol+'.pruned.imports',[inspector,'-p' if win else '-d',binary])
            assert not re.search(rb'(?:libxlc?\.so|\bxlc?\.dll)',imports,re.I)
        report['InputsUnchanged']=all(digest(row['Path'])==row['Sha256'] for row in report['Inputs'])
        report['LogsUnchanged']=all(digest(row['Log'])==row['Sha256'] for row in report['Rows'])
        report['ProductsUnchanged']=all(digest(row['Path'])==row['Sha256'] for row in report['Products'])
        assert report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged'] and not report['Failures']
        report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Native Channel owned-case actual products and independent closures passed:',len(report['Rows']),'phases',flush=True)
if __name__=='__main__': main()
