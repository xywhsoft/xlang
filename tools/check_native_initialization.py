"""Non-null native defaults reject in semantic checking; nullable/defaultable
controls keep their existing contracts. Source compiler only, no foreign calls.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,os


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','driver','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();driver=args.driver.resolve();out=args.out.resolve()
    assert not out.exists(), 'Preserve prior initialization evidence'
    out.mkdir(parents=True)
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    inputs={str(p):sha(p) for p in (Path(__file__).resolve(),driver)}
    provider=root/('release/xl.dll' if os.name=='nt' else 'release/libxl.so')
    inputs[str(provider)]=sha(provider)
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(root))
    native='class NativeProbe\n native point handle\nend\n'
    cases={
        'global':(native+'NativeProbe value\n',False),
        'local':(native+'func check() as int\n NativeProbe value\n return 0\nend\n',False),
        'field':(native+'class Owner\n NativeProbe value\nend\n',False),
        'static-field':(native+'class Owner\n static NativeProbe value\nend\n',False),
        'inherited':(native+'class Child extends NativeProbe\nend\nChild value\n',False),
        'alias':(native+'typedef Alias = NativeProbe\nAlias value\n',False),
        'multiple-native-fields':('class Pair\n native point first\n native point second\nend\nPair value\n',False),
        'nullable':(native+'NativeProbe? global\nclass Owner\n NativeProbe? member\n static NativeProbe? shared\nend\nfunc check() as int\n NativeProbe? local\n return 0\nend\n',True),
        'nullable-alias':(native+'typedef MaybeNative = NativeProbe?\nMaybeNative value\n',True),
        'plain-class':('class Plain\n point handle\nend\nPlain value\n',True),
        'builtin-default':('mutex value\n',True),
        'raw-point-alias':('extern type HANDLE = point\nHANDLE value\n',True),
        'explicit-native':(native+'point raw = null\nNativeProbe value = NativeProbe(raw)\n',True),
    }
    rows=[]
    for name,(text,accepted) in cases.items():
        source=out/(name+'.xl');source.write_text(text,encoding='utf-8');inputs[str(source)]=sha(source)
        for mode in (('ir-typed','ir-lowered','cgen-default') if accepted else ('ir-typed','cgen-default')):
            output=out/(name+'.'+mode+('.c' if mode=='cgen-default' else '.json'));log=out/(name+'.'+mode+'.log')
            with log.open('wb') as stream:
                code=subprocess.run([str(driver),str(source),mode,str(output)],cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT).returncode
            if accepted:
                assert code==0 and output.is_file(),log
            else:
                assert code!=0 and not output.exists(),log
                assert 'non-null native resource needs explicit initialization or nullable storage' in log.read_text(encoding='utf-8-sig'),log
            rows.append(dict(Name=name,Mode=mode,Accepted=accepted,Exit=code,Log=str(log),Sha256=sha(log)))
    assert all(sha(Path(p))==h for p,h in inputs.items())
    products={str(p):sha(p) for p in out.iterdir() if p.is_file()}
    report=dict(Complete=True,InputsUnchanged=True,Inputs=inputs,Rows=rows,Products=products,
                Cases=len(cases),SemanticRefusals=sum(not item[1] for item in cases.values()),
                Scope=__doc__,ShippingAccepted=False,FullGoalComplete=False)
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(len(cases),'initialization cases;',len(rows),'actual compiler stages passed')


if __name__=='__main__':
    main()
