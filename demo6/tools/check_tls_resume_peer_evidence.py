"""Independently rehash the resume-peer cohort's inputs, logs and products."""
from pathlib import Path
import argparse, hashlib, json, os


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--evidence',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();base=a.evidence.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    platform='windows' if os.name=='nt' else 'linux'
    names=[('native-'+('win' if os.name=='nt' else 'linux')+'-r2',12 if os.name=='nt' else 16),
        ('revocation-runtime-'+platform+'-r1',9 if os.name=='nt' else 11),
        ('peer-runtime-'+platform+'-r1',9 if os.name=='nt' else 11),
        ('public-'+platform+'-r1',60),('migration-'+platform+'-r1',10),('contract-'+platform+'-r1',6)]
    expected={};rows=[];stages=0
    def add(path,digest):
        path=str(Path(path).resolve());assert path not in expected or expected[path]==digest.lower(),path
        expected[path]=digest.lower()
    for name,count in names:
        path=base/name/'results.json';report=json.loads(path.read_text(encoding='utf-8-sig'))
        assert report['Complete'] and report['InputsUnchanged'] and not report.get('Failures')
        assert len(report['Rows'])==count and all(row['Exit']==0 for row in report['Rows'])
        assert not report.get('NetworkTraffic',False) and not report.get('SecurityTests',False)
        for source,digest in report['Inputs'].items():add(source,digest)
        for row in report['Rows']:add(row['Log'],row['Sha256'])
        for product,digest in report.get('Products',{}).items():add(product,digest)
        add(path,sha(path));stages+=count
        rows.append(dict(Report=str(path),Sha256=sha(path),Stages=count))
    build=base/('source-'+platform+'-r1/build.json');data=json.loads(build.read_text(encoding='utf-8-sig'))
    assert data.get('Complete',os.name=='nt') and data['InputsUnchanged'] and data['SourceXrt'] and not data['RuntimeOnly']
    assert len(data['Objects'])==len(data['Sources'])==(1287 if os.name=='nt' else 1286)
    for row in data['Inputs']+data['Headers']+data['Objects']:add(row['Path'],row['Sha256'])
    add(data['Driver'],data['DriverSha256']);add(build,sha(build))
    for path,digest in expected.items():assert sha(path)==digest,path
    result=dict(Complete=True,Platform=platform,Stages=stages,Reports=rows,
        Rehashed=len(expected),FullSourceTranslationUnits=len(data['Sources']),
        ShippingAccepted=False,FullTlsAccepted=False,NetworkTraffic=False,SecurityTests=False,
        Inputs=expected)
    (out/'proof.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print(platform,stages,'successful stages;',len(expected),'independently rehashed files',flush=True)


if __name__=='__main__':main()
