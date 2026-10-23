"""Normal fixed-key DNS/loopback-IP certificates; no network/security tests."""
from datetime import datetime,timezone
from ipaddress import ip_address
from pathlib import Path
import hashlib,json
from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from check_key_format_wire import vectors

def fixtures(root,out):
    out=Path(out);rows=[];lines=['/* Normal public fixed-key certificates; signature checked independently. */',
        'typedef struct IdentityVector {const char* name;xbytesview der;} IdentityVector;'];items=[]
    for i,vector in enumerate(vectors(root)[:4]):
        name=vector['name'];key=vector['key'];subject=x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'offline identity '+name)])
        cert=(x509.CertificateBuilder().subject_name(subject).issuer_name(subject).public_key(key.public_key()).serial_number(100+i)
            .not_valid_before(datetime(2020,1,1,tzinfo=timezone.utc)).not_valid_after(datetime(2040,1,1,tzinfo=timezone.utc))
            .add_extension(x509.BasicConstraints(ca=False,path_length=None),True)
            .add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost'),x509.IPAddress(ip_address('127.0.0.1')),x509.IPAddress(ip_address('::1'))]),False)
            .sign(key,None if name=='ed25519' else hashes.SHA256()))
        cert.verify_directly_issued_by(cert);data=cert.public_bytes(serialization.Encoding.DER);path=out/(name+'.identity.der');path.write_bytes(data)
        symbol='identity_'+name;lines.append('static const uint8 '+symbol+'[]={'+','.join(hex(b) for b in data)+'};');items.append('{"'+name+'",{'+symbol+',sizeof('+symbol+')}}')
        rows.append(dict(Name=name,Path=str(path),Sha256=hashlib.sha256(data).hexdigest(),SignatureIndependentlyVerified=True))
    lines.append('static const IdentityVector identityVectors[]={'+','.join(items)+'};');header=out/'x509_identity_vectors.h';header.write_text('\n'.join(lines)+'\n')
    (out/'fixtures.json').write_text(json.dumps(dict(Complete=True,NormalFamilies=4,EverySignatureIndependentlyVerified=True,Files=rows,HeaderSha256=hashlib.sha256(header.read_bytes()).hexdigest(),NetworkTraffic=False,SecurityTests=False),indent=2)+'\n')
    return rows
