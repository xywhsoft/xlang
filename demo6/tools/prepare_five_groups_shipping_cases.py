"""Prepare the existing complete ordinary manifests plus current resume policies.
Fixed valid offline material, ordinary contracts and self-owned literal loopback.
No attack, malformed-crypto or public-network test campaign is added.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,sys
from tls_resume_peer_fixtures import fixtures

MANIFESTS=['concurrency_public_fixtures.json','crypto_surface_public_fixtures.json','crypto_advanced_public_fixtures.json',
    'x509_public_fixtures.json','x509_policy_public_fixtures.json',
    'tls_surface_public_fixtures.json','tls_negotiation_public_fixtures.json','network_surface_public_fixtures.json']
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['root','out']:parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve();assert not out.exists()
    out.mkdir(parents=True);cases={};support=set();pinned={Path(__file__).resolve()};populations=[]
    for name in MANIFESTS:
        path=root/'tools'/name;pinned.add(path);data=json.loads(path.read_text(encoding='utf8'))
        populations.append(dict(Manifest=name,Cases=len(data['Cases'])))
        for row in data['Cases']:
            assert row['Name'] not in cases or cases[row['Name']]==row
            cases[row['Name']]=row
        support.update(data.get('Support',[]))
    paths={row['Source'] for row in cases.values()}|support|{'test/cases/xlir_core_guard_source_host.c'}
    for relative in sorted(paths):
        source=(root/relative).resolve();assert source.is_relative_to(root) and source.is_file()
        target=out/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target);assert sha(source)==sha(target);pinned.add(source)
    generated=out/'test/cases/tls_resume_current';generated.mkdir()
    additions=fixtures(root,generated);assert len(additions)==6
    for path in additions:
        name='current_'+path.stem.replace('.','_');assert name not in cases
        cases[name]=dict(Name=name,Source=path.relative_to(out).as_posix(),Negative=False)
    # Pin the actual transitive local generator modules, not a hand-maintained
    # subset. A parent source freeze still binds the generator's data inputs.
    pinned.update(Path(module.__file__).resolve() for module in sys.modules.values()
        if getattr(module,'__file__',None) and Path(module.__file__).suffix=='.py' and
        Path(module.__file__).resolve().is_relative_to(Path(__file__).resolve().parent))
    manifest=out/'manifest.json';manifest.write_text(json.dumps(dict(Cases=list(cases.values()),Support=sorted(support)),indent=2)+'\n')
    report=dict(Complete=True,Root=str(root),Controls=str(out),Manifest=str(manifest),Populations=populations,
        Cases=len(cases),Positive=sum(not row['Negative'] for row in cases.values()),
        Negative=sum(row['Negative'] for row in cases.values()),NewCurrentResumeFixtures=len(additions),
        Scope=__doc__,FullGoalComplete=False,GeneratorDataProvenance='Bind the complete parent source-freeze manifest separately',
        Inputs={str(p):sha(p) for p in sorted(pinned)},
        Files={str(p):sha(p) for p in out.rglob('*') if p.is_file()})
    (out/'preparation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['Cases'],'ordinary cases;',report['Positive'],'positive;',report['Negative'],'semantic negatives',flush=True)

if __name__=='__main__':main()
