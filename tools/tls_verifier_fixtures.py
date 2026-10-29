"""Normal offline TLS trust/name/role fixtures; no network/security campaigns."""
from datetime import datetime,timezone
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from check_key_format_wire import vectors
from x509_path_fixtures import fixtures as paths

def fixtures(root,out):
    out=Path(out);assert paths(root,out)==4
    keys=vectors(root)[:4];records=[];lines=[(out/'x509_path_vectors.h').read_text()]
    before=datetime(2020,1,1,tzinfo=timezone.utc);after=datetime(2040,1,1,tzinfo=timezone.utc)
    for row in keys:
        name=row['name'];key=row['key'];issuer=x509.load_der_x509_certificate((out/(name+'.mid.der')).read_bytes())
        subject=x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'ordinary offline client')])
        client=(x509.CertificateBuilder().subject_name(subject).issuer_name(issuer.subject).public_key(key.public_key()).serial_number(5)
            .not_valid_before(before).not_valid_after(after).add_extension(x509.BasicConstraints(ca=False,path_length=None),True)
            .add_extension(x509.KeyUsage(digital_signature=True,content_commitment=False,key_encipherment=False,data_encipherment=False,key_agreement=False,key_cert_sign=False,crl_sign=False,encipher_only=False,decipher_only=False),True)
            .add_extension(x509.ExtendedKeyUsage([x509.ExtendedKeyUsageOID.CLIENT_AUTH]),False).sign(key,None if name=='ed25519' else hashes.SHA256()))
        client.verify_directly_issued_by(issuer);data=client.public_bytes(serialization.Encoding.DER);(out/(name+'.client.der')).write_bytes(data)
        symbol='tls_'+name+'_client';lines.append('static const uint8 '+symbol+'[]={'+','.join(hex(b) for b in data)+'};')
        records.append('{'+symbol+',sizeof('+symbol+')}')
    lines.append('static const xbytesview tlsClients[]={'+','.join(records)+'};')
    # A normal nine-certificate path exercises the heap view path, not a giant
    # synthetic count or invalid DER. Every signature is independently checked.
    key=keys[1]['key'];parent=None;chain=[]
    for i in range(10):
        ca=i!=9;name=x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'ordinary deep '+str(i))])
        builder=(x509.CertificateBuilder().subject_name(name).issuer_name(parent.subject if parent else name).public_key(key.public_key()).serial_number(100+i)
            .not_valid_before(before).not_valid_after(after).add_extension(x509.BasicConstraints(ca=ca,path_length=12 if ca else None),True)
            .add_extension(x509.KeyUsage(digital_signature=True,content_commitment=False,key_encipherment=False,data_encipherment=False,key_agreement=False,key_cert_sign=ca,crl_sign=ca,encipher_only=False,decipher_only=False),True))
        if not ca:builder=builder.add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost')]),False).add_extension(x509.ExtendedKeyUsage([x509.ExtendedKeyUsageOID.SERVER_AUTH]),False)
        cert=builder.sign(key,hashes.SHA256());cert.verify_directly_issued_by(parent or cert);chain.append(cert);parent=cert
    records=[]
    for i,cert in enumerate(chain):
        data=cert.public_bytes(serialization.Encoding.DER);(out/('deep.'+str(i)+'.der')).write_bytes(data);symbol='tls_deep_'+str(i)
        lines.append('static const uint8 '+symbol+'[]={'+','.join(hex(b) for b in data)+'};');records.append('{'+symbol+',sizeof('+symbol+')}')
    lines.append('static const xbytesview tlsDeep[]={'+','.join(records)+'};')
    (out/'tls_verifier_vectors.h').write_text('\n'.join(lines)+'\n')
    return ['rsa','p256','p384','ed25519']

def language_fixtures(root,out):
    out=Path(out);names=fixtures(root,out);cases=[];literal=lambda data:'b"'+''.join('\\x%02x'%b for b in data)+'"'
    for name in names:
        rootDer=literal((out/(name+'.root.der')).read_bytes());leaf=literal((out/(name+'.leaf.der')).read_bytes());mid=literal((out/(name+'.mid.der')).read_bytes());client=literal((out/(name+'.client.der')).read_bytes())
        text='''func make() as TlsVerifier
 auto store = TrustStore()
 assert store.add(Certificate.fromDer('''+rootDer+'''))
 auto policy = TlsVerificationPolicy().withTime(time(1893456000000000))
 return TlsVerifier(store.snapshot(), policy)
end
auto verifier = make()
array<Certificate> server = [Certificate.fromDer('''+leaf+'''), Certificate.fromDer('''+mid+''')]
array<Certificate> client = [Certificate.fromDer('''+client+'''), server[1]]
verifier.verifyServer(server, "localhost")
verifier.verifyServer(server, "localhost:unselected", 9)
verifier.verifyServer(server, "localhost", 100)
verifier.verifyClient(client)
assert verifier.policy.fixedTime == time(1893456000000000)
assert not verifier.policy.usesCurrentTime and not verifier.policy.allowsSha1
'''
        path=out/('tls.verifier.'+name+'.xl');path.write_text(text);cases.append(path)
    # Both constructor overloads and all current targets appear in the IR
    # contract input; system-store factory is deliberately not executed.
    contract=out/'tls.verifier.contract.xl';contract.write_text((cases[0]).read_text()+'''
auto trust = TrustStore()
assert trust.add(Certificate.fromDer('''+literal((out/'rsa.root.der').read_bytes())+'''))
auto defaults = TlsVerifier(trust.snapshot())
assert defaults.policy.usesCurrentTime
'''+(root/'test/cases/stdlib_tls_verification_policy.xl').read_text())
    deep=out/'tls.verifier.deep.xl';deep.write_text('''func make() as TlsVerifier
 auto store=TrustStore()
 assert store.add(Certificate.fromDer('''+literal((out/'deep.0.der').read_bytes())+'''))
 return TlsVerifier(store.snapshot(),TlsVerificationPolicy().withTime(time(1893456000000000)))
end
auto verifier=make()
array<Certificate> chain=['''+','.join('Certificate.fromDer('+literal((out/('deep.'+str(i)+'.der')).read_bytes())+')' for i in range(9,0,-1))+''']
verifier.verifyServer(chain,"localhost")
''');cases.append(deep)
    owned=out/'tls.verifier.owned.xl';owned.write_text((cases[1]).read_text()+'''
func reader(TlsVerifier verifier,Certificate leaf,Certificate mid) as func() as int
 return func() as int
  array<Certificate> chain=[leaf,mid]
  verifier.verifyServer(chain,"localhost")
  if verifier.policy.fixedTime == time(1893456000000000) then return 0 end
  return 1
 end
end
auto first=thread(reader(verifier,server[0],server[1]))
auto second=thread(reader(verifier,server[0],server[1]))
assert first.waitTimeout(30000) and second.waitTimeout(30000)
assert first.exitCode() == 0 and second.exitCode() == 0
''');cases.append(owned)
    configure=out/'tls.verifier.configure.xl';configure.write_text('''auto store=TrustStore()
assert store.add(Certificate.fromDer('''+literal((out/'p256.root.der').read_bytes())+'''))
auto verifier=TlsVerifier(store.snapshot())
assert verifier.policy.usesCurrentTime and not verifier.policy.allowsSha1
''');cases.append(configure)
    return cases,contract
