"""Independently rehash the raw AES/Poly1305 source cohort and acceptance files."""
from pathlib import Path
import argparse,hashlib,json,os

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['runtime','public','build','out']:parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();out=args.out.resolve();assert not out.exists();out.mkdir(parents=True)
    expected={};reports=[];stages=0
    def add(path,digest):
        path=str(Path(path).resolve());digest=digest.lower()
        assert path not in expected or expected[path]==digest,path
        expected[path]=digest
    for path,count in [(args.runtime.resolve(),9 if os.name=='nt' else 11),(args.public.resolve(),40)]:
        data=json.loads(path.read_text(encoding='utf-8-sig'))
        assert data['Complete'] and data['InputsUnchanged'] and not data.get('Failures')
        assert not data['NetworkTraffic'] and not data['SecurityTests'] and not data['ShippingAccepted']
        assert len(data['Rows'])==count
        for row in data['Rows']:
            assert (row['Exit']!=0 if row.get('ExpectedFailure') else row['Exit']==0)
            add(row['Log'],row['Sha256'])
        for source,digest in data['Inputs'].items():add(source,digest)
        for product,digest in data['Products'].items():add(product,digest)
        add(path,sha(path));stages+=count
        reports.append(dict(Path=str(path),Sha256=sha(path),Stages=count))
    public=json.loads(args.public.read_text(encoding='utf-8-sig'))
    assert sum(bool(row.get('ExpectedFailure')) for row in public['Rows'])==2
    build=json.loads(args.build.read_text(encoding='utf-8-sig'))
    assert build.get('Complete',os.name=='nt') and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly']
    assert len(build['Objects'])==len(build['Sources'])==(1295 if os.name=='nt' else 1294)
    for row in build['Inputs']+build['Headers']+build['Objects']:add(row['Path'],row['Sha256'])
    add(build['Driver'],build['DriverSha256']);add(args.build,sha(args.build))
    add(Path(__file__).resolve(),sha(Path(__file__).resolve()))
    for path,digest in expected.items():assert sha(path)==digest,path
    result=dict(Complete=True,Platform='windows' if os.name=='nt' else 'linux',Stages=stages,
        FullSourceTranslationUnits=len(build['Sources']),Rehashed=len(expected),Reports=reports,Inputs=expected,
        ShippingAccepted=False,FullGoalComplete=False,NetworkTraffic=False,SecurityTests=False)
    (out/'proof.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print(result['Platform'],stages,'stages;',len(expected),'independently rehashed files',flush=True)

if __name__=='__main__':main()
