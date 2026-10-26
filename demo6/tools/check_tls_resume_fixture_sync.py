"""Check committed normal resume fixtures against their authoritative generator.
No runtime/cryptographic/network test or source-provider replacement.
"""
from pathlib import Path
import argparse,hashlib,json,re,sys
from cryptography import x509
from tls_client_resume_fixtures import fixtures
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
CERTIFICATE=re.compile(r'Certificate\.fromDer\(b"((?:\\x[0-9a-fA-F]{2})+)"\)')

def normal(path):
    """Compare every statement and certificate field, not random ECDSA nonces.
    A fresh generator signs the same fixed-key certificate again. Only the
    verified signature BIT STRING is omitted; TBS and outer algorithm TLVs
    remain byte-exact. Neither issuer/key/validity/policy nor source assertions
    may drift. This is not a substitute for executing the shipping fixtures.
    """
    source=Path(path).read_text(encoding='utf8')
    matches=list(CERTIFICATE.finditer(source));assert len(matches)==3,path
    values=[bytes.fromhex(m[1].replace('\\x','')) for m in matches]
    certificates=[x509.load_der_x509_certificate(value) for value in values]
    assert len({cert.subject.public_bytes() for cert in certificates})==3
    by_subject={cert.subject.public_bytes():cert for cert in certificates}
    for cert in certificates:
        cert.verify_directly_issued_by(by_subject[cert.issuer.public_bytes()])
    def tlv(data,offset):
        start=offset;offset+=1;n=data[offset];offset+=1
        if n & 128:
            count=n & 127;assert 0 < count <= 4
            n=int.from_bytes(data[offset:offset+count],'big');offset+=count
        end=offset+n;assert end<=len(data)
        return start,offset,end
    canonical=[]
    for value in values:
        _,start,end=tlv(value,0);assert value[0]==0x30 and end==len(value)
        _,_,tbs_end=tlv(value,start);assert value[start]==0x30
        _,_,algorithm_end=tlv(value,tbs_end);assert value[tbs_end]==0x30
        _,_,signature_end=tlv(value,algorithm_end)
        assert value[algorithm_end]==0x03 and signature_end==end
        canonical.append(value[start:algorithm_end].hex())
    iterator=iter(canonical)
    return CERTIFICATE.sub(lambda _: 'Certificate.fromDer(verifiedTbsAndAlgorithm="'+next(iterator)+'")',source)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['root','out']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    generated=fixtures(root,out);assert len(generated)==4
    selected=[root/'test/cases/tls_public/tls_client_resume.xl',root/'test/cases/tls_public/tls_client_resume_stream.xl']
    rows=[]
    for actual,expected in zip(selected,generated[:2]):
        assert normal(actual)==normal(expected),str(actual)
        rows.append(dict(Path=str(actual),Sha256=sha(actual),Generated=str(expected),GeneratedSha256=sha(expected)))
    modules={Path(module.__file__).resolve() for module in sys.modules.values()
        if getattr(module,'__file__',None) and Path(module.__file__).suffix=='.py' and
        Path(module.__file__).resolve().is_relative_to(Path(__file__).resolve().parent)}
    modules.add(Path(__file__).resolve())
    inputs={str(path):sha(path) for path in sorted(modules|set(selected))}
    (out/'results.json').write_text(json.dumps(dict(Complete=True,Rows=rows,Inputs=inputs,
        GeneratedFiles={str(path):sha(path) for path in out.rglob('*') if path.is_file()},
        Scope=__doc__,RuntimeAcceptance=False),indent=2)+'\n')
    print('Two normal resume fixtures match: exact statements/TBS/algorithm; every ECDSA signature verified')
if __name__=='__main__':main()
