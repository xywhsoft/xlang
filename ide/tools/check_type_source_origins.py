"""Compare audited synthetic type origins and complete accepted/current IR.

Build a diagnostic semantic TU for each frozen producer. The common fixture
expects the baseline's missing metadata and the candidate's explicit origin.
All 14 complete language reports and IR dumps must remain byte-identical.
No production test hook, cache relaxation or timing claim is involved.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,sys


def verify(source: Path, output: Path, variants: dict):
    read=lambda p:json.loads(Path(p).read_text(encoding='utf8'));sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    output.mkdir(parents=True,exist_ok=False);common=output/'common';common.mkdir();commands=[];inputs={};outputs={};counts={}
    fixture=source/'tests/analysis_type_source_fixture.c';copied=output/'fixture.c';shutil.copy2(fixture,copied)
    inputs[str(fixture)]=inputs[str(copied)]=sha(fixture)
    report=dict(status='failed',scope=__doc__,commands=commands,inputs=inputs,outputs=outputs,checks=counts)
    sys.path.insert(0,str(source/'tools'));from build_lsp_core import dependencies
    def run(name,command,cwd):
        cp=subprocess.run(command,capture_output=True,cwd=cwd,timeout=900);log=output/(name+'.log');log.write_bytes(cp.stdout+cp.stderr)
        commands.append(dict(name=name,command=command,exit=cp.returncode,log=str(log),logSha256=sha(log)))
        print(name,cp.returncode,(cp.stdout+cp.stderr).decode('utf8','replace')[-1800:],flush=True);assert not cp.returncode,name
        return cp.stdout.decode('utf8')
    try:
        for name in ['before','after']:
            variant=variants[name];native=Path(variant['native']);raw=read(native/'build-result.json');assert raw['status']=='passed'
            owner=Path(variant['source']);sem=Path(variant.get('sem',native/'frontend/xl6_sem.c'));archive=Path(variant.get('archive',native/'xlang-analysis-core.a'))
            folder=output/name;folder.mkdir();obj=folder/'type_origin.o';exe=common/('type_origin_'+name+'.exe');dep=folder/'type_origin.d';linkmap=folder/'type_origin.map'
            template=next(c for c in raw['commands'] if '-c' in c and c[c.index('-o')+1].endswith('analysis_ir_memo_test.o'))
            prefix=[template[0],'-I'+str(owner/'src'),*template[1:template.index('-c')]]
            compile=[*prefix,'-DXLS_TYPE_SOURCE_EXPECT='+str(int(name=='after')),'-DXLS_TYPE_SOURCE_FIXTURE="'+sem.as_posix()+'"','-MMD','-MT','typeOriginObject','-MF',str(dep),'-c',str(copied),'-o',str(obj)]
            link=next(c for c in raw['commands'] if '-c' not in c and '-o' in c and c[c.index('-o')+1].endswith('analysis_ir_memo_test.exe'))
            link=[str(obj) if x.endswith('analysis_ir_memo_test.o') else str(archive) if x.endswith('xlang-analysis-core.a') else x for x in link];link[link.index('-o')+1]=str(exe);link+=['-Wl,-Map,'+str(linkmap)]
            run(name+'-compile',compile,folder);run(name+'-link',link,folder)
            stdout=run(name+'-run',[str(exe),str(folder),str(common/'entry.xl'),str(common/'type_origin_defs.xl')],common)
            import re
            counts[name]=int(re.search(r'passed: (\d+) checks',stdout)[1])
            assert 'xlang-analysis-core.a(xl6_sem.o)' not in linkmap.read_text(),name
            for p in [native/'build-result.json',sem,archive,*dependencies(dep.read_text())]:
                key=str(p);h=sha(p)
                if key in inputs:assert inputs[key]==h,key
                inputs[key]=h
            for p in folder.rglob('*'):
                if p.is_file():outputs[str(p)]=sha(p)
            outputs[str(exe)]=sha(exe)
        comparisons={}
        for i in range(14):
            for prefix in ['corpus','ir']:
                name=f'{prefix}-{i}.json';before=output/'before'/name;after=output/'after'/name
                assert before.read_bytes()==after.read_bytes(),name
                comparisons[name]=dict(before=str(before),after=str(after),sha256=sha(before))
        assert all(sha(p)==h for p,h in inputs.items());report.update(status='passed',comparisons=comparisons,normalProductInstrumented=False)
    finally:(output/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print('Type origins and14 complete baseline/candidate language+IR corpora passed',counts,flush=True)
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native',type=Path,required=True);parser.add_argument('--baseline-native',type=Path,required=True);parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();native=args.native.resolve(strict=True);baseline=args.baseline_native.resolve(strict=True);source=args.source.resolve(strict=True)
    verify(source,args.output.resolve(),dict(before=dict(native=baseline,source=baseline.parent),after=dict(native=native,source=source)))


if __name__=='__main__':main()
