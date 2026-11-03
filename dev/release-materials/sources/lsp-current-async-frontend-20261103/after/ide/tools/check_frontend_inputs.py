"""Exercise exact frontend preparation across real and synthetic checkouts."""
from pathlib import Path
import argparse,hashlib,json,subprocess
from native_c_interop_profile import pins_for,profile
from native_frontend_inputs import prepare


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--frontend-root',type=Path,required=True)
    p.add_argument('--previous-root',type=Path)
    p.add_argument('--other-revision',type=Path,help='reject each mixed file from the other audited revision')
    p.add_argument('--checkout-ref',help='also exercise the repository checkout filters at this commit')
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();frontend=a.frontend_root.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    repo=Path(__file__).resolve().parents[2];profile(frontend);PINS=pins_for(frontend)
    protected={str(frontend/rel):digest for rel,digest in PINS.items()}
    for name in (Path(__file__).name,'native_frontend_inputs.py','native_c_interop_profile.py'):
        path=Path(__file__).with_name(name).resolve();protected[str(path)]=sha(path)
    report=dict(status='running',scope=__doc__,protected=protected,cases=[])
    def save():(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
    def validated(name,source):
        result,metadata=prepare(source,out/(name+'-prepared'));profile(result)
        for rel,digest in PINS.items():assert sha(result/rel)==digest,rel
        receipt=out/(name+'-preparation.json');receipt.write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf8')
        report['cases'].append(dict(name=name,status='passed',transformations=len(metadata['transformations']),receipt=str(receipt),receiptSha256=sha(receipt)));save()
        return result,metadata
    try:
        result,metadata=validated('exact',frontend)
        assert result==frontend and metadata['status']=='exact' and not (out/'exact-prepared').exists()
        for mode in ('lf','crlf'):
            source=out/(mode+'-checkout');source.mkdir()
            for rel in PINS:
                data=(frontend/rel).read_bytes().replace(b'\r\n',b'\n')
                if mode=='crlf':data=data.replace(b'\n',b'\r\n')
                path=source/rel;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
            _,metadata=validated(mode,source);assert metadata['status']=='canonicalized'
        if a.checkout_ref:
            head=subprocess.check_output(['git','rev-parse','--verify','--end-of-options',a.checkout_ref+'^{commit}'],cwd=repo,text=True).strip()
            attributes=repo/'.gitattributes';protected[str(attributes)]=sha(attributes)
            source=out/'git-checkout';source.mkdir();commands=[]
            for rel in PINS:
                command=['git','cat-file','--filters',head+':'+rel];commands.append(command)
                data=subprocess.check_output(command,cwd=repo);path=source/rel;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
            validated('git',source);report['gitCheckout']=dict(commit=head,commands=commands);save()
        if a.previous_root:
            previous=a.previous_root.resolve(strict=True)
            for rel in ('src/ast/xl6_ast.h','lib/xrt/xrt.h'):protected[str(previous/rel)]=sha(previous/rel)
            result,metadata=prepare(previous,out/'previous-prepared')
            assert result==previous and metadata['status']=='unchanged-other-cohort' and not (out/'previous-prepared').exists()
            report['cases'].append(dict(name='previous-cohort-untouched',status='passed'));save()
        if a.other_revision:
            other=a.other_revision.resolve(strict=True);profile(other);other_pins=pins_for(other)
            changed=[rel for rel in PINS if PINS[rel]!=other_pins[rel]]
            assert len(changed)==4,changed
            for index,rel in enumerate(changed):
                protected[str(other/rel)]=other_pins[rel]
                source=out/'lf-checkout';path=source/rel;original=path.read_bytes()
                path.write_bytes((other/rel).read_bytes().replace(b'\r\n',b'\n'))
                destination=out/('mixed-'+str(index)+'-prepared')
                try:
                    try:prepare(source,destination)
                    except ValueError as error:
                        assert 'beyond an audited EOL counterpart' in str(error)
                        report['cases'].append(dict(name='mixed-revision-'+rel,status='passed',error=str(error)));save()
                    else:raise AssertionError('mixed revision admitted: '+rel)
                    assert not destination.exists()
                finally:path.write_bytes(original)
        source=out/'lf-checkout';path=source/'src/sem/xl6_sem.c';original=path.read_bytes();path.write_bytes(original+b'\n')
        try:
            try:prepare(source,out/'unknown-prepared')
            except ValueError as error:
                assert 'beyond an audited EOL counterpart' in str(error)
                report['cases'].append(dict(name='extra-newline-refused',status='passed',error=str(error)));save()
            else:raise AssertionError('unknown content admitted')
            assert not (out/'unknown-prepared').exists()
        finally:path.write_bytes(original)
        # Mutations happen only in this test's synthetic checkout. Both an
        # upcoming copy and an already copied source must revoke preparation.
        for name,rel in (('during-copy','src/ast/xl6_ast.h'),('after-copy','lib/xrt/xrt.h')):
            path=source/rel;original=path.read_bytes();destination=out/(name+'-prepared');write=Path.write_bytes;mutations=[]
            def intervene(target,data):
                count=write(target,data)
                if target.is_relative_to(destination) and not mutations:
                    write(path,original.replace(b'\n',b'\r\n'));mutations.append(str(path))
                return count
            Path.write_bytes=intervene
            try:
                try:prepare(source,destination)
                except ValueError as error:
                    assert 'changed during EOL preparation' in str(error) and len(mutations)==1
                    report['cases'].append(dict(name=name,status='passed',error=str(error)))
                else:raise AssertionError('changed source preparation published')
            finally:Path.write_bytes=write;path.write_bytes(original)
            save()
        for path,digest in protected.items():assert sha(path)==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),report=str(out/'result.json'))))


if __name__=='__main__':main()
