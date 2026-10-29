"""Ordinary fixed public test keys and certificates; no network/security suite."""
from pathlib import Path
import hashlib,json
from datetime import datetime,timezone
from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from check_key_format_wire import vectors
from x509_identity_fixtures import fixtures as certificates

def fixtures(root,out):
    out=Path(out);rows=certificates(root,out);lines=[];items=[]
    for index,vector in enumerate(vectors(root)[:4]):
        name=vector['name'];data=vector['inner'] if name=='rsa' else vector['raw']
        symbol='tls_key_'+name
        lines.append('static const uint8 '+symbol+'[]={'+','.join(hex(b) for b in data)+'};')
        items.append('{'+symbol+',sizeof('+symbol+')}')
        path=out/(name+'.tls-key.der' if name=='rsa' else name+'.tls-key.raw');path.write_bytes(data)
        pkcs8=vector['key'].private_bytes(serialization.Encoding.DER,serialization.PrivateFormat.PKCS8,serialization.NoEncryption())
        pkcs8Path=out/(name+'.tls-key.pkcs8');pkcs8Path.write_bytes(pkcs8)
        rows[index]['KeyPath']=str(path);rows[index]['KeySha256']=hashlib.sha256(data).hexdigest()
        rows[index]['Pkcs8Path']=str(pkcs8Path);rows[index]['Pkcs8Sha256']=hashlib.sha256(pkcs8).hexdigest()
    lines.append('static const xbytesview tlsIdentityKeys[]={'+','.join(items)+'};')
    # A normal nine-certificate chain exercises the adapter's heap-view branch.
    # The same public fixed P256 test key keeps the fixture deterministic in
    # identity; every DER signature and issuer relationship is checked here.
    key=vectors(root)[1]['key'];chain=[];chainRows=[];chainItems=[]
    names=[x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'offline TLS level '+str(i))]) for i in range(9)]
    for i in range(9):
        cert=(x509.CertificateBuilder().subject_name(names[i]).issuer_name(names[min(i+1,8)]).public_key(key.public_key())
            .serial_number(1000+i).not_valid_before(datetime(2020,1,1,tzinfo=timezone.utc)).not_valid_after(datetime(2040,1,1,tzinfo=timezone.utc))
            .add_extension(x509.BasicConstraints(ca=i>0,path_length=i-1 if i else None),True).sign(key,hashes.SHA256()))
        chain.append(cert);data=cert.public_bytes(serialization.Encoding.DER);path=out/('long-chain-'+str(i)+'.der');path.write_bytes(data)
        symbol='tls_chain_'+str(i);lines.append('static const uint8 '+symbol+'[]={'+','.join(hex(b) for b in data)+'};')
        chainItems.append('{'+symbol+',sizeof('+symbol+')}');chainRows.append(dict(Path=str(path),Sha256=hashlib.sha256(data).hexdigest()))
    for i,cert in enumerate(chain):cert.verify_directly_issued_by(chain[min(i+1,8)])
    lines.append('static const xbytesview tlsIdentityLongChain[]={'+','.join(chainItems)+'};')
    (out/'tls_identity_keys.h').write_text('\n'.join(lines)+'\n')
    (out/'tls-fixtures.json').write_text(json.dumps(dict(Complete=True,NormalFamilies=4,EverySignatureIndependentlyVerified=True,
        FixedPublicTestKeys=True,Files=rows,LongChain=chainRows,LongChainEverySignatureIndependentlyVerified=True,
        NetworkTraffic=False,SecurityTests=False),indent=2)+'\n')
    return rows
